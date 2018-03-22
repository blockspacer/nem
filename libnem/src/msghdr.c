#include "nem.h"
#include <bson.h>

typedef struct {
	char *buf;
	size_t off_err;
	size_t off_route;

	size_t len;
	size_t cap;
}
NEM_msghdr_work_t;

static void*
NEM_msghdr_work_alloc(NEM_msghdr_work_t *work, size_t len)
{
	if (len > work->len + work->cap) {
		work->cap = work->cap + len;
		work->buf = NEM_panic_if_null(realloc(work->buf, work->cap));
	}
	if (len > work->len + work->cap) {
		// :vomit:
		NEM_panic("XXX: change this if the alloc strategy changes");
		//return NULL;
	}

	void *ret = &work->buf[work->len];
	work->len += len;
	return ret;
}

#define DEF_GET_METHOD(field) \
	static NEM_msghdr_##field##_t* \
	NEM_msghdr_work_##field(NEM_msghdr_work_t *work) \
	{ \
		if (0 != work->off_##field) { \
			return (NEM_msghdr_##field##_t*) &work->buf[work->off_##field]; \
		} \
		char *ptr = NEM_msghdr_work_alloc(work, sizeof(NEM_msghdr_##field##_t)); \
		if (NULL == ptr) { \
			return NULL; \
		} \
		work->off_##field = ptr - work->buf; \
		return (NEM_msghdr_##field##_t*) ptr; \
	}

DEF_GET_METHOD(err);
DEF_GET_METHOD(route);
#undef DEF_GET_METHOD

static int64_t
read_int64(NEM_msghdr_work_t *work, bson_iter_t *iter)
{
	if (BSON_TYPE_INT64 != bson_iter_type(iter)) {
		return 0;
	}

	return bson_iter_int64(iter);
}

static char*
read_string(NEM_msghdr_work_t *work, bson_iter_t *iter)
{
	if (BSON_TYPE_UTF8 != bson_iter_type(iter)) {
		return NULL;
	}

	uint32_t len;
	const char *val = bson_iter_utf8(iter, &len);

	char *outval = NEM_msghdr_work_alloc(work, len + 1);
	memcpy(outval, val, len);
	outval[len] = 0; // lol mongodb.

	return outval;
}

NEM_err_t
NEM_msghdr_unmarshal_err(NEM_msghdr_work_t *work, bson_iter_t *iter)
{
	NEM_msghdr_err_t hdr = {0};

	while (bson_iter_next(iter)) {
		const char *key = bson_iter_key(iter);

		if (!strcmp(key, "code")) {
			hdr.code = read_int64(work, iter);
		}
		else if (!strcmp(key, "reason")) {
			hdr.reason = read_string(work, iter);
		}
	}

	*NEM_msghdr_work_err(work) = hdr;
	return NEM_err_none;
}

NEM_err_t
NEM_msghdr_unmarshal_route(NEM_msghdr_work_t *work, bson_iter_t *iter)
{
	NEM_msghdr_route_t hdr = {0};

	while (bson_iter_next(iter)) {
		const char *key = bson_iter_key(iter);

		if (!strcmp(key, "cluster")) {
			hdr.cluster = read_string(work, iter);
		}
		else if (!strcmp(key, "host")) {
			hdr.host = read_string(work, iter);
		}
		else if (!strcmp(key, "inst")) {
			hdr.inst = read_string(work, iter);
		}
		else if (!strcmp(key, "obj")) {
			hdr.obj = read_string(work, iter);
		}
	}

	*NEM_msghdr_work_route(work) = hdr;
	return NEM_err_none;
}

NEM_err_t
NEM_msghdr_unmarshal(NEM_msghdr_work_t *work, bson_t *doc)
{
	bson_iter_t iter;

	if (!bson_iter_init(&iter, doc)) {
		return NEM_err_static("NEM_msghdr_unmarshal: bson_iter_init failed");
	}

	static const struct {
		const char *key;
		NEM_err_t(*fn)(NEM_msghdr_work_t*, bson_iter_t*);
	}
	unmarshal_methods[] = {
		{ "err",   &NEM_msghdr_unmarshal_err   },
		{ "route", &NEM_msghdr_unmarshal_route },
	};

	while (bson_iter_next(&iter)) {
		if (BSON_TYPE_DOCUMENT != bson_iter_type(&iter)) {
			continue;
		}

		const char *key = bson_iter_key(&iter);
		bson_iter_t subiter;

		if (!bson_iter_recurse(&iter, &subiter)) {
			return NEM_err_static(
				"NEM_msghdr_unmarshal: bson_iter_recurse failed"
			);
		}

		for (size_t i = 0; i < NEM_ARRSIZE(unmarshal_methods); i += 1) {
			if (!strcmp(key, unmarshal_methods[i].key)) {
				NEM_err_t err = unmarshal_methods[i].fn(work, &subiter);
				if (!NEM_err_ok(err)) {
					return err;
				}

				break;
			}
		}
	}

	return NEM_err_none;
}

NEM_err_t
NEM_msghdr_alloc(NEM_msghdr_t **hdr, const void *bs, size_t len)
{
	bson_t doc;

	if (!bson_init_static(&doc, bs, len)) {
		return NEM_err_static("NEM_msghdr_alloc: invalid bson");
	}

	NEM_msghdr_work_t work;
	work.len = sizeof(NEM_msghdr_t);
	work.cap = sizeof(NEM_msghdr_t);// + len;
   	work.buf = NEM_malloc(work.cap);

	NEM_err_t err = NEM_msghdr_unmarshal(&work, &doc);
	bson_destroy(&doc);
	return err;
}
