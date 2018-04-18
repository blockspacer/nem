#include "test.h"

typedef struct {
	NEM_kq_t kq;
	NEM_chan_t c_1, c_2;
	int freed[2];
	int ctr;
}
work_t;

static void
work_stop_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_kq_stop(&work->kq);
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
	ck_err(NEM_kq_init_root(&work->kq));

	NEM_fd_t *fd1 = NEM_malloc(sizeof(NEM_fd_t));
	NEM_fd_t *fd2 = NEM_malloc(sizeof(NEM_fd_t));
	ck_err(NEM_fd_init_unix(fd1, fd2, work->kq.kq));

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

	NEM_kq_after(&work->kq, 3000, NEM_thunk1_new_ptr(
		&work_stop_cb,
		work
	));
}

static void
work_free(work_t *work)
{
	if (!work->freed[0]) {
		NEM_chan_free(&work->c_1);
	}
	if (!work->freed[1]) {
		NEM_chan_free(&work->c_2);
	}
	NEM_kq_free(&work->kq);
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
	NEM_kq_stop(&work->kq);

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

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
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
	NEM_kq_stop(&work->kq);

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

	NEM_msg_t *msg = NEM_msg_new(6, 0);
	memcpy(msg->header, "hello", 6);
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
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

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	ck_err(NEM_msg_set_header_raw(msg, strdup("hello"), 6));
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

static void
send_body_inline_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);

	work->ctr += 1;
	ck_assert_int_eq(0, ca->msg->fd);
	ck_assert_int_eq(0, ca->msg->flags & NEM_MSGFLAG_HAS_FD);
	ck_assert_int_eq(0, ca->msg->packed.header_len);
	ck_assert_int_eq(6, ca->msg->packed.body_len);
	ck_assert_ptr_eq(NULL, ca->msg->header);
	ck_assert_str_eq("world", ca->msg->body);

	NEM_kq_stop(&work->kq);
}

START_TEST(send_body_inline)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_body_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 6);
	memcpy(msg->body, "world", 6);
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

START_TEST(send_body)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_body_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	ck_err(NEM_msg_set_body(msg, strdup("world"), 6));
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
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

	NEM_kq_stop(&work->kq);
}

START_TEST(send_hdrbody_inline)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_hdrbody_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(6, 6);
	memcpy(msg->header, "hello", 6);
	memcpy(msg->body, "world", 6);
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

START_TEST(send_hdrbody_ihdr)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_hdrbody_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(6, 0);
	memcpy(msg->header, "hello", 6);
	ck_err(NEM_msg_set_body(msg, strdup("world"), 6));
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

START_TEST(send_hdrbody_ibody)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_hdrbody_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 6);
	ck_err(NEM_msg_set_header_raw(msg, strdup("hello"), 6));
	memcpy(msg->body, "world", 6);
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

START_TEST(send_hdrbody)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_hdrbody_inline_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	ck_err(NEM_msg_set_header_raw(msg, strdup("hello"), 6));
	ck_err(NEM_msg_set_body(msg, strdup("world"), 6));
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

static void
send_fd_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);

	work->ctr += 1;
	ck_assert_int_ne(0, ca->msg->fd);
	ck_assert_int_ne(0, ca->msg->flags & NEM_MSGFLAG_HAS_FD);
	ck_assert_int_eq(0, ca->msg->packed.header_len);
	ck_assert_int_eq(0, ca->msg->packed.body_len);
	ck_assert_ptr_eq(NULL, ca->msg->header);
	ck_assert_ptr_eq(NULL, ca->msg->body);
	ck_assert_int_eq(0, close(ca->msg->fd));

	NEM_kq_stop(&work->kq);
}

START_TEST(send_fd)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_fd_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	ck_err(NEM_msg_set_fd(msg, STDOUT_FILENO));
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

static void
send_fd_hdr_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);

	work->ctr += 1;
	ck_assert_int_ne(0, ca->msg->fd);
	ck_assert_int_ne(0, ca->msg->flags & NEM_MSGFLAG_HAS_FD);
	ck_assert_int_eq(6, ca->msg->packed.header_len);
	ck_assert_int_eq(0, ca->msg->packed.body_len);
	ck_assert_str_eq("hello", ca->msg->header);
	ck_assert_ptr_eq(NULL, ca->msg->body);
	ck_assert_int_eq(0, close(ca->msg->fd));

	NEM_kq_stop(&work->kq);
}

