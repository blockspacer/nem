#include "test.h"
#include "tls.h"

START_TEST(key_init_free)
{
	NEM_tls_key_t *key;
	ck_err(NEM_tls_key_init_file(&key, "./test/data/test.nem.rocks.key"));
	NEM_tls_key_free(key);
}
END_TEST

Suite*
suite_tls()
{
	tcase_t tests[] = {
		{ "key_init_free", &key_init_free },
	};

	return tcase_build_suite("tls", tests, sizeof(tests));
}
