#include "nem.h"
#include <sys/queue.h>
#include "imgdb.h"
#include "config.h"
#include "fsutils.h"
#include "database.h"

typedef LIST_HEAD(NEM_hostd_img_list_t, NEM_hostd_img_t) NEM_hostd_img_list_t;

static struct {
	char *imgpath;
	NEM_hostd_img_list_t images;
	sqlite3 *db;
}
static_db;

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	bzero(&static_db, sizeof(static_db));
	const NEM_hostd_config_t *config = NEM_hostd_config();
	if (0 > asprintf(&static_db.imgpath, "%s/images", config->rundir)) {
		return NEM_err_errno();
	}

	NEM_err_t err = NEM_ensure_dir(static_db.imgpath);
	if (!NEM_err_ok(err)) {
		free(static_db.imgpath);
		return err;
	}

	err = NEM_hostd_db_migrate(
		"imgdb",
		&db_versions,
		NEM_ARRSIZE(db_versions)
	);

	return NEM_err_none;
}

static void
teardown()
{
	// XXX: Free images.

	free(static_db.imgpath);

	if (NULL != static_db.db) {
		sqlite3_close(static_db.db);
	}
}

const NEM_app_comp_t NEM_hostd_c_imgdb = {
	.setup    = &setup,
	.teardown = &teardown,
};
