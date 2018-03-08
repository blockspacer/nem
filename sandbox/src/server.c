#include <sys/types.h>
#include <libgen.h>
#include <signal.h>

#include "nem.h"

static void
on_child_close(NEM_thunk1_t *thunk, void *varg)
{
	printf("[parent] child reported dead\n");
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
	NEM_panic_if_err(NEM_child_init(&child, app.kq, "./client", NULL));

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
