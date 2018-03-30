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
