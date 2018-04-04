#include "nem.h"
#include "svcdef.h"
#include "lifecycle.h"
#include "state.h"

NEM_rootd_svcdef_t NEM_rootd_svc_daemon = {};
char *tmp_dir = NULL;

static void
svc_daemon_info(NEM_thunk_t *thunk, void *varg)
{
	if (NEM_rootd_verbose()) {
		printf("svc-daemon: info requested\n");
	}

	NEM_rootd_cmd_ca *ca = varg;

	NEM_msg_t *msg = NEM_msg_new_reply(ca->msg, 0, 6);
	snprintf(msg->body, 6, "hello");
	NEM_chan_send(ca->chan, msg, NULL);
}

static void
svc_daemon_getcfg(NEM_thunk_t *thunk, void *varg)
{
	if (NEM_rootd_verbose()) {
		printf("svc-daemon: getcfg requested\n");
	}
}

static void
svc_daemon_setcfg(NEM_thunk_t *thunk, void *varg)
{
	if (NEM_rootd_verbose()) {
		printf("svc-daemon: setcfg requested\n");
	}
}

static void
svc_daemon_stop(NEM_thunk_t *thunk, void *varg)
{
	if (NEM_rootd_verbose()) {
		printf("svc-daemon: stop requested\n");
	}
}

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("svc-daemon: setup\n");
	}

	static const struct {
		uint16_t cmd_id;
		void(*fn)(NEM_thunk_t*, void*);
	}
	handlers[] = {
		{ NEM_cmdid_daemon_info,   &svc_daemon_info   },
		{ NEM_cmdid_daemon_getcfg, &svc_daemon_getcfg },
		{ NEM_cmdid_daemon_setcfg, &svc_daemon_setcfg },
		{ NEM_cmdid_daemon_stop,   &svc_daemon_stop   },
	};

	NEM_rootd_svcdef_init(&NEM_rootd_svc_daemon);

	for (size_t i = 0; i < NEM_ARRSIZE(handlers); i += 1) {
		NEM_err_t err = NEM_rootd_svcdef_add(
			&NEM_rootd_svc_daemon,
			NEM_svcid_daemon,
			handlers[i].cmd_id,
			NEM_thunk_new(handlers[i].fn, 0)
		);
		if (!NEM_err_ok(err)) {
			return err;
		}
	}

	return NEM_err_none;
}

static void
teardown()
{
	if (NEM_rootd_verbose()) {
		printf("svc-daemon: teardown\n");
	}

	NEM_rootd_svcdef_free(&NEM_rootd_svc_daemon);
}

const NEM_rootd_comp_t NEM_rootd_c_svc_daemon = {
	.name     = "svc-daemon",
	.setup    = &setup,
	.teardown = &teardown,
};

