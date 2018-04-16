#include "test.h"

typedef struct {
	NEM_app_t app;
	NEM_fd_t fd_1, fd_2;
	NEM_txnmgr_t t_1, t_2;
	NEM_svcmux_t svc_1, svc_2;
	int ctr, ctr2;
}
work_t;

static void
work_stop_clean(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_app_stop(&work->app);
}

static void
work_stop_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_app_stop(&work->app);
	ck_assert_msg(false, "too long");
}

static void
work_svc_1_1(NEM_thunk_t *thunk, void *varg)
{
	NEM_txn_ca *ca = varg;
	work_t *work = NEM_thunk_ptr(thunk);
	work->ctr += 10;

	ck_err(ca->err);
	ck_assert_ptr_ne(NULL, ca->msg);
	ck_assert_ptr_ne(NULL, ca->txnin);
	ck_assert_ptr_eq(NULL, ca->txnout);
	ck_assert(ca->done);

	NEM_msg_t *msg = NEM_msg_new_reply(ca->msg, 0, 6);
	memcpy(msg->body, "hello", 6);
	NEM_txnin_reply(ca->txnin, msg);
}

static void
work_svc_1_2(NEM_thunk_t *thunk, void *varg)
{
	NEM_txn_ca *ca = varg;
	work_t *work = NEM_thunk_ptr(thunk);
	work->ctr += 100;

	ck_err(ca->err);
	ck_assert_ptr_ne(NULL, ca->msg);
	ck_assert_ptr_ne(NULL, ca->txnin);
	ck_assert_ptr_eq(NULL, ca->txnout);
	ck_assert(ca->done);

	NEM_msg_t *msg1 = NEM_msg_new_reply(ca->msg, 0, 0);
	NEM_msg_t *msg2 = NEM_msg_new_reply(ca->msg, 0, 6);
	memcpy(msg2->body, "world", 6);

	NEM_txnin_reply_continue(ca->txnin, msg1);
	NEM_txnin_reply(ca->txnin, msg2);
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

	NEM_svcmux_entry_t svcs_1[] = {
		{ 1, 1, NEM_thunk_new_ptr(&work_svc_1_1, work) },
		{ 1, 2, NEM_thunk_new_ptr(&work_svc_1_2, work) },
	};
	NEM_svcmux_entry_t svcs_2[] = {
	};

	NEM_svcmux_init(&work->svc_1);
	NEM_svcmux_init(&work->svc_2);

	NEM_svcmux_add_handlers(&work->svc_1, svcs_1, NEM_ARRSIZE(svcs_1));
	NEM_svcmux_add_handlers(&work->svc_2, svcs_2, NEM_ARRSIZE(svcs_2));
	NEM_txnmgr_set_mux(&work->t_1, &work->svc_1);
	NEM_txnmgr_set_mux(&work->t_2, &work->svc_2);

	NEM_svcmux_decref(&work->svc_1);
	NEM_svcmux_decref(&work->svc_2);
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

static void
send_recv_1_1_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_txn_ca *ca = varg;

	ck_err(ca->err);
	ck_assert_ptr_ne(NULL, ca->txnout);
	ck_assert_ptr_eq(NULL, ca->txnin);
	ck_assert_ptr_ne(NULL, ca->msg);
	ck_assert(ca->done);
	ck_assert_int_eq(6, ca->msg->packed.body_len);
	ck_assert_str_eq("hello", ca->msg->body);
	// NB: The message is automatically freed here.

	work->ctr2 += 1;
	NEM_app_stop(&work->app);
}

START_TEST(send_recv_1_1)
{
	work_t work;
	work_init(&work);

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 1;

	NEM_txnmgr_req1(&work.t_2, NULL, msg, NEM_thunk_new_ptr(
		&send_recv_1_1_cb,
		&work
	));

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(work.ctr, 10);
	ck_assert_int_eq(work.ctr2, 1);
	work_free(&work);
}
END_TEST

