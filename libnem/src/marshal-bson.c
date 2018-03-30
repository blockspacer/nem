#include <sys/types.h>
#include <bson.h>

#include "nem.h"

static NEM_err_t
NEM_marshal_bson_obj(
	const NEM_marshal_map_t *this,
	bson_t                  *doc,
	const char              *obj
);

static NEM_err_t
NEM_marshal_bson_field(
	const NEM_marshal_field_t *this,
	bson_t                    *doc,
	const char                *name,
	const char                *elem,
	size_t                    *psz
) {
	switch (this->type & NEM_MARSHAL_TYPEMASK) {
#		define NEM_MARSHAL_VISITOR(NTYPE, CTYPE) \
		case NTYPE: \
			if (!bson_append_int64(doc, name, -1, (int64_t)*(CTYPE*)elem)) { \
				return NEM_err_static(\
					"NEM_marshal_bson: bson_append_int64 failed" \
				); \
			} \
			break;
		NEM_MARSHAL_CASE_VISIT_INT_TYPES
#		undef NEM_MARSHAL_VISITOR

		case NEM_MARSHAL_BOOL:
			if (!bson_append_bool(doc, name, -1, *(bool*)elem)) {
				return NEM_err_static(
					"NEM_marshal_bson: bson_append_bool failed"
				);
			}
			break;

		case NEM_MARSHAL_STRING:
			if (!bson_append_utf8(doc, name, -1, *(char**)elem, -1)) {
				return NEM_err_static(
					"NEM_marshal_bson: bson_append_string failed"
				);
			}
			break;

		case NEM_MARSHAL_FIXLEN:
		case NEM_MARSHAL_BINARY: {
			uint8_t *ptr = (this->type == NEM_MARSHAL_FIXLEN)
				? (uint8_t*) elem
				: (uint8_t*) *(char**)elem;

			size_t sz = (this->type == NEM_MARSHAL_FIXLEN)
				? this->offset_len
				: *psz;

			if (0 == sz || NULL == ptr) {
				return NEM_err_none;
			}

			if (!bson_append_binary(doc, this->name, -1, 0, ptr, sz)) {
				return NEM_err_static(
					"NEM_marshal_bson: bson_append_binary failed"
				);
			}
			break;
		}

		case NEM_MARSHAL_STRUCT: {
			bson_t subdoc;
			if (!bson_append_document_begin(doc, name, -1, &subdoc)) {
				return NEM_err_static(
					"NEM_marshal_bson: bson_append_document_begin failed"
				);
			}

			NEM_err_t err = NEM_marshal_bson_obj(this->sub, &subdoc, elem);
			bool ok = bson_append_document_end(doc, &subdoc);
			if (!NEM_err_ok(err)) {
				return err;
			}
			if (!ok) {
				return NEM_err_static(
					"NEM_marshal_bson: bson_append_document_end failed"
				);
			}
			break;
		}

		default:
			NEM_panicf(
				"NEM_marshal_bson: unsupported type %s",
				NEM_marshal_field_type_name(this->type)
			);
	}

	return NEM_err_none;
}

static NEM_err_t
NEM_marshal_bson_array(
	const NEM_marshal_field_t *this,
	bson_t                    *doc,
	const char                *obj
) {
	char *const*elem = (char*const*)(obj + this->offset_elem);
	size_t len = *(size_t*)(obj + this->offset_len);
	size_t stride = NEM_marshal_field_stride(this);

	if (
		(this->type & NEM_MARSHAL_FIXLEN) == NEM_MARSHAL_FIXLEN
		|| (this->type & NEM_MARSHAL_BINARY) == NEM_MARSHAL_BINARY
	) {
		return NEM_err_static(
			"NEM_marshal_bson: cannot have fixlen/binary arrays"
		);
	}	

	const char *ptr = *elem;
	if (NULL == ptr) {
		return NEM_err_none;
	}

	// NB: Arrays can't be nested, so no need to explicitly pass the name
	// field through. Just use field->name.
	bson_t subdoc;
	if (!bson_append_array_begin(doc, this->name, -1, &subdoc)) {
		return NEM_err_static(
			"NEM_marshal_bson: bson_append_array_begin failed"
		);
	}

	char idxbuf[16];
	const char *idxstr;
	NEM_err_t err = NEM_err_none;

	for (size_t i = 0; i < len; i += 1) {
		bson_uint32_to_string(i, &idxstr, idxbuf, sizeof(idxbuf));

		err = NEM_marshal_bson_field(
			this,
			&subdoc,
			idxstr,
			ptr + stride * i,
			NULL
		);
		if (!NEM_err_ok(err)) {
			break;
		}
	}

	if (!bson_append_array_end(doc, &subdoc)) {
		return NEM_err_static(
			"NEM_marshal_bson: bson_append_array_end failed"
		);
	}

	return err;
}

