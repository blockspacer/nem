#include "test.h"
#include "args.h"

extern bool NEM_hostd_args_testing;
extern int opterr;

START_TEST(parse_empty)
{
	const char *args[] = {
		"/exe/path",
	};

	NEM_err_t err = NEM_hostd_c_args.setup(
		NULL,
		NEM_ARRSIZE(args),
		(char**) args
	);
	ck_err(err);
	ck_assert_str_eq("/exe/path", NEM_hostd_args()->own_path);
	ck_assert_ptr_eq(NULL, NEM_hostd_args()->config_path);
	ck_assert_int_eq(0, NEM_hostd_args()->verbose);
	NEM_hostd_c_args.teardown(NULL);
}
END_TEST

START_TEST(parse_config)
{
	const char *args[] = {
		"/exe/path",
		"--config=/foo/bar",
	};

	NEM_err_t err = NEM_hostd_c_args.setup(
		NULL,
		NEM_ARRSIZE(args),
		(char**) args
	);
	ck_err(err);
	ck_err(err);
	ck_assert_str_eq("/exe/path", NEM_hostd_args()->own_path);
	ck_assert_str_eq("/foo/bar", NEM_hostd_args()->config_path);
	ck_assert_int_eq(0, NEM_hostd_args()->verbose);
	NEM_hostd_c_args.teardown(NULL);
}
END_TEST

START_TEST(parse_verbose)
{
	const char *args[] = {
		"/exe/path",
		"--verbose"
	};

	NEM_err_t err = NEM_hostd_c_args.setup(
		NULL,
		NEM_ARRSIZE(args),
		(char**) args
	);
	ck_err(err);
	ck_assert_str_eq("/exe/path", NEM_hostd_args()->own_path);
	ck_assert_ptr_eq(NULL, NEM_hostd_args()->config_path);
	ck_assert_int_eq(1, NEM_hostd_args()->verbose);
	NEM_hostd_c_args.teardown(NULL);
}
END_TEST

START_TEST(err_invalid_arg)
{
	NEM_hostd_args_testing = true;
	opterr = 0;

	const char *args[] = {
		"/exe/path",
		"--hi",
	};
	NEM_err_t err = NEM_hostd_c_args.setup(
		NULL,
		NEM_ARRSIZE(args),
		(char**) args
	);
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_no_config)
{
	NEM_hostd_args_testing = true;

	const char *args[] = {
		"/exe/path",
		"--config"
	};
	NEM_err_t err = NEM_hostd_c_args.setup(
		NULL,
		NEM_ARRSIZE(args),
		(char**) args
	);
	ck_assert(!NEM_err_ok(err));
}
END_TEST

Suite*
suite_args()
{
	tcase_t tests[] = {
		{ "parse_empty",     &parse_empty     },
		{ "parse_config",    &parse_config    },
		{ "parse_verbose",   &parse_verbose   },
		{ "err_invalid_arg", &err_invalid_arg },
		{ "err_no_config",   &err_no_config   },
	};

	return tcase_build_suite("args", tests, sizeof(tests));
}
