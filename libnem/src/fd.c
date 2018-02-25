#include "nem.h"

static NEM_err_t
NEM_fd_stream_read(void *vthis, void *buf, size_t len, NEM_thunk1_t *cb)
{
	NEM_fd_t *this = vthis;
	return NEM_fd_read(this, buf, len, cb);
}

static NEM_err_t
NEM_fd_stream_write(void *vthis, void *buf, size_t len, NEM_thunk1_t *cb)
{
	NEM_fd_t *this = vthis;
	return NEM_fd_write(this, buf, len, cb);
}

static NEM_err_t
NEM_fd_stream_read_fd(void *vthis, int *fdout)
{
	NEM_fd_t *this = vthis;
	return NEM_fd_read_fd(this, fdout);
}

static NEM_err_t
NEM_fd_stream_write_fd(void *vthis, int fd)
{
	NEM_fd_t *this = vthis;
	return NEM_fd_write_fd(this, fd);
}

static NEM_err_t
NEM_fd_stream_close(void *vthis)
{
	NEM_fd_t *this = vthis;
	NEM_fd_close(this);
	return NEM_err_none;
}

static NEM_err_t
NEM_fd_stream_on_close(void *vthis, NEM_thunk1_t *cb)
{
	NEM_fd_t *this = vthis;
	NEM_fd_on_close(this, cb);
	return NEM_err_none;
}

static void
NEM_fd_shutdown(NEM_fd_t *this)
{
	bool was_running = this->running;
	this->running = false;

	if (!was_running) {
		return;
	}

	NEM_stream_ca ca = {
		.err    = NEM_err_static("NEM_fd_t: shutdown"),
		.stream = NEM_fd_as_stream(this),
	};

	if (NULL != this->on_write) {
		NEM_thunk1_invoke(&this->on_write, &ca);
	}

	if (NULL != this->on_read) {
		NEM_thunk1_invoke(&this->on_read, &ca);
	}

	if (NULL != this->on_close) {
		NEM_thunk1_invoke(&this->on_close, &ca);
	}

	if (close(this->fd_in)) {
		NEM_panicf_errno("NEM_fd_free: close(fd_in): %s");
	}
	if (this->fd_in != this->fd_out && close(this->fd_out)) {
		NEM_panicf_errno("NEM_fd_free: close(fd_out): %s");
	}
}

static void
NEM_fd_on_read(NEM_fd_t *this, size_t avail)
{
	this->ravail = avail;

	if (NULL == this->on_read || avail == 0) {
		return;
	}

	size_t want = avail > this->rcap ? this->rcap : avail;
	ssize_t got = read(this->fd_in, this->rbuf, want);
	if (-1 == got) {
		NEM_fd_shutdown(this);
		return;
	}

	this->rbuf += got;
	this->rcap -= got;
	this->ravail -= got;

	if (0 == this->rcap) {
		NEM_stream_ca ca = {
			.err    = NEM_err_none,
			.stream = NEM_fd_as_stream(this),
		};

		NEM_thunk1_invoke(&this->on_read, &ca);
	}
}

static void
NEM_fd_on_write(NEM_fd_t *this, size_t avail)
{
	this->wavail = avail;
	
	if (NULL == this->on_write || avail == 0) {
		return;
	}

	size_t want = avail > this->wcap ? this->wcap : avail;
	ssize_t got = write(this->fd_out, this->wbuf, want);
	if (-1 == got) {
		NEM_fd_shutdown(this);
		return;
	}

	this->wbuf += got;
	this->wcap -= got;
	this->wavail -= got;

	if (0 == this->wcap) {
		NEM_stream_ca ca = {
			.err    = NEM_err_none,
			.stream = NEM_fd_as_stream(this),
		};

		NEM_thunk1_invoke(&this->on_write, &ca);
	}
}

static void
NEM_fd_on_kevent(NEM_thunk_t *thunk, void *varg)
{
	struct kevent *ev = varg;
	NEM_fd_t *this = NEM_thunk_ptr(thunk);

	if (EVFILT_READ == ev->filter) {
		NEM_fd_on_read(this, ev->data);

		// XXX: This might be a bit fucked; we want to ingest the remainder
		// of the data on the read side before closing (so we don't close
		// prematurely) but also want to eventually close.
		if ((ev->flags & EV_EOF) && 0 == this->ravail) {
			NEM_fd_shutdown(this);
		}
	}
	else if (EVFILT_WRITE == ev->filter) {
		// Conversely, if the remote is shut down don't bother writing.
		if ((ev->flags & EV_EOF)) {
			NEM_fd_shutdown(this);
		}
		else {
			NEM_fd_on_write(this, ev->data);
		}
	}
	else {
		NEM_panicf("NEM_fd_on_kevent: unknown filter %d", ev->filter);
	}
}

