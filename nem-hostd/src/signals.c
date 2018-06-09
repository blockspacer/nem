#include "nem.h"

#include <signal.h>

static NEM_thunk_t *kevent_thunk = NULL;

static const char*
sig_to_string(int sig)
{
	static const struct {
		int         sig;
		const char *str;
	}
	table[] = {
		{ SIGINT,  "SIGINT"  },
		{ SIGQUIT, "SIGQUIT" },
		{ SIGPIPE, "SIGPIPE" },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(table); i += 1) {
		if (table[i].sig == sig) {
			return table[i].str;
		}
	}

	return "(unknown signal)";
}

static void
on_kevent(NEM_thunk_t *thunk, void *varg)
{
	struct kevent *kev = varg;

	if (kev->ident == SIGPIPE) {
		printf("got SIGPIPE?\n");
		return;
	}

	printf(
		"got %s; shutting down\n",
		sig_to_string((int)kev->ident)
	);

	NEM_app_t *app = NEM_thunk_ptr(thunk);
	NEM_app_shutdown(app);
}

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	int sigs[] = {
		SIGINT,
		SIGQUIT,
		SIGPIPE,
	};

	struct kevent kev[NEM_ARRSIZE(sigs)];
	kevent_thunk = NEM_thunk_new_ptr(&on_kevent, app);

	for (size_t i = 0; i < NEM_ARRSIZE(sigs); i += 1) {
		signal(sigs[i], SIG_IGN);
		EV_SET(
			&kev[i],
			sigs[i],
			EVFILT_SIGNAL,
			EV_ADD | EV_CLEAR,
			0,
			0,
			kevent_thunk
		);
	}
	
	if (0 != kevent(app->kq.kq, kev, NEM_ARRSIZE(sigs), NULL, 0, NULL)) {
		return NEM_err_errno();
	}

	return NEM_err_none;
}

static void
teardown(NEM_app_t *app)
{
	if (NULL != kevent_thunk) {
		NEM_thunk_free(kevent_thunk);
	}
}

const NEM_app_comp_t NEM_hostd_c_signals = {
	.name     = "signals",
	.setup    = &setup,
	.teardown = &teardown,
};
