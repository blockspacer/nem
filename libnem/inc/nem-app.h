#pragma once

typedef struct NEM_timer_t {
	SPLAY_ENTRY(NEM_timer_t) link;
	struct timeval invoke_at;
	NEM_thunk1_t *thunk;
}
NEM_timer_t;

typedef SPLAY_HEAD(NEM_timer_tree_t, NEM_timer_t) NEM_timer_tree_t;

typedef struct {
	int  kq;
	bool running;

	NEM_thunk_t     *on_timer;
	NEM_timer_tree_t timers;
}
NEM_app_t;

NEM_err_t NEM_app_init(NEM_app_t *this);
NEM_err_t NEM_app_init_root(NEM_app_t *this);
void NEM_app_free(NEM_app_t *this);

void NEM_app_after(NEM_app_t *this, uint64_t ms, NEM_thunk1_t *cb);
void NEM_app_defer(NEM_app_t *this, NEM_thunk1_t *cb);

NEM_err_t NEM_app_run(NEM_app_t *this);
void NEM_app_stop(NEM_app_t *this);

