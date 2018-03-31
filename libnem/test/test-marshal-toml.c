#include "test.h"
#include "test-marshal.h"

static const char
	*marshal_prims_toml =
		"u8 = 8\n"
		"u16 = 16\n"
		"u32 = 32\n"
		"u64 = 64\n"
		"i8 = -8\n"
		"i16 = -16\n"
		"i32 = -32\n"
		"i64 = -64\n"
		"b = true\n",
	*marshal_ptrs_toml =
		"i64 = 42\n"
		"s3 = 'hello'\n"
		"[o2]\n"
		"i = 56\n"
		"s = 'world'\n",
	*marshal_prims_ary_toml =
		"i64s = [42, 56, 2352]\n"
		"ss = ['hello', '', 'world', '']\n",
	*marshal_strs_toml =
		"s1 = 'hello'\n"
		"s2 = ''\n",
	*marshal_obj_toml =
		"[prim]\n"
		"u8 = 8\n"
		"u16 = 16\n"
		"u32 = 32\n"
		"u64 = 64\n"
		"i8 = -8\n"
		"i16 = -16\n"
		"i32 = -32\n"
		"i64 = -64\n"
		"b = true\n"
		"[strs]\n"
		"s1 = 'hello'\n"
		"s2 = ''\n",
	*marshal_aryobj_toml =
		"[[objs]]\n"
		"i = 42\n"
		"s = 'hello'\n"
		"[[objs]]\n"
		"i = 56\n"
		"s = 'world'\n";

static void
test_marshal_toml_rt(
	const NEM_marshal_map_t *map,
	const char              *toml,
	marshal_init_fn          init_fn,
	marshal_cmp_fn           cmp_fn 
) {
	void *bs1 = NEM_malloc(map->elem_size);
	void *bs2 = NEM_malloc(map->elem_size);

	init_fn(bs1);
	init_fn(bs2);

	// NB: toml doesn't support null strings in arrays, so just make the
	// expected values be empty strings.
	if (map == &marshal_prims_ary_m) {
		marshal_prims_ary_t *this = bs2;
		ck_assert_ptr_eq(NULL, this->ss[1]);
		this->ss[1] = strdup("");
	}

	ck_err(NEM_unmarshal_toml(map, bs1, map->elem_size, toml, strlen(toml)));
	cmp_fn(bs1, bs2);

	NEM_unmarshal_free(map, bs1, map->elem_size);
	NEM_unmarshal_free(map, bs2, map->elem_size);
	free(bs1);
	free(bs2);
}

#define MARSHAL_VISITOR(TY) \
	START_TEST(toml_rt_##TY) { \
		test_marshal_toml_rt( \
			&TY##_m, \
			TY##_toml, \
			&TY##_init, \
			&TY##_cmp \
		); \
	} END_TEST

	MARSHAL_VISIT_TYPES_NOBIN
#undef MARSHAL_VISITOR


Suite*
suite_marshal_toml()
{
	tcase_t tests[] = {
#		define MARSHAL_VISITOR(TY) \
		{ "toml_rt_" #TY, &toml_rt_##TY },

		MARSHAL_VISIT_TYPES_NOBIN
#		undef MARSHAL_VISITOR
	};

	return tcase_build_suite("marshal-toml", tests, sizeof(tests));
}
