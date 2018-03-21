#include "test.h"
#include "svcmgr.h"

START_TEST(init_free)
{
	NEM_rootd_svcmgr_t mgr;
	NEM_rootd_svcmgr_init(&mgr);
	NEM_rootd_svcmgr_free(&mgr);
}
END_TEST

START_TEST(add_svcs)
{
	NEM_rootd_svcmgr_t mgr;
	NEM_rootd_svcmgr_init(&mgr);
	ck_err(NEM_rootd_svcmgr_add(&mgr, 1, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcmgr_add(&mgr, 1, 2, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcmgr_add(&mgr, 2, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcmgr_add(&mgr, 2, 2, NEM_thunk_new(NULL, 0)));
	NEM_err_t err = NEM_rootd_svcmgr_add(&mgr, 1, 1, NEM_thunk_new(NULL, 0));
	ck_assert(!NEM_err_ok(err));
	NEM_rootd_svcmgr_free(&mgr);
}
END_TEST

START_TEST(dispatch_empty)
{
	NEM_msg_t *msg = NEM_msg_alloc(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 2;

	NEM_rootd_svcmgr_t mgr;
	NEM_rootd_svcmgr_init(&mgr);
	ck_assert(!NEM_rootd_svcmgr_dispatch(&mgr, msg));

	NEM_rootd_svcmgr_t sub;
	NEM_rootd_svcmgr_init(&sub);
	NEM_rootd_svcmgr_set_next(&mgr, &sub);
	ck_assert(!NEM_rootd_svcmgr_dispatch(&mgr, msg));

	NEM_rootd_svcmgr_free(&sub);
	NEM_rootd_svcmgr_free(&mgr);
	NEM_msg_free(msg);
}
END_TEST

static void
set_ptr(NEM_thunk_t *thunk, void *varg)
{
	int *ptr = NEM_thunk_ptr(thunk);
	NEM_rootd_cmd_ca *ca = varg;
	*ptr = (int)ca->msg->packed.seq;
}

START_TEST(dispatch_self)
{
	int val = 0;

	NEM_msg_t *msg = NEM_msg_alloc(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 2;
	msg->packed.seq = 1;

	NEM_rootd_svcmgr_t mgr;
	NEM_rootd_svcmgr_init(&mgr);

	ck_err(NEM_rootd_svcmgr_add(&mgr, 1, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcmgr_add(&mgr, 1, 2, NEM_thunk_new_ptr(&set_ptr, &val)));
	ck_err(NEM_rootd_svcmgr_add(&mgr, 2, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcmgr_add(&mgr, 2, 2, NEM_thunk_new(NULL, 0)));

	ck_assert(NEM_rootd_svcmgr_dispatch(&mgr, msg));
	ck_assert_int_eq(1, val);

	NEM_rootd_svcmgr_free(&mgr);
	NEM_msg_free(msg);
}
END_TEST

START_TEST(dispatch_child)
{
	int val = 0;

	NEM_msg_t *msg = NEM_msg_alloc(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 2;
	msg->packed.seq = 10;

	NEM_rootd_svcmgr_t mgr;
	NEM_rootd_svcmgr_init(&mgr);
	ck_err(NEM_rootd_svcmgr_add(&mgr, 1, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcmgr_add(&mgr, 2, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcmgr_add(&mgr, 2, 2, NEM_thunk_new(NULL, 0)));

	NEM_rootd_svcmgr_t sub;
	NEM_rootd_svcmgr_init(&sub);
	ck_err(NEM_rootd_svcmgr_add(&sub, 1, 2, NEM_thunk_new_ptr(&set_ptr, &val)));
	NEM_rootd_svcmgr_set_next(&mgr, &sub);

	ck_assert(NEM_rootd_svcmgr_dispatch(&mgr, msg));
	ck_assert_int_eq(10, val);

	NEM_rootd_svcmgr_free(&sub);
	NEM_rootd_svcmgr_free(&mgr);
	NEM_msg_free(msg);
}
END_TEST

Suite*
suite_svcmgr()
{
	tcase_t tests[] = {
		{ "init_free",      &init_free      },
		{ "add_svcs",       &add_svcs       },
		{ "dispatch_empty", &dispatch_empty },
		{ "dispatch_self",  &dispatch_self  },
		{ "dispatch_child", &dispatch_child },
	};

	return tcase_build_suite("svcmgr", tests, sizeof(tests));
}
