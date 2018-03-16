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

	// XXX: Hook up entropy.

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
 * NEM_tls_conn_t
 */

typedef struct {
	NEM_tls_t          *tls;
	mbedtls_ssl_context ctx;

	bool in_handshake;
	bool running;

	int           fd;
	NEM_thunk_t  *on_kevent;
	NEM_thunk1_t *on_close;
	NEM_thunk1_t *on_handshake;

	size_t        wcap;
	size_t        wavail;
	char         *wbuf;
	NEM_thunk1_t *on_write;

	size_t        rcap;
	size_t        ravail;
	char         *rbuf;
	NEM_thunk1_t *on_read;
}
NEM_tls_conn_t;

static NEM_stream_t NEM_tls_conn_as_stream(NEM_tls_conn_t *this);

static void
NEM_tls_conn_shutdown(NEM_tls_conn_t *this, NEM_err_t err)
{
	bool was_running = this->running;
	this->running = false;

	if (!was_running) {
		return;
	}

	if (NEM_err_ok(err)) {
		err = NEM_err_static("NEM_tls_conn_t: shutdown");
	}

	close(this->fd);
	NEM_thunk_free(this->on_kevent);

	mbedtls_ssl_free(&this->ctx);
	NEM_tls_free(this->tls);

	NEM_stream_ca ca = {
		.stream = NEM_tls_conn_as_stream(this),
		.err    = err,
	};
	if (NULL != this->on_handshake) {
		NEM_thunk1_discard(&this->on_handshake);
	}
	if (NULL != this->on_write) {
		NEM_thunk1_invoke(&this->on_write, &ca);
	}
	if (NULL != this->on_read) {
		NEM_thunk1_invoke(&this->on_read, &ca);
	}
	// NB: Invoke on_close last.
	if (NULL != this->on_close) {
		NEM_thunk1_invoke(&this->on_close, &ca);
	}
}

static int
NEM_tls_conn_biowrite(void *vthis, const unsigned char *buf, size_t len)
{
	NEM_tls_conn_t *this = vthis;
	if (0 == this->wavail) {
		return MBEDTLS_ERR_SSL_WANT_WRITE;
	}
	if (len > this->wavail) {
		len = this->wavail;
	}

	ssize_t nwrote = write(this->fd, buf, len);
	this->wavail -= nwrote;
	if (0 > nwrote) {
		NEM_tls_conn_shutdown(this, NEM_err_errno());
	}

	return (int) nwrote;
}

static int
NEM_tls_conn_bioread(void *vthis, unsigned char *buf, size_t len)
{
	NEM_tls_conn_t *this = vthis;
	if (0 == this->ravail) {
		return MBEDTLS_ERR_SSL_WANT_READ;
	}
	if (len > this->ravail) {
		len = this->ravail;
	}

	ssize_t nread = read(this->fd, buf, len);
	this->ravail -= nread;
	if (0 > nread) {
		NEM_tls_conn_shutdown(this, NEM_err_errno());
	}

	return (int) nread;
}

static void
NEM_tls_conn_try_read(NEM_tls_conn_t *this)
{
	if (0 == this->ravail) {
		return;
	}

	int nread = mbedtls_ssl_read(
		&this->ctx,
		(unsigned char*) this->rbuf,
		this->rcap
	);
	if (0 < nread) {
		NEM_tls_conn_shutdown(this, NEM_err_mbedtls(nread));
		return;
	}

	this->rcap -= nread;
	this->rbuf += nread;

	if (0 == this->rcap) {
		NEM_stream_ca ca = {
			.err    = NEM_err_none,
			.stream = NEM_tls_conn_as_stream(this),
		};
		NEM_thunk1_invoke(&this->on_read, &ca);
	}
}

static void
NEM_tls_conn_try_write(NEM_tls_conn_t *this)
{
	if (0 == this->wavail) {
		return;
	}

	int nwrote = mbedtls_ssl_write(
		&this->ctx,
		(unsigned char*) this->wbuf,
		this->wcap
	);
	if (0 < nwrote) {
		NEM_tls_conn_shutdown(this, NEM_err_mbedtls(nwrote));
		return;
	}

	this->wcap -= nwrote;
	this->wbuf += nwrote;

	if (0 == this->wcap) {
		NEM_stream_ca ca = {
			.err    = NEM_err_none,
			.stream = NEM_tls_conn_as_stream(this),
		};
		NEM_thunk1_invoke(&this->on_write, &ca);
	}
}

