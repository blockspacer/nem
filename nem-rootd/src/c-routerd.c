#include "nem.h"
#include "state.h"
#include "lifecycle.h"
#include "c_routerd.h"
#include "txnmgr.h"
#include "svcmgr.h"

static NEM_child_t        child;
static NEM_rootd_svcmgr_t svcmgr;
static NEM_rootd_txnmgr_t txnmgr;
static bool               is_running = false;
static bool               want_running = true;
static bool               shutdown_sent = false;

extern NEM_rootd_svcmgr_t NEM_rootd_svc_daemon;

static NEM_err_t routerd_start(NEM_app_t *app);

static void
routerd_restart(NEM_thunk1_t *thunk, void *varg)
{
	if (!want_running) {
		printf("c-routerd: nevermind, leave it dead\n");
		return;
	}
	if (is_running) {
		printf("c-routerd: child resurrected itself?!\n");
		return;
	}

	NEM_app_t *app = NEM_thunk1_ptr(thunk);
	NEM_err_t err = routerd_start(app);
	if (!NEM_err_ok(err)) {
		if (NEM_rootd_verbose()) {
			printf(
				"c-routerd: couldn't start child: %s\n",
				NEM_err_string(err)
			);
		}

		NEM_app_after(app, 1000, NEM_thunk1_new_ptr(
			&routerd_restart,
			app
		));
	}
}

static void
on_child_died(NEM_thunk1_t *thunk, void *varg)
{
	NEM_rootd_txnmgr_free(&txnmgr);
	if (is_running) {
		// NB: Dirty hack to detect that we're shutting down and that
		// this was called due to the child being freed.
		NEM_child_free(&child);
	}
	is_running = false;

	if (!want_running) {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: child died? good riddance\n");
		}
		return;
	}

	if (NEM_rootd_verbose()) {
		printf("c-routerd: child died? restarting in 10ms\n");
	}

	NEM_app_t *app = NEM_thunk1_ptr(thunk);
	NEM_app_after(app, 10, NEM_thunk1_new_ptr(
		&routerd_restart,
		app
	));
}

static void
routerd_dispatch(NEM_thunk_t *thunk, void *varg)
{
	NEM_chan_ca *ca = varg;
	bool handled = NEM_rootd_svcmgr_dispatch(&svcmgr, ca->msg, ca->chan);

	if (!handled) {
		if (NEM_rootd_verbose()) {
			printf(
				"c-routerd: unhandled seq=%lu,"
				" service=%s (%hu), command=%s (%hu)\n",
				ca->msg->packed.seq,
				NEM_svcid_to_string(ca->msg->packed.service_id),
				ca->msg->packed.service_id,
				NEM_cmdid_to_string(
					ca->msg->packed.service_id,
					ca->msg->packed.command_id
				),
				ca->msg->packed.command_id
			);
		}

		// XXX: Send an error reply here. We're not equipped to send errors
		// currently since the headers and stuff aren't set up yet.
	}
}

static NEM_err_t
routerd_start(NEM_app_t *app)
{
	if (is_running) {
		return NEM_err_static("routerd_start: already running?");
	}

	NEM_err_t err = NEM_child_init(
		&child,
		app->kq,
		NEM_rootd_routerd_path(),
		// XXX: May want to bind stdout/stderr to something
		// so we can track/log output. Just leave them the same
		// as the parent stdout/stderr for now.
		NULL
	);
	if (!NEM_err_ok(err)) {
		return err;
	}
	err = NEM_child_on_close(
		&child,
		NEM_thunk1_new_ptr(
			&on_child_died,
			app
		)
	);
	if (!NEM_err_ok(err)) {
		NEM_child_free(&child);
		return err;
	}

	NEM_rootd_txnmgr_init(&txnmgr, &child.chan, NEM_thunk_new_ptr(
		&routerd_dispatch,
		NULL
	));

	if (NEM_rootd_verbose()) {
		printf("c-routerd: routerd running, pid=%d\n", child.pid); 
	}

	is_running = true;
	return NEM_err_none;
}

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-routerd: setup\n");
	}

	NEM_rootd_svcmgr_init(&svcmgr);
	NEM_rootd_svcmgr_set_next(&svcmgr, &NEM_rootd_svc_daemon);

	return routerd_start(app);
}

static void
on_shutdown_msg(NEM_thunk1_t *thunk, void *varg)
{
	NEM_rootd_txn_ca *ca = varg;
	if (!NEM_err_ok(ca->err)) {
		if (NEM_rootd_verbose()) {
			printf(
				"c-routerd: error sending shutdown message: %s\n",
				NEM_err_string(ca->err)
			);
		}
	}
	else {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: client shutdown acknowledged\n");
		}
	}
}

static bool
try_shutdown()
{
	printf("c-routerd: try-shutdown\n");
	want_running = false;

	if (is_running && !shutdown_sent) {
		shutdown_sent = true;

		NEM_msg_t *msg = NEM_msg_new(0, 0);
		msg->packed.service_id = NEM_svcid_daemon;
		msg->packed.command_id = NEM_cmdid_daemon_stop;
		NEM_rootd_txnmgr_req1(
			&txnmgr,
			msg,
			NEM_thunk1_new(&on_shutdown_msg, 0)
		);
	}

	return !is_running;
}

static void
teardown()
{
	printf("c-routerd: teardown\n");
	if (is_running) {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: killing child\n");
		}
		// NB: Set is_running first to signal that the child shouldn't
		// be freed by the on_child_died.
		is_running = false;
		NEM_child_free(&child);
	}

	NEM_rootd_svcmgr_free(&svcmgr);
}

const NEM_rootd_comp_t NEM_rootd_c_routerd = {
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};

