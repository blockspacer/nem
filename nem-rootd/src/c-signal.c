#include "nem.h"
#include "lifecycle.h"
#include "state.h"

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
		printf("c-signal: got SIGPIPE?\n");
		return;
	}

	if (NEM_rootd_verbose()) {
		printf(
			"c-signal: got %s; shutting down\n",
			sig_to_string((int)kev->ident)
		);
	}

	NEM_rootd_shutdown();
}

static NEM_err_t
setup(NEM_app_t *app)
{
	int sigs[] = {
		SIGINT,
		SIGQUIT,
		SIGPIPE,
	};

	if (NEM_rootd_verbose()) {
		printf("c-signal: startup\n");
	}

	struct kevent kev[NEM_ARRSIZE(sigs)];
	kevent_thunk = NEM_thunk_new(&on_kevent, 0);

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
	
	if (0 != kevent(app->kq, kev, NEM_ARRSIZE(sigs), NULL, 0, NULL)) {
		return NEM_err_errno();
	}

	return NEM_err_none;
}

static void
teardown()
{
	if (NEM_rootd_verbose()) {
		printf("c-signal: teardown\n");
	}

	if (NULL != kevent_thunk) {
		NEM_thunk_free(kevent_thunk);
	}
}

const NEM_rootd_comp_t NEM_rootd_c_signal = {
	.name     = "c-signal",
	.setup    = &setup,
	.teardown = &teardown,
};
