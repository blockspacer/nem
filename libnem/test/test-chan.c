#include "test.h"

typedef struct {
	NEM_app_t app;
	NEM_chan_t c_1, c_2;
	int ctr;
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

static void
send_empty_msg_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);

	work->ctr += 1;
	NEM_app_stop(&work->app);

	ck_assert_int_eq(0, ca->msg->fd);
	ck_assert_int_eq(0, ca->msg->flags & NEM_MSGFLAG_HAS_FD);

	ck_assert_int_eq(0, ca->msg->packed.header_len);
	ck_assert_int_eq(0, ca->msg->packed.body_len);
}

START_TEST(send_empty_msg)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_empty_msg_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_alloc(0, 0);
	NEM_chan_send(&work.c_2, msg);

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

static void
send_hdr_inline_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);

	work->ctr += 1;
	NEM_app_stop(&work->app);

	ck_assert_int_eq(0, ca->msg->fd);
	ck_assert_int_eq(0, ca->msg->flags & NEM_MSGFLAG_HAS_FD);

	ck_assert_int_eq(6, ca->msg->packed.header_len);
	ck_assert_int_eq(0, ca->msg->packed.body_len);
	ck_assert_str_eq("hello", ca->msg->header);
}

START_TEST(send_hdr_inline)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_hdr_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_alloc(6, 0);
	memcpy(msg->header, "hello", 6);
	NEM_chan_send(&work.c_2, msg);

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

START_TEST(send_hdr)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_hdr_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_alloc(0, 0);
	ck_err(NEM_msg_set_header(msg, strdup("hello"), 6));
	NEM_chan_send(&work.c_2, msg);

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

static void
send_hdrbody_inline_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);

	work->ctr += 1;
	ck_assert_int_eq(0, ca->msg->fd);
	ck_assert_int_eq(0, ca->msg->flags & NEM_MSGFLAG_HAS_FD);
	ck_assert_int_eq(6, ca->msg->packed.header_len);
	ck_assert_int_eq(6, ca->msg->packed.body_len);
	ck_assert_str_eq("hello", ca->msg->header);
	ck_assert_str_eq("world", ca->msg->body);

	NEM_app_stop(&work->app);
}

START_TEST(send_hdrbody_inline)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_hdrbody_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_alloc(6, 6);
	memcpy(msg->header, "hello", 6);
	memcpy(msg->body, "world", 6);
	NEM_chan_send(&work.c_2, msg);

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

Suite*
suite_chan()
{
	tcase_t tests[] = {
		{ "init_free",           &init_free           },
		{ "send_empty_msg",      &send_empty_msg      },
		{ "send_hdr_inline",     &send_hdr_inline     },
		{ "send_hdr",            &send_hdr            },
		{ "send_hdrbody_inline", &send_hdrbody_inline },
	};

	return tcase_build_suite("chan", tests, sizeof(tests));
}