NEM_err_t
NEM_fd_init(NEM_fd_t *this, int kq, int fd)
{
	return NEM_fd_init2(this, kq, fd, fd);
}

NEM_err_t
NEM_fd_init2(NEM_fd_t *this, int kq, int fd_in, int fd_out)
{
	bzero(this, sizeof(*this));
	this->fd_in = fd_in;
	this->fd_out = fd_out;
	NEM_thunk_t *on_ev = NEM_thunk_new_ptr(&NEM_fd_on_kevent, this);

	struct kevent evs[2];
	EV_SET(&evs[0], fd_in, EVFILT_READ, EV_ADD|EV_CLEAR, 0, 0, on_ev);
	EV_SET(&evs[1], fd_out, EVFILT_WRITE, EV_ADD|EV_CLEAR, 0, 0, on_ev);
	if (-1 == kevent(kq, evs, NEM_ARRSIZE(evs), NULL, 0, NULL)) {
		close(fd_in);
		close(fd_out);
		NEM_thunk_free(on_ev);
		return NEM_err_errno();
	}

	this->on_kevent = on_ev;
	this->running = true;

	return NEM_err_none;
}

NEM_err_t
NEM_fd_init_pipe(NEM_fd_t *this, NEM_fd_t *that, int kq)
{
	int fds[2];

	if (0 != pipe2(fds, O_CLOEXEC)) {
		return NEM_err_errno();
	}

	NEM_err_t err;

	err = NEM_fd_init(this, fds[0], kq);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = NEM_fd_init(that, fds[1], kq);
	if (!NEM_err_ok(err)) {
		NEM_fd_free(this);
		return err;
	}

	return NEM_err_none;
}

NEM_err_t
NEM_fd_init_unix(NEM_fd_t *this, NEM_fd_t *that, int kq)
{
	// UNIX sockets require a path, so create an arbitrary but unique string.
	// XXX: This should be moved to /tmp which is kind of a mess but whatever.
	static int ctr = 0;
	size_t path_len = NEM_MSIZE(struct sockaddr_un, sun_path);
	char *path = alloca(path_len);
	snprintf(path, path_len, "nem-%d-%d.sock", getpid(), ++ctr);

	// Bind a socket to that path. The file exists after the call to bind(2).
	int fd_list = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == fd_list) {
		goto cleanup_1;
	}

	struct sockaddr_un addr = {};
	strlcpy(addr.sun_path, path, path_len);
	addr.sun_family = AF_LOCAL;
	addr.sun_len = SUN_LEN(&addr);

	if (-1 == bind(fd_list, (struct sockaddr*) &addr, SUN_LEN(&addr))) {
		goto cleanup_2;
	}
	if (-1 == listen(fd_list, 1)) {
		goto cleanup_3;
	}

	// Create a second socket and connect it to the first one.
	int fd_cli = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == fd_cli) {
		goto cleanup_3;
	}

	if (0 != connect(fd_cli, (struct sockaddr*) &addr, SUN_LEN(&addr))) {
		goto cleanup_4;
	}

	// Then have the kernel send our credentials to the remote.
	size_t clen = sizeof(struct cmsghdr) + sizeof(struct cmsgcred);
	struct cmsghdr *cmsg = alloca(clen);
	bzero(cmsg, clen);
	cmsg->cmsg_len = clen;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_CREDS;
	struct cmsgcred *cred = (struct cmsgcred*) CMSG_DATA(cmsg);

	struct msghdr msg = {
		.msg_control    = cmsg,
		.msg_controllen = clen,
	};

	if (-1 == sendmsg(fd_cli, &msg, 0)) {
		goto cleanup_4;
	}

	int fd_srv;

	do {
		// Pull client sockets from the listening one until we get one that
		// already has data pending (since we already sent it).
		fd_srv = accept(fd_list, NULL, NULL);
		if (-1 == fd_srv) {
			goto cleanup_4;
		}

		bzero(cmsg, clen);

		if (-1 == recvmsg(fd_srv, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC)) {
			// EAGAIN is hit when fd_srv doesn't have any data pending.
			// ECONNRESET is if fd_srv's remote disconnects between accept
			// and recvmsg.
			if (EAGAIN == errno || ECONNRESET == errno) {
				close(fd_srv);
				continue;
			}

			close(fd_srv);
			goto cleanup_4;
		}

		// Then validate the incoming connection is from this process.
		if (SCM_CREDS != cmsg->cmsg_type || cred->cmcred_pid != getpid()) {
			close(fd_srv);
			continue;
		}

		break;
	}
	while (true);

	// Clean up the listening socket. Technically we can do this sooner.
	if (0 != unlink(path)) {
		goto cleanup_5;
	}

	if (0 != close(fd_list)) {
		goto cleanup_5;
	}

	// Then hook up the NEM_fd_t's to fd_cli and fd_srv.
	NEM_err_t err;
	err = NEM_fd_init(this, fd_srv, kq);
	if (!NEM_err_ok(err)) {
		close(fd_srv);
		close(fd_cli);
		return err;
	}

	err = NEM_fd_init(that, fd_cli, kq);
	if (!NEM_err_ok(err)) {
		close(fd_cli);
		NEM_fd_free(this);
		return err;
	}

	return NEM_err_none;

	{
	// Ignore errors here, we've already broken something.
	cleanup_5:
		close(fd_srv);
	cleanup_4:
		close(fd_cli);
	cleanup_3:
		unlink(path);
	cleanup_2:
		close(fd_list);
	cleanup_1:
		return NEM_err_errno();
	}
}

