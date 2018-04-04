#include "test.h"
#include "txnmgr.h"

typedef struct {
	NEM_app_t app;
	NEM_chan_t c_1, c_2;
}
work_t;

static void
work_close_fd(NEM_thunk1_t *thunk, void *varg)
{
	NEM_fd_t *fd = NEM_thunk1_inlineptr(thunk);
	NEM_fd_free(fd);
}

static void
work_stop_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_app_stop(&work->app);
	ck_assert_msg(false, "too long");
}

static void
never_called(NEM_thunk_t *thunk, void *varg)
{
	ck_assert_msg(false, "never called called");
}

static void
work_init(work_t *work)
{
	bzero(work, sizeof(*work));
	ck_err(NEM_app_init_root(&work->app));

	NEM_thunk1_t *fd1_close = NEM_thunk1_new(&work_close_fd, sizeof(NEM_fd_t));
	NEM_thunk1_t *fd2_close = NEM_thunk1_new(&work_close_fd, sizeof(NEM_fd_t));
	NEM_fd_t *fd1 = NEM_thunk1_inlineptr(fd1_close);
	NEM_fd_t *fd2 = NEM_thunk1_inlineptr(fd2_close);

	ck_err(NEM_fd_init_unix(fd1, fd2, work->app.kq));
	NEM_fd_on_close(fd1, fd1_close);
	NEM_fd_on_close(fd2, fd2_close);

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

START_TEST(scaffolding)
{
	work_t work;
	work_init(&work);
	work_free(&work);
}
END_TEST

START_TEST(init_free)
{
	work_t work;
	work_init(&work);
	NEM_rootd_txnmgr_t mgr;
	NEM_rootd_txnmgr_init(&mgr, &work.c_1, NEM_thunk_new(&never_called, 0));
	NEM_rootd_txnmgr_free(&mgr);
	work_free(&work);
}
END_TEST

static void
roundtrip_txn_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_rootd_txn_ca *ca = varg;
	ck_err(ca->err);

	ck_assert_int_eq(6, ca->msg->packed.body_len);
	ck_assert_str_eq("hello", ca->msg->body);
	NEM_app_stop(&work->app);
}

static void
roundtrip_txn_srv(NEM_thunk_t *thunk, void *varg)
{
	NEM_chan_ca *ca = varg;
	ck_err(ca->err);
	ck_assert_ptr_ne(NULL, ca->chan);
	ck_assert_ptr_ne(NULL, ca->msg);

	NEM_msg_t *msg = NEM_msg_new_reply(ca->msg, 0, 6);
	snprintf(msg->body, 6, "hello");
	NEM_chan_send(ca->chan, msg, NULL);
}

START_TEST(roundtrip_txn)
{
	work_t work;
	work_init(&work);

	NEM_rootd_txnmgr_t mgr;
	NEM_rootd_txnmgr_init(&mgr, &work.c_1, NEM_thunk_new_ptr(
		&never_called, &work
	));

	NEM_chan_on_msg(&work.c_2, NEM_thunk_new_ptr(&roundtrip_txn_srv, &work));

	NEM_rootd_txnmgr_req1(
		&mgr,
		NEM_msg_new(0, 0),
		NEM_thunk1_new_ptr(&roundtrip_txn_cb, &work)
	);

	ck_err(NEM_app_run(&work.app));

	NEM_rootd_txnmgr_free(&mgr);
	work_free(&work);
}
END_TEST

Suite*
suite_txnmgr()
{
	tcase_t tests[] = {
		{ "scaffolding",   &scaffolding   },
		{ "init_free",     &init_free     },
		{ "roundtrip_txn", &roundtrip_txn },
	};

	return tcase_build_suite("txnmgr", tests, sizeof(tests));
}
