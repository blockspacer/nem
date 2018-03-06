#pragma once
#include <sys/types.h>
#include <sys/event.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>

#define NEM_ARRSIZE(x) (sizeof(x)/sizeof((x)[0]))
#define NEM_MSIZE(t, f) (sizeof(((t*)0)->f))
#define NEM_ALIGN _Alignas(void*)

#include "nem-error.h"
#include "nem-thunk.h"
#include "nem-panic.h"
#include "nem-stream.h"
#include "nem-fd.h"
#include "nem-list.h"
#include "nem-msg.h"
#include "nem-chan.h"

#include "nem-app.h"
