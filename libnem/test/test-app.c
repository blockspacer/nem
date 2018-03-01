#include "test.h"

typedef struct {
	NEM_app_t app;
	int ctr;
}
work_t;

static void
work_init(work_t *work)
{
	bzero(work, sizeof(*work));
	NEM_app_init_root(&work->app);
}

static void
work_free(work_t *work)
{
	NEM_app_free(&work->app);
}

static void
work_stop(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_app_stop(&work->app);
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

	NEM_app_defer(&work.app, NEM_thunk1_new_ptr(
		&work_stop,
		&work
	));
	ck_err(NEM_app_run(&work.app));

	work_free(&work);
}
END_TEST

START_TEST(stop_run_free)
{
	work_t work;
	work_init(&work);

	NEM_app_stop(&work.app);
	ck_err(NEM_app_run(&work.app));

	work_free(&work);
}
END_TEST

static void
defer_parallel_cb(NEM_thunk1_t *thunk, void *vargs)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	work->ctr += 1;

	if (work->ctr == 5) {
		NEM_app_stop(&work->app);
	}
}

START_TEST(defer_parallel)
{
	work_t work;
	work_init(&work);

	for (size_t i = 0; i < 5; i += 1) {
		NEM_app_defer(&work.app, NEM_thunk1_new_ptr(
			&defer_parallel_cb,
			&work
		));
	}

	ck_err(NEM_app_run(&work.app));
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
		NEM_app_stop(&work->app);
	}
	else {
		NEM_app_defer(&work->app, NEM_thunk1_new_ptr(
			&defer_chain_cb,
			work
		));
	}
}

START_TEST(defer_chain)
{
	work_t work;
	work_init(&work);

	NEM_app_defer(&work.app, NEM_thunk1_new_ptr(
		&defer_chain_cb,
		&work
	));

	ck_err(NEM_app_run(&work.app));
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
	NEM_app_stop(&work->app);
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

	NEM_app_after(&work.app, 10, NEM_thunk1_new_ptr(
		&defer_timer_timer_cb,
		&work
	));

	NEM_app_defer(&work.app, NEM_thunk1_new_ptr(
		&defer_timer_defer_cb,
		&work
	));

	ck_err(NEM_app_run(&work.app));
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
		NEM_app_stop(&ord->work->app);
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

	NEM_app_after(&work.app, 10, NEM_thunk1_new_ptr(
		&timer_ordering_n,
		&ord10
	));

	timer_ordering_t ord30 = {
		.work = &work,
		.n    = 2,
	};

	NEM_app_after(&work.app, 30, NEM_thunk1_new_ptr(
		&timer_ordering_n,
		&ord30
	));

	timer_ordering_t ord20 = {
		.work = &work,
		.n    = 1,
		.stop = true,
	};

	NEM_app_after(&work.app, 20, NEM_thunk1_new_ptr(
		&timer_ordering_n,
		&ord20
	));

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(work.ctr, 2);

	work_free(&work);
}
END_TEST

Suite*
suite_app()
{
	tcase_t tests[] = {
		{ "initroot_free",  &initroot_free  },
		{ "run_stop_free",  &run_stop_free  },
		{ "stop_run_free",  &stop_run_free  },
		{ "defer_parallel", &defer_parallel },
		{ "defer_chain",    &defer_chain    },
		{ "defer_timer",    &defer_timer    },
		{ "timer_ordering", &timer_ordering },
	};

	return tcase_build_suite("app", tests, sizeof(tests));
}
