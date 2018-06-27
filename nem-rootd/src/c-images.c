#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <mbedtls/sha256.h>

#include "nem.h"
#include "imgset.h"
#include "utils.h"
#include "c-log.h"
#include "c-config.h"
#include "c-database.h"

static char *images_path = NULL;
static NEM_imgset_t static_imgset;

static NEM_err_t
make_directories(const char *base)
{
	static const struct {
		const char *path;
		char      **save;
	}
	paths[] = {
		{ "images", &images_path },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(paths); i += 1) {
		NEM_err_t err = NEM_path_join(paths[i].save, base, paths[i].path);
		if (!NEM_err_ok(err)) {
			return err;
		}

		int ret = mkdir(*paths[i].save, 0755);
		if (0 > ret) {
			if (EEXIST != errno) {
				return NEM_err_errno();
			}
		}
	}

	return NEM_err_none;
}

static NEM_err_t
images_db_migration_1(sqlite3 *db)
{
	int code = sqlite3_exec(
		db, 
		"CREATE TABLE images ("
		"  image_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
		"  image_name TEXT NOT NULL"
		"); "
		"CREATE TABLE image_versions ("
		"  imgver_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
		"  imgver_created DATETIME NOT NULL,"
		"  imgver_size INTEGER NOT NULL,"
		"  imgver_sha256 TEXT NOT NULL,"
		"  imgver_version TEXT NOT NULL"
		"); "
		"CREATE TABLE image_rels ("
		"  imgver_id INTEGER NOT NULL REFERENCES image_versions(imgver_id),"
		"  image_id INTEGER NOT NULL REFERENCES images(image_id)"
		"); "
		"CREATE UNIQUE INDEX idx_images_version_sha256"
		" ON image_versions(imgver_sha256);",
		NULL,
		NULL,
		NULL
	);
	if (SQLITE_OK != code) {
		return NEM_err_sqlite(db);
	}

	return NEM_err_none;
}

static const NEM_rootd_dbver_t db_migrations[] = {
	{ .version = 1, .fn = &images_db_migration_1 },
};

static void
hex_encode(char *out, const char *in, size_t in_len)
{
	static const char table[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
	};
	_Static_assert(sizeof(table) == 16, "wtf");

	for (size_t i = 0; i < in_len; i += 1) {
		out[i*2] = table[(in[i] & 0xf0) >> 4];
		out[i*2 + 1] = table[in[i] & 0x0f];
	}
}

static struct {
	const char *prefix;
	size_t      amount;
}
eng_encode_units[] = {
	{ "Pb", 1024L * 1024L * 1024L * 1024L * 1024L },
	{ "Tb", 1024L * 1024L * 1024L * 1024L },
	{ "Gb", 1024L * 1024L * 1024L },
	{ "Mb", 1024L * 1024L },
	{ "Kb", 1024L },
};

static long
eng_encode(size_t val)
{
	for (size_t i = 0; i < NEM_ARRSIZE(eng_encode_units); i += 1) {
		if (val > 9 * eng_encode_units[i].amount) {
			return val / eng_encode_units[i].amount;
		}
	}

	return val;
}

static const char*
eng_encode_unit(size_t val)
{
	for (size_t i = 0; i < NEM_ARRSIZE(eng_encode_units); i += 1) {
		if (val > 9 * eng_encode_units[i].amount) {
			return eng_encode_units[i].prefix;
		}
	}

	return "b ";
}