START_TEST(send_fd_hdr)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_fd_hdr_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	ck_err(NEM_msg_set_fd(msg, STDOUT_FILENO));
	ck_err(NEM_msg_set_header_raw(msg, strdup("hello"), 6));
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

START_TEST(send_fd_hdr_inline)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_fd_hdr_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(6, 0);
	memcpy(msg->header, "hello", 6);
	ck_err(NEM_msg_set_fd(msg, STDOUT_FILENO));
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

static void
send_fd_body_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);

	work->ctr += 1;
	ck_assert_int_ne(0, ca->msg->fd);
	ck_assert_int_ne(0, ca->msg->flags & NEM_MSGFLAG_HAS_FD);
	ck_assert_int_eq(0, ca->msg->packed.header_len);
	ck_assert_int_eq(6, ca->msg->packed.body_len);
	ck_assert_ptr_eq(NULL, ca->msg->header);
	ck_assert_str_eq("world", ca->msg->body);
	ck_assert_int_eq(0, close(ca->msg->fd));

	NEM_kq_stop(&work->kq);
}

START_TEST(send_fd_body)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_fd_body_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	ck_err(NEM_msg_set_body(msg, strdup("world"), 6));
	ck_err(NEM_msg_set_fd(msg, STDOUT_FILENO));
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

START_TEST(send_fd_body_inline)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_fd_body_cb,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 6);
	ck_err(NEM_msg_set_fd(msg, STDOUT_FILENO));
	memcpy(msg->body, "world", 6);
	NEM_chan_send(&work.c_2, msg, NULL);

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);

	work_free(&work);
}
END_TEST

static void
send_callback_on_msg(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);
	work->ctr += 1;
}

static void
send_callback_on_send(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);
	ck_assert_ptr_ne(NULL, ca->msg);
	ck_assert_ptr_ne(NULL, ca->chan);
	work->ctr += 10;
	NEM_kq_stop(&work->kq);
}

START_TEST(send_callback)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_callback_on_msg,
		&work
	));

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	NEM_chan_send(&work.c_2, msg, NEM_thunk1_new_ptr(
		&send_callback_on_send,
		&work
	));

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(11, work.ctr);
	work_free(&work);
}
END_TEST

static void
err_send_callback_on_send(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_assert(!NEM_err_ok(ca->err));
	work->ctr += 1;
	NEM_kq_stop(&work->kq);
}

START_TEST(err_send_callback)
{
	work_t work;
	work_init(&work);

	NEM_chan_free(&work.c_1);
	work.freed[0] = 1;

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	NEM_chan_send(&work.c_2, msg, NEM_thunk1_new_ptr(
		&err_send_callback_on_send,
		&work
	));

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(1, work.ctr);
	work_free(&work);
}
END_TEST

static void
send_ordering_on_msg(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);

	int *body = (int*) ca->msg->body;
	ck_assert_int_eq(*body, work->ctr);
	work->ctr += 1;

	if (work->ctr == 4) {
		NEM_kq_stop(&work->kq);
	}
}

START_TEST(send_ordering)
{
	work_t work;
	work_init(&work);

	NEM_chan_on_msg(&work.c_1, NEM_thunk_new_ptr(
		&send_ordering_on_msg,
		&work
	));

	for (size_t i = 0; i < 4; i += 1) {
		NEM_msg_t *msg = NEM_msg_new(0, sizeof(int));
		int body = (int)i;
		memcpy(msg->body, &body, sizeof(int));
		NEM_chan_send(&work.c_2, msg, NULL);
	}

	ck_err(NEM_kq_run(&work.kq));
	ck_assert_int_eq(4, work.ctr);
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
		{ "send_body_inline",    &send_body_inline    },
		{ "send_body",           &send_body           },
		{ "send_hdrbody_inline", &send_hdrbody_inline },
		{ "send_hdrbody_ihdr",   &send_hdrbody_ihdr   },
		{ "send_hdrbody_ibody",  &send_hdrbody_ibody  },
		{ "send_hdrbody",        &send_hdrbody        },
		{ "send_fd",             &send_fd             },
		{ "send_fd_hdr",         &send_fd_hdr         },
		{ "send_fd_hdr_inline",  &send_fd_hdr_inline  },
		{ "send_fd_body",        &send_fd_body        },
		{ "send_fd_body_inline", &send_fd_body_inline },
		{ "send_callback",       &send_callback       },
		{ "err_send_callback",   &err_send_callback   },
		{ "send_ordering",       &send_ordering       },
	};

	return tcase_build_suite("chan", tests, sizeof(tests));
}
