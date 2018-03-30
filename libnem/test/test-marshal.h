#pragma once

#define O(F) offsetof(TYPE, F)
#define M(F) NEM_MSIZE(TYPE, F)

/*
 * marshal_prims_t
 */

typedef struct {
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	bool b;
}
marshal_prims_t;

static void
marshal_prims_init(void *vthis)
{
	marshal_prims_t *this = vthis;

	this->u8 = 8;
	this->u16 = 16;
	this->u32 = 32;
	this->u64 = 64;
	this->i8 = -8;
	this->i16 = -16;
	this->i32 = -32;
	this->i64 = -64;
	this->b = true;
}

static void
marshal_prims_cmp(const void *vthis, const void *vthat)
{
	const marshal_prims_t *this = vthis;
	const marshal_prims_t *that = vthat;

	ck_assert_int_eq(this->u8, that->u8);
	ck_assert_int_eq(this->u16, that->u16);
	ck_assert_int_eq(this->u32, that->u32);
	ck_assert_int_eq(this->u64, that->u64);
	ck_assert_int_eq(this->i8, that->i8);
	ck_assert_int_eq(this->i16, that->i16);
	ck_assert_int_eq(this->i32, that->i32);
	ck_assert_int_eq(this->i64, that->i64);
	ck_assert_int_eq(this->b, that->b);
}

#define TYPE marshal_prims_t
static const NEM_marshal_field_t marshal_prims_fs[] = {
	{ "u8",  NEM_MARSHAL_UINT8,  O(u8),  -1, NULL },
	{ "u16", NEM_MARSHAL_UINT16, O(u16), -1, NULL },
	{ "u32", NEM_MARSHAL_UINT32, O(u32), -1, NULL },
	{ "u64", NEM_MARSHAL_UINT64, O(u64), -1, NULL },
	{ "i8",  NEM_MARSHAL_INT8,   O(i8),  -1, NULL },
	{ "i16", NEM_MARSHAL_INT16,  O(i16), -1, NULL },
	{ "i32", NEM_MARSHAL_INT32,  O(i32), -1, NULL },
	{ "i64", NEM_MARSHAL_INT64,  O(i64), -1, NULL },
	{ "b",   NEM_MARSHAL_BOOL,   O(b),   -1, NULL },
};
static const NEM_marshal_map_t marshal_prims_m = {
	.fields     = marshal_prims_fs,
	.fields_len = NEM_ARRSIZE(marshal_prims_fs),
	.elem_size  = sizeof(TYPE),
};
#undef TYPE

/*
 * marshal_prims_ary_t
 */

typedef struct {
	size_t u8_len;
	uint8_t *u8s;

	size_t i64_len;
	int64_t *i64s;

	size_t s_len;
	char **ss;
}
marshal_prims_ary_t;

static void
marshal_prims_ary_init(void *vthis)
{
	marshal_prims_ary_t *this = vthis;

	static const int64_t i64s[] = {
		42, 56, 42*56
	};

	static const char *ss[] = {
		"hello", NULL, "world", ""
	};

	this->u8_len = 0; 
	this->u8s = NULL;

	this->i64_len = NEM_ARRSIZE(i64s);
	this->i64s = NEM_malloc(sizeof(i64s));
	memcpy(this->i64s, i64s, sizeof(i64s));

	this->s_len = NEM_ARRSIZE(ss);
	this->ss = NEM_malloc(sizeof(ss));
	for (size_t i = 0; i < NEM_ARRSIZE(ss); i += 1) {
		if (NULL != ss[i]) {
			this->ss[i] = strdup(ss[i]);
		}
	}
}

static void
marshal_prims_ary_cmp(const void *vthis, const void *vthat)
{
	const marshal_prims_ary_t *this = vthis;
	const marshal_prims_ary_t *that = vthat;

	ck_assert_int_eq(this->u8_len, that->u8_len);
	ck_assert_int_eq(this->i64_len, that->i64_len);
	ck_assert_int_eq(this->s_len, that->s_len);

	if (NULL == this->u8s) {
		ck_assert_int_eq(0, that->u8_len);
		ck_assert_ptr_eq(NULL, that->u8s);
	}
	else {
		ck_assert_ptr_ne(this->u8s, that->u8s);
		for (size_t i = 0; i < this->u8_len; i += 1) {
			ck_assert_int_eq(this->u8s[i], that->u8s[i]);
		}
	}

	if (NULL == this->i64s) {
		ck_assert_int_eq(0, that->i64_len);
		ck_assert_ptr_eq(NULL, that->i64s);
	}
	else {
		ck_assert_ptr_ne(this->i64s, that->i64s);
		for (size_t i = 0; i < this->i64_len; i += 1) {
			ck_assert_int_eq(this->i64s[i], that->i64s[i]);
		}
	}

	if (NULL == this->ss) {
		ck_assert_int_eq(0, that->s_len);
		ck_assert_ptr_eq(NULL, that->ss);
	}
	else {
		ck_assert_ptr_ne(this->ss, that->ss);
		for (size_t i = 0; i < this->s_len; i += 1) {
			if (NULL == this->ss[i]) {
				ck_assert_ptr_eq(NULL, that->ss[i]);
			}
			else {
				ck_assert_ptr_ne(this->ss[i], that->ss[i]);
				ck_assert_str_eq(this->ss[i], that->ss[i]);
			}
		}
	}
}

