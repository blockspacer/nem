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
	NEM_thunk1_t     *on_close;
	NEM_pipechan_t    pipe;
}
NEM_child_t;

static void
NEM_child_on_kevent(NEM_thunk_t *thunk, void *varg)
{
	NEM_child_t *this = NEM_thunk_ptr(thunk);
	struct kevent *ev = varg;

	if (EVFILT_PROC == ev->filter) {
		fprintf(stdout, "[parent] proc exit (%d)\n", (int)ev->data);
		this->state = CHILD_STOPPED;

		if (NULL != this->on_close) {
			NEM_thunk1_invoke(&this->on_close, NULL);
		}
	}
	else {
		NEM_panicf("unexpected kevent %d", ev->filter);
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

	NEM_err_t err;
	err = NEM_pipechan_init(
		&this->pipe,
		kq,
		this->fds_stdout[0],
		this->fds_stdin[0]
	);
	if (!NEM_err_ok(err)) {
		close(this->fds_stdin[0]);
		close(this->fds_stdin[1]);
		close(this->fds_stdout[0]);
		close(this->fds_stdout[1]);
		return NEM_err_errno();
	}

	this->kq = kq;
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

		execve(path, args, env);
		NEM_panic("NEM_child_run: execve");
	}

	this->on_kevent = NEM_thunk_new_ptr(
		&NEM_child_on_kevent,
		this
	);

	struct kevent ev;
	EV_SET(
		&ev,
		this->pid,
		EVFILT_PROC,
		EV_ADD,
		NOTE_EXIT,
		0,
		this->on_kevent
	);
	if (-1 == kevent(this->kq, &ev, 1, NULL, 0, NULL)) {
		// XXX: ???
		NEM_panicf("NEM_child_run: cannot add to kqueue: %s", NEM_err_string(NEM_err_errno()));
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

	if (NULL != this->on_kevent) {
		NEM_thunk_free(this->on_kevent);
	}

	NEM_pipechan_free(&this->pipe);

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

static void
on_child_msg(NEM_thunk_t *thunk, void *varg)
{
	NEM_msg_t *msg = varg;
	fprintf(stdout, "[child] %s\n", msg->body);
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
		NEM_panicf("NEM_child_init: %s", NEM_err_string(err));
	}

	close(STDIN_FILENO);

	err = NEM_child_run(&child, "./client");
	if (!NEM_err_ok(err)) {
		NEM_panicf("NEM_child_run: %s", NEM_err_string(err));
	}

	child.on_close = NEM_thunk1_new_ptr(
		&on_child_close,
		&running
	);

	NEM_pipechan_on_msg(&child.pipe, NEM_thunk_new(&on_child_msg, 0));
	NEM_msg_t *msg = NEM_msg_alloc(0, 6);
	memcpy(msg->body, "hello", 6);
	NEM_pipechan_send_msg(&child.pipe, msg);

	while (running) {
		struct kevent trig;
		if (-1 == kevent(kq, NULL, 0, &trig, 1, NULL)) {
			NEM_panic("kevent");
		}
		if (EV_ERROR == (trig.flags & EV_ERROR)) {
			fprintf(stderr, "EV_ERROR: %s", strerror(trig.data));
			break;
		}

		NEM_thunk_t *thunk = trig.udata;
		if (NULL == trig.udata) {
			NEM_panicf("NULL udata filter=%d", trig.filter);
		}
		NEM_thunk_invoke(thunk, &trig);
	}

	NEM_child_free(&child);
}
