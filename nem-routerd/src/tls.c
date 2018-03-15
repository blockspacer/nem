#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/ssl.h>
#include <mbedtls/pk.h>

#include "nem.h"
#include "tls.h"

/*
 * NEM_tls_key_t
 */

struct NEM_tls_key_t {
	int                refcount;
	mbedtls_pk_context key;
};

NEM_err_t
NEM_tls_key_init_file(NEM_tls_key_t **this, const char *path)
{
	int fd = open(path, O_RDONLY);
	if (0 > fd) {
		return NEM_err_errno();
	}

	struct stat sb;
	if (0 != fstat(fd, &sb)) {
		return NEM_err_errno();
	}


	void *bs = mmap(NULL, sb.st_size, PROT_READ, MAP_NOCORE, fd, 0);
	if (MAP_FAILED == bs) {
		close(fd);
		return NEM_err_errno();
	}

	NEM_err_t err = NEM_tls_key_init(this, bs, sb.st_size);

	munmap(bs, sb.st_size);
	close(fd);

	return err;
}

NEM_err_t
NEM_tls_key_init(NEM_tls_key_t **this, const void *bs, size_t len)
{
	*this = NEM_malloc(sizeof(NEM_tls_key_t));

	// NB: mbedtls wants NULL-terminated keys. If the buffer isn't
	// NULL-terminated, we need to realloc it and make it NULL terminated.
	char *buf = (char*) bs;
	if (0 != buf[len - 1]) {
		buf = NEM_malloc(len + 1);
		memcpy(buf, bs, len);
		len += 1;
	}

	mbedtls_pk_init(&(*this)->key);

	int err = mbedtls_pk_parse_key(
		&(*this)->key,
		(const void*) buf,
		len,
		NULL,
		0
	);
	if (buf != bs) {
		free(buf);
	}
	if (0 != err) {
		return NEM_err_mbedtls(err);
	}

	(*this)->refcount += 1;
	return NEM_err_none;
}

static NEM_tls_key_t*
NEM_tls_key_copy(NEM_tls_key_t *this)
{
	this->refcount += 1;
	return this;
}

static void
NEM_tls_key_free_internal(NEM_tls_key_t *this)
{
	mbedtls_pk_free(&this->key);
	free(this);
}

void
NEM_tls_key_free(NEM_tls_key_t *this)
{
	this->refcount -= 1;
	if (0 == this->refcount) {
		NEM_tls_key_free_internal(this);
	}
}

/*
 * NEM_tls_cert_t
 */

struct NEM_tls_cert_t {
	int              refcount;
	mbedtls_x509_crt crt;
};

/*
 * NEM_tls_t
 */

struct NEM_tls_t {
	int                refcount;
	mbedtls_ssl_config ssl_cfg;
};

/*
 * NEM_tls_list_t
 */

struct NEM_tls_list_t {
};

/*
 * NEM_tls_dial_t
 */

struct NEM_tls_dial_t {
};
