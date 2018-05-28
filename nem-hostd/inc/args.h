#pragma once

typedef struct {
	const char *own_path;
	const char *config_path;
	bool initialized;
	bool verbose;
}
NEM_hostd_args_t;

const NEM_hostd_args_t* NEM_hostd_args();

extern const NEM_app_comp_t NEM_hostd_c_args;
