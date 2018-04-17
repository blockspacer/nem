#pragma once

// NEM_APP_FILENO is the fileno that's used to pass a control fd from a
// parent to child process. It's automatically acquired by NEM_app_init
// (use NEM_app_init_root to avoid this behavior).
static const int NEM_APP_FILENO = 4;

// NEM_timer1_t is an internal struct for timer recordkeeping.
typedef struct NEM_timer1_t {
	SPLAY_ENTRY(NEM_timer1_t) link;
	struct timeval invoke_at;
	NEM_thunk1_t *thunk;
}
NEM_timer1_t;

typedef SPLAY_HEAD(NEM_timer1_tree_t, NEM_timer1_t) NEM_timer1_tree_t;

// NEM_app_t provides a wrapper around a kq and does initialization for
// parent-child message passing. It also provides the basic runloop.
typedef struct {
	int  kq;
	bool running;

	NEM_chan_t *chan;

	NEM_thunk_t      *on_timer;
	NEM_timer1_tree_t timers;
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

// NEM_timer_t is a struct for cancellable/recurring timers. These are
// built on NEM_timer1_t (which are for NEM_app_defer/after) and are a bit
// more heavyweight since bad design.
typedef struct {
	NEM_app_t    *app;
	NEM_timer1_t *active;
	NEM_thunk_t  *thunk;
}
NEM_timer_t;

// NEM_timer_init initializes a new timer. The timer must be free'd before
// the app is otherwise it will leak. The thunk is owned by and freed with
// the timer.
void NEM_timer_init(NEM_timer_t *this, NEM_app_t *app, NEM_thunk_t *thunk);

// NEM_timer_set updates the next firing time of the timer. If the timer is
// already set to fire, the time is adjusted. If it isn't, it's scheduled.
void NEM_timer_set(NEM_timer_t *this, uint64_t ms_after);
void NEM_timer_set_abs(NEM_timer_t *this, struct timeval t);

// NEM_timer_cancel cancels the next execution of the timer. If the timer
// isn't set, this is a no-op.
void NEM_timer_cancel(NEM_timer_t *this);

// NEM_timer_free frees the underlying timer. This must be called before
// NEM_app_free.
void NEM_timer_free(NEM_timer_t *this);

