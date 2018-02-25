#include "test.h"

typedef struct {
	int kq;
	int fds[2];
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
	};

	return tcase_build_suite("fd", tests, sizeof(tests));
}
