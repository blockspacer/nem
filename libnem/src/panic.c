#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <execinfo.h>

#include "nem.h"

void
NEM_panic(const char *msg)
{
	static void *buf[10];
	static size_t max_depth = sizeof(buf) / sizeof(void*);

	fprintf(stderr, "panic: %s\n", msg);
	max_depth = backtrace(&buf[0], max_depth);
	backtrace_symbols_fd(&buf[0], max_depth, 2);
	kill(getpid(), SIGTRAP);

	exit(1);
}

void
NEM_panicf(const char *fmt, ...)
{
	va_list va;
	char buf[1024];

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	NEM_panic(buf);
}

void
NEM_panicf_errno(const char *fmt, ...)
{
	va_list va;
	char buf[1024];

	va_start(va, fmt);
	size_t off = vsnprintf(buf, sizeof(buf), fmt,  va);
	if (sizeof(buf) > off) {
		snprintf(buf + off, sizeof(buf) - off, ": %s", strerror(errno));
	}

	NEM_panic(buf);
}