#define TYPE marshal_prims_ary_t
static const NEM_marshal_field_t marshal_prims_ary_fs[] = {
	{ "u8s",  NEM_MARSHAL_ARRAY|NEM_MARSHAL_UINT8,  O(u8s),  O(u8_len),  NULL },
	{ "i64s", NEM_MARSHAL_ARRAY|NEM_MARSHAL_INT64,  O(i64s), O(i64_len), NULL },
	{ "ss",   NEM_MARSHAL_ARRAY|NEM_MARSHAL_STRING, O(ss),   O(s_len),   NULL }
};
static const NEM_marshal_map_t marshal_prims_ary_m = {
	.fields     = marshal_prims_ary_fs,
	.fields_len = NEM_ARRSIZE(marshal_prims_ary_fs),
	.elem_size  = sizeof(TYPE),
};
#undef TYPE

/*
 * marshal_strs_t
 */

typedef struct {
	const char *s1;
	const char *s2;
	const char *s3;
}
marshal_strs_t;

static void
marshal_strs_init(void *vthis)
{
	marshal_strs_t *this = vthis;

	this->s1 = strdup("hello");
	this->s2 = strdup("");
	this->s3 = NULL;
}

static void
marshal_strs_cmp(const void *vthis, const void *vthat)
{
	const marshal_strs_t *this = vthis;
	const marshal_strs_t *that = vthat;

	if (NULL == this->s1) {
		ck_assert_ptr_eq(this->s1, that->s1);
	}
	else {
		ck_assert_ptr_ne(this->s1, that->s1);
		ck_assert_str_eq(this->s1, that->s1);
	}

	if (NULL == this->s2) {
		ck_assert_ptr_eq(this->s2, that->s2);
	}
	else {
		ck_assert_ptr_ne(this->s2, that->s2);
		ck_assert_str_eq(this->s2, that->s2);
	}

	if (NULL == this->s3) {
		ck_assert_ptr_eq(this->s3, that->s3);
	}
	else {
		ck_assert_ptr_ne(this->s3, that->s3);
		ck_assert_str_eq(this->s3, that->s3);
	}
}

#define TYPE marshal_strs_t
static const NEM_marshal_field_t marshal_strs_fs[] = {
	{ "s1", NEM_MARSHAL_STRING, O(s1), -1, NULL },
	{ "s2", NEM_MARSHAL_STRING, O(s2), -1, NULL },
	{ "s3", NEM_MARSHAL_STRING, O(s3), -1, NULL },
};
static const NEM_marshal_map_t marshal_strs_m = {
	.fields     = marshal_strs_fs,
	.fields_len = NEM_ARRSIZE(marshal_strs_fs),
	.elem_size  = sizeof(TYPE),
};
#undef TYPE

/*
 * marshal_bin_t
 */

typedef struct {
	size_t blen;
	void  *b;
	char   f[16];
}
marshal_bin_t;

static void
marshal_bin_init(void *vthis)
{
	marshal_bin_t *this = vthis;

	this->blen = 5;
	this->b = NEM_malloc(this->blen);
	memcpy(this->b, "world", this->blen);
	memcpy(this->f, "something there", sizeof(this->f));
}

static void
marshal_bin_cmp(const void *vthis, const void *vthat)
{
	const marshal_bin_t *this = vthis;
	const marshal_bin_t *that = vthat;

	ck_assert_int_eq(this->blen, that->blen);
	if (NULL == this->b) {
		ck_assert_ptr_eq(this->b, that->b);
	}
	else {
		ck_assert_ptr_ne(this->b, that->b);
		ck_assert_mem_eq(this->b, that->b, this->blen);
	}

	ck_assert_mem_eq(this->f, that->f, sizeof(this->f));
}

