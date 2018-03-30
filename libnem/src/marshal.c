#include "nem.h"

const char*
NEM_marshal_field_type_name(NEM_marshal_type_t type)
{
	off_t off = (type & NEM_MARSHAL_ARRAY) ? 2 : 0;

	switch (type & NEM_MARSHAL_TYPEMASK) {
		case NEM_MARSHAL_UINT8:  return &"[]uint8"[off];
		case NEM_MARSHAL_UINT16: return &"[]uint16"[off];
		case NEM_MARSHAL_UINT32: return &"[]uint32"[off];
		case NEM_MARSHAL_UINT64: return &"[]uint64"[off];
		case NEM_MARSHAL_INT8:   return &"[]int8"[off];
		case NEM_MARSHAL_INT16:  return &"[]int16"[off];
		case NEM_MARSHAL_INT32:  return &"[]int32"[off];
		case NEM_MARSHAL_INT64:  return &"[]int64"[off];
		case NEM_MARSHAL_BOOL:   return &"[]bool"[off];
		case NEM_MARSHAL_FIXLEN: return &"[]fixlen"[off];
		case NEM_MARSHAL_STRING: return &"[]string"[off];
		case NEM_MARSHAL_BINARY: return &"[]binary"[off];
		case NEM_MARSHAL_STRUCT: return &"[]struct"[off];
	}

	NEM_panicf("field_type_name: invalid type %d", type);
}

size_t 
NEM_marshal_field_stride(const NEM_marshal_field_t *field)
{
	switch (field->type & ~NEM_MARSHAL_TYPEMASK) {
		case NEM_MARSHAL_UINT8:  return sizeof(uint8_t);
		case NEM_MARSHAL_UINT16: return sizeof(uint16_t);
		case NEM_MARSHAL_UINT32: return sizeof(uint32_t);
		case NEM_MARSHAL_UINT64: return sizeof(uint64_t);
		case NEM_MARSHAL_INT8:   return sizeof(int8_t);
		case NEM_MARSHAL_INT16:  return sizeof(int16_t);
		case NEM_MARSHAL_INT32:  return sizeof(int32_t);
		case NEM_MARSHAL_INT64:  return sizeof(int64_t);
		case NEM_MARSHAL_BOOL:   return sizeof(bool);
		case NEM_MARSHAL_FIXLEN: return field->offset_len;
		case NEM_MARSHAL_STRING: return sizeof(char*);
	}

	NEM_panicf("field_stride: invalid type %d", field->type);
}

static void
free_field(
	const NEM_marshal_field_t *field,
	char                      *elem,
	NEM_marshal_type_t         type
) {
	if (0 == type) {
		type = field->type;
	}

	if (NEM_MARSHAL_ARRAY & type) {
		type &= ~NEM_MARSHAL_ARRAY;
		size_t *psz = (size_t*)(elem + field->offset_len);
		size_t stride = NEM_marshal_field_stride(field);
		char *base = *(char**)(elem + field->offset_elem);

		for (size_t i = 0; i < *psz; i += 1) {
			free_field(
				field,
				base + (stride * i),
				type
			);
		}
		free(base);
		return;
	}
	if (NEM_MARSHAL_PTR & type) {
		type &= ~NEM_MARSHAL_PTR;
		char *base = *(char**)(elem + field->offset_elem);
		if (NULL != base) {
			free_field(field, base, type);
			free(base);
		}
	}

	switch (type) {
		// Basic fields that don't need anything done.
		case NEM_MARSHAL_UINT8:
		case NEM_MARSHAL_UINT16:
		case NEM_MARSHAL_UINT32:
		case NEM_MARSHAL_INT8:
		case NEM_MARSHAL_INT16:
		case NEM_MARSHAL_INT32:
		case NEM_MARSHAL_INT64:
		case NEM_MARSHAL_UINT64:
		case NEM_MARSHAL_FIXLEN:
		case NEM_MARSHAL_BOOL:
			break;

		case NEM_MARSHAL_BINARY:
		case NEM_MARSHAL_STRING: {
			char **pdata = (char**)(elem + field->offset_elem);
			free(*pdata);
			break;
		}
			
		case NEM_MARSHAL_STRUCT: {
			// This is defined inline (e.g., not a pointer) but it needs
			// to be recursively freed.
			NEM_unmarshal_free(
				field->sub,
				elem + field->offset_elem, 
				field->sub->elem_size
			);
			break;
		}
	}
}

void
NEM_unmarshal_free(
	const NEM_marshal_map_t *mapping,
	void                    *elem,
	size_t                   elem_len
) {
	if (elem_len != mapping->elem_size) {
		NEM_panicf(
			"NEM_unmarshal_free: elem_len for %s doesn't match metadata: "
			"%d != %d",
			mapping->type_name ? mapping->type_name : "(missing type_name)",
			elem_len,
			mapping->elem_size
		);
	}

	for (size_t i = 0; i < mapping->fields_len; i += 1) {
		const NEM_marshal_field_t *field = &mapping->fields[i];
		free_field(field, elem, 0);
	}
}
