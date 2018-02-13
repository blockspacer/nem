#include "test.h"
#include "nem.h"

START_TEST(alloc)
{
	NEM_msg_t *msg = NEM_msg_alloc(4, 12);
	ck_assert_ptr_ne(msg, NULL);
	ck_assert_ptr_ne(msg->body, NULL);
	ck_assert_ptr_ne(msg->header, NULL);
	NEM_msg_free(msg);
}
END_TEST

START_TEST(alloc_empty)
{
	NEM_msg_t *msg = NEM_msg_alloc(0, 0);
	ck_assert_ptr_ne(msg, NULL);
	ck_assert_ptr_eq(msg->body, NULL);
	ck_assert_ptr_eq(msg->header, NULL);
	NEM_msg_free(msg);
}
END_TEST

Suite*
suite_msg()
{
	tcase_t tests[] = {
		{ "alloc",       &alloc       },
		{ "alloc_empty", &alloc_empty },
	};

	return tcase_build_suite("msg", tests, sizeof(tests));
}
