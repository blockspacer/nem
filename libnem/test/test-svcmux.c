#include "test.h"

START_TEST(init_unref)
{
	NEM_svcmux_t mux;
	NEM_svcmux_init(&mux);
	NEM_svcmux_unref(&mux);
}
END_TEST

START_TEST(add_resolve)
{
	NEM_thunk_t *thunks[] = {
		NEM_thunk_new(NULL, 0),
		NEM_thunk_new(NULL, 0),
		NEM_thunk_new(NULL, 0),
	};
	NEM_svcmux_entry_t entries[] = {
		{ 1, 1, thunks[0] },
		{ 1, 2, thunks[1] },
		{ 2, 1, thunks[2] },
	};

	NEM_svcmux_t mux;
	NEM_svcmux_init(&mux);
	NEM_svcmux_add_handlers(&mux, entries, NEM_ARRSIZE(entries));
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 1, 1), thunks[0]);
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 1, 2), thunks[1]);
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 2, 1), thunks[2]);
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 2, 2), NULL);
	NEM_svcmux_unref(&mux);
}
END_TEST

START_TEST(override)
{
	NEM_thunk_t *thunks[] = {
		NEM_thunk_new(NULL, 0),
		NEM_thunk_new(NULL, 0),
		NEM_thunk_new(NULL, 0),
	};
	NEM_svcmux_entry_t entries1[] = {
		{ 1, 1, thunks[0] },
		{ 2, 1, thunks[1] },
	};
	NEM_svcmux_entry_t entries2[] = {
		{ 2, 1, thunks[2] },
	};

	NEM_svcmux_t mux;
	NEM_svcmux_init(&mux);
	NEM_svcmux_add_handlers(&mux, entries1, NEM_ARRSIZE(entries1));
	NEM_svcmux_add_handlers(&mux, entries2, NEM_ARRSIZE(entries2));
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 1, 1), thunks[0]);
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 2, 1), thunks[2]);
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 2, 2), NULL);
	NEM_svcmux_unref(&mux);
}
END_TEST

START_TEST(override_null)
{
	NEM_thunk_t *thunks[] = {
		NEM_thunk_new(NULL, 0),
		NEM_thunk_new(NULL, 0),
	};
	NEM_svcmux_entry_t entries1[] = {
		{ 1, 1, thunks[0] },
		{ 1, 2, thunks[1] },
	};
	NEM_svcmux_entry_t entries2[] = {
		{ 1, 2, NULL },
	};

	NEM_svcmux_t mux;
	NEM_svcmux_init(&mux);
	NEM_svcmux_add_handlers(&mux, entries1, NEM_ARRSIZE(entries1));
	NEM_svcmux_add_handlers(&mux, entries2, NEM_ARRSIZE(entries2));
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 1, 1), thunks[0]);
	ck_assert_ptr_eq(NEM_svcmux_resolve(&mux, 1, 2), NULL);
	NEM_svcmux_unref(&mux);
}
END_TEST

START_TEST(ref)
{
	NEM_svcmux_entry_t entries[] = {
		{ 1, 1, NEM_thunk_new(NULL, 0) },
	};

	NEM_svcmux_t mux;
	NEM_svcmux_init(&mux);
	NEM_svcmux_add_handlers(&mux, entries, NEM_ARRSIZE(entries));
	ck_assert_ptr_eq(&mux, NEM_svcmux_ref(&mux));
	NEM_svcmux_unref(&mux);
	NEM_svcmux_unref(&mux);
}
END_TEST

START_TEST(ret_default)
{
	NEM_thunk_t *thunks[] = {
		NEM_thunk_new(NULL, 0),
		NEM_thunk_new(NULL, 0),
	};
	NEM_svcmux_entry_t entries[] = {
		{ 1, 1, thunks[0] },
	};

	NEM_svcmux_t mux;
	NEM_svcmux_init(&mux);
	NEM_svcmux_set_default(&mux, thunks[1]);
	ck_assert_ptr_eq(thunks[1], NEM_svcmux_resolve(&mux, 1, 1));
	NEM_svcmux_add_handlers(&mux, entries, NEM_ARRSIZE(entries));
	ck_assert_ptr_eq(thunks[0], NEM_svcmux_resolve(&mux, 1, 1));
	ck_assert_ptr_eq(thunks[1], NEM_svcmux_resolve(&mux, 1, 2));
	NEM_svcmux_set_default(&mux, NEM_thunk_new(NULL, 0));
	NEM_svcmux_unref(&mux);
}
END_TEST

Suite*
suite_svcmux()
{
	tcase_t tests[] = {
		{ "init_unref",    &init_unref    },
		{ "add_resolve",   &add_resolve   },
		{ "override",      &override      },
		{ "override_null", &override_null },
		{ "ref",           &ref           },
		{ "ret_default",   &ret_default   },
	};

	return tcase_build_suite("svcmux", tests, sizeof(tests));
}
