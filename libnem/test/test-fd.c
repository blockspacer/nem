#include "test.h"

typedef struct {
	int kq;
	int fds[2];
	char *buf;
	NEM_app_t app;
	bool fds_freed[2];
}
work_t;

static void
work_init(work_t *work)
{
	bzero(work, sizeof(*work));
	work->kq = kqueue();
	ck_assert_int_ne(-1, work->kq);
	ck_assert_int_eq(0, pipe2(work->fds, O_CLOEXEC));
}

static void
work_free(work_t *work)
{
	if (work->fds_freed[0]) {
		ck_assert_int_eq(-1, fcntl(work->fds[0], F_GETFD));
	}
	else {
		ck_assert_int_eq(0, close(work->fds[0]));
	}

	if (work->fds_freed[1]) {
		ck_assert_int_eq(-1, fcntl(work->fds[1], F_GETFD));
	}
	else {
		ck_assert_int_eq(0, close(work->fds[1]));
	}

	ck_assert_int_eq(0, close(work->kq));
}

START_TEST(init_free)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	ck_err(NEM_fd_init(&fd, work.kq, work.fds[0]));
	NEM_fd_free(&fd);

	work.fds_freed[0] = true;
	work_free(&work);
}
END_TEST

START_TEST(init_close_free)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	ck_err(NEM_fd_init(&fd, work.kq, work.fds[0]));
	NEM_fd_close(&fd);

	work.fds_freed[0] = true;
	work_free(&work);

	NEM_fd_free(&fd);
}
END_TEST

START_TEST(err_init_invalid_kq)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	NEM_err_t err = NEM_fd_init(&fd, -1, work.fds[0]);
	ck_assert(!NEM_err_ok(err));

	work.fds_freed[0] = true;
	work_free(&work);
}
END_TEST

START_TEST(err_init_invalid_fd)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	NEM_err_t err = NEM_fd_init(&fd, work.kq, -1);
	ck_assert(!NEM_err_ok(err));

	work_free(&work);
}
END_TEST

START_TEST(init2_free)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	ck_err(NEM_fd_init2(&fd, work.kq, work.fds[0], work.fds[1]));
	NEM_fd_free(&fd);

	work.fds_freed[0] = true;
	work.fds_freed[1] = true;
	work_free(&work);
}
END_TEST

START_TEST(init2_close_free)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	ck_err(NEM_fd_init2(&fd, work.kq, work.fds[0], work.fds[1]));
	NEM_fd_close(&fd);

	work.fds_freed[0] = true;
	work.fds_freed[1] = true;
	work_free(&work);

	NEM_fd_free(&fd);
}
END_TEST

START_TEST(err_init2_invalid_kq)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	NEM_err_t err = NEM_fd_init2(&fd, -1, work.fds[0], work.fds[1]);
	ck_assert(!NEM_err_ok(err));

	work.fds_freed[0] = true;
	work.fds_freed[1] = true;
	work_free(&work);
}
END_TEST

START_TEST(err_init2_invalid_fd1)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	NEM_err_t err = NEM_fd_init2(&fd, work.kq, -1, work.fds[0]);
	ck_assert(!NEM_err_ok(err));

	work.fds_freed[0] = true;
	work_free(&work);
}
END_TEST

START_TEST(err_init2_invalid_fd2)
{
	work_t work;
	work_init(&work);

	NEM_fd_t fd;
	NEM_err_t err = NEM_fd_init2(&fd, work.kq, work.fds[0], -1);
	ck_assert(!NEM_err_ok(err));

	work.fds_freed[0] = true;
	work_free(&work);
}
END_TEST

static void
read_1_cb(NEM_thunk1_t *thunk, void *varg)
{
	NEM_stream_ca *ca = varg;
	work_t *work = NEM_thunk1_ptr(thunk);

	ck_err(ca->err);
	ck_assert_str_eq("hello", work->buf);

	NEM_app_stop(&work->app);
}

