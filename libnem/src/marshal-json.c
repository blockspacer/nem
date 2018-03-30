#include <sys/types.h>
#include <string.h>
#include <json.h>

#include "nem.h"

static json_object*
NEM_marshal_json_obj(
	const NEM_marshal_map_t *this,
	const char              *elem
);

static json_object*
NEM_marshal_json_field(
	const NEM_marshal_field_t *this,
	const char                *elem
) {
	// NB: NEM_MARSHAL_ARRAY/NEM_MARSHAL_PTR must have already been handled.

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
	char *const*elem = (char*const*)(obj + this->offset_elem);
	const size_t *sz = (const size_t*)(obj + this->offset_len);
	size_t stride = NEM_marshal_field_stride(this);

	const char *ptr = *elem;
	if (NULL == ptr) {
		return NULL;
	}

	json_object *jobj = json_object_new_array();

	if (NEM_MARSHAL_STRUCT == this->type) {
		for (size_t i = 0; i < *sz; i += 1) {
			json_object *sub = NEM_marshal_json_obj(this->sub, ptr);
			ptr += stride;
			json_object_array_add(jobj, sub);
		}
	}
	else {
		for (size_t i = 0; i < *sz; i += 1) {
			json_object *sub = NEM_marshal_json_field(this, ptr);
			ptr += stride;
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
			else {
				fieldelem = elem + field->offset_elem;
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

static void
NEM_unmarshal_json_obj(
	const NEM_marshal_map_t *this, 
	json_object             *json,
	char                    *obj
);

static bool
NEM_unmarshal_json_field(
	const NEM_marshal_field_t *this,
	json_object               *json,
	char                      *elem
) {
	switch (this->type & NEM_MARSHAL_TYPEMASK) {
#		define NEM_MARSHAL_VISITOR(NTYPE, CTYPE) \
			case NTYPE: \
				if (json_type_int == json_object_get_type(json)) { \
					*(CTYPE*)elem = (CTYPE) json_object_get_int64(json); \
					return true; \
				} \
				break;
		NEM_MARSHAL_CASE_VISIT_INT_TYPES
#		undef NEM_MARSHAL_VISITOR

		case NEM_MARSHAL_BOOL:
			if (json_type_boolean == json_object_get_type(json)) {
				*(bool*)elem = json_object_get_boolean(json);
				return true;
			}
			break;

		case NEM_MARSHAL_STRING:
			if (json_type_string == json_object_get_type(json)) {
				size_t len = json_object_get_string_len(json);
				char **outval = (char**)elem;
				*outval = NEM_malloc(len + 1);
				memcpy(*outval, json_object_get_string(json), len);
				(*outval)[len] = 0;
				return true;
			}
			break;

		case NEM_MARSHAL_STRUCT:
			if (json_type_object == json_object_get_type(json)) {
				NEM_unmarshal_json_obj(
					this->sub,
					json,
					elem
				);
				return true;
			}
			break;

		default:
			NEM_panicf(
				"NEM_unmarshal_json: unhandled type %s",
				NEM_marshal_field_type_name(this->type)
			);
	}

	return false;
}

static void
NEM_unmarshal_json_array(
	const NEM_marshal_field_t *this,
	json_object               *json,
	char                      *obj
) {
	if (json_type_array != json_object_get_type(json)) {
		return;
	}

	char **pdata = (char**)(obj + this->offset_elem);
	size_t *psz = (size_t*)(obj + this->offset_len);
	size_t stride = NEM_marshal_field_stride(this);

	*psz = json_object_array_length(json);
	*pdata = NEM_malloc(stride * *psz);

	if ((this->type & NEM_MARSHAL_TYPEMASK) == NEM_MARSHAL_STRUCT) {
		for (size_t i = 0; i < *psz; i += 1) {
			NEM_unmarshal_json_obj(
				this->sub,
				json_object_array_get_idx(json, i),
				(*pdata) + stride * i
			);
		}
	}
	else {
		for (size_t i = 0; i < *psz; i += 1) {
			NEM_unmarshal_json_field(
				this,
				json_object_array_get_idx(json, i),
				(*pdata) + stride * i
			);
		}
	}
}

static void
NEM_unmarshal_json_obj(
	const NEM_marshal_map_t *this, 
	json_object             *json,
	char                    *obj
) {
	if (json_type_object != json_object_get_type(json)) {
		return;
	}

	for (size_t i = 0; i < this->fields_len; i += 1) {
		const NEM_marshal_field_t *field = &this->fields[i];
		json_object *sub = NULL;

		if (!json_object_object_get_ex(json, field->name, &sub)) {
			continue;
		}

		bool is_array = field->type & NEM_MARSHAL_ARRAY;
		bool is_ptr = field->type & NEM_MARSHAL_PTR;

		if (is_array && is_ptr) {
			NEM_panic("NEM_unmarshal_json: array+ptr not allowed");
		}
		if (is_array) {
			NEM_unmarshal_json_array(field, sub, obj);
			continue;
		}
		if (is_ptr) {
			char **ptr = (char**)(obj + field->offset_elem);
			*ptr = NEM_malloc(NEM_marshal_field_stride(field));
			if (!NEM_unmarshal_json_field(field, sub, *ptr)) {
				free(*ptr);
				*ptr = NULL;
			}
			continue;
		}

		NEM_unmarshal_json_field(field, sub, obj + field->offset_elem);
	}
}

NEM_err_t
NEM_unmarshal_json(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *json,
	size_t                   json_len
) {
	if (elem_len != this->elem_size) {
		NEM_panic("NEM_unmarshal_json: invalid elem_len");
	}

	bzero(elem, elem_len);

	NEM_err_t err = NEM_err_none;
	json_tokener *parser = json_tokener_new();

	json_object *obj = json_tokener_parse_ex(parser, json, json_len);
	if (NULL == obj) {
		enum json_tokener_error code = json_tokener_get_error(parser);
		err = NEM_err_static(json_tokener_error_desc(code));
	}
	else {
		NEM_unmarshal_json_obj(this, obj, (char*)elem);
	}

	json_object_put(obj);
	json_tokener_free(parser);
	return err;
}
