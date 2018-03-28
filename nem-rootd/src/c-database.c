#include <sys/types.h>
#include <sqlite3.h>

#include "nem.h"
#include "state.h"
#include "lifecycle.h"
#include "c-database.h"
#include "utils.h"

static char *database_path = NULL;
static sqlite3 *database = NULL;

static void
path_to_abs(char **path)
{
	char *tmp = *path;
	*path = NEM_panic_if_null(realpath(*path, NULL));
	free(tmp);
}

NEM_err_t
setup(NEM_app_t *app)
{
	asprintf(&database_path, "%s/db.sqlite3", NEM_rootd_jail_root());
	int tmp_fd = open(database_path, O_CREAT|O_RDWR, 0640);
	if (0 > tmp_fd) {
		return NEM_err_errno();
	}
	close(tmp_fd);
	path_to_abs(&database_path);

	if (SQLITE_OK != sqlite3_open(database_path, &database)) {
		return NEM_err_sqlite(database);
	}

	int err = sqlite3_exec(
		database,
		"CREATE TABLE IF NOT EXISTS versions"
		" ( component TEXT PRIMARY KEY"
		" , version INTEGER NOT NULL"
		" );"
		"INSERT OR REPLACE INTO versions (component, version)"
		"VALUES ('database', '1')",
		NULL,
		NULL,
		NULL
	);
	if (SQLITE_OK != err) {
		return NEM_err_sqlite(database);
	}

	return NEM_err_none;
}

void
teardown()
{
	if (NULL != database) {
		sqlite3_close(database);
	}

	free(database_path);
}

sqlite3*
NEM_rootd_db()
{
	if (NULL == database) {
		NEM_panic("NEM_rootd_db: c-database not initialized");
	}

	return database;
}

NEM_err_t
NEM_rootd_db_migrate(
	const char              *component,
	const NEM_rootd_dbver_t *versions,
	size_t                   num_versions
) {
	int code;
	NEM_err_t err = NEM_err_none;

	sqlite3_stmt *stmt_get = NULL;
	sqlite3_stmt *stmt_up = NULL;

	code = sqlite3_exec(database, "BEGIN", NULL, NULL, NULL);
	if (SQLITE_OK != code) {
		return NEM_err_sqlite(database);
	}

	code = sqlite3_prepare_v2(
		database,
		"SELECT COALESCE(version, 0) FROM versions WHERE component = ?1",
		-1,
		&stmt_get,
		NULL
	);
	if (SQLITE_OK != code) {
		err = NEM_err_sqlite(database);
		goto done;
	}
	code = sqlite3_bind_text(stmt_get, 1, component, -1, SQLITE_STATIC);
	if (SQLITE_OK != code) {
		err = NEM_err_sqlite(database);
		goto done;
	}

	code = sqlite3_prepare_v2(
		database,
		"INSERT OR REPLACE INTO VERSIONS (component, version) VALUES(?1, ?2)",
		-1,
		&stmt_up,
		NULL
	);
	code = sqlite3_bind_text(stmt_up, 1, component, -1, SQLITE_STATIC);
	if (SQLITE_OK != code) {
		err = NEM_err_sqlite(database);
		goto done;
	}

	for (size_t i = 0; i < num_versions; i += 1) {
		code = sqlite3_step(stmt_get);
		int ver = 0;

		if (SQLITE_ROW == code) {
			ver = sqlite3_column_int(stmt_get, 0);
		}
		if (SQLITE_OK != sqlite3_reset(stmt_get)) {
			err = NEM_err_sqlite(database);
			goto done;
		}

		if (ver < versions[i].version) {
			err = versions[i].fn(database);
			if (!NEM_err_ok(err)) {
				goto done;
			}
		}

		code = sqlite3_bind_int(stmt_up, 2, versions[i].version);
		if (SQLITE_DONE != sqlite3_step(stmt_up)) {
			err = NEM_err_sqlite(database);
			goto done;
		}
		if (SQLITE_OK != sqlite3_reset(stmt_up)) {
			err = NEM_err_sqlite(database);
			goto done;
		}
	}
done:
	if (NULL != stmt_get) {
		sqlite3_finalize(stmt_get);
	}
	if (NULL != stmt_up) {
		sqlite3_finalize(stmt_up);
	}
	if (!NEM_err_ok(err)) {
		sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
	}
	else {
		sqlite3_exec(database, "COMMIT", NULL, NULL, NULL);
	}

	return err;
}

const NEM_rootd_comp_t NEM_rootd_c_database = {
	.name     = "c-database",
	.setup    = &setup,
	.teardown = &teardown,
};
