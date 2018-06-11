#pragma once
#include <sys/types.h>

bool NEM_rootd_verbose();
bool NEM_rootd_capsicum();

const char *NEM_rootd_routerd_path();
const char *NEM_rootd_run_root();
const char *NEM_rootd_config_root();

extern const NEM_app_comp_t NEM_rootd_c_args;
