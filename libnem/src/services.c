#include "nem.h"

static const struct {
	uint16_t svcid;
	const char *name;
}
svc_id_map[] = {
	{ NEM_svcid_daemon, "daemon" },
	{ NEM_svcid_host,   "host"   },
	{ NEM_svcid_router, "router" },
};

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