void
NEM_fd_free(NEM_fd_t *this)
{
	NEM_fd_shutdown(this);

	// NB: fds should be removed from the kqueue now, so we can safely
	// kill the kevent thunk.
	NEM_thunk_free(this->on_kevent);
}

NEM_stream_t
NEM_fd_as_stream(NEM_fd_t *this)
{
	NEM_stream_t stream = {
		.vt   = &NEM_fd_stream_vt,
		.this = this,
	};

	return stream;
}

NEM_err_t
NEM_fd_read(NEM_fd_t *this, void *buf, size_t len, NEM_thunk1_t *cb)
{
	if (!this->running) {
		return NEM_err_static("NEM_fd_read: already closed");
	}
	if (NULL != this->on_read) {
		return NEM_err_static("NEM_fd_read: interleaved reads");
	}

	this->rbuf = buf;
	this->rcap = len;
	this->on_read = cb;

	NEM_fd_on_read(this, this->ravail);
	return NEM_err_none;
}

NEM_err_t
NEM_fd_write(NEM_fd_t *this, void *buf, size_t len, NEM_thunk1_t *cb)
{
	if (!this->running) {
		return NEM_err_static("NEM_fd_write: already closed");
	}
	if (NULL != this->on_write) {
		return NEM_err_static("NEM_fd_write: interleaved writes");
	}

	this->wbuf = buf;
	this->wcap = len;
	this->on_write = cb;

	NEM_fd_on_write(this, this->wavail);
	return NEM_err_none;
}

NEM_err_t
NEM_fd_read_fd(NEM_fd_t *this, int *fdout)
{
	NEM_panic("TODO");
}

NEM_err_t
NEM_fd_write_fd(NEM_fd_t *this, int fd)
{
	NEM_panic("TODO");
}

void
NEM_fd_close(NEM_fd_t *this)
{
	NEM_fd_shutdown(this);
}

void
NEM_fd_on_close(NEM_fd_t *this, NEM_thunk1_t *cb)
{
	// XXX: Maybe this should be an error instead.
	if (NULL != this->on_close) {
		NEM_thunk1_discard(&this->on_close);
	}

	this->on_close = cb;
}

const NEM_stream_vt NEM_fd_stream_vt = {
	.read     = &NEM_fd_stream_read,
	.write    = &NEM_fd_stream_write,
	.read_fd  = &NEM_fd_stream_read_fd,
	.write_fd = &NEM_fd_stream_write_fd,
	.close    = &NEM_fd_stream_close,
	.on_close = &NEM_fd_stream_on_close,
};
