#include "test.h"

START_TEST(parse)
{
	NEM_semver_t ver;
	ck_err(NEM_semver_init(&ver, "1.2.3"));
	ck_assert_int_eq(1, ver.major);
	ck_assert_int_eq(2, ver.minor);
	ck_assert_int_eq(3, ver.patch);
}
END_TEST

START_TEST(err_parse)
{
	const char *bad_strings[] = {
		NULL,
		"",
		".",
		"1",
		"1.",
		"1..",
		"1.2",
		"1.2.",
		"1.2..",
		"1.2.3.",
	};

	for (size_t i = 0; i < NEM_ARRSIZE(bad_strings); i += 1) {
		NEM_semver_t ver;
		NEM_err_t err = NEM_semver_init(&ver, bad_strings[i]);
		ck_assert(!NEM_err_ok(err));
	}
}
END_TEST

START_TEST(parse_match)
{
	static const struct {
		NEM_semver_match_t expected;
		const char        *str;
	}
	tests[] = {
		{ NEM_SEMVER_MATCH_EXACT, "1.2.3"  },
		{ NEM_SEMVER_MATCH_MAJOR, "^1.2.3" },
		{ NEM_SEMVER_MATCH_MINOR, "~1.2.3" },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(tests); i += 1) {
		NEM_semver_t ver;
		NEM_semver_match_t match = 4;
		ck_err(NEM_semver_init_match(&ver, &match, tests[i].str));
		ck_assert_int_eq(1, ver.major);
		ck_assert_int_eq(2, ver.minor);
		ck_assert_int_eq(3, ver.patch);
		ck_assert_int_eq(tests[i].expected, match);
	}
}
END_TEST

START_TEST(err_parse_match)
{
	const char *bad_strings[] = {
		NULL,
		"",
		"!1.2.3"
	};

	for (size_t i = 0; i < NEM_ARRSIZE(bad_strings); i += 1) {
		NEM_semver_t ver;
		NEM_semver_match_t match;
		NEM_err_t err = NEM_semver_init_match(&ver, &match, bad_strings[i]);
		ck_assert(!NEM_err_ok(err));
	}
}
END_TEST

START_TEST(cmp_exact)
{
	static const struct {
		const char *base;
		const char *test;
		int         result;
	}
	tests[] = {
		{ "2.3.4", "2.3.4",  0 },
		{ "2.3.4", "1.3.4", -1 },
		{ "2.3.4", "3.3.4", -1 },
		{ "2.3.4", "2.2.4", -1 },
		{ "2.3.4", "2.4.4", -1 },
		{ "2.3.4", "2.3.3", -1 },
		{ "2.3.4", "2.3.5", -1 },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(tests); i += 1) {
		NEM_semver_t base;
		NEM_semver_t test;
		ck_err(NEM_semver_init(&base, tests[i].base));
		ck_err(NEM_semver_init(&test, tests[i].test));

		int actual = NEM_semver_cmp(&base, &test, NEM_SEMVER_MATCH_EXACT);
		int expected = tests[i].result;

		ck_assert_msg(
			actual == expected,
			"expected %d, got %d for '%s'='%s'",
			expected,
			actual,
			tests[i].base,
			tests[i].test
		);
	}
}
END_TEST

START_TEST(cmp_minor)
{
	static const struct {
		const char *base;
		const char *test;
		int         result;
	}
	tests[] = {
		{ "2.3.4", "2.3.4",  0 },
		{ "2.3.4", "1.3.4", -1 },
		{ "2.3.4", "3.3.4", -1 },
		{ "2.3.4", "2.2.4", -1 },
		{ "2.3.4", "2.4.4", -1 },
		{ "2.3.4", "2.3.3", -1 },
		{ "2.3.4", "2.3.5",  1 },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(tests); i += 1) {
		NEM_semver_t base;
		NEM_semver_t test;
		ck_err(NEM_semver_init(&base, tests[i].base));
		ck_err(NEM_semver_init(&test, tests[i].test));

		int actual = NEM_semver_cmp(&base, &test, NEM_SEMVER_MATCH_MINOR);
		int expected = tests[i].result;

		ck_assert_msg(
			actual == expected,
			"expected %d, got %d for '%s'='%s'",
			expected,
			actual,
			tests[i].base,
			tests[i].test
		);
	}
}
END_TEST

START_TEST(cmp_major)
{
	static const struct {
		const char *base;
		const char *test;
		int         result;
	}
	tests[] = {
		{ "2.3.4", "2.3.4",  0 },
		{ "2.3.4", "1.3.4", -1 },
		{ "2.3.4", "3.3.4", -1 },
		{ "2.3.4", "2.2.4", -1 },
		{ "2.3.4", "2.4.4",  1 },
		{ "2.3.4", "2.3.3", -1 },
		{ "2.3.4", "2.3.5",  1 },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(tests); i += 1) {
		NEM_semver_t base;
		NEM_semver_t test;
		ck_err(NEM_semver_init(&base, tests[i].base));
		ck_err(NEM_semver_init(&test, tests[i].test));

		int actual = NEM_semver_cmp(&base, &test, NEM_SEMVER_MATCH_MAJOR);
		int expected = tests[i].result;

		ck_assert_msg(
			actual == expected,
			"expected %d, got %d for '%s'='%s'",
			expected,
			actual,
			tests[i].base,
			tests[i].test
		);
	}
}
END_TEST

Suite*
suite_semver()
{
	tcase_t tests[] = {
		{ "parse",           &parse           },
		{ "err_parse",       &err_parse       },
		{ "parse_match",     &parse_match     },
		{ "err_parse_match", &err_parse_match },
		{ "cmp_exact",       &cmp_exact       },
		{ "cmp_minor",       &cmp_minor       },
		{ "cmp_major",       &cmp_major       },
	};

	return tcase_build_suite("semver", tests, sizeof(tests));
}
