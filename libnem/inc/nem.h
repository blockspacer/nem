#pragma once
#include <sys/types.h>
#include <stdbool.h>

#define NEM_ARRSIZE(x) (sizeof(x)/sizeof((x)[0]))
#define NEM_MSIZE(t, f) (sizeof(((t*)0)->f))
#define NEM_ALIGN _Alignas(void*)

#include "nem-error.h"
#include "nem-thunk.h"
#include "nem-panic.h"
#include "nem-msg.h"
