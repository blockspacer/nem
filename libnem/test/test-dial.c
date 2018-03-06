#include "test.h"

static const char *unix_path = "nem.sock";

typedef struct {
	NEM_app_t app;
	NEM_list_t list;
	int nconns;
	int ctr;
}
work_t;

static void
work_init(work_t *work)
{
	bzero(work, sizeof(*work));
	ck_err(NEM_app_init_root(&work->app));

	struct stat sb;
	if (0 == stat(unix_path, &sb)) {
		if (S_ISSOCK(sb.st_mode)) {
			unlink(unix_path);
		}
	}
}

static void
work_free(work_t *work)
{
	NEM_list_close(work->list);
	NEM_app_free(&work->app);

	struct stat sb;
	ck_assert_int_eq(-1, stat(unix_path, &sb));
	ck_assert_int_eq(ENOENT, errno);
}

static void
listen_dial_conn(NEM_thunk_t *thunk, void *varg)
{
	work_t *work = NEM_thunk_ptr(thunk);
	NEM_list_ca *ca = varg;
	ck_err(ca->err);
	NEM_stream_close(ca->stream);
	work->nconns += 1;
	work->ctr += 1;

	if (work->ctr == 2) {
		NEM_app_stop(&work->app);
	}
}

static void
listen_dial_cb(NEM_thunk1_t *thunk, void *varg)
{
	work_t *work = NEM_thunk1_ptr(thunk);
	NEM_dial_ca *ca = varg;
	ck_err(ca->err);
	NEM_stream_close(ca->stream);
	work->ctr += 1;

	if (work->ctr == 2) {
		NEM_app_stop(&work->app);
	}
}

START_TEST(listen_dial_unix)
{
	work_t work;
	work_init(&work);

	ck_err(NEM_list_init_unix(
		&work.list,
		work.app.kq,
		unix_path,
		NEM_thunk_new_ptr(
			&listen_dial_conn,
			&work
		)
	));

	NEM_dial_unix(
		work.app.kq,
		unix_path,
		NEM_thunk1_new_ptr(
			&listen_dial_cb,
			&work
		)
	);

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(1, work.nconns);
	ck_assert_int_eq(2, work.ctr);

	work_free(&work);
}
END_TEST

START_TEST(listen_dial_tcp)
{
	work_t work;
	work_init(&work);

	ck_err(NEM_list_init_tcp(
		&work.list,
		work.app.kq,
		12894,
		NULL,
		NEM_thunk_new_ptr(
			&listen_dial_conn,
			&work
		)
	));

	NEM_dial_tcp(
		work.app.kq,
		12894,
		"127.0.0.1",
		NEM_thunk1_new_ptr(
			&listen_dial_cb,
			&work
		)
	);

	ck_err(NEM_app_run(&work.app));
	ck_assert_int_eq(1, work.nconns);
	ck_assert_int_eq(2, work.ctr);

	work_free(&work);
}
END_TEST

Suite*
suite_dial()
{
	tcase_t tests[] = {
		{ "listen_dial_unix", &listen_dial_unix },
		{ "listen_dial_tcp",  &listen_dial_tcp  },
	};

	return tcase_build_suite("dial", tests, sizeof(tests));
}
