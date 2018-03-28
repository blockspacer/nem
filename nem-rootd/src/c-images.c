#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "nem.h"
#include "lifecycle.h"
#include "state.h"
#include "c-database.h"

static char *images_path = NULL;
static char *persisted_path = NULL;

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

	free(images_path);
	free(persisted_path);
}

const NEM_rootd_comp_t NEM_rootd_c_images = {
	.name         = "c-images",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
