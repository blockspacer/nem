#include "test.h"

typedef struct {
	NEM_app_t app;
	NEM_fd_t fd_1, fd_2;
	NEM_txnmgr_t t_1, t_2;
}
work_t;

static void
work_stop_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_app_stop(&work->app);
	ck_assert_msg(false, "too long");
}

static void
work_init(work_t *work)
{
	bzero(work, sizeof(*work));
	ck_err(NEM_app_init_root(&work->app));

	ck_err(NEM_fd_init_unix(&work->fd_1, &work->fd_2, work->app.kq));

	NEM_txnmgr_init(&work->t_1, NEM_fd_as_stream(&work->fd_1));
	NEM_txnmgr_init(&work->t_2, NEM_fd_as_stream(&work->fd_2));

	NEM_app_after(&work->app, 3000, NEM_thunk1_new_ptr(
		&work_stop_cb,
		work
	));
}

static void
work_free(work_t *work)
{
	NEM_txnmgr_free(&work->t_1);
	NEM_txnmgr_free(&work->t_2);
	NEM_fd_free(&work->fd_1);
	NEM_fd_free(&work->fd_2);
	NEM_app_free(&work->app);
}

START_TEST(scaffolding)
{
	work_t work;
	work_init(&work);
	work_free(&work);
}
END_TEST

Suite*
suite_txnmgr()
{
	tcase_t tests[] = {
		{ "scaffolding", &scaffolding },
	};

	return tcase_build_suite("txnmgr", tests, sizeof(tests));
}
