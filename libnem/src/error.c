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