static NEM_err_t
load_version_status(NEM_imgver_t *imgv)
{
	NEM_err_t err = NEM_err_none;
	char *path;
	err = NEM_path_join(&path, images_path, imgv->sha256);
	if (!NEM_err_ok(err)) {
		return err;
	}

	int fd = open(path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
	free(path);
	if (0 > fd) {
		if (ENOENT == errno) {
			imgv->status = NEM_ROOTD_IMGV_MISSING;
			return NEM_err_none;
		}

		return NEM_err_errno();
	}

	struct stat sb;
	if (0 != fstat(fd, &sb)) {
		close(fd);
		return NEM_err_errno();
	}
	if (sb.st_size != imgv->size) {
		imgv->status = NEM_ROOTD_IMGV_BAD_SIZE;
	}

	void *bs = mmap(NULL, sb.st_size, PROT_READ, MAP_NOCORE, fd, 0);
	unsigned char binhash[32];
	mbedtls_sha256(bs, sb.st_size, binhash, 0);
	munmap(bs, sb.st_size);
	close(fd);

	char hexhash[65] = {0};
	hex_encode(hexhash, (char*) binhash, sizeof(binhash));
	if (strcmp(hexhash, imgv->sha256)) {
		NEM_logf(
			COMP_IMAGES,
			"\n  bad hash:\n  wanted: %s\n  actual: %s",
			imgv->sha256,
			hexhash
		);

		imgv->status = NEM_ROOTD_IMGV_BAD_HASH;
	}

	return NEM_err_none;
}

static NEM_err_t
load_image_versions(sqlite3 *db, NEM_img_t *img)
{
	NEM_err_t err = NEM_err_none;
	sqlite3_stmt *stmt = NULL;

	int code = sqlite3_prepare_v2(
		db,
		"SELECT "
		"  v.imgver_id, v.imgver_created, v.imgver_size,"
		"  v.imgver_sha256, v.imgver_version "
		"FROM image_versions v "
		"JOIN image_rels r ON v.imgver_id = r.imgver_id "
		"WHERE r.image_id = ?1"
		"ORDER BY v.imgver_id ASC",
		-1,
		&stmt,
		NULL
	);
	if (SQLITE_OK != code) {
		err = NEM_err_sqlite(db);
		goto done;
	}
	code = sqlite3_bind_int(stmt, 1, img->id);
	if (SQLITE_OK != code) {
		err = NEM_err_sqlite(db);
		goto done;
	}

	while (SQLITE_ROW == sqlite3_step(stmt)) {
		NEM_imgver_t tmp = {
			.id      = sqlite3_column_int(stmt, 0),
			.size    = sqlite3_column_int(stmt, 2),
			.sha256  = strdup((const char*) sqlite3_column_text(stmt, 3)),
			.version = strdup((const char*) sqlite3_column_text(stmt, 4)),
		};

		const char *created_str = (const char*) sqlite3_column_text(stmt, 1);
		strptime(created_str, "%F %T", &tmp.created);

		NEM_imgver_t *ver = &tmp;
		err = NEM_imgset_add_ver(&static_imgset, &ver, img);
		if (!NEM_err_ok(err)) {
			goto done;
		}

		err = load_version_status(ver);
		if (!NEM_err_ok(err)) {
			goto done;
		}
	}

done:
	if (NULL != stmt) {
		sqlite3_finalize(stmt);
	}

	return err;
}

static NEM_err_t
load_images()
{
	sqlite3 *db = NEM_rootd_db();
	NEM_err_t err = NEM_err_none;
	int code;

	sqlite3_stmt *stmt = NULL;

	code = sqlite3_prepare_v2(
		db,
		"SELECT image_id, image_name FROM images",
		-1,
		&stmt,
		NULL
	);
	if (SQLITE_OK != code) {
		err = NEM_err_sqlite(db);
		goto done;
	}

	while (SQLITE_ROW == sqlite3_step(stmt)) {
		NEM_img_t tmp = {
			.id = sqlite3_column_int(stmt, 0),
			.name = strdup((const char*) sqlite3_column_text(stmt, 1)),
		};
		NEM_img_t *img = &tmp;

		err = NEM_imgset_add_img(&static_imgset, &img);
		if (!NEM_err_ok(err)) {
			goto done;
		}

		err = load_image_versions(db, img);
		if (!NEM_err_ok(err)) {
			goto done;
		}
	}

done:
	if (NULL != stmt) {
		sqlite3_finalize(stmt);
	}

	return err;
}

static NEM_err_t
purge_extra_files()
{
	int fd = open(images_path, O_RDONLY);
	if (0 > fd) {
		return NEM_err_errno();
	}

	NEM_err_t err = NEM_err_none;
	long basep = 0;
	size_t buf_len = 4096;
	char *buf = NEM_malloc(buf_len);

	for (;;) {
		int nbytes = getdirentries(fd, buf, buf_len, &basep);
		if (0 == nbytes) {
			break;
		}
		if (0 > nbytes) {
			err = NEM_err_errno();
			goto done;
		}

		size_t off = 0;
		while (off < nbytes) {
			struct dirent *ent = (struct dirent*) &buf[off];
			off += ent->d_reclen;

			if (!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name)) {
				continue;
			}

			if (NULL == NEM_imgset_imgver_by_hash(&static_imgset, ent->d_name)) {
				NEM_logf(
					COMP_IMAGES,
					"purging unknown image '%s'",
					ent->d_name
				);

				char *file_path = NULL;
				err = NEM_path_join(&file_path, images_path, ent->d_name);
				if (!NEM_err_ok(err)) {
					goto done;
				}
				int code = unlink(file_path);
				free(file_path);
				if (0 != code) {
					err = NEM_err_errno();
					goto done;
				}
			}
		}
	}

done:
	free(buf);
	close(fd);
	return err;
}

