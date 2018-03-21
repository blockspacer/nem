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
};

static const cmd_data_t router_cmds[] = {
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
			for (size_t j = 0; j < svc_id_map[i].num_commands; i += 1) {
				if (cmds[j].cmdid == cmdid) {
					return cmds[j].name;
				}
			}
			break;
		}
	}

	return "unknown";
}
