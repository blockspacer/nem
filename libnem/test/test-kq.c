#include "test.h"

typedef struct {
	NEM_kq_t kq;
	int ctr;
}
work_t;

static void
work_init(work_t *work)
{
	bzero(work, sizeof(*work));
	NEM_kq_init_root(&work->kq);
}

static void
work_free(work_t *work)
{
	NEM_kq_free(&work->kq);
}

static void
work_stop(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_kq_stop(&work->kq);
}

START_TEST(initroot_free)
{
	work_t work;
	work_init(&work);
	work_free(&work);
}
END_TEST

START_TEST(run_stop_free)
{
	work_t work;
	work_init(&work);

	NEM_kq_defer(&work.kq, NEM_thunk1_new_ptr(
		&work_stop,
		&work
	));
	ck_err(NEM_kq_run(&work.kq));

	work_free(&work);
}
END_TEST

START_TEST(stop_run_free)
{
	work_t work;
	work_init(&work);

	NEM_kq_stop(&work.kq);
	ck_err(NEM_kq_run(&work.kq));

	work_free(&work);
}
END_TEST

static void
defer_parallel_cb(NEM_thunk1_t *thunk, void *vargs)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	work->ctr += 1;

	if (work->ctr == 5) {
		NEM_kq_stop(&work->kq);
	}
}

START_TEST(defer_parallel)
{
	work_t work;
	work_init(&work);

	for (size_t i = 0; i < 5; i += 1) {
		NEM_kq_defer(&work.kq, NEM_thunk1_new_ptr(
			&defer_parallel_cb,
			&work
		));
	}

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(work.ctr, 5);

	work_free(&work);
}
END_TEST

static void
defer_chain_cb(NEM_thunk1_t *thunk, void *vargs)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	work->ctr += 1;

	if (work->ctr == 10) {
		NEM_kq_stop(&work->kq);
	}
	else {
		NEM_kq_defer(&work->kq, NEM_thunk1_new_ptr(
			&defer_chain_cb,
			work
		));
	}
}

START_TEST(defer_chain)
{
	work_t work;
	work_init(&work);

	NEM_kq_defer(&work.kq, NEM_thunk1_new_ptr(
		&defer_chain_cb,
		&work
	));

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(work.ctr, 10);

	work_free(&work);
}
END_TEST

static void
defer_timer_timer_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	ck_assert_int_eq(1, work->ctr);
	work->ctr += 1;
	NEM_kq_stop(&work->kq);
}

static void
defer_timer_defer_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	ck_assert_int_eq(0, work->ctr);
	work->ctr += 1;
}

START_TEST(defer_timer)
{
	work_t work;
	work_init(&work);

	NEM_kq_after(&work.kq, 10, NEM_thunk1_new_ptr(
		&defer_timer_timer_cb,
		&work
	));

	NEM_kq_defer(&work.kq, NEM_thunk1_new_ptr(
		&defer_timer_defer_cb,
		&work
	));

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(work.ctr, 2);

	work_free(&work);
}
END_TEST

typedef struct {
	work_t *work;
	int n;
	bool stop;
}
timer_ordering_t;

static void
timer_ordering_n(NEM_thunk1_t *thunk, void *varg)
{
	timer_ordering_t *ord = NEM_thunk1_ptr(thunk);
	ck_assert_int_eq(ord->work->ctr, ord->n);
	ord->work->ctr += 1;

	if (ord->stop) {
		NEM_kq_stop(&ord->work->kq);
	}
}

START_TEST(timer_ordering)
{
	work_t work;
	work_init(&work);

	timer_ordering_t ord10 = {
		.work = &work,
		.n    = 0,
	};

	NEM_kq_after(&work.kq, 10, NEM_thunk1_new_ptr(
		&timer_ordering_n,
		&ord10
	));

	timer_ordering_t ord30 = {
		.work = &work,
		.n    = 2,
	};

	NEM_kq_after(&work.kq, 40, NEM_thunk1_new_ptr(
		&timer_ordering_n,
		&ord30
	));

	timer_ordering_t ord20 = {
		.work = &work,
		.n    = 1,
		.stop = true,
	};

	NEM_kq_after(&work.kq, 20, NEM_thunk1_new_ptr(
		&timer_ordering_n,
		&ord20
	));

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(work.ctr, 2);

	work_free(&work);
}
END_TEST

