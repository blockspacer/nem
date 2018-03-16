#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/ssl.h>
#include <mbedtls/pk.h>

#include "nem.h"
#include "tls.h"

static NEM_err_t
read_file_null_terminated(const char *path, void **out, size_t *outlen)
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

	char *buf = NEM_malloc(sb.st_size + 1);
	memcpy(buf, bs, sb.st_size);

	munmap(bs, sb.st_size);
	close(fd);

	*out = buf;
	*outlen = sb.st_size + 1;

	return NEM_err_none;
}

/*
 * NEM_tls_key_t
 */

struct NEM_tls_key_t {
	mbedtls_pk_context key;
};

NEM_err_t
NEM_tls_key_init_file(NEM_tls_key_t **this, const char *path)
{
	void *bs;
	size_t len;

	NEM_err_t err = read_file_null_terminated(path, &bs, &len);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = NEM_tls_key_init(this, bs, len);
	free(bs);

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
		free(*this);
		return NEM_err_mbedtls(err);
	}

	return NEM_err_none;
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
	mbedtls_pk_free(&this->key);
	free(this);
}

/*
 * NEM_tls_cert_t
 */

struct NEM_tls_cert_t {
	mbedtls_x509_crt crt;
};

NEM_err_t
NEM_tls_cert_init_file(NEM_tls_cert_t **this, const char *path)
{
	void *bs;
	size_t len;

	NEM_err_t err = read_file_null_terminated(path, &bs, &len);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = NEM_tls_cert_init(this, bs, len);
	free(bs);

	return err;
}

NEM_err_t
NEM_tls_cert_init(NEM_tls_cert_t **this, const void *bs, size_t len)
{
	*this = NEM_malloc(sizeof(NEM_tls_cert_t));

	mbedtls_x509_crt_init(&(*this)->crt);

	int err = mbedtls_x509_crt_parse(&(*this)->crt, bs, len);
	if (0 != err) {
		free(*this);

		if (0 < err) {
			// No way to get the actual error out? :(
			return NEM_err_static(
				"mbedtls_x509_crt_parse: one or more invalid certs"
			);
		}
		else {
			return NEM_err_mbedtls(err);
		}
	}

	return NEM_err_none;
}

void
NEM_tls_cert_free(NEM_tls_cert_t *this)
{
	mbedtls_x509_crt_free(&this->crt);
	free(this);
}

/*
 * NEM_tls_t
 */

typedef struct {
	NEM_tls_key_t  *key;
	NEM_tls_cert_t *cert;
}
NEM_tls_keycert_t;

struct NEM_tls_t {
	int                refcount;
	mbedtls_ssl_config cfg_cli;
	mbedtls_ssl_config cfg_srv;

	size_t             num_keycerts;
	NEM_tls_keycert_t *keycerts;

	size_t           num_cas;
	NEM_tls_cert_t **cas;
};

NEM_err_t
NEM_tls_init(NEM_tls_t **this)
{
	NEM_tls_t *self = NEM_malloc(sizeof(NEM_tls_t));
	int errcode;

	mbedtls_ssl_config_init(&self->cfg_cli);
	errcode = mbedtls_ssl_config_defaults(
		&self->cfg_cli,
		MBEDTLS_SSL_IS_CLIENT,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT
	);
	if (0 != errcode) {
		mbedtls_ssl_config_free(&self->cfg_cli);
		free(self);
		return NEM_err_mbedtls(errcode);
	}

	mbedtls_ssl_config_init(&self->cfg_srv);
	errcode = mbedtls_ssl_config_defaults(
		&self->cfg_srv,
		MBEDTLS_SSL_IS_SERVER,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT
	);
	if (0 != errcode) {
		mbedtls_ssl_config_free(&self->cfg_srv);
		mbedtls_ssl_config_free(&self->cfg_cli);
		free(self);
		return NEM_err_mbedtls(errcode);
	}

	self->refcount = 1;
	*this = self;
	return NEM_err_none;

}

void
NEM_tls_add_ca(NEM_tls_t *this, NEM_tls_cert_t *cacert)
{
	mbedtls_ssl_conf_ca_chain(&this->cfg_srv, &cacert->crt, NULL);
	mbedtls_ssl_conf_ca_chain(&this->cfg_cli, &cacert->crt, NULL);

	this->num_cas += 1;
	this->cas = NEM_panic_if_null(realloc(
		this->cas,
		this->num_cas * sizeof(NEM_tls_cert_t*)
	));
	this->cas[this->num_cas - 1] = cacert;
}

NEM_err_t
NEM_tls_add_cert(NEM_tls_t *this, NEM_tls_key_t *key, NEM_tls_cert_t *cert)
{
	int errcode;

	errcode = mbedtls_ssl_conf_own_cert(&this->cfg_cli, &cert->crt, &key->key);
	if (errcode) {
		NEM_tls_key_free(key);
		NEM_tls_cert_free(cert);
		return NEM_err_mbedtls(errcode);
	}

	errcode = mbedtls_ssl_conf_own_cert(&this->cfg_srv, &cert->crt, &key->key);
	if (errcode) {
		NEM_tls_key_free(key);
		NEM_tls_cert_free(cert);
		return NEM_err_mbedtls(errcode);
	}

	this->num_keycerts += 1;
	this->keycerts = NEM_panic_if_null(realloc(
		this->keycerts,
		this->num_keycerts * sizeof(NEM_tls_keycert_t)
	));

	NEM_tls_keycert_t *new_entry = &this->keycerts[this->num_keycerts - 1];
	new_entry->key = key;
	new_entry->cert = cert;

	return NEM_err_none;
}

static void
NEM_tls_free_internal(NEM_tls_t *this)
{
	mbedtls_ssl_config_free(&this->cfg_cli);
	mbedtls_ssl_config_free(&this->cfg_srv);

	for (size_t i = 0; i < this->num_cas; i += 1) {
		NEM_tls_cert_free(this->cas[i]);
	}
	free(this->cas);

	for (size_t i = 0; i < this->num_keycerts; i += 1) {
		NEM_tls_keycert_t entry = this->keycerts[i];
		NEM_tls_key_free(entry.key);
		NEM_tls_cert_free(entry.cert);
	}
	free(this->keycerts);

	free(this);
}

static NEM_tls_t*
NEM_tls_copy(NEM_tls_t *this)
{
	this->refcount += 1;
	return this;
}

void
NEM_tls_free(NEM_tls_t *this)
{
	this->refcount -= 1;
	if (0 == this->refcount) {
		NEM_tls_free_internal(this);
	}
}

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
