#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "nem.h"
#include "lifecycle.h"
#include "state.h"
#include "c-database.h"

typedef struct {
	int       id;
	int       image_id;
	struct tm created;
	int       size;
	char     *sha256;
	char     *version;
}
NEM_rootd_imgv_t;

typedef struct {
	int   id;
	char *name;

	size_t            num_versions;
	NEM_rootd_imgv_t *versions;
}
NEM_rootd_image_t;

static char *images_path = NULL;
static char *persisted_path = NULL;
static size_t num_all_images = 0;
static NEM_rootd_image_t *all_images = NULL;

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
load_image_versions(sqlite3 *db, NEM_rootd_image_t *img)
{
	NEM_err_t err = NEM_err_none;
	sqlite3_stmt *stmt = NULL;
	size_t cap = 0;

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
		if (cap <= img->num_versions) {
			cap = cap ? 2 * cap : 8;
			img->versions = NEM_panic_if_null(
				realloc(img->versions, sizeof(NEM_rootd_imgv_t) * cap)
			);
		}

		NEM_rootd_imgv_t *ver = &img->versions[img->num_versions];
		ver->id = sqlite3_column_int(stmt, 0);
		// XXX: Probably want to check the return value here.
		const char *created_str = (const char*) sqlite3_column_text(stmt, 1);
		strptime(created_str, "%F %T", &ver->created);
		ver->size = sqlite3_column_int(stmt, 2);
		ver->sha256 = strdup((const char*) sqlite3_column_text(stmt, 3));
		ver->version = strdup((const char*) sqlite3_column_text(stmt, 4));
		img->num_versions += 1;
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
	size_t cap = 0;
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
		if (cap <= num_all_images) {
			cap = cap ? 2 * cap : 8;
			all_images = NEM_panic_if_null(
				realloc(all_images, sizeof(NEM_rootd_image_t) * cap)
			);
		}

		NEM_rootd_image_t *img = &all_images[num_all_images];
		bzero(img, sizeof(*img));
		img->id = sqlite3_column_int(stmt, 0);
		img->name = strdup((const char*) sqlite3_column_text(stmt, 1));
		num_all_images += 1;

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
		printf("c-images: loaded %lu images\n", num_all_images);
		for (size_t i = 0; i < num_all_images; i += 1) {
			printf(" - %s\n", all_images[i].name);
			for (size_t j = 0; j < all_images[i].num_versions; j += 1) {
				printf(
					"    %8s %s (%d bytes)\n", 
					all_images[i].versions[j].sha256,
					all_images[i].versions[j].version,
					all_images[i].versions[j].size
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

	for (size_t i = 0; i < num_all_images; i += 1) {
		for (size_t j = 0; j < all_images[i].num_versions; j += 1) {
			free(all_images[i].versions[j].sha256);
			free(all_images[i].versions[j].version);
		}
		free(all_images[i].versions);
		free(all_images[i].name);
	}

	free(images_path);
	free(persisted_path);
}

const NEM_rootd_comp_t NEM_rootd_c_images = {
	.name         = "c-images",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
