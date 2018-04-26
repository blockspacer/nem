#define O(F) offsetof(TYPE, F)
#define M(F) NEM_MSIZE(TYPE, F)
#define NAME(t) #t
#define MAP(MAPNAME, FIELDS) \
	const NEM_marshal_map_t MAPNAME = { \
		.fields     = FIELDS, \
		.fields_len = NEM_ARRSIZE(FIELDS), \
		.elem_size  = sizeof(TYPE), \
		.type_name  = NAME(TYPE), \
	}
