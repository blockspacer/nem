#include "test.h"

START_TEST(roundtrip_empty)
{
	NEM_msghdr_t hdr_in = {0};
	NEM_msghdr_t *hdr_out = NULL;
	void *bs;
	size_t len;

	ck_err(NEM_msghdr_pack(&hdr_in, &bs, &len));
	ck_err(NEM_msghdr_new(&hdr_out, bs, len));
	free(bs);

	ck_assert_ptr_ne(NULL, hdr_out);
	ck_assert_ptr_eq(NULL, hdr_out->err);
	ck_assert_ptr_eq(NULL, hdr_out->route);
	NEM_msghdr_free(hdr_out);
}
END_TEST

START_TEST(roundtrip_err)
{
	NEM_msghdr_err_t hdr_err = {
		.code   = 42,
		.reason = "poop",
	};
	NEM_msghdr_t hdr_in = {
		.err = &hdr_err,
	};
	NEM_msghdr_t *hdr_out = NULL;
	void *bs;
	size_t len;

	ck_err(NEM_msghdr_pack(&hdr_in, &bs, &len));
	ck_err(NEM_msghdr_new(&hdr_out, bs, len));
	free(bs);

	ck_assert_ptr_ne(NULL, hdr_out);
	ck_assert_ptr_ne(NULL, hdr_out->err);
	ck_assert_int_eq(42, hdr_out->err->code);
	ck_assert_str_eq("poop", hdr_out->err->reason);
	ck_assert_ptr_eq(NULL, hdr_out->route);
	NEM_msghdr_free(hdr_out);
}
END_TEST

START_TEST(roundtrip_route)
{
	NEM_msghdr_route_t hdr_route = {
		.cluster = "nem.rocks",
		.host    = "sawhorse",
		.inst    = "www",
		.obj     = "123",
	};
	NEM_msghdr_t hdr_in = {
		.route = &hdr_route,
	};
	NEM_msghdr_t *hdr_out = NULL;
	void *bs;
	size_t len;

	ck_err(NEM_msghdr_pack(&hdr_in, &bs, &len));
	ck_err(NEM_msghdr_new(&hdr_out, bs, len));
	free(bs);

	ck_assert_ptr_ne(NULL, hdr_out);
	ck_assert_ptr_ne(NULL, hdr_out->route);
	ck_assert_str_eq("123", hdr_out->route->obj);
	ck_assert_str_eq("nem.rocks", hdr_out->route->cluster);
	ck_assert_str_eq("sawhorse", hdr_out->route->host);
	ck_assert_str_eq("www", hdr_out->route->inst);
	ck_assert_ptr_eq(NULL, hdr_out->err);
	NEM_msghdr_free(hdr_out);
}
END_TEST

Suite*
suite_msghdr()
{
	tcase_t tests[] = {
		{ "roundtrip_empty", &roundtrip_empty },
		{ "roundtrip_err",   &roundtrip_err   },
		{ "roundtrip_route", &roundtrip_route },
	};

	return tcase_build_suite("msghdr", tests, sizeof(tests));
}
