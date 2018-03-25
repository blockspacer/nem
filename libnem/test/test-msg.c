#include "test.h"
#include "nem.h"

START_TEST(alloc)
{
	NEM_msg_t *msg = NEM_msg_new(4, 12);
	ck_assert_ptr_ne(msg, NULL);
	ck_assert_ptr_ne(msg->body, NULL);
	ck_assert_ptr_ne(msg->header, NULL);
	NEM_msg_free(msg);
}
END_TEST

START_TEST(alloc_empty)
{
	NEM_msg_t *msg = NEM_msg_new(0, 0);
	ck_assert_ptr_ne(msg, NULL);
	ck_assert_ptr_eq(msg->body, NULL);
	ck_assert_ptr_eq(msg->header, NULL);
	NEM_msg_free(msg);
}
END_TEST

START_TEST(set_header)
{
	NEM_msg_t *msg = NEM_msg_new(0, 0);
	NEM_msghdr_t hdr = {0};
	NEM_msg_set_header(msg, &hdr);
	ck_assert_ptr_ne(msg->header, NULL);
	ck_assert_int_lt(0, msg->packed.header_len);
	NEM_msg_free(msg);
}
END_TEST

Suite*
suite_msg()
{
	tcase_t tests[] = {
		{ "alloc",       &alloc       },
		{ "alloc_empty", &alloc_empty },
		{ "set_header",  &set_header  },
	};

	return tcase_build_suite("msg", tests, sizeof(tests));
}
