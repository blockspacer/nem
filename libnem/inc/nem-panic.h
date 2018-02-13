#pragma once
#include <sys/types.h>
#include <stdlib.h>

// NEM_panic unwinds the stack with the specified message.
_Noreturn void NEM_panic(const char *msg);

// NEM_panicf unwinds the stack with the specified format string.
_Noreturn void NEM_panicf(const char *msg, ...);

// NEM_panicf_errno is the same as NEM_panicf, but appends the errno to the
// computed message with a preceding colon.
//
//     NEM_panicf_errno("open(%s) failed", filename)
//
//     panic: open(foo.sh) failed: No such file or directory
//     <stacktrace>
_Noreturn void NEM_panicf_errno(const char *msg, ...);

// NEM_panic_if_null panics if the provided pointer is NULL. If the pointer
// is non-NULL, it is returned. This is useful for guarding malloc failures.
static inline void*
NEM_panic_if_null(void *ptr)
{
	if (NULL == ptr) {
		NEM_panic("non-null pointer is null");
	}

	return ptr;
}

// NEM_malloc is a wrapper around calloc+panic_if_null.
static inline void*
NEM_malloc(size_t len)
{
	return NEM_panic_if_null(calloc(1, len));
}
