#include "test.h"
#include "test-marshal.h"
#include "nem-marshal-macros.h"

static const char
	*marshal_prims_yaml =
		"u8: 8\n"
		"u16: 16\n"
		"u32: 32\n"
		"u64: 64\n"
		"i8: -8\n"
		"i16: -16\n"
		"i32: -32\n"
		"i64: -64\n"
		"b: y\n",
	*marshal_ptrs_yaml =
		"i64: 42\n"
		"s3: hello\n"
		"o2:\n"
		"  i: 56\n"
		"  s: world\n",
	*marshal_prims_ary_yaml =
		"u8s: []\n"
		"i64s:\n"
		"- 42\n"
		"- 56\n"
		"- 2352\n"
		"ss:\n"
		"- hello\n"
		"- \n"
		"- world\n"
		"- \n",
	*marshal_strs_yaml =
		"s1: hello\n",
	*marshal_obj_yaml =
		"prim:\n"
		"  u8: 8\n"
		"  u16: 16\n"
		"  u32: 32\n"
		"  u64: 64\n"
		"  i8: -8\n"
		"  i16: -16\n"
		"  i32: -32\n"
		"  i64: -64\n"
		"  b: y\n"
		"strs:\n"
		"  s1: hello\n",
	*marshal_aryobj_yaml =
		"objs:\n"
		"- i: 42\n"
		"  s: hello\n"
		"- i: 56\n"
		"  s: world\n";

static void
test_marshal_yaml(
	const NEM_marshal_map_t *map,
	const char              *yaml,
	marshal_init_fn          init_fn,
	marshal_cmp_fn           cmp_fn
) {
	void *bs1 = NEM_malloc(map->elem_size);
	void *bs2 = NULL;
	size_t bs2_len = 0;

	init_fn(bs1);
	ck_err(NEM_marshal_yaml(map, &bs2, &bs2_len, bs1, map->elem_size));
	ck_assert_str_eq(yaml, bs2);
	NEM_unmarshal_free(map, bs1, map->elem_size);
	free(bs2);
	free(bs1);
}

typedef struct {
	char **strings;
	size_t strings_len;
}
marshal_null_string_t;

#define TYPE marshal_null_string_t
static const NEM_marshal_field_t marshal_null_string_fs[] = {
	{
		"strings",
		NEM_MARSHAL_ARRAY|NEM_MARSHAL_STRING,
		O(strings), O(strings_len),
   	}
};
static MAP(marshal_null_string_m, marshal_null_string_fs);
#undef TYPE

START_TEST(marshal_null_string)
{
	char *strings[] = { "", NULL };
	marshal_null_string_t s = {
		.strings     = strings,
		.strings_len = NEM_ARRSIZE(strings),
	};
	void *bs;
	size_t bs_len;

	ck_err(NEM_marshal_yaml(
		&marshal_null_string_m, &bs, &bs_len, &s, sizeof(s)
	));
	ck_assert_ptr_ne(NULL, bs);
	ck_assert_int_ne(0, bs_len);
	ck_assert_str_eq("strings:\n- \n- \n", bs);
	free(bs);
}
END_TEST

#define MARSHAL_VISITOR(TY) \
	START_TEST(yaml_unmarshal_##TY) { \
		test_marshal_yaml( \
			&TY##_m, \
			TY##_yaml, \
			&TY##_init, \
			&TY##_cmp \
		); \
	} END_TEST

	MARSHAL_VISIT_TYPES_NOBIN
#undef MARSHAL_VISITOR

Suite*
suite_marshal_yaml()
{
	tcase_t tests[] = {
		{ "marshal_null_string", &marshal_null_string },
#		define MARSHAL_VISITOR(TY) \
		{ "yaml_unmarshal_" #TY, &yaml_unmarshal_##TY },

		MARSHAL_VISIT_TYPES_NOBIN
#		undef MARSHAL_VISITOR
	};

	return tcase_build_suite("marshal-yaml", tests, sizeof(tests));
}

