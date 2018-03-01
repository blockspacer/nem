#include "test.h"

typedef struct {
	NEM_app_t app;
	NEM_stream_t s_1;
	NEM_stream_t s_2;
	NEM_fd_t  fd_1;
	NEM_fd_t  fd_2;
	bool      fds_freed[2];
	char     *bufs[2];
	int       state[2];
}
work_t;

typedef NEM_err_t(*fd_init_fn)(NEM_fd_t*, NEM_fd_t*, int);

static void
work_stop_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_app_stop(&work->app);
	ck_assert_msg(false, "too long");
}

static void
work_init(work_t *work, fd_init_fn fn)
{
	bzero(work, sizeof(*work));
	ck_err(NEM_app_init_root(&work->app));
	ck_err(fn(&work->fd_1, &work->fd_2, work->app.kq));

	work->s_1 = NEM_fd_as_stream(&work->fd_1);
	work->s_2 = NEM_fd_as_stream(&work->fd_2);
	
	NEM_app_after(&work->app, 3000, NEM_thunk1_new_ptr(
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

#define DEFINE_TESTS(fn) \
	START_TEST(fn##_pipe) { fn(&NEM_fd_init_pipe); } END_TEST \
	START_TEST(fn##_unix) { fn(&NEM_fd_init_unix); } END_TEST

#define USE_TESTS(fn) \
	{ #fn"_pipe", &fn##_pipe }, \
	{ #fn"_unix", &fn##_unix }

static void
init_free(fd_init_fn fn)
{
	work_t work;
	work_init(&work, fn);
	work_free(&work);
}
DEFINE_TESTS(init_free);

static void
write_once_rcb(NEM_thunk1_t *thunk, void *varg)
{
	NEM_stream_ca *ca = varg;
	ck_err(ca->err);

	work_t *work = NEM_thunk1_ptr(thunk);
	ck_assert_str_eq("hello", work->bufs[0]);
	NEM_app_stop(&work->app);
}

static void
write_once_wcb(NEM_thunk1_t *thunk, void *varg)
{
	NEM_stream_ca *ca = varg;
	ck_err(ca->err);

	bool *called = NEM_thunk1_ptr(thunk);
	*called = true;
}

static void
write_once(fd_init_fn fn)
{
	work_t work;
	work_init(&work, fn);
	work.bufs[0] = alloca(6);

	NEM_stream_read(work.s_1, work.bufs[0], 6, NEM_thunk1_new_ptr(
		&write_once_rcb,
		&work
	));

	bool called = false;
	NEM_stream_write(work.s_2, "hello", 6, NEM_thunk1_new_ptr(
		&write_once_wcb,
		&called
	));

	ck_err(NEM_app_run(&work.app));
	ck_assert_str_eq("hello", work.bufs[0]);
	ck_assert(called);

	work_free(&work);
}
DEFINE_TESTS(write_once);

static void
echo_rcb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_stream_ca *ca = varg;
	ck_err(ca->err);

	if (0 == work->state[0]) {
		work->state[0] = 1;
		ck_assert_str_eq("hello", work->bufs[0]);
		ck_err(
			NEM_stream_write(work->s_1, work->bufs[0], 6, NEM_thunk1_new_ptr(
				&echo_rcb,
				work
			))
		);
	}
}

static void
echo_wcb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_stream_ca *ca = varg;
	ck_err(ca->err);

	if (0 == work->state[1]) {
		work->state[1] = 1;
		ck_err(
			NEM_stream_read(work->s_2, work->bufs[1], 6, NEM_thunk1_new_ptr(
				&echo_wcb,
				work
			))
		);
	}
	else {
		work->state[1] = 2;
		ck_assert_str_eq("hello", work->bufs[1]);
		NEM_app_stop(&work->app);
	}
}

static void
echo(fd_init_fn fn)
{
	work_t work;
	work_init(&work, fn);
	work.bufs[0] = alloca(6);
	work.bufs[1] = alloca(6);

	ck_err(NEM_stream_read(work.s_1, work.bufs[0], 6, NEM_thunk1_new_ptr(
		&echo_rcb,
		&work
	)));

	ck_err(NEM_stream_write(work.s_2, "hello", 6, NEM_thunk1_new_ptr(
		&echo_wcb,
		&work
	)));

	ck_err(NEM_app_run(&work.app));
	ck_assert_str_eq("hello", work.bufs[0]);
	ck_assert_str_eq("hello", work.bufs[1]);
	ck_assert_int_eq(1, work.state[0]);
	ck_assert_int_eq(2, work.state[1]);

	work_free(&work);
}
DEFINE_TESTS(echo);

static void
err_read_closed_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_stream_ca *ca = varg;
	ck_assert(!NEM_err_ok(ca->err));

	work->state[0] = 1;
	NEM_app_stop(&work->app);
}

static void
err_read_closed(fd_init_fn fn)
{
	work_t work;
	work_init(&work, fn);
	work.bufs[0] = alloca(6);

	ck_err(NEM_stream_read(work.s_1, work.bufs[0], 6, NEM_thunk1_new_ptr(
		&err_read_closed_cb,
		&work
	)));

	ck_err(NEM_stream_close(work.s_2));

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(1, work.state[0]);

	work_free(&work);
}
DEFINE_TESTS(err_read_closed);

static void
err_read_preclosed(fd_init_fn fn)
{
	work_t work;
	work_init(&work, fn);

	ck_err(NEM_stream_close(work.s_1));

	NEM_err_t err = NEM_stream_read(work.s_1, work.bufs[0], 5, NEM_thunk1_new_ptr(
		&work_stop_cb,
		&work
	));
	ck_assert(!NEM_err_ok(err));

	work_free(&work);
}
DEFINE_TESTS(err_read_preclosed);

static void
err_write_preclosed(fd_init_fn fn)
{
	work_t work;
	work_init(&work, fn);

	ck_err(NEM_stream_close(work.s_1));

	NEM_err_t err = NEM_stream_write(work.s_1, "hello", 6, NEM_thunk1_new_ptr(
		&work_stop_cb,
		&work
	));
	ck_assert(!NEM_err_ok(err));

	work_free(&work);
}
DEFINE_TESTS(err_write_preclosed);

static void
dangerous_reuse_cb(NEM_thunk1_t *thunk, void *varg)
{
	int *ctr = NEM_thunk1_ptr(thunk);
	*ctr += 1;
}

static void
dangerous_reuse_onclose(NEM_thunk1_t *thunk, void *varg)
{
	NEM_fd_t *fd = NEM_thunk1_ptr(thunk);
	NEM_fd_free(fd);
	free(fd);
}

static void
dangerous_reuse(fd_init_fn fn)
{
	char buf[6];
	NEM_fd_t *fd1 = NEM_malloc(sizeof(NEM_fd_t));
	NEM_fd_t *fd2 = NEM_malloc(sizeof(NEM_fd_t));

	int kq = kqueue();
	ck_assert_int_ne(-1, kq);

	ck_err(NEM_fd_init_pipe(fd1, fd2, kq));
	NEM_fd_on_close(fd1, NEM_thunk1_new_ptr(
		&dangerous_reuse_onclose,
		fd1
	));
	NEM_fd_on_close(fd2, NEM_thunk1_new_ptr(
		&dangerous_reuse_onclose,
		fd2
	));

	NEM_fd_close(fd2);

	int ctr = 0;
	ck_err(NEM_fd_read(fd1, buf, 6, NEM_thunk1_new_ptr(
		&dangerous_reuse_cb,
		&ctr
	)));
	ck_err(NEM_fd_write(fd1, buf, 6, NEM_thunk1_new_ptr(
		&dangerous_reuse_cb,
		&ctr
	)));

	NEM_fd_close(fd1);

	close(kq);
}
DEFINE_TESTS(dangerous_reuse);

Suite*
suite_stream()
{
	tcase_t tests[] = {
		USE_TESTS(init_free),
		USE_TESTS(write_once),
		USE_TESTS(echo),
		USE_TESTS(err_read_closed),
		USE_TESTS(err_read_preclosed),
		USE_TESTS(err_write_preclosed),
		USE_TESTS(dangerous_reuse),
	};

	return tcase_build_suite("stream", tests, sizeof(tests));
}
