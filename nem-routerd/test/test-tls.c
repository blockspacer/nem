#include "test.h"
#include "tls.h"

START_TEST(key_init_free)
{
	NEM_tls_key_t *key;
	ck_err(NEM_tls_key_init_file(&key, "./test/data/test.nem.rocks.key"));
	NEM_tls_key_free(key);
}
END_TEST

START_TEST(err_key_bad_path)
{
	NEM_tls_key_t *key;
	NEM_err_t err = NEM_tls_key_init_file(&key, "hello");
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_key_invalid_data)
{
	NEM_tls_key_t *key;
	NEM_err_t err = NEM_tls_key_init(&key, "hello", 6);
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(cert_init_free)
{
	NEM_tls_cert_t *cert;
	ck_err(NEM_tls_cert_init_file(&cert, "./test/data/test.nem.rocks.crt"));
	NEM_tls_cert_free(cert);
}
END_TEST

START_TEST(err_cert_bad_path)
{
	NEM_tls_cert_t *cert;
	NEM_err_t err = NEM_tls_cert_init_file(&cert, "hello");
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(err_cert_invalid_data)
{
	NEM_tls_cert_t *cert;
	NEM_err_t err = NEM_tls_cert_init(&cert, "hello", 6);
	ck_assert(!NEM_err_ok(err));
}
END_TEST

START_TEST(tls_init_free)
{
	NEM_tls_t *tls;
	ck_err(NEM_tls_init(&tls));
	NEM_tls_free(tls);
}
END_TEST

START_TEST(tls_add_cert)
{
	NEM_tls_t *tls;
	NEM_tls_cert_t *cert;
	NEM_tls_key_t *key;
	ck_err(NEM_tls_cert_init_file(&cert, "./test/data/test.nem.rocks.crt"));
	ck_err(NEM_tls_key_init_file(&key, "./test/data/test.nem.rocks.key"));
	ck_err(NEM_tls_init(&tls));
	ck_err(NEM_tls_add_cert(tls, key, cert));
	NEM_tls_free(tls);
}
END_TEST

static void
never_call(NEM_thunk_t *thunk, void *varg)
{
	ck_assert_msg(false, "never_call called");
}

START_TEST(tls_list_init_free)
{
	int kq = kqueue();
	ck_assert_int_ne(-1, kq);

	NEM_tls_t *tls;
	ck_err(NEM_tls_init(&tls));

	NEM_list_t list;
	ck_err(NEM_tls_list_init(
		&list,
		tls,
		kq,
		10000,
		NULL,
		NEM_thunk_new_ptr(
			&never_call,
			NULL
		)
	));

	ck_assert_ptr_ne(list.this, NULL);
	ck_assert_ptr_ne(list.vt, NULL);
	
	NEM_list_close(list);
	NEM_tls_free(tls);
	close(kq);
}
END_TEST

Suite*
suite_tls()
{
	tcase_t tests[] = {
		{ "key_init_free",         &key_init_free         },
		{ "err_key_bad_path",      &err_key_bad_path      },
		{ "err_key_invalid_data",  &err_key_invalid_data  },
		{ "cert_init_free",        &cert_init_free        },
		{ "err_cert_bad_path",     &err_cert_bad_path     },
		{ "err_cert_invalid_data", &err_cert_invalid_data },
		{ "tls_init_free",         &tls_init_free         },
		{ "tls_add_cert",          &tls_add_cert          },
		{ "tls_list_init_free",    &tls_list_init_free    },
	};

	return tcase_build_suite("tls", tests, sizeof(tests));
}
