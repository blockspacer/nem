#pragma once
#include <sys/types.h>
#include <stdbool.h>
#include "nem-error.h"

NEM_err_t NEM_rootd_state_init(int argc, char *argv[]);
void NEM_rootd_state_close();
