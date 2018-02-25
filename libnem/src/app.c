#include "nem.h"

#include <sys/time.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>

static inline int
NEM_timer_cmp(const void *vlhs, const void *vrhs)
{
	const NEM_timer_t *lhs = vlhs;
	const NEM_timer_t *rhs = vrhs;

	if (lhs->invoke_at.tv_sec < rhs->invoke_at.tv_sec) {
		return -1;
	}
	else if (lhs->invoke_at.tv_sec > rhs->invoke_at.tv_sec) {
		return 1;
	}
	else if (lhs->invoke_at.tv_usec < rhs->invoke_at.tv_usec) {
		return -1;
	}
	else if (lhs->invoke_at.tv_usec > rhs->invoke_at.tv_usec) {
		return 1;
	}

	return 0;
}

static inline bool
NEM_timer_before(NEM_timer_t *this, struct timeval t)
{
	if (this->invoke_at.tv_sec > t.tv_sec) {
		return false;
	}
	else if (this->invoke_at.tv_sec < t.tv_sec) {
		return true;
	}
	else {
		return this->invoke_at.tv_sec < t.tv_sec;
	}
}

// XXX: Use SPLAY_PROTOTYPE_STATIC/SPLAY_GENERATE_STATIC instead here.
SPLAY_PROTOTYPE(NEM_timer_tree_t, NEM_timer_t, link, NEM_timer_cmp);
SPLAY_GENERATE(NEM_timer_tree_t, NEM_timer_t, link, NEM_timer_cmp);

static NEM_err_t
NEM_app_timer_schedule(NEM_app_t *this, struct timeval now, NEM_timer_t *next)
{
	struct kevent ev;

	if (NULL == next) {
		// Clear timer.
		EV_SET(&ev, 1, EVFILT_TIMER, EV_CLEAR, 0, 0, NULL);
		if (0 != kevent(this->kq, &ev, 1, NULL, 0, NULL)) {
			return NEM_err_errno();
		}

		return NEM_err_none;
	}

	// Otherwise set the timer.
	intptr_t after = 0;
	after += 1000 * (next->invoke_at.tv_sec - now.tv_sec);
	after += (next->invoke_at.tv_usec - now.tv_usec) / 1000;

	if (after < 1) {
		after = 1;
	}

	EV_SET(&ev, 1, EVFILT_TIMER, EV_ADD|EV_ONESHOT, 0, after, this->on_timer);
	if (0 != kevent(this->kq, &ev, 1, NULL, 0, NULL)) {
		return NEM_err_errno();
	}

	return NEM_err_none;
}

static inline void
NEM_app_timer_now(struct timeval *t)
{
	if (gettimeofday(t, NULL)) {
		NEM_panicf("gettimeofday: %s", strerror(errno));
	}
}

static inline void
NEM_app_timer_add_ms(struct timeval *t, int ms)
{
	t->tv_sec += ms / 1000;
	t->tv_usec += (ms % 1000) * 1000;
}

static void
NEM_app_on_timer(NEM_thunk_t *thunk, void *varg)
{
	NEM_app_t *this = NEM_thunk_ptr(thunk);
	struct timeval now;
	NEM_app_timer_now(&now);

	for (;;) {
		NEM_timer_t *timer = SPLAY_MIN(NEM_timer_tree_t, &this->timers);
		if (NULL == timer || !NEM_timer_before(timer, now)) {
			break;
		}

		NEM_thunk1_invoke(&timer->thunk, NULL);
		SPLAY_REMOVE(NEM_timer_tree_t, &this->timers, timer);
		free(timer);
	}

	NEM_app_timer_schedule(
		this,
		now,
		SPLAY_MIN(NEM_timer_tree_t, &this->timers)
	);
}

static void
NEM_app_on_stop(NEM_thunk1_t *thunk, void *varg)
{
	NEM_app_t *this = NEM_thunk1_ptr(thunk);
	this->running = false;
}

static void
NEM_app_run_defers(NEM_app_t *this)
{
	// NB: While processing defers, there might be concurrent
	// writes. So save the current defer list before processing.
	NEM_thunk1_t **defers = this->defers;
	size_t defers_len = this->defers_len;
	size_t defers_cap = this->defers_cap;

	this->defers = NULL;
	this->defers_len = 0;
	this->defers_cap = 0;

	for (size_t i = 0; i < defers_len; i += 1) {
		NEM_thunk1_invoke(&defers[i], NULL);
	}

	// Then if there were no subsequent defers, save ourselves a malloc.
	if (0 == this->defers_len) {
		this->defers = defers;
		this->defers_cap = defers_cap;
	}
	else {
		free(defers);
	}
}