static void
NEM_tls_conn_on_kevent(NEM_thunk_t *thunk, void *varg)
{
	NEM_tls_conn_t *this = NEM_thunk_ptr(thunk);
	struct kevent *kev = varg;

	if (EVFILT_READ == kev->filter) {
		this->ravail = kev->data;
		if (!this->in_handshake && 0 < this->rcap) {
			NEM_tls_conn_try_read(this);
		}

		// XXX: Might be prematurely doing this, but ah well. Similar bug in
		// NEM_fd_t's read bits.
		if ((kev->flags & EV_EOF) && 0 == this->ravail) {
			NEM_tls_conn_shutdown(this, NEM_err_none);
		}
	}
	else if (EVFILT_WRITE == kev->filter) {
		this->wavail = kev->data;

		if (!this->in_handshake && 0 < this->wcap) {
			NEM_tls_conn_try_write(this);
		}
	}

	if (this->in_handshake) {
		int errcode = mbedtls_ssl_handshake(&this->ctx);
		if (0 == errcode) {
			this->in_handshake = false;
			NEM_thunk1_invoke(&this->on_handshake, this);
		}
		else if (
			MBEDTLS_ERR_SSL_WANT_READ != errcode
			&& MBEDTLS_ERR_SSL_WANT_WRITE != errcode
		) {
			NEM_tls_conn_shutdown(this, NEM_err_mbedtls(errcode));
			return;
		}
	}
}

static NEM_err_t
NEM_tls_conn_init(
	NEM_tls_conn_t *this,
	NEM_tls_t      *tls,
	int             fd,
	int             kq,
	bool            is_srv,
	NEM_thunk1_t   *on_done
) {
	bzero(this, sizeof(*this));
	this->fd = fd;
	this->on_handshake = on_done;

	mbedtls_ssl_init(&this->ctx);
	int errcode = mbedtls_ssl_setup(
		&this->ctx,
		(is_srv) ? &tls->cfg_srv : &tls->cfg_cli
	);
	if (0 != errcode) {
		NEM_err_t err = NEM_err_mbedtls(errcode);
		NEM_tls_conn_shutdown(this, err);
		return err;
	}

	mbedtls_ssl_set_bio(
		&this->ctx,
		this,
		&NEM_tls_conn_biowrite,
		&NEM_tls_conn_bioread,
		NULL
	);

	this->on_kevent = NEM_thunk_new_ptr(
		&NEM_tls_conn_on_kevent,
		this
	);

	struct kevent kev[2];
	EV_SET(&kev[0], fd, EVFILT_READ, EV_ADD|EV_CLEAR, 0, 0, this->on_kevent);
	EV_SET(&kev[1], fd, EVFILT_WRITE, EV_ADD|EV_CLEAR, 0, 0, this->on_kevent);
	if (-1 == kevent(kq, kev, NEM_ARRSIZE(kev), NULL, 0, NULL)) {
		NEM_err_t err = NEM_err_errno();
		NEM_tls_conn_shutdown(this, err);
		return err;
	}

	return NEM_err_none;
}

static NEM_err_t
NEM_tls_conn_read(
	void         *vthis,
	void         *buf,
	size_t        len,
	NEM_thunk1_t *cb
) {
	NEM_tls_conn_t *this = vthis;
	if (this->in_handshake) {
		NEM_panic("NEM_tls_conn_read: still doing handshake");
	}
	if (NULL != this->on_read) {
		NEM_thunk1_discard(&cb);
		return NEM_err_static("NEM_tls_conn_read: interleaved read");
	}
	if (!this->running) {
		NEM_thunk1_discard(&cb);
		return NEM_err_static("NEM_tls_conn_read: closed");
	}

	this->rcap = len;
	this->rbuf = buf;
	this->on_read = cb;

	NEM_tls_conn_try_read(this);

	return NEM_err_none;
}

static NEM_err_t
NEM_tls_conn_write(
	void         *vthis,
	void         *buf,
	size_t        len,
	NEM_thunk1_t *cb
) {
	NEM_tls_conn_t *this = vthis;
	if (this->in_handshake) {
		NEM_panic("NEM_tls_conn_write: still doing handshake");
	}
	if (NULL != this->on_write) {
		NEM_thunk1_discard(&cb);
		return NEM_err_static("NEM_tls_conn_write: interleaved write");
	}
	if (!this->running) {
		NEM_thunk1_discard(&cb);
		return NEM_err_static("NEM_tls_conn_write: closed");
	}

	this->wcap = len;
	this->wbuf = buf;
	this->on_write = cb;

	NEM_tls_conn_try_write(this);

	return NEM_err_none;
}

static NEM_err_t
NEM_tls_conn_close(void *vthis)
{
	NEM_tls_conn_t *this = vthis;
	NEM_tls_conn_shutdown(this, NEM_err_none);
	return NEM_err_none;
}

