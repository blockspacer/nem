#include <sys/types.h>
#include <toml2.h>

#include "nem.h"

static void
NEM_unmarshal_toml_iter(
	const NEM_marshal_map_t *this,
	toml2_iter_t            *iter,
	char                    *obj
);

static bool
NEM_unmarshal_toml_field(
	const NEM_marshal_field_t *this,
	toml2_t                   *node,
	char                      *elem
) {
	switch (this->type & NEM_MARSHAL_TYPEMASK) {
#		define NEM_MARSHAL_VISITOR(NTYPE, CTYPE) \
		case NTYPE: \
			if ( \
				TOML2_INT == toml2_type(node) \
				|| TOML2_FLOAT == toml2_type(node) \
			) { \
				*(CTYPE*)elem = (CTYPE) toml2_int(node); \
				return true; \
			} \
			break;
		NEM_MARSHAL_CASE_VISIT_INT_TYPES
#		undef NEM_MARSHAL_VISITOR

		case NEM_MARSHAL_BOOL:
			if (TOML2_BOOL == toml2_type(node)) {
				*(bool*)elem = toml2_bool(node);
				return true;
			}
			break;

		case NEM_MARSHAL_STRING:
			if (TOML2_STRING == toml2_type(node)) {
				*(char**)elem = strdup(toml2_string(node));
				return true;
			}
			break;

		case NEM_MARSHAL_FIXLEN:
		case NEM_MARSHAL_BINARY:
			NEM_panic("NEM_unmarshal_toml: binary types not supported");

		case NEM_MARSHAL_STRUCT:
			if (TOML2_TABLE == toml2_type(node)) {
				toml2_iter_t sub;
				if (toml2_iter_init(&sub, node)) {
					NEM_panic("NEM_unmarshal_toml: toml2_iter_init failed");
				}

				NEM_unmarshal_toml_iter(this->sub, &sub, elem);
				return true;
			}
	}

	return false;
}

static void
NEM_unmarshal_toml_array(
	const NEM_marshal_field_t *this,
	toml2_t                   *node,
	char                      *obj
) {
	if (TOML2_LIST != toml2_type(node)) {
		return;
	}

	char **pdata = (char**)(obj + this->offset_elem);
	size_t *psz = (size_t*)(obj + this->offset_len);
	size_t stride = NEM_marshal_field_stride(this);

	*psz = toml2_len(node);
	*pdata = NEM_malloc(stride * *psz);

	for (size_t i = 0; i < *psz; i += 1) {
		NEM_unmarshal_toml_field(
			this,
			toml2_index(node, i),
			(*pdata) + stride * i
		);
	}
}

static void
NEM_unmarshal_toml_iter(
	const NEM_marshal_map_t *this,
	toml2_iter_t            *iter,
	char                    *obj
) {
	toml2_t *sub;

	while (NULL != (sub = toml2_iter_next(iter))) {
		const char *key = toml2_name(sub);

		for (size_t i = 0; i < this->fields_len; i += 1) {
			const NEM_marshal_field_t *field = &this->fields[i];
			if (strcmp(field->name, key)) {
				continue;
			}

			if (field->type & NEM_MARSHAL_ARRAY) {
				NEM_unmarshal_toml_array(field, sub, obj);
			}
			else if (field->type & NEM_MARSHAL_PTR) {
				char **ptr = (char**)(obj + field->offset_elem);
				*ptr = NEM_malloc(NEM_marshal_field_stride(field));
				if (!NEM_unmarshal_toml_field(field, sub, *ptr)) {
					free(*ptr);
					*ptr = NULL;
				}
			}
			else {
				NEM_unmarshal_toml_field(
					field,
					sub,
					obj + field->offset_elem
				);
			}
		}
	}
}

static void
NEM_unmarshal_toml_obj(
	const NEM_marshal_map_t *this,
	toml2_t                 *node,
	char                    *obj
) {
	if (TOML2_TABLE != toml2_type(node)) {
		return;
	}

	toml2_iter_t iter;
	if (toml2_iter_init(&iter, node)) {
		NEM_panicf("NEM_unmarshal_toml: toml2_iter_init failed");
	}

	NEM_unmarshal_toml_iter(this, &iter, obj);
	toml2_iter_free(&iter);
}

NEM_err_t
NEM_unmarshal_toml(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *toml,
	size_t                   toml_len
) {
	if (elem_len != this->elem_size) {
		NEM_panic("NEM_unmarshal_toml: invalid elem_len");
	}

	bzero(elem, elem_len);

	toml2_t root;
	toml2_init(&root);

	int parse_ret = toml2_parse(&root, toml, toml_len);
	if (0 != parse_ret) {
		toml2_free(&root);
		return NEM_err_static("NEM_unmarshal_toml: invalid toml");
	}

	NEM_unmarshal_toml_obj(this, &root, elem);

	toml2_free(&root);
	return NEM_err_none;
}
