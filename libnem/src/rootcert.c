#include "nem.h"

extern const void *NEM_root_cert_pem_raw;
extern const void *NEM_root_cert_pem_len_raw;

const char*
NEM_root_cert_pem(size_t *lenout)
{
	if (NULL != lenout) {
		*lenout = (size_t)&NEM_root_cert_pem_len_raw;
	}

	return (const char*)&NEM_root_cert_pem_raw;
}
