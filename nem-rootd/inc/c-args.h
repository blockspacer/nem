#pragma once
#include <sys/types.h>

bool NEM_rootd_verbose();
const char *NEM_rootd_own_path();
const char *NEM_rootd_config_path();

extern const NEM_app_comp_t NEM_rootd_c_args;
