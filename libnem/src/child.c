#include "nem.h"

static void
NEM_child_on_kevent(NEM_thunk_t *thunk, void *varg)
{
	NEM_child_t *this = NEM_thunk_ptr(thunk);
	struct kevent *kev = varg;

	if (EVFILT_PROC != kev->filter) {
		NEM_panic("NEM_child_on_kevent: unexpected kevent");
	}

	this->state = CHILD_STOPPED;
	if (NULL != this->on_close) {
		NEM_thunk1_invoke(&this->on_close, NULL);
	}
}

NEM_err_t
NEM_child_init(
	NEM_child_t  *this,
	int           kq,
	const char   *path,
	NEM_thunk1_t *preexec
) {
	bzero(this, sizeof(*this));

	this->state = CHILD_RUNNING;
	NEM_fd_t fd_out;

	NEM_err_t err = NEM_fd_init_unix(&this->fd, &fd_out, kq);
	if (!NEM_err_ok(err)) {
		return err;
	}

	this->pid = fork();
	if (0 == this->pid) {
		int devnull = open("/dev/null", O_RDWR | O_CLOEXEC);
		if (devnull < 1) {
			NEM_panicf_errno("NEM_child_init: can't open devnull");
		}

		if (STDIN_FILENO != dup2(devnull, STDIN_FILENO)) {
			NEM_panicf_errno("NEM_child_init: dup2");
		}
		if (NEM_APP_FILENO != dup2(fd_out.fd_in, NEM_APP_FILENO)) {
			NEM_panicf_errno("NEM_child_init: dup2");
		}
		// NB: Inherit stdout/stderr for now.

		if (NULL != preexec) {
			NEM_thunk1_invoke(&preexec, NULL);
		}
		
		char *args[] = { NULL };
		char *env[] = { NULL };

		execve(path, args, env);
		NEM_panicf_errno("NEM_child_init: execve");
	}
	NEM_fd_free(&fd_out);
	if (NULL != preexec) {
		NEM_thunk1_discard(&preexec);
	}

	this->on_kevent = NEM_thunk_new_ptr(
		&NEM_child_on_kevent,
		this
	);

	struct kevent kev;
	EV_SET(&kev, this->pid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, this->on_kevent);
	if (-1 == kevent(kq, &kev, 1, NULL, 0, NULL)) {
		NEM_panicf_errno("NEM_child_run: kevent");
	}

	this->kq = kq;
	NEM_chan_init(&this->chan, NEM_fd_as_stream(&this->fd));

	return NEM_err_none;
}

NEM_err_t
NEM_child_on_close(NEM_child_t *this, NEM_thunk1_t *thunk)
{
	if (this->state != CHILD_RUNNING) {
		NEM_thunk1_invoke(&thunk, NULL);
		return NEM_err_none;
	}
	if (NULL != this->on_close) {
		return NEM_err_static("NEM_child_on_close: already bound");
	}

	this->on_close = thunk;
	return NEM_err_none;
}

void
NEM_child_free(NEM_child_t *this)
{
	struct kevent kev;
	EV_SET(&kev, this->pid, EVFILT_PROC, EV_DELETE, NOTE_EXIT, 0, 0);
	kevent(this->kq, &kev, 1, NULL, 0, NULL);

	if (CHILD_STOPPED != this->state) {
		kill(this->pid, SIGKILL);
	}

	NEM_fd_free(&this->fd);
	NEM_chan_free(&this->chan);

	if (NULL != this->on_kevent) {
		NEM_thunk_free(this->on_kevent);
	}
	if (NULL != this->on_close) {
		NEM_thunk1_invoke(&this->on_close, NULL);
	}
}
