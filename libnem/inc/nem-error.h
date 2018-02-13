#pragma once

typedef enum {
	NEM_ERR_SOURCE_NONE   = 0,
	NEM_ERR_SOURCE_POSIX  = 1,
	NEM_ERR_SOURCE_STATIC = 2,
}
NEM_err_src_t;

typedef struct {
	NEM_err_src_t source;

	union {
		int         code;
		const char *str;
	};
}
NEM_err_t;

extern NEM_err_t NEM_err_none;

static inline bool
NEM_err_ok(NEM_err_t err)
{
	return err.source == NEM_ERR_SOURCE_NONE;
}

NEM_err_t NEM_err_errno();
NEM_err_t NEM_err_static(const char *str);

const char* NEM_err_string(NEM_err_t err);