static void
send_recv_1_2_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_txn_ca *ca = varg;

	ck_err(ca->err);
	ck_assert_ptr_ne(NULL, ca->txnout);
	ck_assert_ptr_eq(NULL, ca->txnin);
	ck_assert_ptr_ne(NULL, ca->msg);

	if (0 == work->ctr2) {
		ck_assert_int_eq(0, ca->msg->packed.body_len);
		ck_assert_int_eq(0, ca->msg->packed.header_len);
	}
	else if (1 == work->ctr2) {
		ck_assert_int_eq(6, ca->msg->packed.body_len);
		ck_assert_int_eq(0, ca->msg->packed.header_len);
		ck_assert_str_eq("world", ca->msg->body);
		ck_assert(ca->done);
		NEM_app_stop(&work->app);
	}

	work->ctr2 += 1;
}

START_TEST(send_recv_1_2)
{
	work_t work;
	work_init(&work);

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 2;

	NEM_txnmgr_req1(&work.t_2, NULL, msg, NEM_thunk_new_ptr(
		&send_recv_1_2_cb,
		&work
	));

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(work.ctr, 100);
	ck_assert_int_eq(work.ctr2, 2);
	work_free(&work);
}
END_TEST

static void
err_fd_closed_clisend_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_txn_ca *ca = varg;
	ck_assert(!NEM_err_ok(ca->err));
	ck_assert_ptr_eq(NULL, ca->txnin);
	ck_assert_ptr_ne(NULL, ca->txnout);
	ck_assert_int_eq(0, ca->txnout->base.messages_len);
	ck_assert_ptr_eq(NULL, ca->msg);
	ck_assert(ca->done);
	work->ctr2 += 1;
	NEM_app_stop(&work->app);
}

START_TEST(err_fd_closed_clisend)
{
	work_t work;
	work_init(&work);

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 1;

	NEM_fd_close(&work.fd_1);

	NEM_txnmgr_req1(&work.t_2, NULL, msg, NEM_thunk_new_ptr(
		&err_fd_closed_clisend_cb,
		&work
	));

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(work.ctr, 0);
	ck_assert_int_eq(work.ctr2, 1);
	work_free(&work);
}
END_TEST

static void
err_fd_closed_srvsend_cb(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_txn_ca *ca = varg;

	if (work->ctr2 == 0) {
		ck_err(ca->err);
		ck_assert_ptr_eq(NULL, ca->txnin);
		ck_assert_ptr_ne(NULL, ca->txnout);
		ck_assert_int_eq(1, ca->txnout->base.messages_len);
		ck_assert_ptr_ne(NULL, ca->msg);
		ck_assert(!ca->done);
		NEM_fd_close(&work->fd_1);
		NEM_fd_close(&work->fd_2);
	}
	else {
		ck_assert(!NEM_err_ok(ca->err));
		ck_assert_ptr_eq(NULL, ca->txnin);
		ck_assert_ptr_ne(NULL, ca->txnout);
		ck_assert_int_eq(1, ca->txnout->base.messages_len);
		ck_assert_ptr_eq(NULL, ca->msg);
		ck_assert(ca->done);
	}

	work->ctr2 += 1;

	NEM_app_defer(&work->app, NEM_thunk1_new_ptr(&work_stop_clean, work));
}

START_TEST(err_fd_closed_srvsend)
{
	work_t work;
	work_init(&work);

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 2;

	NEM_txnmgr_req1(&work.t_2, NULL, msg, NEM_thunk_new_ptr(
		&err_fd_closed_srvsend_cb,
		&work
	));

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(work.ctr, 100);
	ck_assert_int_eq(work.ctr2, 2);
	work_free(&work);
}
END_TEST

Suite*
suite_txnmgr()
{
	tcase_t tests[] = {
		{ "scaffolding",           &scaffolding           },
		{ "send_recv_1_1",         &send_recv_1_1         },
		{ "send_recv_1_2",         &send_recv_1_2         },
		{ "err_fd_closed_clisend", &err_fd_closed_clisend },
		{ "err_fd_closed_srvsend", &err_fd_closed_srvsend },
	};

	return tcase_build_suite("txnmgr", tests, sizeof(tests));
}
