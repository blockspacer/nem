#include "nem.h"
#include <errno.h>
#include <string.h>

NEM_err_t NEM_err_none = {
	.source = NEM_ERR_SOURCE_NONE,
	.code   = 0,
};

NEM_err_t
NEM_err_errno()
{
	NEM_err_t ret = {
		.source = NEM_ERR_SOURCE_POSIX,
		.code   = errno,
	};

	return ret;
}

NEM_err_t
NEM_err_static(const char *str)
{
	NEM_err_t ret = {
		.source = NEM_ERR_SOURCE_STATIC,
		.str    = str,
	};
	return ret;
}

const char *
NEM_err_string(NEM_err_t err)
{
	switch (err.source) {
		case NEM_ERR_SOURCE_NONE:
			return "no error";

		case NEM_ERR_SOURCE_POSIX:
			return strerror(err.code);

		case NEM_ERR_SOURCE_STATIC:
			return err.str;

		default:
			NEM_panicf(
				"NEM_err_string: unknown error source %d (code %d). Memory corruption?",
				err.source,
				err.code
			);
	}
}
