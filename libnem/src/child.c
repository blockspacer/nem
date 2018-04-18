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

static void
NEM_child_on_txnmgr_close(NEM_thunk1_t *thunk, void *varg)
{
	NEM_child_t *this = NEM_thunk1_ptr(thunk);
	NEM_child_stop(this);
}

NEM_err_t
NEM_child_init(
	NEM_child_t  *this,
	NEM_kq_t     *kq,
	const char   *path,
	NEM_thunk1_t *preexec
) {
	bzero(this, sizeof(*this));

	this->exe_fd = open(path, O_EXEC | O_CLOEXEC);
	if (0 > this->exe_fd) {
		return NEM_err_errno();
	}

	this->state = CHILD_RUNNING;
	NEM_fd_t fd_out;

	NEM_err_t err = NEM_fd_init_unix(&this->fd, &fd_out, kq->kq);
	if (!NEM_err_ok(err)) {
		close(this->exe_fd);
		this->exe_fd = 0;
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
		if (NEM_KQ_PARENT_FILENO != dup2(fd_out.fd_in, NEM_KQ_PARENT_FILENO)) {
			NEM_panicf_errno("NEM_child_init: dup2");
		}
		// NB: Inherit stdout/stderr for now.

		if (NULL != preexec) {
			NEM_thunk1_invoke(&preexec, NULL);
		}
		
		char *args[] = { NULL };
		char *env[] = { NULL };

		fexecve(this->exe_fd, args, env);
		NEM_panicf_errno("NEM_child_init: execve");
	}
	NEM_fd_free(&fd_out);
	if (NULL != preexec) {
		NEM_thunk1_discard(&preexec);
	}
	close(this->exe_fd);
	this->exe_fd = 0;

	this->on_kevent = NEM_thunk_new_ptr(
		&NEM_child_on_kevent,
		this
	);

	struct kevent kev;
	EV_SET(&kev, this->pid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, this->on_kevent);
	if (-1 == kevent(kq->kq, &kev, 1, NULL, 0, NULL)) {
		NEM_panicf_errno("NEM_child_run: kevent");
	}

	this->kq = kq->kq;
	NEM_txnmgr_init(&this->txnmgr, NEM_fd_as_stream(&this->fd), kq);
	NEM_txnmgr_on_close(&this->txnmgr, NEM_thunk1_new_ptr(
		&NEM_child_on_txnmgr_close,
		this
	));

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
NEM_child_stop(NEM_child_t *this)
{
	if (CHILD_STOPPED != this->state) {
		kill(this->pid, SIGKILL);
	}
}

void
NEM_child_free(NEM_child_t *this)
{
	// NB: Squelsh the event that'll be generated when we kill the
	// child, since we won't be around to handle it anymore.
	struct kevent kev;
	EV_SET(&kev, this->pid, EVFILT_PROC, EV_DELETE, NOTE_EXIT, 0, 0);
	kevent(this->kq, &kev, 1, NULL, 0, NULL);

	NEM_child_stop(this);

	NEM_fd_free(&this->fd);
	NEM_txnmgr_free(&this->txnmgr);

	if (NULL != this->on_kevent) {
		NEM_thunk_free(this->on_kevent);
	}
	if (NULL != this->on_close) {
		NEM_thunk1_invoke(&this->on_close, NULL);
	}
}
