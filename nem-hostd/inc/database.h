#pragma once
#include <sys/types.h>
#include <sqlite3.h>
#include "nem.h"

// NEM_hostd_dbver_t is a migration to handle upgrading to a new version.
// These should be statically constructed by components and passed to
// NEM_hostd_db_migrate during setup to initialize the database. The 
// versions are tracked per-component.
typedef struct {
	int version;
	NEM_err_t(*fn)(sqlite3*);
}
NEM_hostd_dbver_t;

// NEM_hostd_db returns a database handle. It panics if the database is
// not initialized.
sqlite3* NEM_hostd_db();

// NEM_hostd_db_migrate migrates the database with the given migrations.
// The migrations are done on a per-component basis: each table should
// be owned by exactly one component. The schema versions are tracked
// per-component.
//
// The passed in array should be sorted by `version`. 
NEM_err_t NEM_hostd_db_migrate(
	const char              *component,
	const NEM_hostd_dbver_t *versions,
	size_t                   num_versions
);
