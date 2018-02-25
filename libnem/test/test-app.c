#include "test.h"

typedef struct {
	NEM_app_t app;
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

Suite*
suite_app()
{
	tcase_t tests[] = {
		{ "initroot_free", &initroot_free },
		{ "run_stop_free", &run_stop_free },
		{ "stop_run_free", &stop_run_free },
	};

	return tcase_build_suite("app", tests, sizeof(tests));
}
