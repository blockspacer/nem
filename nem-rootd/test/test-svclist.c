#include "test.h"
#include "svclist.h"

START_TEST(init_free)
{
	NEM_rootd_svclist_t list;
	NEM_rootd_svclist_init(&list);
	NEM_rootd_svclist_free(&list);
}
END_TEST

static void
add_one(NEM_thunk_t *thunk, void *varg)
{
	int *val = NEM_thunk_ptr(thunk);
	*val += 1;
}

START_TEST(dispatch_one)
{
	int ctr = 0;

	NEM_rootd_svcdef_t def;
	NEM_rootd_svcdef_init(&def);
	ck_err(NEM_rootd_svcdef_add(
		&def,
		1,
		1,
		NEM_thunk_new_ptr(&add_one, &ctr)
	));

	NEM_rootd_svclist_t list;
	NEM_rootd_svclist_init(&list);
	NEM_rootd_svclist_add(&list, &def);

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 1;

	ck_assert(NEM_rootd_svclist_dispatch(&list, msg, NULL));
	ck_assert_int_eq(1, ctr);

	NEM_msg_free(msg);
	NEM_rootd_svclist_free(&list);
	NEM_rootd_svcdef_free(&def);
}
END_TEST

START_TEST(dispatch_nested)
{
	int ctr = 0;

	NEM_rootd_svcdef_t def1, def2;
	NEM_rootd_svcdef_init(&def1);
	NEM_rootd_svcdef_init(&def2);
	ck_err(NEM_rootd_svcdef_add(
		&def2,
		1,
		2,
		NEM_thunk_new_ptr(&add_one, &ctr)
	));

	NEM_rootd_svclist_t list;
	NEM_rootd_svclist_init(&list);
	NEM_rootd_svclist_add(&list, &def1);
	NEM_rootd_svclist_add(&list, &def2);

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 2;

	ck_assert(NEM_rootd_svclist_dispatch(&list, msg, NULL));
	ck_assert_int_eq(1, ctr);

	NEM_msg_free(msg);
	NEM_rootd_svclist_free(&list);
	NEM_rootd_svcdef_free(&def2);
	NEM_rootd_svcdef_free(&def1);
}
END_TEST

Suite*
suite_svclist()
{
	tcase_t tests[] = {
		{ "init_free",       &init_free       },
		{ "dispatch_one",    &dispatch_one    },
		{ "dispatch_nested", &dispatch_nested },
	};

	return tcase_build_suite("svclist", tests, sizeof(tests));
}
