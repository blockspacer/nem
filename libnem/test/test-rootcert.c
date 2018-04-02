#include "test.h"

START_TEST(exists)
{
	size_t len = 0;
	const char *pem = NEM_root_cert_pem(&len);

	ck_assert_ptr_ne(pem, NULL);
	ck_assert_int_gt(len, 0);
	ck_assert_int_eq(len, strlen(pem) + 1);
}
END_TEST

Suite*
suite_rootcert()
{
	tcase_t tests[] = {
		{ "exists", &exists },
	};

	return tcase_build_suite("rootcert", tests, sizeof(tests));
}