static NEM_err_t
NEM_marshal_bson_obj(
	const NEM_marshal_map_t *this,
	bson_t                  *doc,
	const char              *elem
) {
	for (size_t i = 0; i < this->fields_len; i += 1) {
		const NEM_marshal_field_t *field = &this->fields[i];
		bool is_array = field->type & NEM_MARSHAL_ARRAY;
		bool is_ptr = field->type & NEM_MARSHAL_PTR;
		const char *field_elem = NULL;

		if (is_array && is_ptr) {
			NEM_panic("NEM_marshal_bson: field with both ARRAY and PTR");
		}
		if (is_array) {
			NEM_err_t err = NEM_marshal_bson_array(field, doc, elem);
			if (!NEM_err_ok(err)) {
				return err;
			}
		}
		else {
			if (is_ptr) {
				field_elem = *(char**)(elem + field->offset_elem);
				if (NULL == field_elem) {
					continue;
				}
			}
			else {
				field_elem = elem + field->offset_elem;
			}

			NEM_err_t err = NEM_marshal_bson_field(
				field,
				doc,
				field->name,
				field_elem,
				(size_t*)(elem + field->offset_len)
			);
			if (!NEM_err_ok(err)) {
				return err;
			}
		}
	}

	return NEM_err_none;
}

NEM_err_t
NEM_marshal_bson(
	const NEM_marshal_map_t *this,
	void                   **out,
	size_t                  *out_len,
	const void              *elem,
	size_t                   elem_len
) {
	bson_t doc;
	bson_init(&doc);

	NEM_err_t err = NEM_marshal_bson_obj(this, &doc, elem);
	if (!NEM_err_ok(err)) {
		bson_destroy(&doc);
		return err;
	}

	uint32_t len = 0;
	uint8_t *ptr = bson_destroy_with_steal(&doc, true, &len);
	if (NULL == ptr) {
		return NEM_err_static(
			"NEM_marshal_bson: bson_destroy_with_steal failed"
		);
	}

	*out = ptr;
	*out_len = (size_t)len;
	return NEM_err_none;
}

static NEM_err_t
NEM_unmarshal_bson_iter(
	const NEM_marshal_map_t *this,
	bson_iter_t             *doc,
	char                    *obj
);

static NEM_err_t
NEM_unmarshal_bson_obj(
	const NEM_marshal_map_t *this,
	const bson_t            *doc,
	char                    *obj
) {
	bson_iter_t iter;

	if (!bson_iter_init(&iter, doc)) {
		return NEM_err_static("NEM_unmarshal_bson: bson_iter_init failed");
	}

	return NEM_unmarshal_bson_iter(this, &iter, obj);
}

static NEM_err_t
NEM_unmarshal_bson_field(
	const NEM_marshal_field_t *this,
	const bson_iter_t         *iter,
	char                      *elem,
	size_t                    *psz,
	bool                      *wrote
) {
	bool did_write = false;

	switch (this->type & NEM_MARSHAL_TYPEMASK) {
#		define NEM_MARSHAL_VISITOR(NTYPE, CTYPE) \
			case NTYPE: \
				if (BSON_TYPE_INT64 == bson_iter_type(iter)) { \
					*(CTYPE*)elem = (CTYPE) bson_iter_int64(iter); \
					did_write = true; \
				} \
				else if (BSON_TYPE_INT32 == bson_iter_type(iter)) { \
					*(CTYPE*)elem = (CTYPE) bson_iter_int32(iter); \
					did_write = true; \
				} \
				break;
		NEM_MARSHAL_CASE_VISIT_INT_TYPES
#		undef NEM_MARSHAL_VISITOR

		case NEM_MARSHAL_BOOL:
			if (BSON_TYPE_BOOL == bson_iter_type(iter)) {
				*(bool*)elem = bson_iter_bool(iter);
				did_write = true;
			}
			break;

		case NEM_MARSHAL_STRING:
			if (BSON_TYPE_UTF8 == bson_iter_type(iter)) {
				uint32_t len;
				const char *val = bson_iter_utf8(iter, &len);

				char *out = NEM_malloc(len + 1);
				memcpy(out, val, len);
				out[len] = 0;

				*(char**)elem = out;
				did_write = true;
			}
			break;

		case NEM_MARSHAL_FIXLEN:
			if (BSON_TYPE_BINARY == bson_iter_type(iter)) {
				uint32_t len;
				const uint8_t *data;
				bson_iter_binary(iter, NULL, &len, &data);
				if (this->offset_len != len) {
					return NEM_err_static(
						"NEM_unmarshal_bson: fixlen size mismatch"
					);
				}

				memcpy((char*)elem, data, len);
				did_write = true;
			}
			break;

		case NEM_MARSHAL_BINARY:
			if (BSON_TYPE_BINARY == bson_iter_type(iter)) {
				uint32_t len;
				const uint8_t *data;
				bson_iter_binary(iter, NULL, &len, &data);

				*(char**)elem = NEM_malloc(len);
				*psz = len;
				memcpy(*(char**)elem, data, len);
				did_write = true;
			}
			break;

		case NEM_MARSHAL_STRUCT:
			if (BSON_TYPE_DOCUMENT == bson_iter_type(iter)) {
				bson_iter_t sub;
				if (!bson_iter_recurse(iter, &sub)) {
					return NEM_err_static(
						"NEM_unmarshal_bson: bson_iter_recurse failed"
					);
				}

				NEM_err_t err = NEM_unmarshal_bson_iter(this->sub, &sub, elem);
				if (!NEM_err_ok(err)) {
					return err;
				}
				did_write = true;
			}
			break;

		default:
			NEM_panicf(
				"NEM_unmarshal_bson: unexpected type %s",
				NEM_marshal_field_type_name(this->type)
			);
	}

	if (NULL != wrote) {
		*wrote = did_write;
	}

	return NEM_err_none;
}

