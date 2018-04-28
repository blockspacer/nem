#pragma once

// NEM_marshal_type_t represents a C type. It's used to construct
// NEM_marshal_field_t's to indicate the C representation of data.
typedef enum {
	NEM_MARSHAL_UINT8  = 1,  // uint8_t
	NEM_MARSHAL_UINT16 = 2,  // uint16_t
	NEM_MARSHAL_UINT32 = 3,  // uint32_t
	NEM_MARSHAL_UINT64 = 4,  // uint64_t
	NEM_MARSHAL_INT8   = 5,  // int8_t
	NEM_MARSHAL_INT16  = 6,  // int16_t
	NEM_MARSHAL_INT32  = 7,  // int32_t
	NEM_MARSHAL_INT64  = 8,  // int64_t
	NEM_MARSHAL_STRING = 9,  // const char*, heap-allocated
	NEM_MARSHAL_FIXLEN = 10, // uint8_t[N], offset_len should be N
	NEM_MARSHAL_BINARY = 11, // uint8_t+size_t, heap-allocated
	NEM_MARSHAL_STRUCT = 12, // embedded structure
	NEM_MARSHAL_BOOL   = 13, // bool.
}
NEM_marshal_type_t;

// NEM_MARSHAL_CASE_VISIT_INT_TYPES is a vistor-style macro that expands
// NEM_MARSHAL_VISTOR(NEM_marshal_type_t, C-type) for each int type.
// This is to simplify the marshalling logic since for some formats these
// are all basically the same code (since several formats just expand them
// out to int/uint64_t and we truncate them on the C side).
#define NEM_MARSHAL_CASE_VISIT_SINT_TYPES \
	NEM_MARSHAL_VISITOR(NEM_MARSHAL_INT8, int8_t); \
	NEM_MARSHAL_VISITOR(NEM_MARSHAL_INT16, int16_t); \
	NEM_MARSHAL_VISITOR(NEM_MARSHAL_INT32, int32_t); \
	NEM_MARSHAL_VISITOR(NEM_MARSHAL_INT64, int64_t);
#define NEM_MARSHAL_CASE_VISIT_UINT_TYPES \
	NEM_MARSHAL_VISITOR(NEM_MARSHAL_UINT8, uint8_t); \
	NEM_MARSHAL_VISITOR(NEM_MARSHAL_UINT16, uint16_t); \
	NEM_MARSHAL_VISITOR(NEM_MARSHAL_UINT32, uint32_t); \
	NEM_MARSHAL_VISITOR(NEM_MARSHAL_UINT64, uint64_t);
#define NEM_MARSHAL_CASE_VISIT_INT_TYPES \
	NEM_MARSHAL_CASE_VISIT_SINT_TYPES \
	NEM_MARSHAL_CASE_VISIT_UINT_TYPES

// NB: NEM_marshal_type_t values cannot exceed the 32 currently -- both the
// typemask and the array flag need to be made larger to handle those values.
static const int NEM_MARSHAL_TYPEMASK = 64 - 1;

// NEM_MARSHAL_ARRAY is a flag that can be combined with any other
// NEM_marshal_type_t. It cannot be combined with other flags. It also
// cannot be combined with NEM_MARSHAL_FIXLEN (which uses the offset_len
// parameter internally). It indicates that the type is a pointer to an
// array, and the length of the array is at offset_len.
static const int NEM_MARSHAL_ARRAY    = 64; // struct*+size_t, heap-allocated

// NEM_MARSHAL_PTR is a flag that can be combined with any other
// NEM_marshal_type_t. It indicates that the value should be heap-allocated
// rather than statically allocated. If the value is omitted during 
// unmarshalling, the pointer will be NULL (likewise, it'll be omitted
// during marshalling if the value is NULL). It cannot be combined with
// NEM_MARSHAL_ARRAY.
static const int NEM_MARSHAL_PTR      = 128; // type*, heap-allocated
static const int NEM_MARSHAL_STRUCTPTR = NEM_MARSHAL_PTR|NEM_MARSHAL_STRUCT;

