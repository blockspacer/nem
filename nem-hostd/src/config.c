#include "nem.h"
#include "nem-marshal-macros.h"
#include "config.h"

#define TYPE NEM_hostd_config_jail_t
static const NEM_marshal_field_t NEM_hostd_config_jail_fs[] = {
	{ "name",   NEM_MARSHAL_STRING, O(name),   -1, NULL, },
	{ "config", NEM_MARSHAL_STRING, O(config), -1, NULL, },
};
MAP(NEM_hostd_config_jail_m, NEM_hostd_config_jail_fs);
#undef TYPE

#define TYPE NEM_hostd_config_t
static const NEM_marshal_field_t NEM_hostd_config_fs[] = {
	{ "rootdir", NEM_MARSHAL_STRING, O(rootdir), -1, NULL },
	{
		"jails",
		NEM_MARSHAL_ARRAY|NEM_MARSHAL_STRUCT, 
		O(jails), O(jails_len),
		&NEM_hostd_config_jail_m,
	},
};
MAP(NEM_hostd_config_m, NEM_hostd_config_fs);
#undef TYPE