START_TEST(read_1)
{
	work_t work;
	work_init(&work);

	NEM_app_init_root(&work.app);

	NEM_fd_t fd;
	ck_err(NEM_fd_init(&fd, work.app.kq, work.fds[0]));
	work.fds_freed[0] = true;

	ck_assert_int_eq(6, write(work.fds[1], "hello", 6));
	work.buf = alloca(6);

	ck_err(NEM_fd_read(&fd, work.buf, 6, NEM_thunk1_new_ptr(
		&read_1_cb,
		&work
	)));

	NEM_app_run(&work.app);
	NEM_fd_free(&fd);
	NEM_app_free(&work.app);

	work_free(&work);
}
END_TEST

static void
err_fail_cb(NEM_thunk1_t *thunk, void *varg)
{
	NEM_panic("should not be called");
}

static void
err_read_interleaved_cb(NEM_thunk1_t *thunk, void *varg)
{
	NEM_stream_ca *ca = varg;
	ck_assert(!NEM_err_ok(ca->err));

	bool *called = NEM_thunk1_ptr(thunk);
	*called = true;
}

START_TEST(err_read_interleaved)
{
	work_t work;
	work_init(&work);

	NEM_app_init_root(&work.app);

	NEM_fd_t fd;
	ck_err(NEM_fd_init(&fd, work.app.kq, work.fds[0]));
	work.fds_freed[0] = true;

	work.buf = alloca(12);

	bool called = false;

	ck_err(NEM_fd_read(&fd, work.buf, 6, NEM_thunk1_new_ptr(
		&err_read_interleaved_cb,
		&called
	)));

	NEM_err_t err = NEM_fd_read(&fd, work.buf, 6, NEM_thunk1_new_ptr(
		&err_fail_cb,
		NULL
	));
	ck_assert(!NEM_err_ok(err));

	NEM_fd_free(&fd);
	NEM_app_free(&work.app);
	work_free(&work);

	ck_assert(called);
}
END_TEST

START_TEST(err_read_closed)
{
	work_t work;
	work_init(&work);

	NEM_app_init_root(&work.app);

	NEM_fd_t fd;
	ck_err(NEM_fd_init(&fd, work.app.kq, work.fds[0]));
	work.fds_freed[0] = true;
	work.buf = alloca(6);

	NEM_fd_close(&fd);
	NEM_err_t err = NEM_fd_read(&fd, work.buf, 6, NEM_thunk1_new_ptr(
		&err_fail_cb,
		NULL
	));
	ck_assert(!NEM_err_ok(err));

	NEM_fd_free(&fd);
	NEM_app_free(&work.app);
	work_free(&work);
}
END_TEST

static void
err_read_then_close_cb(NEM_thunk1_t *thunk, void *varg)
{
	NEM_stream_ca *ca = varg;
	ck_assert(!NEM_err_ok(ca->err));
	bool *called = NEM_thunk1_ptr(thunk);
	*called = true;
}

START_TEST(err_read_then_close)
{
	work_t work;
	work_init(&work);
	NEM_app_init_root(&work.app);

	NEM_fd_t fd;
	ck_err(NEM_fd_init(&fd, work.app.kq, work.fds[0]));
	work.fds_freed[0] = true;
	work.buf = alloca(6);

	bool called = false;
	ck_err(NEM_fd_read(&fd, work.buf, 6, NEM_thunk1_new_ptr(
		&err_read_then_close_cb,
		&called
	)));

	NEM_fd_free(&fd);
	NEM_app_free(&work.app);
	work_free(&work);
}
END_TEST

Suite*
suite_fd()
{
	tcase_t tests[] = {
		{ "init_free",             &init_free             },
		{ "init_close_free",       &init_close_free       },
		{ "err_init_invalid_kq",   &err_init_invalid_kq   },
		{ "err_init_invalid_fd",   &err_init_invalid_fd   },
		{ "init2_free",            &init2_free            },
		{ "init2_close_free",      &init2_close_free      },
		{ "err_init2_invalid_kq",  &err_init2_invalid_kq  },
		{ "err_init2_invalid_fd1", &err_init2_invalid_fd1 },
		{ "err_init2_invalid_fd2", &err_init2_invalid_fd2 },
		{ "read_1",                &read_1                },
		{ "err_read_interleaved",  &err_read_interleaved  },
		{ "err_read_closed",       &err_read_closed       },
		{ "err_read_then_close",   &err_read_then_close   },
	};

	return tcase_build_suite("fd", tests, sizeof(tests));
}
