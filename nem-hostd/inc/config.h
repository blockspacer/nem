#pragma once
#include <sys/types.h>
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
	const char              *rootdir;
	NEM_hostd_config_jail_t *jails;
	size_t                   jails_len;
}
NEM_hostd_config_t;
const NEM_marshal_map_t NEM_hostd_config_vt;
