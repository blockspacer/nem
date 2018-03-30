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

#define MARSHAL_VISITOR(ty) \
	START_TEST(marshal_json_empty_##ty) { \
		test_marshal_json_empty(&ty##_m); \
	} END_TEST \
	START_TEST(marshal_json_init_##ty) { \
		test_marshal_json_init(&ty##_m, &ty##_init); \
	} END_TEST

	MARSHAL_VISIT_TYPES_NOBIN
#undef MARSHAL_VISITOR

Suite*
suite_marshal_json()
{
	tcase_t tests[] = {
#		define MARSHAL_VISITOR(ty) \
		{ "marshal_json_empty_" #ty, &marshal_json_empty_##ty }, \
		{ "marshal_json_init_" #ty,  &marshal_json_init_##ty  }, 

		MARSHAL_VISIT_TYPES_NOBIN
#		undef MARSHAL_VISITOR
	};

	return tcase_build_suite("marshal-json", tests, sizeof(tests));
}
