#pragma once
#include <sys/types.h>
#include <stdbool.h>

bool NEM_rootd_state_init(int argc, char *argv[]);
void NEM_rootd_state_close();
