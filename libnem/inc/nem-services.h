#pragma once

static const uint16_t 
	NEM_svcid_daemon = 1,
	NEM_svcid_host = 2,
	NEM_svcid_router = 3;

const char* NEM_svcid_to_string(uint16_t svcid);
