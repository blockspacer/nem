#include "test.h"

typedef struct {
	NEM_app_t app;
	int ctr1, ctr2;
}
work_t;

static void
work_init(work_t *work)
{
	bzero(work, sizeof(*work));
	NEM_app_init_root(&work->app);
	work->app.data = work;
}

static void
shutdown_comp_setup_cb(NEM_thunk1_t *thunk, void *varg)
{
	NEM_app_t *app = NEM_thunk1_ptr(thunk);
	NEM_app_shutdown(app);
}

NEM_err_t
shutdown_comp_setup(NEM_app_t *app, int argc, char *argv[])
{
	NEM_kq_defer(&app->kq, NEM_thunk1_new_ptr(&shutdown_comp_setup_cb, app));
	return NEM_err_none;
}

static NEM_app_comp_t shutdown_comp = {
	.name         = "shutdown-comp",
	.setup        = &shutdown_comp_setup,
	.try_shutdown = NULL,
	.teardown     = NULL,
};

START_TEST(init_shutdown)
{
	work_t work;
	work_init(&work);
	NEM_app_add_comp(&work.app, &shutdown_comp);
	ck_err(NEM_app_main(&work.app, 0, NULL));
}
END_TEST

static NEM_err_t
setup_teardown_order_setup1(NEM_app_t *app, int argc, char *argv[])
{
	work_t *work = app->data;
	work->ctr1 += 1;
	ck_assert_int_eq(work->ctr1, 1); // first to setup
	return NEM_err_none;
}

static void
setup_teardown_order_teardown1(NEM_app_t *app)
{
	work_t *work = app->data;
	work->ctr2 += 1;
	ck_assert_int_eq(work->ctr2, 2); // second to teardown
}

static NEM_app_comp_t setup_teardown_order_comp1 = {
	.name     = "setup-teardown-order-comp1",
	.setup    = &setup_teardown_order_setup1,
	.teardown = &setup_teardown_order_teardown1,
};

static NEM_err_t
setup_teardown_order_setup2(NEM_app_t *app, int argc, char *argv[])
{
	work_t *work = app->data;
	work->ctr1 += 1;
	ck_assert_int_eq(work->ctr1, 2); // second to setup
	return NEM_err_none;
}

static void
setup_teardown_order_teardown2(NEM_app_t *app)
{
	work_t *work = app->data;
	work->ctr2 += 1;
	ck_assert_int_eq(work->ctr2, 1); // first to teardown
}

static NEM_app_comp_t setup_teardown_order_comp2 = {
	.name     = "setup-teardown-order-comp2",
	.setup    = &setup_teardown_order_setup2,
	.teardown = &setup_teardown_order_teardown2,
};

START_TEST(setup_teardown_order)
{
	work_t work; 
	work_init(&work);
	const NEM_app_comp_t *comps[] = {
		&setup_teardown_order_comp1,
		&setup_teardown_order_comp2,
		&shutdown_comp,
	};
	NEM_app_add_comps(&work.app, comps, NEM_ARRSIZE(comps));
	ck_err(NEM_app_main(&work.app, 0, NULL));
	ck_assert_int_eq(2, work.ctr1);
	ck_assert_int_eq(2, work.ctr2);
}
END_TEST

Suite*
suite_app()
{
	tcase_t tests[] = {
		{ "init_shutdown",        &init_shutdown        },
		{ "setup_teardown_order", &setup_teardown_order },
	};

	return tcase_build_suite("app", tests, sizeof(tests));
}