NEM_err_t
NEM_rootd_find_image(const NEM_jailimg_t *ji, char **out)
{
	NEM_img_t *img = NEM_imgset_img_by_name(&static_imgset, ji->name);
	if (NULL == img) {
		return NEM_err_static("NEM_rootd_find_image: image missing");
	}

	NEM_imgver_t *ver = NULL;

	if (NULL != ji->sha256) {
		ver = NEM_img_imgver_by_hash(&static_imgset, img, ji->sha256);
		if (NULL == ver) {
			return NEM_err_static("NEM_rootd_find_image: no matching hash");
		}
	}
	else if (NULL != ji->semver) {
		ver = NEM_img_imgver_by_semver(&static_imgset, img, ji->semver);
		if (NULL == ver) {
			return NEM_err_static("NEM_rootd_find_image: no matching version");
		}
	}
	else {
		ver = NEM_img_imgver_latest(&static_imgset, img);
		if (NULL == ver) {
			return NEM_err_static("NEM_rootd_find_image: no versions of image");
		}
	}

	return NEM_path_join(out, images_path, ver->sha256);
}

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	NEM_logf(COMP_IMAGES, "setup");

	NEM_imgset_init(&static_imgset);

	NEM_err_t err;
	const char *base = NEM_rootd_run_root();

	err = make_directories(base);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = NEM_rootd_db_migrate(
		"images",
		db_migrations,
		NEM_ARRSIZE(db_migrations)
	);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = load_images();
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = purge_extra_files();
	if (!NEM_err_ok(err)) {
		return err;
	}

	NEM_logf(COMP_IMAGES, "loaded %lu images", static_imgset.imgs_len);
	for (size_t i = 0; i < static_imgset.imgs_len; i += 1) {
		NEM_img_t *img = &static_imgset.imgs[i];
		NEM_logf(COMP_IMAGES, " - %s", img->name);
		for (size_t j = 0; j < img->vers_len; j += 1) {
			int id = img->vers[j];
			NEM_imgver_t *ver = NEM_imgset_imgver_by_id(&static_imgset, id);
			NEM_logf(
				COMP_IMAGES,
				"    %12.12s... %8s %5d%s %s", 
				ver->sha256,
				ver->version,
				eng_encode(ver->size),
				eng_encode_unit(ver->size),
				NEM_imgver_status_string(ver->status)
			);
		}
	}

	return NEM_err_none;
}

static bool
try_shutdown(NEM_app_t *app)
{
	NEM_logf(COMP_IMAGES, "try-shutdown");

	return true;
}

static void
teardown(NEM_app_t *app)
{
	NEM_logf(COMP_IMAGES, "teardown");

	NEM_imgset_free(&static_imgset);

	free(images_path);
}

const NEM_app_comp_t NEM_rootd_c_images = {
	.name         = "images",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};

