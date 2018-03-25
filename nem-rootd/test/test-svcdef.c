#include "test.h"
#include "svcdef.h"

START_TEST(init_free)
{
	NEM_rootd_svcdef_t def;
	NEM_rootd_svcdef_init(&def);
	NEM_rootd_svcdef_free(&def);
}
END_TEST

START_TEST(add_svcs)
{
	NEM_rootd_svcdef_t def;
	NEM_rootd_svcdef_init(&def);
	ck_err(NEM_rootd_svcdef_add(&def, 1, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcdef_add(&def, 1, 2, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcdef_add(&def, 2, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcdef_add(&def, 2, 2, NEM_thunk_new(NULL, 0)));
	NEM_err_t err = NEM_rootd_svcdef_add(&def, 1, 1, NEM_thunk_new(NULL, 0));
	ck_assert(!NEM_err_ok(err));
	NEM_rootd_svcdef_free(&def);
}
END_TEST

START_TEST(dispatch_empty)
{
	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 2;

	NEM_rootd_svcdef_t def;
	NEM_rootd_svcdef_init(&def);
	ck_assert(!NEM_rootd_svcdef_dispatch(&def, msg, NULL));

	NEM_rootd_svcdef_free(&def);
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

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 2;
	msg->packed.seq = 1;

	NEM_rootd_svcdef_t def;
	NEM_rootd_svcdef_init(&def);

	ck_err(NEM_rootd_svcdef_add(&def, 1, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcdef_add(&def, 1, 2, NEM_thunk_new_ptr(&set_ptr, &val)));
	ck_err(NEM_rootd_svcdef_add(&def, 2, 1, NEM_thunk_new(NULL, 0)));
	ck_err(NEM_rootd_svcdef_add(&def, 2, 2, NEM_thunk_new(NULL, 0)));

	ck_assert(NEM_rootd_svcdef_dispatch(&def, msg, NULL));
	ck_assert_int_eq(1, val);

	NEM_rootd_svcdef_free(&def);
	NEM_msg_free(msg);
}
END_TEST

static void
dispatch_data_cb(NEM_thunk_t *thunk, void *varg)
{
	NEM_rootd_cmd_ca *ca = varg;
	*ca->handled = false;

	int *val = NEM_thunk_ptr(thunk);
	*val = 10;

	int *data = (int*) ca->data;
	ck_assert_int_eq(42, *data);
}

START_TEST(dispatch_data)
{
	int val = 1;

	NEM_msg_t *msg = NEM_msg_new(0, 0);
	msg->packed.service_id = 1;
	msg->packed.command_id = 1;
	msg->packed.seq = 10;

	NEM_rootd_svcdef_t def;
	NEM_rootd_svcdef_init(&def);
	ck_err(NEM_rootd_svcdef_add(&def, 1, 1, NEM_thunk_new_ptr(
		&dispatch_data_cb, &val
	)));

	int val2 = 42;

	ck_assert(!NEM_rootd_svcdef_dispatch(&def, msg, &val2));
	ck_assert_int_eq(10, val);

	NEM_rootd_svcdef_free(&def);
	NEM_msg_free(msg);
}
END_TEST

Suite*
suite_svcdef()
{
	tcase_t tests[] = {
		{ "init_free",      &init_free      },
		{ "add_svcs",       &add_svcs       },
		{ "dispatch_empty", &dispatch_empty },
		{ "dispatch_self",  &dispatch_self  },
		{ "dispatch_data",  &dispatch_data  },
	};

	return tcase_build_suite("svcdef", tests, sizeof(tests));
}
