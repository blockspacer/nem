#include "nem.h"

static void
NEM_dial_fd_free(NEM_thunk1_t *thunk, void *varg)
{
	NEM_fd_t *fd = NEM_thunk1_ptr(thunk);
	NEM_fd_free(fd);
	free(fd);
}

void
NEM_dial_unix(int kq, const char *path, NEM_thunk1_t *on_stream)
{
	NEM_err_t err = NEM_err_none;
	NEM_dial_ca ca = {};
	int fd = 0;

	fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == fd) {
		err = NEM_err_errno();
		goto done;
	}

	struct sockaddr_un addr = {};
	strlcpy(addr.sun_path, path, sizeof(addr.sun_path));
	addr.sun_family = AF_LOCAL;
	addr.sun_len = SUN_LEN(&addr);

	if (-1 == connect(fd, (struct sockaddr*) &addr, sizeof(addr))) {
		err = NEM_err_errno();
		goto done;
	}

	NEM_fd_t *nfd = NEM_malloc(sizeof(NEM_fd_t));
	err = NEM_fd_init(nfd, kq, fd);
	if (!NEM_err_ok(err)) {
		free(nfd);
		goto done;
	}

	NEM_fd_on_close(nfd, NEM_thunk1_new_ptr(&NEM_dial_fd_free, nfd));
	ca.stream = NEM_fd_as_stream(nfd);

done:
	if (!NEM_err_ok(err)) {
		if (fd > 0) {
			close(fd);
		}
	}

	ca.err = err;
	NEM_thunk1_invoke(&on_stream, &ca);
}

typedef struct {
	NEM_thunk1_t *on_stream;
	int kq;
}
NEM_dial_tcp_baton_t;

static void
NEM_dial_tcp_cb(NEM_thunk_t *thunk, void *varg)
{
	struct kevent *kev = varg;
	NEM_dial_tcp_baton_t *baton = NEM_thunk_inlineptr(thunk);

	NEM_dial_ca ca = {};
	NEM_fd_t *nfd = NULL;

	if (EV_ERROR == (kev->flags & EV_ERROR)) {
		ca.err.source = NEM_ERR_SOURCE_POSIX;
		ca.err.code = kev->data;
		goto done;
	}

	nfd = NEM_malloc(sizeof(NEM_fd_t));
	ca.err = NEM_fd_init(nfd, baton->kq, kev->ident);
	if (!NEM_err_ok(ca.err)) {
		goto done;
	}

	NEM_fd_on_close(nfd, NEM_thunk1_new_ptr(&NEM_dial_fd_free, nfd));
	ca.stream = NEM_fd_as_stream(nfd);

done:
	if (!NEM_err_ok(ca.err)) {
		if (NULL != nfd) {
			free(nfd);
		}
	}

	NEM_thunk1_invoke(&baton->on_stream, &ca);
	NEM_thunk_free(thunk);
}

void
NEM_dial_tcp(int kq, int port, const char *ip, NEM_thunk1_t *on_stream)
{
	NEM_thunk_t *thunk = NULL;
	NEM_err_t err = NEM_err_none;
	NEM_dial_ca ca = {};
	int fd = 0;

	fd = socket(PF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (-1 == fd) {
		err = NEM_err_errno();
		goto done;
	}

	struct sockaddr_in addr = {};
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	int ec = inet_aton(ip, &addr.sin_addr);
	if (0 == ec) {
		err = NEM_err_static("NEM_dial_tcp: invalid addr");
		goto done;
	}
	else if (-1 == ec) {
		err = NEM_err_errno();
		goto done;
	}

	if (-1 == connect(fd, (struct sockaddr*) &addr, sizeof(addr))) {
		if (EINPROGRESS != errno) {
			err = NEM_err_errno();
			goto done;
		}
	}

	thunk = NEM_thunk_new(
		&NEM_dial_tcp_cb,
		sizeof(NEM_dial_tcp_baton_t)
	);
	NEM_dial_tcp_baton_t *baton = NEM_thunk_inlineptr(thunk);
	baton->on_stream = on_stream;
	baton->kq = kq;

	struct kevent kev;
	EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, thunk);
	if (-1 == kevent(kq, &kev, 1, NULL, 0, NULL)) {
		err = NEM_err_errno();
		goto done;
	}
	return;

done:
	if (!NEM_err_ok(err)) {
		if (0 < fd) {
			close(fd);
		}

		if (NULL != thunk) {
			NEM_thunk_free(thunk);
		}
	}

	ca.err = err;
	NEM_thunk1_invoke(&on_stream, &ca);
}

