#include "test.h"

START_TEST(noop)
{
}
END_TEST

Suite*
suite_noop()
{
	tcase_t tests[] = {
		{ "noop", &noop },
	};

	return tcase_build_suite("noop", tests, sizeof(tests));
}
