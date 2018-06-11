#include "nem.h"
#include "nemsvc.h"
#include "c-args.h"

static void
svc_daemon_info(NEM_thunk_t *thunk, void *varg)
{
	if (NEM_rootd_verbose()) {
		printf("svc-daemon: info requested\n");
	}

	NEM_txn_ca *ca = varg;
	NEM_msg_t *msg = NEM_msg_new_reply(ca->msg, 0, 6);
	snprintf(msg->body, 6, "hello");
	NEM_txnin_reply(ca->txnin, msg);
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

void
NEM_rootd_svc_daemon_bind(NEM_svcmux_t *mux)
{
	NEM_svcmux_entry_t entries[] = {
		{
			NEM_svcid_daemon,
			NEM_cmdid_daemon_info,
			NEM_thunk_new(&svc_daemon_info, 0),
		},
		{
			NEM_svcid_daemon,
			NEM_cmdid_daemon_getcfg,
			NEM_thunk_new(&svc_daemon_getcfg, 0),
		},
		{
			NEM_svcid_daemon,
			NEM_cmdid_daemon_setcfg,
			NEM_thunk_new(&svc_daemon_setcfg, 0),
		},
		{
			NEM_svcid_daemon,
			NEM_cmdid_daemon_stop,
			NEM_thunk_new(&svc_daemon_stop, 0),
		},
	};

	NEM_svcmux_add_handlers(mux, entries, NEM_ARRSIZE(entries));
}
