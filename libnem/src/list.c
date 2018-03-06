#include "nem.h"

typedef struct {
	int fd;
	int kq;
	NEM_thunk_t *on_kevent;
	NEM_thunk_t *on_stream;
}
NEM_list_base_t;

typedef struct {
	NEM_list_base_t base;
	char           *path;
}
NEM_list_unix_t;

typedef struct {
	NEM_list_base_t base;
}
NEM_list_tcp_t;

static void
NEM_list_unix_close(void *vthis)
{
	NEM_list_unix_t *this = vthis;
	if (0 != close(this->base.fd)) {
		NEM_panicf_errno("NEM_list_unix_close");
	}

	unlink(this->path);
	free(this->path);

	NEM_thunk_free(this->base.on_kevent);
	NEM_thunk_free(this->base.on_stream);
	free(this);
}

static void
NEM_list_tcp_close(void *vthis)
{
	NEM_list_tcp_t *this = vthis;
	if (0 != close(this->base.fd)) {
		NEM_panicf_errno("NEM_list_tcp_close");
	}

	NEM_thunk_free(this->base.on_kevent);
	NEM_thunk_free(this->base.on_stream);
	free(this);
}

static void
NEM_list_base_free_fd(NEM_thunk1_t *thunk, void *varg)
{
	NEM_fd_t *fd = NEM_thunk1_ptr(thunk);
	NEM_fd_free(fd);
	free(fd);
}

static void
NEM_list_base_on_kevent(NEM_thunk_t *thunk, void *varg)
{
	NEM_list_t *list = NEM_thunk_inlineptr(thunk);
	NEM_list_base_t *this = list->this;

	int fd = accept(this->fd, NULL, NULL);
	if (-1 == fd) {
		NEM_panicf_errno("NEM_list_base_on_kevent: accept");
	}

	NEM_fd_t *nfd = NEM_malloc(sizeof(NEM_fd_t));
	NEM_err_t err = NEM_fd_init(nfd, this->kq, fd);
	if (!NEM_err_ok(err)) {
		// err. Just ignore this.
		free(nfd);
		return;
	}

	NEM_fd_on_close(nfd, NEM_thunk1_new_ptr(
		&NEM_list_base_free_fd,
		nfd
	));

	NEM_list_ca ca = {
		.err    = NEM_err_none,
		.list   = *list,
		.stream = NEM_fd_as_stream(nfd),
	};
	NEM_thunk_invoke(this->on_stream, &ca);
}

const NEM_list_vt NEM_list_unix_vt = {
	.close = &NEM_list_unix_close,
};

const NEM_list_vt NEM_list_tcp_vt = {
	.close = &NEM_list_tcp_close,
};

NEM_err_t
NEM_list_init_unix(
	NEM_list_t  *this,
	int          kq,
	const char  *path,
	NEM_thunk_t *on_stream
) {
	NEM_err_t err = NEM_err_none;
	int fd_list = 0;
	NEM_thunk_t *thunk = NULL;

	if (strlen(path) + 1 > NEM_MSIZE(struct sockaddr_un, sun_path)) {
		err = NEM_err_static("NEM_list_init_unix: path too long");
		goto done;
	}

	fd_list = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == fd_list) {
		err = NEM_err_errno();
		goto done;
	}

	struct sockaddr_un addr = {};
	strlcpy(addr.sun_path, path, sizeof(addr.sun_path));
	addr.sun_family = AF_LOCAL;
	addr.sun_len = SUN_LEN(&addr);

	if (-1 == bind(fd_list, (struct sockaddr*) &addr, SUN_LEN(&addr))) {
		err = NEM_err_errno();
		goto done;
	}
	if (-1 == listen(fd_list, 1)) {
		err = NEM_err_errno();
		goto done;
	}

	NEM_list_unix_t *uthis = NEM_malloc(sizeof(NEM_list_unix_t));
	thunk = NEM_thunk_new(&NEM_list_base_on_kevent, sizeof(NEM_list_t));
	NEM_list_t *tlist = NEM_thunk_inlineptr(thunk);
	tlist->this = uthis;
	tlist->vt = &NEM_list_unix_vt;

	struct kevent ev;
	EV_SET(&ev, fd_list, EVFILT_READ, EV_ADD, 0, 0, thunk);
	if (-1 == kevent(kq, &ev, 1, NULL, 0, NULL)) {
		err = NEM_err_errno();
		goto done;
	}

	uthis->base.kq = kq;
	uthis->base.fd = fd_list;
	uthis->base.on_kevent = thunk;
	uthis->base.on_stream = on_stream;
	uthis->path = strdup(path);

	this->this = uthis;
	this->vt = &NEM_list_unix_vt;

done:
	if (!NEM_err_ok(err)) {
		if (NULL != thunk) {
			NEM_thunk_free(thunk);
			free(uthis);
		}

		if (fd_list > 0) {
			unlink(path);
			close(fd_list);
		}

		NEM_thunk_free(on_stream);
	}

	return err;
}

NEM_err_t
NEM_list_init_tcp(
	NEM_list_t  *this,
	int          kq,
	int          port,
	const char  *ip,
	NEM_thunk_t *on_stream
) {
	NEM_err_t err = NEM_err_none;
	int fd_list = 0;
	NEM_thunk_t *thunk = NULL;

	fd_list = socket(PF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == fd_list) {
		err = NEM_err_errno();
		goto done;
	}

	struct sockaddr_in addr = {};
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (NULL == ip) {
		addr.sin_addr.s_addr = INADDR_ANY;
	}
	else {
		int ec = inet_aton(ip, &addr.sin_addr);
		if (0 == ec) {
			err = NEM_err_static("NEM_list_init_tcp: invalid addr");
			goto done;
		}
		else if (-1 == ec) {
			err = NEM_err_errno();
			goto done;
		}
	}

	if (-1 == bind(fd_list, (struct sockaddr*) &addr, sizeof(addr))) {
		err = NEM_err_errno();
		goto done;
	}

	if (-1 == listen(fd_list, 1)) {
		err = NEM_err_errno();
		goto done;
	}

	NEM_list_tcp_t *uthis = NEM_malloc(sizeof(NEM_list_tcp_t));
	thunk = NEM_thunk_new(&NEM_list_base_on_kevent, sizeof(NEM_list_t));
	NEM_list_t *tlist = NEM_thunk_inlineptr(thunk);
	tlist->this = uthis;
	tlist->vt = &NEM_list_tcp_vt;

	struct kevent ev;
	EV_SET(&ev, fd_list, EVFILT_READ, EV_ADD, 0, 0, thunk);
	if (-1 == kevent(kq, &ev, 1, NULL, 0, NULL)) {
		err = NEM_err_errno();
		goto done;
	}

	uthis->base.kq = kq;
	uthis->base.fd = fd_list;
	uthis->base.on_kevent = thunk;
	uthis->base.on_stream = on_stream;

	this->this = uthis;
	this->vt = &NEM_list_tcp_vt;

done:
	if (!NEM_err_ok(err)) {
		if (NULL != thunk) {
			NEM_thunk_free(thunk);
			free(uthis);
		}

		if (fd_list > 0) {
			close(fd_list);
		}

		NEM_thunk_free(on_stream);
	}

	return err;
}
