#pragma once

// NEM_APP_FILENO is the fileno that's used to pass a control fd from a
// parent to child process. It's automatically acquired by NEM_app_init
// (use NEM_app_init_root to avoid this behavior).
static const int NEM_APP_FILENO = 4;

// NEM_timer_t is an internal struct for timer recordkeeping.
typedef struct NEM_timer_t {
	SPLAY_ENTRY(NEM_timer_t) link;
	struct timeval invoke_at;
	NEM_thunk1_t *thunk;
}
NEM_timer_t;

typedef SPLAY_HEAD(NEM_timer_tree_t, NEM_timer_t) NEM_timer_tree_t;

// NEM_app_t provides a wrapper around a kq and does initialization for
// parent-child message passing. It also provides the basic runloop.
typedef struct {
	int  kq;
	bool running;

	NEM_chan_t *chan;

	NEM_thunk_t     *on_timer;
	NEM_timer_tree_t timers;
}
NEM_app_t;

// NEM_app_init initializes the app and hooks up to the parent process via
// NEM_APP_FILENO. NEM_app_init_root does the same thing sans hookup.
NEM_err_t NEM_app_init(NEM_app_t *this);
NEM_err_t NEM_app_init_root(NEM_app_t *this);

// NEM_app_free frees the resources used by app. Notably, this kills the
// underlying kq but does not actually notify any waiting fds that the kq
// died. Everything else needs to be freed manually.
void NEM_app_free(NEM_app_t *this);

// NEM_app_after schedules a callback to run at a time in the future. defer is
// shorthand for "after 0 ms", which triggers on the next iteration of the
// event loop.
void NEM_app_after(NEM_app_t *this, uint64_t ms, NEM_thunk1_t *cb);
void NEM_app_defer(NEM_app_t *this, NEM_thunk1_t *cb);

// NEM_app_run runs the eventloop and does not return until NEM_app_stop is
// called or the heatdeath of the universe.
NEM_err_t NEM_app_run(NEM_app_t *this);

// NEM_app_stop signals NEM_app_run to stop running.
void NEM_app_stop(NEM_app_t *this);