static NEM_err_t
NEM_app_init_internal(NEM_app_t *this)
{
	bzero(this, sizeof(*this));
	this->kq = kqueue();
	if (-1 == this->kq) {
		return NEM_err_errno();
	}

	this->on_timer = NEM_thunk_new_ptr(&NEM_app_on_timer, this);
	SPLAY_INIT(&this->timers);
	return NEM_err_none;
}

NEM_err_t
NEM_app_init(NEM_app_t *this)
{
	NEM_err_t err = NEM_app_init_internal(this);
	if (!NEM_err_ok(err)) {
		return err;
	}

	NEM_panic("TODO: NEM_app_init: initialize parent stream ?");
}

NEM_err_t
NEM_app_init_root(NEM_app_t *this)
{
	return NEM_app_init_internal(this);
}

void
NEM_app_free(NEM_app_t *this)
{
	if (this->running) {
		NEM_panic("NEM_app_free: app still running!");
	}

	for (size_t i = 0; i < this->defers_len; i += 1) {
		NEM_thunk1_discard(&this->defers[i]);
	}
	free(this->defers);

	NEM_timer_t *timer = NULL;
	while (NULL != (timer = SPLAY_MIN(NEM_timer_tree_t, &this->timers))) {
		SPLAY_REMOVE(NEM_timer_tree_t, &this->timers, timer);
		NEM_thunk1_discard(&timer->thunk);
		free(timer);
	}

	NEM_thunk_free(this->on_timer);
	if (0 != close(this->kq)) {
		NEM_panicf_errno("NEM_app_free: close(kq): %s");
	}

	this->kq = 0;
}

void
NEM_app_after(NEM_app_t *this, uint64_t ms, NEM_thunk1_t *cb)
{
	struct timeval now;
	NEM_app_timer_now(&now);

	NEM_timer_t *timer = NEM_malloc(sizeof(NEM_timer_t));
	timer->thunk = cb;
	timer->invoke_at = now;
	NEM_app_timer_add_ms(&timer->invoke_at, ms);

	SPLAY_INSERT(NEM_timer_tree_t, &this->timers, timer);
	NEM_timer_t *next_timer = SPLAY_MIN(NEM_timer_tree_t, &this->timers);

	if (timer == next_timer) {
		NEM_app_timer_schedule(this, now, next_timer);
	}
}

void
NEM_app_defer(NEM_app_t *this, NEM_thunk1_t *cb)
{
	if (this->defers_len == this->defers_cap) {
		if (0 == this->defers_cap) {
			this->defers_cap = 4;
		}
		else if (64 > this->defers_cap) {
			this->defers_cap *= 2;
		}
		else {
			this->defers_cap += 64;
		}

		// XXX: use reallocarray.
		this->defers = NEM_panic_if_null(
			realloc(this->defers, this->defers_cap * sizeof(NEM_thunk1_t))
		);
	}

	this->defers[this->defers_len] = cb;
	this->defers_len += 1;
}

NEM_err_t
NEM_app_run(NEM_app_t *this)
{
	if (0 == this->kq) {
		NEM_panic("NEM_app_run: app not initialized");
	}

	while (this->running) {
		struct kevent trig;
		if (-1 == kevent(this->kq, NULL, 0, &trig, 1, NULL)) {
			NEM_panic("NEM_app_run: kevent");
		}
		if (EV_ERROR == (trig.flags & EV_ERROR)) {
			// XXX: The thunks should process these errors and we shouldn't
			// be logging them.
			fprintf(stderr, "NEM_app_run: EV_ERROR: %s", strerror(trig.data));
			break;
		}

		NEM_thunk_t *thunk = trig.udata;
		if (NULL == trig.udata) {
			NEM_panicf("NEM_app_run: NULL udata filter=%d", trig.filter);
		}
		NEM_thunk_invoke(thunk, &trig);

		// NB: Process defers immediately since the next kevent invocation
		// will block until another event comes in.
		NEM_app_run_defers(this);
	}

	return NEM_err_none;
}

void
NEM_app_stop(NEM_app_t *this)
{
	// NB: Setting a timer procs the kqueue to deliver a message, breaking
	// it out of the wait loop.
	NEM_app_after(this, 0, NEM_thunk1_new_ptr(&NEM_app_on_stop, this));
}
