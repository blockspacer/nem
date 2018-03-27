#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "nem.h"

static void
on_msg(NEM_thunk_t *thunk, void *varg)
{
	NEM_chan_ca *ca = varg;
	NEM_panic_if_err(ca->err);
	NEM_app_t *app = NEM_thunk_ptr(thunk);

	NEM_msg_t *res;
	printf(
		"[child] got command %s/%s %hu (seq=%lu)\n",
		NEM_svcid_to_string(ca->msg->packed.service_id),
		NEM_cmdid_to_string(ca->msg->packed.service_id, ca->msg->packed.command_id),
		ca->msg->packed.command_id,
		ca->msg->packed.seq
	);

	if (ca->msg->packed.service_id != NEM_svcid_daemon) {
		return;
	}

	switch (ca->msg->packed.command_id) {
		case NEM_cmdid_daemon_info:
			res = NEM_msg_new(0, 6);
			memcpy(res->body, "hello", 6);
			res->packed.seq = ca->msg->packed.seq;
			break;

		case NEM_cmdid_daemon_stop:
			res = NEM_msg_new(0, 0);
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
