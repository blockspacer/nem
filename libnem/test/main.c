#include "test.h"

typedef Suite*(*suite_def)();

extern Suite 
	*suite_thunk(),
	*suite_rootcert(),
	*suite_marshal(),
	*suite_marshal_json(),
	*suite_marshal_bson(),
	*suite_marshal_toml(),
	*suite_msghdr(),
	*suite_msg(),
	*suite_kq(),
	*suite_fd(),
	*suite_stream(),
	*suite_list(),
	*suite_dial(),
	*suite_chan(),
	*suite_svcmux(),
	*suite_txnmgr(),
	*suite_app();

static suite_def suites[] = {
	&suite_thunk,
	&suite_rootcert,
	&suite_marshal,
	&suite_marshal_json,
	&suite_marshal_bson,
	&suite_marshal_toml,
	&suite_msghdr,
	&suite_msg,
	&suite_kq,
	&suite_fd,
	&suite_stream,
	&suite_list,
	&suite_dial,
	&suite_chan,
	&suite_svcmux,
	&suite_txnmgr,
	&suite_app,
};

int
main()
{
	int number_failed = 0;
	int i;

	Suite *suite = NULL;
	SRunner *runner = NULL;

	runner = srunner_create(suite);

	for (i = 0; i < sizeof(suites) / sizeof(suite_def); i += 1) {
		srunner_add_suite(runner, suites[i]());
	}

	srunner_run_all(runner, CK_NORMAL);
	number_failed = srunner_ntests_failed(runner);
	srunner_free(runner);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

Suite*
tcase_build_suite_with_fixtures(
	const char *name,
	void(*setup)(),
	void(*teardown)(),
	tcase_t *tests,
   	size_t blen
) {
	Suite *suite = suite_create(name);
	size_t ntests = blen / sizeof(tcase_t);

	for (size_t i = 0; i < ntests; i += 1) {
		tcase_t *test = &tests[i];

		TCase *tc = tcase_create(test->name);
		tcase_add_checked_fixture(tc, setup, teardown);
		tcase_add_test(tc, test->func);
		suite_add_tcase(suite, tc);
	}

	return suite;
}

Suite*
tcase_build_suite(const char *name, tcase_t *tests, size_t blen)
{
	return tcase_build_suite_with_fixtures(name, NULL, NULL, tests, blen);
}
