#pragma once

typedef struct NEM_tls_key_t NEM_tls_key_t;
NEM_err_t NEM_tls_key_init_file(NEM_tls_key_t **this, const char *path);
NEM_err_t NEM_tls_key_init(NEM_tls_key_t **this, const void *bs, size_t len);
void NEM_tls_key_free(NEM_tls_key_t *this);

typedef struct NEM_tls_cert_t NEM_tls_cert_t;
NEM_err_t NEM_tls_cert_init_file(NEM_tls_cert_t **this, const char *path);
NEM_err_t NEM_tls_cert_init(NEM_tls_cert_t **this, const void *bs, size_t len);
void NEM_tls_cert_free(NEM_tls_cert_t *this);

typedef struct NEM_tls_t NEM_tls_t;
void NEM_tls_init(NEM_tls_t **this);
void NEM_tls_add_ca(NEM_tls_t *this, NEM_tls_cert_t *cacert);
void NEM_tls_add_cert(NEM_tls_t *this, NEM_tls_key_t *key, NEM_tls_cert_t *cert);
void NEM_tls_free(NEM_tls_t *this);

void NEM_tls_list_init(
	NEM_tls_t   *tls,
	NEM_list_t  *this,
	NEM_list_t   wrapped,
	NEM_thunk_t *on_stream
);

typedef struct {
	NEM_list_ca list;
	const char *protocol;
}
NEM_tls_list_ca;

void NEM_tls_dial_init(
	NEM_tls_t    *tls,
	const char   *hostname,
	const char   *protocol,
	NEM_stream_t  wrapped,
	NEM_thunk1_t *on_stream
);

typedef struct {
	NEM_dial_ca dial;
}
NEM_tls_dial_ca;