#define TYPE marshal_bin_t
static const NEM_marshal_field_t marshal_bin_fs[] = {
	{ "b", NEM_MARSHAL_BINARY, O(b), O(blen), NULL },
	{ "f", NEM_MARSHAL_FIXLEN, O(f), M(f),    NULL },
};
static const NEM_marshal_map_t marshal_bin_m = {
	.fields     = marshal_bin_fs,
	.fields_len = NEM_ARRSIZE(marshal_bin_fs),
	.elem_size  = sizeof(TYPE),
};
#undef TYPE

/*
 * marshal_obj
 */

typedef struct {
	marshal_prims_t prim;
	marshal_strs_t  strs;
}
marshal_obj_t;

static void
marshal_obj_init(void *vthis)
{
	marshal_obj_t *this = vthis;
	marshal_prims_init(&this->prim);
	marshal_strs_init(&this->strs);
}

static void
marshal_obj_cmp(const void *vthis, const void *vthat)
{
	const marshal_obj_t *this = vthis;
	const marshal_obj_t *that = vthat;

	marshal_prims_cmp(&this->prim, &that->prim);
	marshal_strs_cmp(&this->strs, &that->strs);
}

#define TYPE marshal_obj_t
static const NEM_marshal_field_t marshal_obj_fs[] = {
	{ "prim", NEM_MARSHAL_STRUCT, O(prim), -1, &marshal_prims_m },
	{ "strs", NEM_MARSHAL_STRUCT, O(strs), -1, &marshal_strs_m  },
};
static const NEM_marshal_map_t marshal_obj_m = {
	.fields     = marshal_obj_fs,
	.fields_len = NEM_ARRSIZE(marshal_obj_fs),
	.elem_size  = sizeof(TYPE),
};
#undef TYPE

/*
 * marshal_aryobj
 */

typedef struct {
	int64_t     i;
	const char *s;
}
marshal_aryobj_sub_t;

typedef struct {
	size_t                len;
	marshal_aryobj_sub_t *objs;
}
marshal_aryobj_t;

static void
marshal_aryobj_sub_cmp(const void *vthis, const void *vthat)
{
	const marshal_aryobj_sub_t *this = vthis;
	const marshal_aryobj_sub_t *that = vthat;

	if (this == NULL) {
		ck_assert_ptr_eq(this, that);
	}
	else {
		ck_assert_ptr_ne(this, that);
		ck_assert_int_eq(this->i, that->i);

		if (this->s == NULL) {
			ck_assert_ptr_eq(this->s, that->s);
		}
		else {
			ck_assert_ptr_ne(this->s, that->s);
			ck_assert_str_eq(this->s, that->s);
		}
	}
}

static void
marshal_aryobj_init(void *vthis)
{
	marshal_aryobj_t *this = vthis;
	this->len = 2;
	this->objs = NEM_malloc(this->len * sizeof(marshal_aryobj_sub_t));
	this->objs[0].i = 42;
	this->objs[0].s = strdup("hello");
	this->objs[1].i = 56;
	this->objs[1].s = strdup("world");
}

static void
marshal_aryobj_cmp(const void *vthis, const void *vthat)
{
	const marshal_aryobj_t *this = vthis;
	const marshal_aryobj_t *that = vthat;

	ck_assert_int_eq(this->len, that->len);
	if (NULL == this->objs) {
		ck_assert_ptr_eq(this->objs, that->objs);
	}
	else {
		for (size_t i = 0; i < this->len; i += 1) {
			marshal_aryobj_sub_cmp(&this->objs[i], &that->objs[i]);
		}
	}
}

#define TYPE marshal_aryobj_sub_t
static const NEM_marshal_field_t marshal_aryobj_sub_fs[] = {
	{ "i", NEM_MARSHAL_INT64,  O(i), -1, NULL },
	{ "s", NEM_MARSHAL_STRING, O(s), -1, NULL },
};
static const NEM_marshal_map_t marshal_aryobj_sub_m = {
	.fields     = marshal_aryobj_sub_fs,
	.fields_len = NEM_ARRSIZE(marshal_aryobj_sub_fs),
	.elem_size  = sizeof(TYPE),
};
#undef TYPE
#define TYPE marshal_aryobj_t
static const NEM_marshal_field_t marshal_aryobj_fs[] = {
	{
		"objs", NEM_MARSHAL_ARRAY|NEM_MARSHAL_STRUCT,
		O(objs), O(len), &marshal_aryobj_sub_m
	},
};
static const NEM_marshal_map_t marshal_aryobj_m = {
	.fields     = marshal_aryobj_fs,
	.fields_len = NEM_ARRSIZE(marshal_aryobj_fs),
	.elem_size  = sizeof(TYPE),
};
#undef TYPE

typedef struct {
	uint64_t    *u64;
	int64_t     *i64;
	const char **s1;
	const char **s2;
	const char **s3;

	marshal_aryobj_sub_t *o1;
	marshal_aryobj_sub_t *o2;
}
marshal_ptrs_t;

