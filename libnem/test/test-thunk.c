#include "test.h"
#include "nem.h"

static const char *teststr = "hello world";

static void
add1(NEM_thunk1_t *thunk, void *data)
{
	int *i = NEM_thunk1_ptr(thunk);
	*i += 1;
}

static void
add1ctx(NEM_thunk1_t *thunk, void *data)
{
	int *i = (int*) data;
	*i += 1;
}

static void
checkstr(NEM_thunk1_t *thunk, void *data)
{
	ck_assert_str_eq((char*) &thunk->data, teststr);
}

static void
checkctx(NEM_thunk1_t *thunk, void *data)
{
	ck_assert_ptr_eq(NEM_thunk1_ptr(thunk), data);
}

START_TEST(discard)
{
	NEM_thunk1_t *thunk;
	
	thunk = NEM_thunk1_new(&add1, 0);
	NEM_thunk1_discard(&thunk);
	ck_assert_ptr_eq(thunk, NULL);

	thunk = NEM_thunk1_new(&add1, 10);
	NEM_thunk1_discard(&thunk);
	ck_assert_ptr_eq(thunk, NULL);
}
END_TEST

START_TEST(invoke)
{
	int i = 4;

	NEM_thunk1_t *thunk = NEM_thunk1_new(&add1, sizeof(int*));
	*(int**) thunk->data = &i;
	NEM_thunk1_invoke(&thunk, NULL);

	ck_assert_ptr_eq(thunk, NULL);
	ck_assert_int_eq(i, 5);
}
END_TEST

START_TEST(invoke_ctx)
{
	int i = 3;

	NEM_thunk1_t *thunk = NEM_thunk1_new(&add1ctx, 0);
	NEM_thunk1_invoke(&thunk, &i);

	ck_assert_ptr_eq(thunk, NULL);
	ck_assert_int_eq(i, 4);
}
END_TEST

START_TEST(invoke_chunk)
{
	NEM_thunk1_t *thunk = NEM_thunk1_new(&checkstr, strlen(teststr) + 1);
	strcpy((char*) &thunk->data, teststr);
	NEM_thunk1_invoke(&thunk, NULL);

	ck_assert_ptr_eq(thunk, NULL);
}
END_TEST

START_TEST(new_ptr)
{
	int i = 5;

	NEM_thunk1_t *thunk = NEM_thunk1_new_ptr(&add1, &i);
	NEM_thunk1_invoke(&thunk, NULL);

	ck_assert_ptr_eq(thunk, NULL);
	ck_assert_int_eq(i, 6);
}
END_TEST

START_TEST(ptr)
{
	int i = 5;

	NEM_thunk1_t *thunk = NEM_thunk1_new_ptr(&checkctx, &i);
	NEM_thunk1_invoke(&thunk, &i);

	ck_assert_ptr_eq(thunk, NULL);
}
END_TEST

Suite*
suite_thunk()
{
	tcase_t tests[] = {
		{ "discard",      &discard      },
		{ "invoke",       &invoke       },
		{ "invoke_ctx",   &invoke_ctx   },
		{ "invoke_chunk", &invoke_chunk },
		{ "new_ptr",      &new_ptr      },
		{ "ptr",          &ptr          },
	};

	return tcase_build_suite("thunk", tests, sizeof(tests));
}
