#include "test.h"

START_TEST(mbed_tls_err)
{
	NEM_err_t err = NEM_err_mbedtls(0);
	const char *str = NEM_err_string(err);
	ck_assert_ptr_ne(NULL, str);
	ck_assert_str_eq("", str);
}
END_TEST

Suite*
suite_error()
{
	tcase_t tests[] = {
		{ "mbed_tls_err", &mbed_tls_err },
	};

	return tcase_build_suite("error", tests, sizeof(tests));
}
