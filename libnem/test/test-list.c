#include "test.h"

static void
err_dont_call(NEM_thunk_t *thunk, void *varg)
{
	NEM_panic("err_dont_call: called");
}

START_TEST(unix_init_free)
{
	int kq = kqueue();
	ck_assert_int_ne(-1, kq);
	const char *path = "foo.sock";

	struct stat sb;
	if (-1 != stat(path, &sb) && S_ISSOCK(sb.st_mode)) {
		unlink(path);
	}

	NEM_list_t list;
	ck_err(NEM_list_init_unix(&list, kq, path, NEM_thunk_new(
		&err_dont_call,
		0
	)));
	NEM_list_close(list);

	ck_assert_int_eq(-1, stat(path, &sb));
	ck_assert_int_eq(ENOENT, errno);

	close(kq);
}
END_TEST

START_TEST(tcp_init_free)
{
	int kq = kqueue();
	ck_assert_int_ne(-1, kq);

	NEM_list_t list;
	ck_err(NEM_list_init_tcp(&list, kq, 1934, NULL, NEM_thunk_new(
		&err_dont_call,
		0
	)));
	NEM_list_close(list);

	close(kq);
}
END_TEST

START_TEST(err_tcp_bad_port)
{
	int kq = kqueue();
	ck_assert_int_ne(-1, kq);

	NEM_list_t list;
	NEM_err_t err = NEM_list_init_tcp(&list, kq, 900000, NULL, NEM_thunk_new(
		&err_dont_call,
		0
	));

	ck_assert(!NEM_err_ok(err));
	close(kq);
}
END_TEST

START_TEST(err_tcp_bad_addr)
{
	int kq = kqueue();
	ck_assert_int_ne(-1, kq);

	NEM_list_t list;
	NEM_err_t err = NEM_list_init_tcp(
		&list,
		kq,
		1935,
		"nem.rocks",
		NEM_thunk_new(&err_dont_call, 0)
	);

	ck_assert(!NEM_err_ok(err));
	close(kq);
}
END_TEST

Suite*
suite_list()
{
	tcase_t tests[] = {
		{ "unix_init_free",   &unix_init_free   },
		{ "tcp_init_free",    &tcp_init_free    },
		{ "err_tcp_bad_port", &err_tcp_bad_port },
		{ "err_tcp_bad_addr", &err_tcp_bad_addr },
	};

	return tcase_build_suite("list", tests, sizeof(tests));
}
