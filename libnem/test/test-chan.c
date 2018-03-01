#include "test.h"

typedef struct {
	NEM_app_t app;
	NEM_chan_t c_1, c_2;
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
work_close_fd(NEM_thunk1_t *thunk, void *varg)
{
	NEM_fd_t *fd = NEM_thunk1_ptr(thunk);
	NEM_fd_free(fd);
	free(fd);
}

static void
work_init(work_t *work)
{
	bzero(work, sizeof(*work));
	ck_err(NEM_app_init_root(&work->app));

	NEM_fd_t *fd1 = NEM_malloc(sizeof(NEM_fd_t));
	NEM_fd_t *fd2 = NEM_malloc(sizeof(NEM_fd_t));
	ck_err(NEM_fd_init_pipe(fd1, fd2, work->app.kq));

	NEM_fd_on_close(fd1, NEM_thunk1_new_ptr(
		&work_close_fd,
		fd1
	));
	NEM_fd_on_close(fd2, NEM_thunk1_new_ptr(
		&work_close_fd,
		fd2
	));

	NEM_chan_init(&work->c_1, NEM_fd_as_stream(fd1));
	NEM_chan_init(&work->c_2, NEM_fd_as_stream(fd2));

	NEM_app_after(&work->app, 3000, NEM_thunk1_new_ptr(
		&work_stop_cb,
		work
	));
}

static void
work_free(work_t *work)
{
	NEM_chan_free(&work->c_1);
	NEM_chan_free(&work->c_2);
	NEM_app_free(&work->app);
}

START_TEST(init_free)
{
	work_t work;
	work_init(&work);
	work_free(&work);
}
END_TEST

Suite*
suite_chan()
{
	tcase_t tests[] = {
		{ "init_free", &init_free },
	};

	return tcase_build_suite("chan", tests, sizeof(tests));
}
