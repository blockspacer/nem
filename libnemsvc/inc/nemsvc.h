#pragma once
#include <sys/types.h>
#include <stdbool.h>
#include "nem-error.h"
#include "nem-marshal.h"

static const uint16_t 
	NEM_svcid_daemon  = 1,
	NEM_svcid_router  = 2,
	NEM_svcid_imghost = 3;

#include "nemsvc-daemon.h"
#include "nemsvc-router.h"
#include "nemsvc-imghost.h"

const char* NEM_svcid_to_string(uint16_t svcid);
bool NEM_svcid_is_routable(uint16_t svcid);
const char* NEM_cmdid_to_string(uint16_t svcid, uint16_t cmdid);
