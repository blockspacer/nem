#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <mbedtls/sha256.h>

#include "nem.h"
#include "lifecycle.h"
#include "state.h"
#include "imgset.h"
#include "utils.h"
#include "c-database.h"

static char *images_path = NULL;
static char *persisted_path = NULL;
static char *shared_mounts_path = NULL;
static NEM_rootd_imgset_t imgset;

static NEM_err_t
path_join(char **out, const char *base, const char *rest)
{
	int ret = asprintf(out, "%s/%s", base, rest);
	if (0 > ret) {
		return NEM_err_errno();
	}

	return NEM_err_none;
}

static NEM_err_t
make_directories(const char *base)
{
	static const struct {
		const char *path;
		char **save;
	}
	paths[] = {
		{ "images",        &images_path        },
		{ "shared_mounts", &shared_mounts_path },
		{ "persisted",     &persisted_path     },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(paths); i += 1) {
		NEM_err_t err = path_join(paths[i].save, base, paths[i].path);
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
		"  imgv_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
		"  imgv_image_id INTEGER NOT NULL REFERENCES images(image_id),"
		"  imgv_created DATETIME NOT NULL,"
		"  imgv_size INTEGER NOT NULL,"
		"  imgv_sha256 TEXT NOT NULL,"
		"  imgv_version TEXT NOT NULL"
		");",
		NULL,
		NULL,
		NULL
	);
	if (SQLITE_OK != code) {
		return NEM_err_sqlite(db);
	}

	return NEM_err_none;
}

static NEM_err_t
images_db_migration_2(sqlite3 *db)
{
	int code = sqlite3_exec(
		db,
		"CREATE UNIQUE INDEX idx_images_version_sha256"
		" ON image_versions(imgv_sha256);",
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
	{ .version = 2, .fn = &images_db_migration_2 },
};

static const char*
imgv_status_string(int status)
{
	static const struct {
		int status;
		const char *str;
	}
	table[] = {
		{ NEM_ROOTD_IMGV_OK,       "OKAY"     },
		{ NEM_ROOTD_IMGV_BAD_HASH, "BAD HASH" },
		{ NEM_ROOTD_IMGV_BAD_SIZE, "BAD SIZE" },
		{ NEM_ROOTD_IMGV_MISSING,  "MISSING"  },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(table); i += 1) {
		if (table[i].status == status) {
			return table[i].str;
		}
	}

	return "UNKNOWN";
}

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

static NEM_err_t
load_version_status(NEM_rootd_imgv_t *imgv)
{
	NEM_err_t err = NEM_err_none;
	char *path;
	err = path_join(&path, images_path, imgv->sha256);
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
		if (NEM_rootd_verbose()) {
			printf(
				"c-images: bad hash:\n  wanted: %s\n  actual: %s\n",
				imgv->sha256,
				hexhash
			);
		}

		imgv->status = NEM_ROOTD_IMGV_BAD_HASH;
	}

	return NEM_err_none;
}

static NEM_err_t
load_image_versions(sqlite3 *db, NEM_rootd_img_t *img)
{
	NEM_err_t err = NEM_err_none;
	sqlite3_stmt *stmt = NULL;

	int code = sqlite3_prepare_v2(
		db,
		"SELECT "
		"  imgv_id, imgv_created, imgv_size, imgv_sha256, imgv_version "
		"FROM image_versions "
		"WHERE imgv_image_id = ?1"
		"ORDER BY imgv_id ASC",
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
		NEM_rootd_imgv_t *ver = NEM_rootd_img_add_version(img);
		ver->id = sqlite3_column_int(stmt, 0);
		// XXX: Probably want to check the return value here.
		const char *created_str = (const char*) sqlite3_column_text(stmt, 1);
		strptime(created_str, "%F %T", &ver->created);
		ver->size = sqlite3_column_int(stmt, 2);
		ver->sha256 = strdup((const char*) sqlite3_column_text(stmt, 3));
		ver->version = strdup((const char*) sqlite3_column_text(stmt, 4));

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
		NEM_rootd_img_t *img = NEM_rootd_imgset_add_img(&imgset);
		bzero(img, sizeof(*img));
		img->id = sqlite3_column_int(stmt, 0);
		img->name = strdup((const char*) sqlite3_column_text(stmt, 1));

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

			if (NULL == NEM_rootd_imgset_find_imgv(&imgset, ent->d_name)) {
				if (NEM_rootd_verbose()) {
					printf(
						"c-images: purging unknown image '%s'\n",
						ent->d_name
					);
				}

				char *file_path = NULL;
				err = path_join(&file_path, images_path, ent->d_name);
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

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-images: setup\n");
	}

	NEM_rootd_imgset_init(&imgset);

	NEM_err_t err;
	const char *base = NEM_rootd_jail_root();

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

	if (NEM_rootd_verbose()) {
		printf("c-images: loaded %lu images\n", imgset.imgs_len);
		for (size_t i = 0; i < imgset.imgs_len; i += 1) {
			printf(" - %s\n", imgset.imgs[i].name);
			for (size_t j = 0; j < imgset.imgs[i].versions_len; j += 1) {
				printf(
					"    %12.12s... %12s   %6db %s\n", 
					imgset.imgs[i].versions[j].sha256,
					imgset.imgs[i].versions[j].version,
					imgset.imgs[i].versions[j].size,
					imgv_status_string(imgset.imgs[i].versions[j].status)
				);
			}
		}
	}

	return NEM_err_none;
}

static bool
try_shutdown()
{
	if (NEM_rootd_verbose()) {
		printf("c-images: try-shutdown\n");
	}

	return true;
}

static void
teardown()
{
	if (NEM_rootd_verbose()) {
		printf("c-images: teardown\n");
	}

	NEM_rootd_imgset_free(&imgset);

	free(images_path);
	free(persisted_path);
	free(shared_mounts_path);
}

const NEM_rootd_comp_t NEM_rootd_c_images = {
	.name         = "c-images",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