typedef struct NEM_marshal_field_t NEM_marshal_field_t;
typedef struct NEM_marshal_map_t NEM_marshal_map_t;

// NEM_marshal_field_type_name returns a string representation of the field
// type. It's statically allocated.
const char *NEM_marshal_field_type_name(NEM_marshal_type_t);

// NEM_marshal_field_stride returns the exepected length of the field when
// marshalled into C.
size_t NEM_marshal_field_stride(const NEM_marshal_field_t*);

// NEM_marshal_field_t contains the metadata for a single field of a struct.
struct NEM_marshal_field_t {
	// name is the field name for text formats.
	const char *name;

	// type is the C type of the field.
	NEM_marshal_type_t type;

	// offset_elem is the offset into the structure that the field should be
	// placed.
	off_t offset_elem;

	// offset_len is used for types that have a length parameter. It points
	// to a size_t field representing the length. For NEM_MARSHAL_FIXLEN,
	// it's the length of the field rather than an offset into the struct.
	off_t offset_len;

	// sub is a pointer to the struct metadata for a struct-type field.
	const NEM_marshal_map_t *sub;
};

// NEM_marshal_map_t is the full field metadata for a single struct - it
// describes the layout of the struct and thus how to read/write data to/from
// it for marshalling.
struct NEM_marshal_map_t {
	// type_name is the name of the type being marshalled. It's used mostly
	// for error messages.
	const char *type_name;

	// fields is the array of all the struct's fields.
	const NEM_marshal_field_t *fields;

	// fields_len is the number of fields pointed at by fields.
	size_t fields_len;

	// elem_size is sizeof(elem) that's being marshalled; used for dynamic
	// allocation.
	size_t elem_size;
};

// NEM_unmarshal_bson unmarshals the provided bson/bson_len into the provided
// element. For safety, the elem_len must also be passed. This may make 
// additional heap allocations -- free the element with NEM_unmarshal_free to
// ensure that everything's cleaned up.
NEM_err_t
NEM_unmarshal_bson(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *bson,
	size_t                   bson_len
);
// NEM_marshal_bson marshals the provided element into out/out_len. After use,
// out must be passed to free.
NEM_err_t
NEM_marshal_bson(
	const NEM_marshal_map_t *this,
	void                   **out,
	size_t                  *out_len,
	const void              *elem,
	size_t                   elem_len
);

// NEM_unmarshal_toml... unmarshals... toml. Unlike BSON, toml isn't meant to
// be generated by machines so no marshal method is provided (mostly because
// round-tripping it would strip out a lot of important metadata -- whitespace
// and comments and such).
NEM_err_t NEM_unmarshal_toml(
	const NEM_marshal_map_t *mapping,
	void                    *data,
	size_t                   data_len,
	const void              *toml,
	size_t                   toml_len
);

// NEM_unmarshal_json unmarshals JSON into an object using the provided
// mapping. json_len cannot exceed 2GB (blame json-c). Free the out object 
// with NEM_unmarshal_free.
NEM_err_t
NEM_unmarshal_json(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *json,
	size_t                   json_len
);
NEM_err_t
NEM_marshal_json(
	const NEM_marshal_map_t *this,
	void                   **out,
	size_t                  *out_len,
	const void              *elem,
	size_t                   elem_len
);

// NEM_unmarshal_yaml is. yep.
NEM_err_t
NEM_unmarshal_yaml(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *json,
	size_t                   json_len
);
NEM_err_t
NEM_marshal_yaml(
	const NEM_marshal_map_t *this,
	void                   **out,
	size_t                  *out_len,
	const void              *elem,
	size_t                   elem_len
);

// NEM_unmarshal_free uses the struct metadata to free any heap-allocated 
// fields in the passed elem.
void
NEM_unmarshal_free(
	const NEM_marshal_map_t *mapping,
	void                    *elem,
	size_t                   elem_len
);

// Mappings for primitive arrays. This is a stupid hack.
extern const NEM_marshal_map_t NEM_marshal_uint64s_m;
