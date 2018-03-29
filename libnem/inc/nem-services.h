#pragma once

// This file describes service/command ids. These are basically hardcoded
// everywhere rather than passing strings. It might make more sense to pass
// strings, but those can be stuffed into the headers under a specified 
// "use strings instead" service. Anyway it's easier parsing/dispatching on
// the C side if these are just integers.

// This is the master list of services that can be provided by daemons. These
// are stuffed into NEM_pmsg_t.service_id and indicate which service type should
// be targetted.
static const uint16_t 
	NEM_svcid_daemon  = 1,
	NEM_svcid_host    = 2,
	NEM_svcid_router  = 3,
	NEM_svcid_imghost = 4;

static const uint16_t
	NEM_cmdid_daemon_info   = 1,
	NEM_cmdid_daemon_getcfg = 2,
	NEM_cmdid_daemon_setcfg = 3,
	NEM_cmdid_daemon_stop   = 4;

static const uint16_t
	NEM_cmdid_imghost_list_images   = 1,
	NEM_cmdid_imghost_list_versions = 2,
	NEM_cmdid_imghost_add_image     = 3,
	NEM_cmdid_imghost_get_image     = 4;

const char* NEM_svcid_to_string(uint16_t svcid);
bool NEM_svcid_is_routable(uint16_t svcid);
const char* NEM_cmdid_to_string(uint16_t svcid, uint16_t cmdid);
