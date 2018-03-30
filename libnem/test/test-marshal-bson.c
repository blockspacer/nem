#include "test.h"
#include "test-marshal.h"

static void
test_marshal_bson_empty(const NEM_marshal_map_t *map)
{
	void *bs = NEM_malloc(map->elem_size);
	void *bson = NULL;
	size_t len = 0;
	ck_err(NEM_marshal_bson(map, &bson, &len, bs, map->elem_size));
	ck_assert_ptr_ne(bson, NULL);
	ck_assert_int_ne(len, 0);
	free(bson);
	NEM_unmarshal_free(map, bs, map->elem_size);
	free(bs);
}

static void
test_marshal_bson_init(const NEM_marshal_map_t *map, marshal_init_fn fn)
{
	void *bs = NEM_malloc(map->elem_size);
	fn(bs);

	void *bson = NULL;
	size_t len = 0;
	ck_err(NEM_marshal_bson(map, &bson, &len, bs, map->elem_size));
	ck_assert_ptr_ne(bson, NULL);
	ck_assert_int_ne(len, 0);
	free(bson);
	NEM_unmarshal_free(map, bs, map->elem_size);
	free(bs);
}

static void
test_bson_rt_empty(
	const NEM_marshal_map_t *map,
	marshal_cmp_fn           cmp_fn
) {
	void *bs_in = NEM_malloc(map->elem_size);
	void *bs_out = NEM_malloc(map->elem_size);

	void *bson = NULL;
	size_t len = 0;
	ck_err(NEM_marshal_bson(map, &bson, &len, bs_in, map->elem_size));
	ck_err(NEM_unmarshal_bson(map, bs_out, map->elem_size, bson, len));
	free(bson);

	cmp_fn(bs_in, bs_out);
	NEM_unmarshal_free(map, bs_in, map->elem_size);
	NEM_unmarshal_free(map, bs_out, map->elem_size);
	free(bs_in);
	free(bs_out);
}

static void
test_bson_rt_init(
	const NEM_marshal_map_t *map,
	marshal_cmp_fn           cmp_fn,
	marshal_init_fn          init_fn
) {
	void *bs_in = NEM_malloc(map->elem_size);
	init_fn(bs_in);

	void *bs_out = NEM_malloc(map->elem_size);

	void *bson = NULL;
	size_t len = 0;
	ck_err(NEM_marshal_bson(map, &bson, &len, bs_in, map->elem_size));
	ck_err(NEM_unmarshal_bson(map, bs_out, map->elem_size, bson, len));
	free(bson);

	cmp_fn(bs_in, bs_out);
	NEM_unmarshal_free(map, bs_in, map->elem_size);
	NEM_unmarshal_free(map, bs_out, map->elem_size);
	free(bs_in);
	free(bs_out);
}

#define MARSHAL_VISITOR(TY) \
	START_TEST(marshal_bson_empty_##TY) { \
		test_marshal_bson_empty(&TY##_m); \
	} END_TEST \
	START_TEST(marshal_bson_init_##TY) { \
		test_marshal_bson_init(&TY##_m, &TY##_init); \
	} END_TEST \
	START_TEST(bson_rt_empty_##TY) { \
		test_bson_rt_empty(&TY##_m, &TY##_cmp); \
	} END_TEST \
	START_TEST(bson_rt_init_##TY) { \
		test_bson_rt_init(&TY##_m, &TY##_cmp, &TY##_init); \
	} END_TEST

	MARSHAL_VISIT_TYPES
#undef MARSHAL_VISITOR

Suite*
suite_marshal_bson()
{
	tcase_t tests[] = {
#		define MARSHAL_VISITOR(TY) \
		{ "marshal_bson_empty_" #TY, &marshal_bson_empty_##TY }, \
		{ "marshal_bson_init_" #TY,  &marshal_bson_init_##TY  }, \
		{ "bson_rt_empty_" #TY,      &bson_rt_empty_##TY      }, \
		{ "bson_rt_init_" #TY,       &bson_rt_init_##TY       },

		MARSHAL_VISIT_TYPES
#		undef MARSHAL_VISITOR
	};

	return tcase_build_suite("marshal-bson", tests, sizeof(tests));
}
