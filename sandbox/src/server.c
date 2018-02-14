#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdlib.h>
#define _WITH_DPRINTF
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#include "nem.h"

typedef enum {
	CHILD_INIT,
	CHILD_RUNNING,
	CHILD_STOPPED,
}
NEM_child_state_t;

typedef struct {
	pid_t             pid;
	NEM_child_state_t state;
	int               kq;
	int               fds_stdin[2];
	int               fds_stdout[2];
	NEM_thunk_t      *on_kevent;
	NEM_thunk_t      *on_msg;
	NEM_thunk1_t     *on_close;
}
NEM_child_t;

static void
NEM_child_on_kevent(NEM_thunk_t *thunk, void *varg)
{
	NEM_child_t *this = NEM_thunk_ptr(thunk);
	struct kevent *ev = varg;

	if (EVFILT_READ == ev->filter) {
		fprintf(stdout, "[parent] child read ready (%lu)\n", ev->data);
		if (0 < ev->data) {
			char *data = NEM_malloc(((size_t) ev->data) + 1);
			if (0 > read(ev->ident, data, ev->data)) {
				NEM_panic("read");
			}
			fprintf(stdout, "[child] %s\n", data);
			free(data);
		}
	}
	else if (EVFILT_WRITE == ev->filter) {
		fprintf(stdout, "[parent] child write ready\n");
		dprintf(this->fds_stdin[0], "hello");
	}
	else if (EVFILT_PROC == ev->filter) {
		fprintf(stdout, "[parent] proc exit\n");
		this->state = CHILD_STOPPED;

		if (NULL != this->on_close) {
			NEM_thunk1_invoke(&this->on_close, NULL);
		}
	}
}

NEM_err_t
NEM_child_init(NEM_child_t *this, int kq)
{
	bzero(this, sizeof(*this));

	if (0 != pipe2(this->fds_stdin, O_CLOEXEC)) {
		return NEM_err_errno();
	}

	if (0 != pipe2(this->fds_stdout, O_CLOEXEC)) {
		close(this->fds_stdin[0]);
		close(this->fds_stdin[1]);
		return NEM_err_errno();
	}

	NEM_thunk_t *cb = NEM_thunk_new_ptr(&NEM_child_on_kevent, this);
	this->on_kevent = cb;
	this->kq = kq;

	struct kevent evs[2];
	EV_SET(&evs[0], this->fds_stdin[0], EVFILT_WRITE, EV_ADD, 0, 0, cb);
	EV_SET(&evs[0], this->fds_stdout[0], EVFILT_READ, EV_ADD, 0, 0, cb);
	if (-1 == kevent(kq, evs, NEM_ARRSIZE(evs), NULL, 0, NULL)) {
		close(this->fds_stdin[0]);
		close(this->fds_stdin[1]);
		close(this->fds_stdout[0]);
		close(this->fds_stdout[1]);
		NEM_thunk_free(cb);
		return NEM_err_errno();
	}

	this->state = CHILD_INIT;
	return NEM_err_none;
}

NEM_err_t
NEM_child_run(NEM_child_t *this, const char *path)
{
	if (CHILD_INIT != this->state) {
		return NEM_err_static("NEM_child_run: already running");
	}

	this->state = CHILD_RUNNING;

	this->pid = fork();
	if (0 == this->pid) {
		if (STDIN_FILENO != dup2(this->fds_stdin[1], STDIN_FILENO)) {
			NEM_panic("NEM_child_run: dup2");
		}
		if (STDOUT_FILENO != dup2(this->fds_stdout[1], STDOUT_FILENO)) {
			NEM_panic("NEM_child_run: dup2");
		}

		char *args[] = { NULL };
		char *env[] = { NULL };

		if (-1 == execve(path, args, env)) {
			NEM_panic("NEM_child_run: execve");
		}
	}

	struct kevent evs[1];
	EV_SET(
		&evs[0],
		this->pid,
		EVFILT_PROC,
		EV_ADD,
		NOTE_EXIT,
		0,
		this->on_kevent
	);
	if (-1 == kevent(this->kq, evs, NEM_ARRSIZE(evs), NULL, 0, NULL)) {
		// XXX: ???
		NEM_panic("NEM_child_run: cannot add to kqueue");
	}

	return NEM_err_none;
}	

void
NEM_child_free(NEM_child_t *this)
{
	// NB: Unconditionally remove the event. Ignore any errors.
	struct kevent ev;
	EV_SET(&ev, this->pid, EVFILT_PROC, EV_DELETE, NOTE_EXIT, 0, 0);
	kevent(this->kq, &ev, 1, NULL, 0, NULL);

	if (CHILD_STOPPED != this->state) {
		// If we're still running, unconditionally kill the child.
		kill(this->pid, SIGKILL);

		if (NULL != this->on_close) {
			NEM_thunk1_invoke(&this->on_close, NULL);
		}
	}

	if (NULL != this->on_msg) {
		NEM_thunk_free(this->on_msg);
	}
	if (NULL != this->on_kevent) {
		NEM_thunk_free(this->on_kevent);
	}

	close(this->fds_stdin[0]);
	close(this->fds_stdin[1]);
	close(this->fds_stdout[0]);
	close(this->fds_stdout[1]);
}

static void
on_child_close(NEM_thunk1_t *thunk, void *varg)
{
	bool *running = NEM_thunk1_ptr(thunk);
	*running = false;
}

int
main(int argc, char *argv[])
{
	NEM_err_t err;
	bool running = true;

	if (0 != chdir(dirname(argv[0]))) {
		NEM_panic("chdir");
	}

	int kq = kqueue();
	if (-1 == kq) {
		NEM_panic("kqueue");
	}

	NEM_child_t child;
	err = NEM_child_init(&child, kq);
	if (!NEM_err_ok(err)) {
		NEM_panic(NEM_err_string(err));
	}

	close(STDIN_FILENO);

	err = NEM_child_run(&child, "./client");
	if (!NEM_err_ok(err)) {
		NEM_panic(NEM_err_string(err));
	}

	child.on_close = NEM_thunk1_new_ptr(
		&on_child_close,
		&running
	);

	while (running) {
		struct kevent trig;
		fprintf(stdout, "[parent] waiting\n");
		if (-1 == kevent(kq, NULL, 0, &trig, 1, NULL)) {
			NEM_panic("kevent");
		}
		if (EV_ERROR == (trig.flags & EV_ERROR)) {
			fprintf(stderr, "EV_ERROR: %s", strerror(trig.data));
			break;
		}

		NEM_thunk_t *thunk = trig.udata;
		NEM_thunk_invoke(thunk, &trig);
	}

	NEM_child_free(&child);
}
