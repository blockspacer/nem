#pragma once
#include <sys/types.h>
#include <stdbool.h>
#include <sqlite3.h>

#include "nem-error.h"

// NEM_rootd_state represents the running state. The intent here is that we
// can serialize the state to a new instance of ourself pre-exec to support
// hot reloads. This maybe is stupid, not sure. Doing that won't really work
// if we're running as init, but eh.
//
// This also parses the command-line arguments.
NEM_err_t NEM_rootd_state_init(int argc, char *argv[]);
void NEM_rootd_state_close();

bool NEM_rootd_verbose();
const char *NEM_rootd_routerd_path();
const char *NEM_rootd_jail_root();


