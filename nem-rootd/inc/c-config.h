#pragma once
#include "jaildesc.h"

typedef struct {
	const char *name;
	const char *config;

	// NB: not serialized.
	NEM_jailimg_t *img_config;
}
NEM_rootd_config_jail_t;
const NEM_marshal_map_t NEM_rootd_config_jail_vt;

typedef struct {
	bool                     is_init;
	bool                     is_root;
	const char              *rundir;
	const char              *configdir;
	NEM_rootd_config_jail_t *jails;
	size_t                   jails_len;
}
NEM_rootd_config_t;
const NEM_marshal_map_t NEM_rootd_config_vt;

extern const NEM_app_comp_t NEM_rootd_c_config;

bool NEM_rootd_capsicum();

const char *NEM_rootd_routerd_path();
const char *NEM_rootd_run_root();
const char *NEM_rootd_config_root();
