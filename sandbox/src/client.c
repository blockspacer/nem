#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "nem.h"

static void
NEM_panic_if_err(NEM_err_t err)
{
	if (!NEM_err_ok(err)) {
		NEM_panicf("Error: %s", NEM_err_string(err));
	}
}

static void
on_msg(NEM_thunk_t *thunk, void *varg)
{
	NEM_chan_ca *ca = varg;
	NEM_panic_if_err(ca->err);
	NEM_app_t *app = NEM_thunk_ptr(thunk);

	NEM_msg_t *res;
	printf(
		"[child] got command %hu (seq=%lu)\n",
		ca->msg->packed.command_id,
		ca->msg->packed.seq
	);

	switch (ca->msg->packed.command_id) {
		case 1:
			res = NEM_msg_alloc(0, 6);
			memcpy(res->body, "hello", 6);
			res->packed.seq = ca->msg->packed.seq;
			break;

		case 2:
			res = NEM_msg_alloc(0, 0);
			res->packed.seq = ca->msg->packed.seq;
			NEM_app_stop(app);
			break;

		default:
			NEM_panicf("invalid command %d", ca->msg->packed.command_id);
	}

	NEM_chan_send(app->chan, res);
}

int
main(int argc, char *argv[])
{
	NEM_app_t app;
	NEM_panic_if_err(NEM_app_init(&app));

	NEM_chan_on_msg(app.chan, NEM_thunk_new_ptr(
		&on_msg,
		&app
	));

	NEM_app_run(&app);
	NEM_app_free(&app);
}