static NEM_err_t
NEM_unmarshal_bson_array(
	const NEM_marshal_field_t *this,
	const bson_iter_t         *iter,
	char                      *obj
) {
	if (BSON_TYPE_ARRAY != bson_iter_type(iter)) {
		return NEM_err_none;
	}
	if (
		(this->type & NEM_MARSHAL_BINARY) == NEM_MARSHAL_BINARY
		|| (this->type & NEM_MARSHAL_FIXLEN) == NEM_MARSHAL_FIXLEN
	) {
		NEM_panicf(
			"NEM_unmarshal_bson: %s array of binary/fixlen not allowed",
			this->name
		);
	}

	bson_iter_t subiter;
	if (!bson_iter_recurse(iter, &subiter)) {
		return NEM_err_static("NEM_unmarshal_bson: bson_iter_recurse failed");
	}
	
	size_t stride = NEM_marshal_field_stride(this);
	size_t cap = 0;
	size_t len = 0;
	char *buf = NULL;
	NEM_err_t err = NEM_err_none;

	while (NEM_err_ok(err) && bson_iter_next(&subiter)) {
		if (len >= cap) {
			cap = cap ? cap * 2 : 8;
			buf = NEM_panic_if_null(realloc(buf, cap * stride));
		}

		bzero(buf + (stride * len), stride);
		err = NEM_unmarshal_bson_field(
			this,
			&subiter,
			buf + (stride * len),
			NULL,
			NULL
		);
		len += 1;
	}
	if (!NEM_err_ok(err)) {
		free(buf);
		return err;
	}

	*(char**)(obj + this->offset_elem) = buf;
	*(size_t*)(obj + this->offset_len) = len;

	return NEM_err_none;
}

static NEM_err_t
NEM_unmarshal_bson_iter(
	const NEM_marshal_map_t *this,
	bson_iter_t             *iter,
	char                    *obj
) {
	bzero(obj, this->elem_size);

	while (bson_iter_next(iter)) {
		const char *key = bson_iter_key(iter);
		NEM_err_t err;

		for (size_t i = 0; i < this->fields_len; i += 1) {
			const NEM_marshal_field_t *field = &this->fields[i];
			if (strcmp(field->name, key)) {
				continue;
			}

			size_t *psz = (size_t*)(obj + field->offset_len);

			if (field->type & NEM_MARSHAL_ARRAY) {
				err = NEM_unmarshal_bson_array(field, iter, obj);
			}
			else if (field->type & NEM_MARSHAL_PTR) {
				char **ptr = (char**)(obj + field->offset_elem);
				bool wrote = false;
				*ptr = NEM_malloc(NEM_marshal_field_stride(field));
				err = NEM_unmarshal_bson_field(field, iter, *ptr, psz, &wrote);
				if (!wrote) {
					free(*ptr);
					*ptr = NULL;
				}
			}
			else {
				err = NEM_unmarshal_bson_field(
					field,
					iter, 
					obj + field->offset_elem,
					psz,
					NULL
				);
			}

			if (!NEM_err_ok(err)) {
				return err;
			}
		}
	}

	return NEM_err_none;
}

NEM_err_t
NEM_unmarshal_bson(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *bson,
	size_t                   bson_len
) {
	if (elem_len != this->elem_size) {
		NEM_panic("NEM_unmarshal_bson: invalid elem_len");
	}

	bson_t doc;

	if (!bson_init_static(&doc, bson, bson_len)) {
		return NEM_err_static("NEM_unmarshal_bson: bson_init_static failed");
	}

	NEM_err_t err = NEM_unmarshal_bson_obj(this, &doc, elem);
	bson_destroy(&doc);
	if (!NEM_err_ok(err)) {
		NEM_unmarshal_free(this, elem, elem_len);
	}

	return err;
}
