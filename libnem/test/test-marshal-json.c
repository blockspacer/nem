#include "test.h"
#include "test-marshal.h"

static void
test_marshal_json_empty(const NEM_marshal_map_t *map)
{
	void *bs = NEM_malloc(map->elem_size);
	void *json = NULL;
	size_t len = 0;
	ck_err(NEM_marshal_json(map, &json, &len, bs, map->elem_size));
	ck_assert_ptr_ne(json, NULL);
	ck_assert_int_ne(len, 0);
	free(json);
	NEM_unmarshal_free(map, bs, map->elem_size);
	free(bs);
}

static void
test_marshal_json_init(const NEM_marshal_map_t *map, marshal_init_fn fn)
{
	void *bs = NEM_malloc(map->elem_size);
	fn(bs);

	void *json = NULL;
	size_t len = 0;
	ck_err(NEM_marshal_json(map, &json, &len, bs, map->elem_size));
	ck_assert_ptr_ne(json, NULL);
	ck_assert_int_ne(len, 0);
	free(json);
	NEM_unmarshal_free(map, bs, map->elem_size);
	free(bs);
}

static void
test_json_rt_empty(
	const NEM_marshal_map_t *map,
	marshal_cmp_fn           cmp_fn
) {
	void *bs_in = NEM_malloc(map->elem_size);
	void *bs_out = NEM_malloc(map->elem_size);

	void *json = NULL;
	size_t len = 0;
	ck_err(NEM_marshal_json(map, &json, &len, bs_in, map->elem_size));
	ck_err(NEM_unmarshal_json(map, bs_out, map->elem_size, json, len));
	free(json);

	cmp_fn(bs_in, bs_out);
	NEM_unmarshal_free(map, bs_in, map->elem_size);
	NEM_unmarshal_free(map, bs_out, map->elem_size);
	free(bs_in);
	free(bs_out);
}

static void
test_json_rt_init(
	const NEM_marshal_map_t *map,
	marshal_cmp_fn           cmp_fn,
	marshal_init_fn          init_fn
) {
	void *bs_in = NEM_malloc(map->elem_size);
	init_fn(bs_in);

	void *bs_out = NEM_malloc(map->elem_size);

	void *json = NULL;
	size_t len = 0;
	ck_err(NEM_marshal_json(map, &json, &len, bs_in, map->elem_size));
	ck_err(NEM_unmarshal_json(map, bs_out, map->elem_size, json, len));
	free(json);

	cmp_fn(bs_in, bs_out);
	NEM_unmarshal_free(map, bs_in, map->elem_size);
	NEM_unmarshal_free(map, bs_out, map->elem_size);
	free(bs_in);
	free(bs_out);
}

#define MARSHAL_VISITOR(TY) \
	START_TEST(marshal_json_empty_##TY) { \
		test_marshal_json_empty(&TY##_m); \
	} END_TEST \
	START_TEST(marshal_json_init_##TY) { \
		test_marshal_json_init(&TY##_m, &TY##_init); \
	} END_TEST \
	START_TEST(json_rt_empty_##TY) { \
		test_json_rt_empty(&TY##_m, &TY##_cmp); \
	} END_TEST \
	START_TEST(json_rt_init_##TY) { \
		test_json_rt_init(&TY##_m, &TY##_cmp, &TY##_init); \
	} END_TEST

	MARSHAL_VISIT_TYPES_NOBIN
#undef MARSHAL_VISITOR

Suite*
suite_marshal_json()
{
	tcase_t tests[] = {
#		define MARSHAL_VISITOR(TY) \
		{ "marshal_json_empty_" #TY, &marshal_json_empty_##TY }, \
		{ "marshal_json_init_" #TY,  &marshal_json_init_##TY  }, \
		{ "json_rt_empty_" #TY,      &json_rt_empty_##TY      }, \
		{ "json_rt_init_" #TY,       &json_rt_init_##TY       },

		MARSHAL_VISIT_TYPES_NOBIN
#		undef MARSHAL_VISITOR
	};

	return tcase_build_suite("marshal-json", tests, sizeof(tests));
}
