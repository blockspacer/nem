#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "nem.h"
#include "lifecycle.h"
#include "state.h"
#include "imgdb.h"
#include "c-database.h"

static char *images_path = NULL;
static char *persisted_path = NULL;
static NEM_rootd_imgdb_t imgdb;

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
		{ "images",    &images_path    },
		{ "persisted", &persisted_path },
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
		return NEM_err_static(sqlite3_errmsg(db));
	}

	return NEM_err_none;

}

static const NEM_rootd_dbver_t db_migrations[] = {
	{ .version = 1, .fn = &images_db_migration_1 },
};

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
		err = NEM_err_static(sqlite3_errmsg(db));
		goto done;
	}
	code = sqlite3_bind_int(stmt, 1, img->id);
	if (SQLITE_OK != code) {
		err = NEM_err_static(sqlite3_errmsg(db));
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
		err = NEM_err_static(sqlite3_errmsg(db));
		goto done;
	}

	while (SQLITE_ROW == sqlite3_step(stmt)) {
		NEM_rootd_img_t *img = NEM_rootd_imgdb_add_img(&imgdb);
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
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-images: setup\n");
	}

	NEM_rootd_imgdb_init(&imgdb);

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

	if (NEM_rootd_verbose()) {
		printf("c-images: loaded %lu images\n", imgdb.imgs_len);
		for (size_t i = 0; i < imgdb.imgs_len; i += 1) {
			printf(" - %s\n", imgdb.imgs[i].name);
			for (size_t j = 0; j < imgdb.imgs[i].versions_len; j += 1) {
				printf(
					"    %8s %s (%d bytes)\n", 
					imgdb.imgs[i].versions[j].sha256,
					imgdb.imgs[i].versions[j].version,
					imgdb.imgs[i].versions[j].size
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

	NEM_rootd_imgdb_free(&imgdb);

	free(images_path);
	free(persisted_path);
}

const NEM_rootd_comp_t NEM_rootd_c_images = {
	.name         = "c-images",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
