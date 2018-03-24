#include "nem.h"
#include <bson.h>

typedef struct NEM_msghdr_buf_t NEM_msghdr_buf_t;
struct NEM_msghdr_buf_t {
	size_t            len;
	size_t            cap;
	NEM_msghdr_buf_t *prev;
	NEM_ALIGN char    data[];
};

typedef struct {
	NEM_msghdr_t *hdr;
	NEM_msghdr_buf_t *head;
}
NEM_msghdr_work_t;

static NEM_msghdr_buf_t*
NEM_msghdr_buf_new(size_t cap)
{
	size_t new_buf_len = sizeof(NEM_msghdr_buf_t) + cap;
	NEM_msghdr_buf_t *new_buf = NEM_malloc(new_buf_len);
	new_buf->cap = cap;
	return new_buf;
}

static void*
NEM_msghdr_buf_alloc(NEM_msghdr_buf_t **buf, size_t len)
{
	// NB: Dirty, but allocations should be 8-byte aligned. Pad each
	// allocation out to meet that requirement.
	if (0 != len % 8) {
		len += 8 - (len % 8);
	}

	if (len + (*buf)->len > (*buf)->cap) {
		// Need to allocate a new buffer.
		size_t new_cap = (*buf)->cap * 2;
		if (new_cap < len) {
			new_cap = len;
		}

		NEM_msghdr_buf_t *new_buf = NEM_msghdr_buf_new(new_cap);
		new_buf->prev = *buf;
		*buf = new_buf;
	}

	char *ptr = &(*buf)->data[(*buf)->len];
	(*buf)->len += len;
	return ptr;
}

static void*
NEM_msghdr_work_alloc(NEM_msghdr_work_t *this, size_t len)
{
	return NEM_msghdr_buf_alloc(&this->head, len);
}

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
	NEM_msghdr_err_t *hdr = NEM_msghdr_work_alloc(
		work,
		sizeof(NEM_msghdr_err_t)
	);
	work->hdr->err = hdr;

	while (bson_iter_next(iter)) {
		const char *key = bson_iter_key(iter);

		if (!strcmp(key, "code")) {
			hdr->code = read_int64(work, iter);
		}
		else if (!strcmp(key, "reason")) {
			hdr->reason = read_string(work, iter);
		}
	}

	return NEM_err_none;
}

NEM_err_t
NEM_msghdr_marshal_err(NEM_msghdr_err_t *hdr, bson_t *doc)
{
	BSON_APPEND_INT64(doc, "code", hdr->code);
	if (NULL != hdr->reason) {
		BSON_APPEND_UTF8(doc, "reason", hdr->reason);
	}

	return NEM_err_none;
}

NEM_err_t
NEM_msghdr_unmarshal_route(NEM_msghdr_work_t *work, bson_iter_t *iter)
{
	NEM_msghdr_route_t *hdr = NEM_msghdr_work_alloc(
		work,
		sizeof(NEM_msghdr_route_t)
	);
	work->hdr->route = hdr;

	while (bson_iter_next(iter)) {
		const char *key = bson_iter_key(iter);

		if (!strcmp(key, "cluster")) {
			hdr->cluster = read_string(work, iter);
		}
		else if (!strcmp(key, "host")) {
			hdr->host = read_string(work, iter);
		}
		else if (!strcmp(key, "inst")) {
			hdr->inst = read_string(work, iter);
		}
		else if (!strcmp(key, "obj")) {
			hdr->obj = read_string(work, iter);
		}
	}

	return NEM_err_none;
}

NEM_err_t
NEM_msghdr_marshal_route(NEM_msghdr_route_t *hdr, bson_t *doc)
{
	if (NULL != hdr->cluster) {
		BSON_APPEND_UTF8(doc, "cluster", hdr->cluster);
	}
	if (NULL != hdr->host) {
		BSON_APPEND_UTF8(doc, "host", hdr->host);
	}
	if (NULL != hdr->inst) {
		BSON_APPEND_UTF8(doc, "inst", hdr->inst);
	}
	if (NULL != hdr->obj) {
		BSON_APPEND_UTF8(doc, "obj", hdr->obj);
	}

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
NEM_msghdr_marshal(NEM_msghdr_t *hdr, bson_t *doc)
{
	NEM_err_t err = NEM_err_none;
	bson_t child;

	if (NEM_err_ok(err) && NULL != hdr->err) {
		BSON_APPEND_DOCUMENT_BEGIN(doc, "err", &child);
		err = NEM_msghdr_marshal_err(hdr->err, &child);
		bson_append_document_end(doc, &child);
	}
	if (NEM_err_ok(err) && NULL != hdr->route) {
		BSON_APPEND_DOCUMENT_BEGIN(doc, "route", &child);
		err = NEM_msghdr_marshal_route(hdr->route, &child);
		bson_append_document_end(doc, &child);
	}

	return err;
}

NEM_err_t
NEM_msghdr_new(NEM_msghdr_t **out, const void *bs, size_t len)
{
	bson_t doc;

	if (!bson_init_static(&doc, bs, len)) {
		return NEM_err_static("NEM_msghdr_alloc: invalid bson");
	}

	size_t json_len;
	char *json = bson_as_canonical_extended_json(&doc, &json_len);
	free(json);

	NEM_msghdr_t *hdr = NEM_malloc(
		sizeof(NEM_msghdr_t) + sizeof(NEM_msghdr_work_t)
	);
	NEM_msghdr_work_t *work = (NEM_msghdr_work_t*) &hdr->data;
	work->hdr = hdr;
	work->head = NEM_msghdr_buf_new(512);

	NEM_err_t err = NEM_msghdr_unmarshal(work, &doc);
	bson_destroy(&doc);
	if (!NEM_err_ok(err)) {
		NEM_msghdr_free(hdr);
	}
	else {
		*out = hdr;
	}

	return err;
}

void
NEM_msghdr_free(NEM_msghdr_t *hdr)
{
	NEM_msghdr_work_t *work = (NEM_msghdr_work_t*) &hdr->data[0];
	while (NULL != work->head) {
		NEM_msghdr_buf_t *next = work->head->prev;
		free(work->head);
		work->head = next;
	}

	free(hdr);
}

NEM_err_t
NEM_msghdr_pack(NEM_msghdr_t *hdr, void **out, size_t *outlen)
{
	bson_t doc;
	bson_init(&doc);

	NEM_err_t err = NEM_msghdr_marshal(hdr, &doc);
	if (!NEM_err_ok(err)) {
		bson_destroy(&doc);
		return err;
	}

	uint32_t len;
	uint8_t *bs = bson_destroy_with_steal(&doc, true, &len);
	if (NULL == bs) {
		return NEM_err_static("NEM_msghdr_pack: bson_destroy_with steal failed");
	}

	*out = bs;
	*outlen = (size_t) len;
	return NEM_err_none;
}
