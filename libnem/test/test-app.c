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

Suite*
suite_app()
{
	tcase_t tests[] = {
		{ "initroot_free",  &initroot_free  },
		{ "run_stop_free",  &run_stop_free  },
		{ "stop_run_free",  &stop_run_free  },
		{ "defer_parallel", &defer_parallel },
		{ "defer_chain",    &defer_chain    },
	};

	return tcase_build_suite("app", tests, sizeof(tests));
}