static NEM_err_t
NEM_tls_conn_on_close(void *vthis, NEM_thunk1_t *on_close)
{
	NEM_tls_conn_t *this = vthis;
	if (!this->running) {
		NEM_stream_ca ca = {
			.stream = NEM_tls_conn_as_stream(this),
			.err    = NEM_err_static("NEM_tls_conn_on_close: closed"),
		};
		NEM_thunk1_invoke(&on_close, &ca);
		return NEM_err_none;
	}

	if (NULL != this->on_close) {
		NEM_thunk1_discard(&on_close);
		return NEM_err_static("NEM_tls_conn_on_close: already bound");
	}

	this->on_close = on_close;
	return NEM_err_none;
}

static NEM_stream_t
NEM_tls_conn_as_stream(NEM_tls_conn_t *this)
{
	static const NEM_stream_vt vt = {
		.read     = &NEM_tls_conn_read,
		.write    = &NEM_tls_conn_write,
		.close    = &NEM_tls_conn_close,
		.on_close = &NEM_tls_conn_on_close,
	};

	NEM_stream_t s = {
		.this = this,
		.vt   = &vt,
	};

	return s;
}

/*
 * NEM_tls_list_t
 */

typedef struct {
	int          kq;
	int          fd;
	NEM_list_t   listener;
	NEM_tls_t   *tls;
	NEM_thunk_t *on_stream;
}
NEM_tls_list_t;

static NEM_list_t NEM_tls_list_as_list(NEM_tls_list_t *this);

static void
NEM_tls_list_free(NEM_tls_list_t *this)
{
	if (NULL != this->listener.this) {
		NEM_list_close(this->listener);
	}

	NEM_thunk_free(this->on_stream);
	NEM_tls_free(this->tls);
	free(this);
}

static void
NEM_tls_list_on_avail(NEM_thunk1_t *thunk, void *varg)
{
	NEM_tls_list_t *this = NEM_thunk1_ptr(thunk);
	NEM_tls_conn_t *conn = varg;

	NEM_list_ca ca = {
		.err    = NEM_err_none,
		.list   = NEM_tls_list_as_list(this),
		.stream = NEM_tls_conn_as_stream(conn),
	};
	NEM_thunk_invoke(this->on_stream, &ca);
}

static void
NEM_tls_list_on_conn(NEM_thunk_t *thunk, void *varg)
{
	NEM_list_ca *ca = varg;
	NEM_tls_list_t *this = NEM_thunk_ptr(thunk);

	if (!NEM_err_ok(ca->err)) {
		NEM_panic("NEM_tls_list_on_conn: accept failed?");
	}

	// We're using a TCP listener here, so ... let's be a bit shitty
	// to avoid rewriting code eh?
	NEM_fd_t *nfd = (NEM_fd_t*) ca->stream.this;
	int fd = dup(nfd->fd_in);
	if (-1 == fd) {
		NEM_panicf_errno("NEM_tls_list_on_conn: dup");
	}

	NEM_fd_close(nfd);

	NEM_tls_conn_t *conn = NEM_malloc(sizeof(NEM_tls_conn_t));
	NEM_err_t err = NEM_tls_conn_init(
		conn,
		this->tls,
		fd,
		this->kq,
		true,
		NEM_thunk1_new_ptr(
			&NEM_tls_list_on_avail,
			this
		)
	);
	if (!NEM_err_ok(err)) {
		NEM_panicf("NEM_tls_list_on_conn: %s", NEM_err_string(err));
	}
}

static void
NEM_tls_list_close(void *vthis)
{
	NEM_tls_list_t *this = vthis;
	NEM_tls_list_free(this);
}

static NEM_list_t
NEM_tls_list_as_list(NEM_tls_list_t *this)
{
	static const NEM_list_vt vt = {
		.close = &NEM_tls_list_close,
	};

	NEM_list_t list = {
		.this = this,
		.vt   = &vt,
	};

	return list;
}

NEM_err_t
NEM_tls_list_init(
	NEM_list_t  *list,
	NEM_tls_t   *tls,
	int          kq,
	int          port,
	const char  *addr,
	NEM_thunk_t *on_stream
) {
	NEM_tls_list_t *this = NEM_malloc(sizeof(NEM_tls_list_t));
	this->tls = NEM_tls_copy(tls);
	this->on_stream = on_stream;
	this->kq = kq;

	NEM_thunk_t *thunk = NEM_thunk_new_ptr(
		&NEM_tls_list_on_conn,
		this
	);

	NEM_err_t err = NEM_list_init_tcp(
		&this->listener,
		kq,
		port,
		addr,
		thunk
	);

	if (!NEM_err_ok(err)) {
		NEM_tls_list_free(this);
		return err;
	}

	*list = NEM_tls_list_as_list(this);

	return NEM_err_none;
}
