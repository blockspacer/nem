#include <sys/types.h>
#include <sys/un.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "nem.h"

static void
NEM_unixchan_on_read(NEM_unixchan_t *this, size_t avail)
{
	NEM_panic("TODO");
}

static void
NEM_unixchan_on_write(NEM_unixchan_t *this, size_t avail)
{
	NEM_panic("TODO");
}

static void
NEM_unixchan_on_pre_kevent(NEM_unixchan_t *this, struct kevent *ev)
{
	if (ev->ident == this->fd_s) {
		// Accept the incoming connection. We'll get no more incoming
		// connections until we ask for more.
		if (ev->data <= 0) {
			return;
		}
		if (0 != this->fd_c2) {
			NEM_panic("NEM_unixchan_on_pre_kevent: state machine broken");
		}

		this->fd_c2 = accept(this->fd_s, NULL, NULL);
		if (-1 == this->fd_c2) {
			NEM_panic("NEM_unixchan_on_pre_kevent: accept failed?");
		}

		struct kevent ev;
		EV_SET(&ev, this->fd_c2, EVFILT_READ, EV_ADD, 0, 0, this->on_kevent);
		if (-1 == kevent(this->kq, &ev, 1, NULL, 0, NULL)) {
			NEM_panic(NEM_err_string(NEM_err_errno()));
		}
	}
	else if (ev->ident == this->fd_c1) {
		// This is the client connection. We get notified when we're
		// connected and ready to write. The write needs to send the process
		// credentials to authenticate us.
		size_t clen = sizeof(struct cmsghdr) + sizeof(struct cmsgcred);
		struct cmsghdr *cmsg = alloca(clen);
		bzero(cmsg, clen);
		cmsg->cmsg_len = clen;
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_CREDS;

		struct msghdr msg = {
			.msg_control    = cmsg,
			.msg_controllen = clen,
		};

		if (-1 == sendmsg(this->fd_c1, &msg, 0)) {
			// XXX: uhh.
			NEM_panic(NEM_err_string(NEM_err_errno()));
		}

		// At this point we either get closed or whatever it's fine.
	}
	else if (ev->ident == this->fd_c2) {
		// This is the accepted connection. We get this when there's data
		// available to read, so check the sent credentials.
		size_t clen = sizeof(struct cmsghdr) + sizeof(struct cmsgcred);
		struct cmsghdr *cmsg = alloca(clen);
		cmsg->cmsg_len = clen;
		cmsg->cmsg_level = 0;
		cmsg->cmsg_type = 0;
		struct cmsgcred *cred = (struct cmsgcred*) CMSG_DATA(cmsg);

		struct msghdr msg = {
			.msg_control    = cmsg,
			.msg_controllen = clen,
		};

		if (-1 == recvmsg(this->fd_c2, &msg, 0)) {
			NEM_panic(NEM_err_string(NEM_err_errno()));
		}

		// If the credentials are bad, kill this->fd_c2 and try accepting
		// again until we get the socket we actually initiated.
		if (cmsg->cmsg_type != SCM_CREDS || cred->cmcred_pid != getpid()) {
			close(this->fd_c2);
			this->fd_c2 = 0;

			struct kevent ev;
			EV_SET(
				&ev,
				this->fd_s,
				EVFILT_READ,
				EV_ADD|EV_ONESHOT,
				0,
				0,
				this->on_kevent
			);
			if (-1 == kevent(this->kq, &ev, 1, NULL, 0, NULL)) {
				NEM_panic(NEM_err_string(NEM_err_errno()));
			}
		}
		else {
			// Okay got our sockets.
			close(this->fd_s);
			this->fd_s = 0;

			unlink(this->fd_path);
			free(this->fd_path);
			this->fd_path = NULL;

			// Hook our local socket back up to the normal message processing
			// routines.
			struct kevent ev;
			EV_SET(
				&ev,
				this->fd_c1,
				EVFILT_READ|EVFILT_WRITE,
				EV_ADD,
				0,
				0,
				this->on_kevent
			);
			if (-1 == kevent(this->kq, &ev, 1, NULL, 0, NULL)) {
				NEM_panic(NEM_err_string(NEM_err_errno()));
			}

			this->ready = true;

			if (NULL != this->on_ready) {
				NEM_thunk1_invoke(&this->on_ready, this);
			}
		}
	}
	else {
		NEM_panic("NEM_unixchan_on_pre_kevent: unexpected ident");
	}
}