START_TEST(timer_init_free)
{
	work_t work;
	work_init(&work);
	NEM_timer_t timer;
	NEM_timer_init(&timer, &work.kq, NEM_thunk_new(NULL, 0));
	NEM_timer_free(&timer);
	work_free(&work);
}
END_TEST

START_TEST(timer_init_set_free)
{
	work_t work;
	work_init(&work);
	NEM_timer_t timer;
	NEM_timer_init(&timer, &work.kq, NEM_thunk_new(NULL, 0));
	NEM_timer_set(&timer, 100);
	NEM_timer_free(&timer);
	work_free(&work);
}
END_TEST

static void
timer_set_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	work->ctr += 1;
	NEM_kq_stop(&work->kq);
}

START_TEST(timer_set)
{
	work_t work;
	work_init(&work);
	NEM_timer_t timer;
	NEM_timer_init(&timer, &work.kq, NEM_thunk_new_ptr(
		&timer_set_cb,
		&work
	));
	NEM_timer_set(&timer, 10);
	NEM_timer_set(&timer, 10);
	NEM_timer_set(&timer, 10);
	ck_err(NEM_kq_run(&work.kq));
	NEM_timer_free(&timer);
	work_free(&work);

	ck_assert_int_eq(work.ctr, 1);
}
END_TEST

static void
timer_reset_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	work->ctr += 1;
	NEM_timer_t *timer = varg;

	if (work->ctr == 1) {
		NEM_timer_set(timer, 10);
	}
	else {
		NEM_kq_stop(&work->kq);
	}
}

START_TEST(timer_reset)
{
	work_t work;
	work_init(&work);
	NEM_timer_t timer;
	NEM_timer_init(&timer, &work.kq, NEM_thunk_new_ptr(
		&timer_reset_cb,
		&work
	));
	NEM_timer_set(&timer, 10);
	NEM_timer_set(&timer, 10);
	ck_err(NEM_kq_run(&work.kq));
	NEM_timer_free(&timer);
	work_free(&work);

	ck_assert_int_eq(work.ctr, 2);
}
END_TEST

static void
timer_cancel_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_kq_stop(&work->kq);
}

START_TEST(timer_cancel)
{
	work_t work;
	work_init(&work);
	NEM_kq_defer(&work.kq, NEM_thunk1_new_ptr(&timer_cancel_cb, &work));
	NEM_timer_t timer;
	NEM_timer_init(&timer, &work.kq, NEM_thunk_new(NULL, 0));
	NEM_timer_cancel(&timer);
	NEM_timer_set(&timer, 10);
	NEM_timer_cancel(&timer);
	ck_err(NEM_kq_run(&work.kq));
	NEM_timer_free(&timer);
	work_free(&work);

	ck_assert_int_eq(work.ctr, 0);
}
END_TEST

Suite*
suite_kq()
{
	tcase_t tests[] = {
		{ "initroot_free",       &initroot_free       },
		{ "run_stop_free",       &run_stop_free       },
		{ "stop_run_free",       &stop_run_free       },
		{ "defer_parallel",      &defer_parallel      },
		{ "defer_chain",         &defer_chain         },
		{ "defer_timer",         &defer_timer         },
		{ "timer_ordering",      &timer_ordering      },
		{ "timer_init_free",     &timer_init_free     },
		{ "timer_init_set_free", &timer_init_set_free },
		{ "timer_set",           &timer_set           },
		{ "timer_reset",         &timer_reset         },
		{ "timer_cancel",        &timer_cancel        },
	};

	return tcase_build_suite("kq", tests, sizeof(tests));
}
