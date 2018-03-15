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

NEM_err_t
NEM_err_mbedtls(int code)
{
	NEM_err_t ret = {
		.source = NEM_ERR_SOURCE_MBEDTLS,
		.code   = code,
	};
	return ret;
}

void __attribute__((weak))
mbedtls_strerror(int errnum, char *buf, size_t buflen);

const char *
NEM_err_string(NEM_err_t err)
{
	static char tmpbuf[256];

	switch (err.source) {
		case NEM_ERR_SOURCE_NONE:
			return "no error";

		case NEM_ERR_SOURCE_POSIX:
			return strerror(err.code);

		case NEM_ERR_SOURCE_STATIC:
			return err.str;

		case NEM_ERR_SOURCE_MBEDTLS:
			if (NULL != mbedtls_strerror) {
				mbedtls_strerror(err.code, tmpbuf, sizeof(tmpbuf));
				return tmpbuf;
			}
			return "some mbedtls error, mbedtls not compiled in";

		default:
			NEM_panicf(
				"NEM_err_string: unknown error source %d (code %d). Memory corruption?",
				err.source,
				err.code
			);
	}
}
