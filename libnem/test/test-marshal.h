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
}
marshal_prims_ary_t;

static void
marshal_prims_ary_init(void *vthis)
{
	marshal_prims_ary_t *this = vthis;

	static const int64_t i64s[] = {
		42, 56, 42*56
	};

	this->u8_len = 0; 
	this->u8s = NULL;

	this->i64_len = NEM_ARRSIZE(i64s);
	this->i64s = NEM_malloc(sizeof(i64s));
	memcpy(this->i64s, i64s, sizeof(i64s));
}

static void
marshal_prims_ary_cmp(const void *vthis, const void *vthat)
{
	const marshal_prims_ary_t *this = vthis;
	const marshal_prims_ary_t *that = vthat;

	ck_assert_int_eq(this->u8_len, that->u8_len);
	ck_assert_int_eq(this->i64_len, that->i64_len);

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
}

#define TYPE marshal_prims_ary_t
static const NEM_marshal_field_t marshal_prims_ary_fs[] = {
	{ "u8s",  NEM_MARSHAL_ARRAY|NEM_MARSHAL_UINT8, O(u8s),  O(u8_len),  NULL },
	{ "i64s", NEM_MARSHAL_ARRAY|NEM_MARSHAL_INT64, O(i64s), O(i64_len), NULL },
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
}
marshal_strs_t;

static void
marshal_strs_init(void *vthis)
{
	marshal_strs_t *this = vthis;

	this->s1 = strdup("hello");
	this->s2 = strdup("world");
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
		ck_assert_ptr_eq(this->s1, that->s1);
	}
	else {
		ck_assert_ptr_ne(this->s2, that->s2);
		ck_assert_str_eq(this->s2, that->s2);
	}
}

#define TYPE marshal_strs_t
static const NEM_marshal_field_t marshal_strs_fs[] = {
	{ "s1", NEM_MARSHAL_STRING, O(s1), -1, NULL },
	{ "s2", NEM_MARSHAL_STRING, O(s2), -1, NULL },
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

#undef O

/*
 * utils
 */

typedef void(*marshal_init_fn)(void*);
typedef void(*marshal_cmp_fn)(const void*, const void*);

#define MARSHAL_VISIT_TYPES \
	MARSHAL_VISITOR(marshal_prims) \
	MARSHAL_VISITOR(marshal_prims_ary) \
	MARSHAL_VISITOR(marshal_strs) \
	MARSHAL_VISITOR(marshal_bin)

#define MARSHAL_VISIT_TYPES_NOBIN \
	MARSHAL_VISITOR(marshal_prims) \
	MARSHAL_VISITOR(marshal_prims_ary) \
	MARSHAL_VISITOR(marshal_strs)
