#include <sys/types.h>
#include <string.h>
#include <json.h>

#include "nem.h"

static NEM_err_t
NEM_unmarshal_json_object(
	const NEM_marshal_map_t *this,
	json_object             *json,
	void                    *elem
);

static NEM_err_t
NEM_unmarshal_json_field(
	const NEM_marshal_field_t *this,
	json_object               *json,
	void                      *elem
) {
	switch (this->type & NEM_MARSHAL_TYPEMASK) {
#		define NEM_MARSHAL_VISITOR(ntype, ctype) \
			case ntype: { \
				if (json_type_int == json_object_get_type(json)) { \
					*(ctype*)elem = (ctype) json_object_get_int(json); \
				} \
			}
		NEM_MARSHAL_CASE_VISIT_INT_TYPES;
#		undef NEM_MARSHAL_VISITOR

		case NEM_MARSHAL_BOOL: {
			if (json_type_boolean == json_object_get_type(json)) {
				*(bool*)elem = json_object_get_boolean(json);
			}
			return NEM_err_none;
		}

		case NEM_MARSHAL_STRING: {
			if (json_type_string == json_object_get_type(json)) {
				size_t len = json_object_get_string_len(json);
				char **out = (char**)elem;
				*out = NEM_malloc(len + 1);
				memcpy(*out, json_object_get_string(json), len);
				(*out)[len] = 0;
			}
			return NEM_err_none;
		}

		case NEM_MARSHAL_STRUCT: {
			if (json_type_object == json_object_get_type(json)) {
				return NEM_unmarshal_json_object(
					this->sub,
					json,
					elem
				);
			}
			return NEM_err_none;
		}

		default:
			NEM_panicf(
				"NEM_unmarshal_json: unsupported type %s",
				NEM_marshal_field_type_name(this->type)
			);
	}
}

static NEM_err_t
NEM_unmarshal_json_array(
	const NEM_marshal_field_t *this,
	json_object               *json,
	void                      *elem
) {
	NEM_panic("TODO");
}

static NEM_err_t
NEM_unmarshal_json_object(
	const NEM_marshal_map_t *this,
	json_object             *json,
	void                    *elem
) {
	json_object *sub = NULL;
	NEM_err_t err = NEM_err_none;

	for (size_t i = 0; i < this->fields_len; i += 1) {
		const NEM_marshal_field_t *field = &this->fields[i];
		if (json_object_object_get_ex(json, field->name, &sub)) {
			if (NEM_MARSHAL_ARRAY & field->type) {
				err = NEM_unmarshal_json_array(field, sub, elem);
				if (!NEM_err_ok(err)) {
					break;
				}
			}
			else {
				void *elem_field = ((char*)elem) + field->offset_elem;
				err = NEM_unmarshal_json_field(field, sub, elem_field);
				if (!NEM_err_ok(err)) {
					break;
				}
			}
		}
	}

	return err;
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
	json_object *object = json_tokener_parse_ex(parser, json, json_len);
	if (NULL == object) {
		enum json_tokener_error jerr = json_tokener_get_error(parser);
		err = NEM_err_static(json_tokener_error_desc(jerr));
	}
	else {
		err = NEM_unmarshal_json_object(this, object, elem);
		if (!NEM_err_ok(err)) {
			NEM_unmarshal_free(this, elem, elem_len);
		}
	}

	json_object_put(object);
	json_tokener_free(parser);
	return err;
}

NEM_err_t
NEM_marshal_json(
	const NEM_marshal_map_t *this,
	void                   **out,
	size_t                  *out_len,
	const void              *elem,
	size_t                   elem_len
) {
	NEM_panic("TODO");
}