static void
marshal_ptrs_init(void *vthis)
{
	marshal_ptrs_t *this = vthis;
	this->u64 = NULL;
	this->i64 = NEM_malloc(sizeof(int64_t));
	*this->i64 = 42;
	this->s1 = NULL;
	this->s2 = NEM_malloc(sizeof(char**));
	*this->s2 = NULL;
	this->s3 = NEM_malloc(sizeof(char**));
	*this->s3 = strdup("hello");
	this->o1 = NULL;
	this->o2 = NEM_malloc(sizeof(marshal_aryobj_sub_t));
	this->o2->i = 56;
	this->o2->s = strdup("world");
}

static void
marshal_ptrs_cmp(const void *vthis, const void *vthat)
{
	const marshal_ptrs_t *this = vthis;
	const marshal_ptrs_t *that = vthat;

	if (this == NULL) {
		ck_assert_ptr_eq(this, that);
		return;
	}

	if (this->u64 == NULL) {
		ck_assert_ptr_eq(this->u64, that->u64);
	}
	else {
		ck_assert_ptr_ne(this->u64, that->u64);
		ck_assert_int_eq(*this->u64, *that->u64);
	}

	if (this->i64 == NULL) {
		ck_assert_ptr_eq(this->i64, that->i64);
	}
	else {
		ck_assert_ptr_ne(this->i64, that->i64);
		ck_assert_int_eq(*this->i64, *that->i64);
	}
	
	struct {
		const char **s_this, **s_that;
	}
	tests[] = {
		{ this->s1, that->s1 },
		{ this->s2, that->s2 },
		{ this->s3, that->s3 },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(tests); i += 1) {
		const char **s_this = tests[i].s_this;
		const char **s_that = tests[i].s_that;
		// NB: There's some interesting things with marshalling
		// strings pointers. A non-null pointer to a NULL string is
		// marshalled as empty -- which comes out as a vanilla NULL pointer.
		bool s_this_null = NULL == s_this || NULL == *s_this;
		bool s_that_null = NULL == s_that || NULL == *s_that;

		if (s_this_null) {
			ck_assert(s_that_null);
		}
		else {
			ck_assert_ptr_ne(s_this, s_that);
			ck_assert_ptr_ne(*s_this, *s_that);
			ck_assert_str_eq(*s_this, *s_that);
		}
	}

	marshal_aryobj_sub_cmp(this->o1, that->o1);
	marshal_aryobj_sub_cmp(this->o2, that->o2);
}

#define TYPE marshal_ptrs_t
static const NEM_marshal_field_t marshal_ptrs_fs[] = {
	{ "u64", NEM_MARSHAL_PTR|NEM_MARSHAL_UINT64, O(u64), -1, NULL },
	{ "i64", NEM_MARSHAL_PTR|NEM_MARSHAL_INT64,  O(i64), -1, NULL },
	{ "s1",  NEM_MARSHAL_PTR|NEM_MARSHAL_STRING, O(s1),  -1, NULL },
	{ "s2",  NEM_MARSHAL_PTR|NEM_MARSHAL_STRING, O(s2),  -1, NULL },
	{ "s3",  NEM_MARSHAL_PTR|NEM_MARSHAL_STRING, O(s3),  -1, NULL },
	{
		"o1", NEM_MARSHAL_PTR|NEM_MARSHAL_STRUCT,
		O(o1), -1, &marshal_aryobj_sub_m
	},
	{
		"o2", NEM_MARSHAL_PTR|NEM_MARSHAL_STRUCT,
		O(o2), -1, &marshal_aryobj_sub_m
	},
};
static const NEM_marshal_map_t marshal_ptrs_m = {
	.fields     = marshal_ptrs_fs,
	.fields_len = NEM_ARRSIZE(marshal_ptrs_fs),
	.elem_size  = sizeof(TYPE),
};
#undef TYPE

#undef O

/*
 * utils
 */

typedef void(*marshal_init_fn)(void*);
typedef void(*marshal_cmp_fn)(const void*, const void*);

#define MARSHAL_VISIT_TYPES_NOBIN \
	MARSHAL_VISITOR(marshal_prims) \
	MARSHAL_VISITOR(marshal_prims_ary) \
	MARSHAL_VISITOR(marshal_strs) \
	MARSHAL_VISITOR(marshal_obj) \
	MARSHAL_VISITOR(marshal_aryobj) \
	MARSHAL_VISITOR(marshal_ptrs)

#define MARSHAL_VISIT_TYPES \
	MARSHAL_VISIT_TYPES_NOBIN \
	MARSHAL_VISITOR(marshal_bin)
