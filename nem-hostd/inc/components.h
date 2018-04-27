#pragma once
#include <sys/types.h>
#include <sqlite3.h>
#include "nem.h"

const NEM_app_comp_t 
	NEM_hostd_args_comp,
	NEM_hostd_config_comp,
	NEM_hostd_db_comp,
	NEM_hostd_imgdb_comp,
	NEM_hostd_children_comp;

/*
 * NEM_hostd_args_comp
 */

bool NEM_hostd_args_verbose();
const char* NEM_hostd_args_config();

/*
 * NEM_hostd_config_comp
 */

// NEM_hostd_config_t is the current configuration of the host. The
// configuration can change at runtime; to be notified of configuration
// changes, use NEM_hostd_config_notify to enroll a thunk.
typedef struct {
	bool                  verbose;
	const char           *db_path;
	NEM_hostd_jaildesc_t *jails;
	size_t                jails_len;
}
NEM_hostd_config_t;
const NEM_marshal_map_t NEM_hostd_config_m;

typedef struct {
	const NEM_hostd_config_t *old_config;
	const NEM_hostd_config_t *new_config;
}
NEM_hostd_config_ca;

// NEM_hostd_config returns the current configuration.
const NEM_hostd_config_t* NEM_hostd_config();

// NEM_hostd_config_notify enrolls a thunk in a notify list for configuration
// changes. The thunk is called whenever the config updates, and is passed
// a NEM_hostd_config_ca with the old/new configs.
void NEM_hostd_config_notify(NEM_thunk_t *thunk);

/*
 * NEM_hostd_db_comp
 */

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
