#include <sys/types.h>
#include <libgen.h>
#include <signal.h>

#include "nem.h"

typedef enum {
	CHILD_RUNNING,
	CHILD_STOPPED,
}
NEM_child_state_t;

typedef struct {
	int               kq;
	pid_t             pid;
	NEM_child_state_t state;
	NEM_fd_t          fd;
	NEM_chan_t        chan;
	NEM_thunk_t      *on_kevent;
	NEM_thunk1_t     *on_close;
}
NEM_child_t;

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
NEM_child_init(NEM_child_t *this, int kq, const char *path)
{
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
		
		char *args[] = { NULL };
		char *env[] = { NULL };

		execve(path, args, env);
		NEM_panicf_errno("NEM_child_init: execve");
	}
	NEM_fd_free(&fd_out);

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

void
NEM_child_on_close(NEM_child_t *this, NEM_thunk1_t *thunk)
{
	if (this->state != CHILD_RUNNING) {
		NEM_thunk1_invoke(&thunk, NULL);
		return;
	}

	this->on_close = thunk;
	printf("[parent] child died yay\n");
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
	if (NULL != this->on_close) {
		NEM_thunk1_invoke(&this->on_close, NULL);
	}
	if (NULL != this->on_kevent) {
		NEM_thunk_free(this->on_kevent);
	}

	NEM_fd_free(&this->fd);
}

static void
NEM_panic_if_err(NEM_err_t err)
{
	if (!NEM_err_ok(err)) {
		NEM_panicf("Error: %s", NEM_err_string(err));
	}
}

static void
on_child_close(NEM_thunk1_t *thunk, void *varg)
{
	NEM_app_t *app = NEM_thunk1_ptr(thunk);
	NEM_app_stop(app);
}

static void
on_child_msg(NEM_thunk_t *thunk, void *varg)
{
	NEM_child_t *child = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	NEM_panic_if_err(ca->err);

	switch (ca->msg->packed.seq) {
		case 1: {
			printf("[parent] child sent %s\n", ca->msg->body);
			printf("[parent] telling child to die\n");
			NEM_msg_t *msg = NEM_msg_alloc(0, 0);
			msg->packed.seq = 2;
			msg->packed.command_id = 2;
			NEM_chan_send(&child->chan, msg);
			break;
		}
		case 2:
			printf("[parent] child said it's dying\n");
			break;
		default:
			NEM_panicf("child sent invalid seq %lu", ca->msg->packed.seq);
	}
}

int
main(int argc, char *argv[])
{
	NEM_app_t app;
	NEM_panic_if_err(NEM_app_init_root(&app));

	if (0 != chdir(dirname(argv[0]))) {
		NEM_panicf_errno("chdir");
	}

	printf("[parent] spawning child\n");
	NEM_child_t child;
	NEM_panic_if_err(NEM_child_init(&child, app.kq, "./client"));

	NEM_child_on_close(&child, NEM_thunk1_new_ptr(
		&on_child_close,
		&app
	));

	NEM_chan_on_msg(&child.chan, NEM_thunk_new_ptr(on_child_msg, &child));

	NEM_msg_t *msg = NEM_msg_alloc(0, 0);
	msg->packed.command_id = 1;
	msg->packed.seq = 1;

	NEM_chan_send(&child.chan, msg);

	NEM_panic_if_err(NEM_app_run(&app));
	NEM_child_free(&child);
	NEM_app_free(&app);
}
