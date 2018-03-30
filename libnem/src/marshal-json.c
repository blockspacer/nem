#include <sys/types.h>
#include <string.h>
#include <json.h>

#include "nem.h"

/*
NEM_err_t
NEM_unmarshal_json(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *json,
	size_t                   json_len
) {
}
*/

static json_object*
NEM_marshal_json_obj(
	const NEM_marshal_map_t *this,
	const char              *elem
);

static json_object*
NEM_marshal_json_field(
	const NEM_marshal_field_t *this,
	const char                *obj
) {
	// NB: NEM_MARSHAL_ARRAY/NEM_MARSHAL_PTR must have already been h andeld.
	const char *elem = obj + this->offset_elem;

	switch (this->type & NEM_MARSHAL_TYPEMASK) {
#		define NEM_MARSHAL_VISITOR(ntype, ctype) \
		case ntype: \
			return json_object_new_int64((int64_t)*(ctype*)elem);
		NEM_MARSHAL_CASE_VISIT_INT_TYPES
#		undef NEM_MARSHAL_VISITOR

		case NEM_MARSHAL_BOOL:
			return json_object_new_boolean(*(bool*)elem);

		case NEM_MARSHAL_STRING: {
			const char **str = (const char**)elem;
			if (NULL == *str) {
				return NULL;
			}
			return json_object_new_string(*str);
		}

		case NEM_MARSHAL_STRUCT:
			return NEM_marshal_json_obj(this->sub, elem);

		default:
			NEM_panicf(
				"NEM_marshal_json: unsupported type %s",
				NEM_marshal_field_type_name(this->type)
			);
	}
}

static json_object*
NEM_marshal_json_array(
	const NEM_marshal_field_t *this,
	const char                *obj
) {
	const char *elem = obj + this->offset_elem;
	const size_t *sz = (const size_t*)(obj + this->offset_len);
	size_t stride = NEM_marshal_field_stride(this);

	json_object *jobj = json_object_new_array();

	if (NEM_MARSHAL_STRUCT == this->type) {
		for (size_t i = 0; i < *sz; i += 1) {
			json_object *sub = NEM_marshal_json_obj(this->sub, elem);
			elem += stride;
			json_object_array_add(jobj, sub);
		}
	}
	else {
		for (size_t i = 0; i < *sz; i += 1) {
			json_object *sub = NEM_marshal_json_field(this, elem);
			elem += stride;
			json_object_array_add(jobj, sub);
		}
	}

	return jobj;
}

static json_object*
NEM_marshal_json_obj(
	const NEM_marshal_map_t *this,
	const char              *elem
) {
	json_object *obj = json_object_new_object();
	json_object *sub = NULL;

	for (size_t i = 0; i < this->fields_len; i += 1) {
		const NEM_marshal_field_t *field = &this->fields[i];
		bool is_array = field->type & NEM_MARSHAL_ARRAY;
		bool is_ptr = field->type & NEM_MARSHAL_PTR;
		const char *fieldelem = elem;

		if (is_array && is_ptr) {
			NEM_panic("NEM_marshal_json_obj: field with both ARRAY and PTR");
		}
		if (is_array) {
			sub = NEM_marshal_json_array(field, fieldelem);
		}
		else {
			if (is_ptr) {
				fieldelem = *(char**)(elem + field->offset_elem);
				if (NULL == fieldelem) {
					continue;
				}
			}

			sub = NEM_marshal_json_field(field, fieldelem);
		}
		if (NULL != sub) {
			json_object_object_add(obj, field->name, sub);
		}
	}

	return obj;
}

NEM_err_t
NEM_marshal_json(
	const NEM_marshal_map_t *this,
	void                   **out,
	size_t                  *out_len,
	const void              *elem,
	size_t                   elem_len
) {
	if (elem_len != this->elem_size) {
		return NEM_err_static("NEM_marshal_json: invalid elem_size");
	}

	json_object *obj = NEM_marshal_json_obj(this, elem);
	if (NULL == obj) {
		return NEM_err_static("NEM_marshal_json: the unthinkable happened");
	}

	char *str = strdup(json_object_to_json_string_ext(obj, 0));
	json_object_put(obj);

	*out = str;
	*out_len = strlen(str);
	return NEM_err_none;
}
