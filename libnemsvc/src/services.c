#include "nemsvc.h"
#include "nem.h"

typedef struct {
	uint16_t    cmdid;
	const char *name;
}
cmd_data_t;

static const cmd_data_t daemon_cmds[] = {
	{ NEM_cmdid_daemon_info,   "info"   },
	{ NEM_cmdid_daemon_getcfg, "getcfg" },
	{ NEM_cmdid_daemon_setcfg, "setcfg" },
	{ NEM_cmdid_daemon_stop,   "stop"   },
};

static const cmd_data_t host_cmds[] = {
	{ NEM_cmdid_host_bind,    "bind"    },
	{ NEM_cmdid_host_connect, "connect" },
};

static const cmd_data_t router_cmds[] = {
	{ NEM_cmdid_router_bind,          "bind"          },
	{ NEM_cmdid_router_register_svc,  "register-svc"  },
	{ NEM_cmdid_router_register_http, "register-http" },
};

static const cmd_data_t imghost_cmds[] = {
	{ NEM_cmdid_imghost_list_images,   "list-images"   },
	{ NEM_cmdid_imghost_list_versions, "list-versions" },
	{ NEM_cmdid_imghost_add_image,     "add-image"     },
	{ NEM_cmdid_imghost_get_image,     "get-image"     },
};

#define CMD_DEF(name, routable) {\
	NEM_svcid_##name, #name, routable, name##_cmds, NEM_ARRSIZE(name##_cmds) \
}
static const struct {
	uint16_t          svcid;
	const char       *name;
	bool              routable;
	const cmd_data_t *commands;
	size_t            num_commands;
}
svc_id_map[] = {
	CMD_DEF(daemon, false),
	CMD_DEF(host, false),
	CMD_DEF(router, false),
	CMD_DEF(imghost, false),
};
#undef CMD_DEF

const char*
NEM_svcid_to_string(uint16_t svcid)
{
	for (size_t i = 0; i < NEM_ARRSIZE(svc_id_map); i += 1) {
		if (svc_id_map[i].svcid == svcid) {
			return svc_id_map[i].name;
		}
	}

	return "unknown";
}

bool
NEM_svcid_is_routable(uint16_t svcid)
{
	for (size_t i = 0; i < NEM_ARRSIZE(svc_id_map); i += 1) {
		if (svc_id_map[i].svcid == svcid) {
			return svc_id_map[i].routable;
		}
	}

	return false;
}

const char*
NEM_cmdid_to_string(uint16_t svcid, uint16_t cmdid)
{
	for (size_t i = 0; i < NEM_ARRSIZE(svc_id_map); i += 1) {
		if (svc_id_map[i].svcid == svcid) {
			const cmd_data_t *cmds = svc_id_map[i].commands;
			for (size_t j = 0; j < svc_id_map[i].num_commands; j += 1) {
				if (cmds[j].cmdid == cmdid) {
					return cmds[j].name;
				}
			}
			break;
		}
	}

	return "unknown";
}