static void
NEM_unixchan_on_kevent(NEM_thunk_t *thunk, void *varg)
{
	struct kevent *ev = varg;
	NEM_unixchan_t *this = NEM_thunk_ptr(thunk);

	if (!this->ready) {
		return NEM_unixchan_on_pre_kevent(this, ev);
	}

	if (this->fd_c1 != ev->ident) {
		NEM_panicf("NEM_unixchan_on_kevent: unexpected ident");
	}

	if (EVFILT_READ == ev->filter) {
		NEM_unixchan_on_read(this, ev->data);
	}
	else if (EVFILT_WRITE == ev->filter) {
		NEM_unixchan_on_write(this, ev->data);
	}
	else {
		NEM_panicf("NEM_unixchan_on_kevent: unexpected ev %d", ev->filter);
	}
}

NEM_err_t
NEM_unixchan_init(NEM_unixchan_t *this, int kq, NEM_thunk1_t *on_ready)
{
	bzero(this, sizeof(*this));
	this->on_ready = on_ready;

	this->fd_s = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == this->fd_s) {
		goto cleanup_1;
	}

	static int ctr = 0;
	asprintf(&this->fd_path, "nem-%d-%d.sock", getpid(), ++ctr);

	struct sockaddr_un addr = {};
	strlcpy(addr.sun_path, this->fd_path, sizeof(addr.sun_path));
	addr.sun_family = AF_LOCAL;
	addr.sun_len = SUN_LEN(&addr);

	if (-1 == bind(this->fd_s, (struct sockaddr*) &addr, SUN_LEN(&addr))) {
		goto cleanup_2;
	}

	if (-1 == listen(this->fd_s, 1)) {
		goto cleanup_3;
	}

	this->fd_c1 = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == this->fd_c1) {
		goto cleanup_3;
	}

	if (-1 == fcntl(this->fd_c1, F_SETFL, O_NONBLOCK)) {
		goto cleanup_4;
	}

	struct kevent evs[2];
	NEM_thunk_t *cb = NEM_thunk_new_ptr(&NEM_unixchan_on_kevent, this);
	EV_SET(&evs[0], this->fd_s, EVFILT_READ, EV_ADD|EV_ONESHOT, 0, 0, cb);
	EV_SET(&evs[1], this->fd_c1, EVFILT_WRITE, EV_ADD|EV_ONESHOT, 0, 0, cb);
	this->on_kevent = cb;
	this->kq = kq;

	if (-1 == kevent(kq, evs, NEM_ARRSIZE(evs), NULL, 0, NULL)) {
		goto cleanup_5;
	}

	int err = connect(this->fd_c1, (struct sockaddr*) &addr, SUN_LEN(&addr));
	if (err != 0 && EINPROGRESS != err) {
		goto cleanup_5;
	}

	return NEM_err_none;

	{
	cleanup_5:
		NEM_thunk_free(cb);
	cleanup_4:
		close(this->fd_c1);
	cleanup_3:
		unlink(this->fd_path);
	cleanup_2:
		free(this->fd_path);
		close(this->fd_s);
	cleanup_1:
		NEM_thunk1_discard(&this->on_ready);
		return NEM_err_errno();
	}
}

void
NEM_unixchan_free(NEM_unixchan_t *this)
{
	NEM_panic("TODO");
}

void
NEM_unixchan_on_msg(NEM_unixchan_t *this, NEM_thunk_t *cb)
{
	NEM_panic("TODO");
}

void
NEM_unixchan_send_msg(NEM_unixchan_t *this, NEM_msg_t *msg)
{
	NEM_panic("TODO");
}

int
NEM_unixchan_fd_remote(NEM_unixchan_t *this)
{
	NEM_panic("TODO");
}
