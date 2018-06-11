#pragma once
#include "jaildesc.h"

typedef struct {
	const char *name;
	const char *config;

	// NB: not serialized.
	NEM_hostd_jailimg_t *img_config;
}
NEM_hostd_config_jail_t;
const NEM_marshal_map_t NEM_hostd_config_jail_vt;

typedef struct {
	bool                     is_init;
	bool                     is_root;
	const char              *rundir;
	const char              *configdir;
	NEM_hostd_config_jail_t *jails;
	size_t                   jails_len;
}
NEM_hostd_config_t;
const NEM_marshal_map_t NEM_hostd_config_vt;

const NEM_hostd_config_t *NEM_hostd_config();
extern const NEM_app_comp_t NEM_hostd_c_config;
