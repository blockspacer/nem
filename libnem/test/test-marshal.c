#include "test.h"
#include "test-marshal.h"

static void
test_free_empty(const NEM_marshal_map_t *map)
{
	void *bs = NEM_malloc(map->elem_size);
	NEM_unmarshal_free(map, bs, map->elem_size);
	free(bs);
}

static void
test_init_free(const NEM_marshal_map_t *map, marshal_init_fn fn)
{
	void *bs = NEM_malloc(map->elem_size);
	fn(bs);
	NEM_unmarshal_free(map, bs, map->elem_size);
	free(bs);
}

static void
test_cmp(
	const NEM_marshal_map_t *map,
	marshal_init_fn          init_fn, 
	marshal_cmp_fn           cmp_fn
) {
	void *bs1 = NEM_malloc(map->elem_size);
	void *bs2 = NEM_malloc(map->elem_size);
	init_fn(bs1);
	init_fn(bs2);
	cmp_fn(bs1, bs2);
	NEM_unmarshal_free(map, bs2, map->elem_size);
	NEM_unmarshal_free(map, bs1, map->elem_size);
	free(bs2);
	free(bs1);
}

#define MARSHAL_VISITOR(ty) \
	START_TEST(free_empty_##ty) { test_free_empty(&ty##_m); } END_TEST \
	START_TEST(init_free_##ty) { \
		test_init_free(&ty##_m, &ty##_init); \
	} END_TEST \
	START_TEST(cmp_##ty) { \
		test_cmp(&ty##_m, &ty##_init, &ty##_cmp); \
	} END_TEST

	MARSHAL_VISIT_TYPES

#undef MARSHAL_VISITOR

Suite*
suite_marshal()
{
	tcase_t tests[] = {
#		define MARSHAL_VISITOR(ty) \
		{ "free_empty_" #ty, &free_empty_##ty }, \
		{ "init_free_" #ty,  &init_free_##ty  }, \
		{ "cmp_" #ty,        &cmp_##ty        },

		MARSHAL_VISIT_TYPES

#		undef MARSHAL_VISITOR
	};

	return tcase_build_suite("marshal", tests, sizeof(tests));
}
