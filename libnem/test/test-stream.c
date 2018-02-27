#include "test.h"

typedef struct {
	NEM_app_t app;
	NEM_fd_t  fd_1;
	NEM_fd_t  fd_2;
	bool      fds_freed[2];
}
work_t;

typedef NEM_err_t(*fd_init_fn)(NEM_fd_t*, NEM_fd_t*, int);

static void
work_stop_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_app_stop(&work->app);
	ck_assert_msg(false, "timed out");
}

static void
work_init(work_t *work, fd_init_fn fn)
{
	bzero(work, sizeof(*work));
	ck_err(NEM_app_init_root(&work->app));
	ck_err(fn(&work->fd_1, &work->fd_2, work->app.kq));

	NEM_app_after(&work->app, 10, NEM_thunk1_new_ptr(
		&work_stop_cb,
		work
	));
}

static void
work_free(work_t *work)
{
	if (!work->fds_freed[0]) {
		NEM_fd_free(&work->fd_1);
	}
	if (!work->fds_freed[1]) {
		NEM_fd_free(&work->fd_2);
	}
	NEM_app_free(&work->app);
}

static void
init_free(fd_init_fn fn)
{
	work_t work;
	work_init(&work, fn);
	work_free(&work);
}

START_TEST(pipe_init_free) { init_free(&NEM_fd_init_pipe); } END_TEST
START_TEST(unix_init_free) { init_free(&NEM_fd_init_unix); } END_TEST

Suite*
suite_stream()
{
	tcase_t tests[] = {
		{ "pipe_init_free", &pipe_init_free },
		{ "unix_init_free", &unix_init_free },
	};

	return tcase_build_suite("stream", tests, sizeof(tests));
}
