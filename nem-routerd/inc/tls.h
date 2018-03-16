#pragma once

// NEM_tls_key_t is a private key. It's loaded from PEM format, either on disk
// or memory. You should never need to call NEM_tls_key_free -- passing it into
// a NEM_tls_t passes ownership of the object.
typedef struct NEM_tls_key_t NEM_tls_key_t;
NEM_err_t NEM_tls_key_init_file(NEM_tls_key_t **this, const char *path);
NEM_err_t NEM_tls_key_init(NEM_tls_key_t **this, const void *bs, size_t len);
void NEM_tls_key_free(NEM_tls_key_t *this);

// NEM_tls_cert_t is a certificate. Again, PEM format. Ditto with ownership
// semantics.
typedef struct NEM_tls_cert_t NEM_tls_cert_t;
NEM_err_t NEM_tls_cert_init_file(NEM_tls_cert_t **this, const char *path);
NEM_err_t NEM_tls_cert_init(NEM_tls_cert_t **this, const void *bs, size_t len);
void NEM_tls_cert_free(NEM_tls_cert_t *this);

// NEM_tls_t is the configuration for a TLS connection (either client or 
// server).
typedef struct NEM_tls_t NEM_tls_t;

// NEM_tls_init initializes a new NEM_tls_t.
NEM_err_t NEM_tls_init(NEM_tls_t **this);

// NEM_tls_add_ca adds a CA to the list of CAs from which peer certificates
// are accepted. This applies to both server and client certs.
//
// The passed in NEM_tls_cert_t becomes owned by this NEM_tls_t.
void NEM_tls_add_ca(NEM_tls_t *this, NEM_tls_cert_t *cacert);

// NEM_tls_add_cert adds a key-cert pair to be used for communication.
// Outgoing requests always use the first-added key-cert pair, so make sure
// that's the canonical one for authorization.
//
// Both the NEM_tls_key_t and the NEM_tls_cert_t become owned by this
// NEM_tls_t and are managed by it (even if an error is returned).
NEM_err_t NEM_tls_add_cert(
	NEM_tls_t      *this,
	NEM_tls_key_t  *key,
	NEM_tls_cert_t *cert
);
void NEM_tls_free(NEM_tls_t *this);

NEM_err_t NEM_tls_list_init(
	NEM_list_t  *list,
	int          kq,
	NEM_tls_t   *tls,
	int          port,
	NEM_thunk_t *on_stream
);

typedef struct {
	NEM_err_t    err;
	NEM_list_t   list;
	NEM_stream_t stream;
}
NEM_tls_list_ca;

void NEM_tls_dial_init(
	int           kq,
	NEM_tls_t    *tls,
	const char   *hostname,
	int           port,
	const char   *protocol,
	NEM_thunk1_t *on_stream
);

typedef struct {
	NEM_err_t    err;
	NEM_stream_t stream;
}
NEM_tls_dial_ca;
