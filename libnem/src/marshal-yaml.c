#include <sys/types.h>
#include <yaml.h>

#include "nem.h"

typedef struct {
	char *buf;
	size_t buf_len;
	size_t buf_cap;
}
NEM_marshal_yaml_work_t;

static const char*
yaml_event_doc_start(yaml_event_t *ev)
{
	return "";
}

static const char*
yaml_event_alias(yaml_event_t *ev)
{
	return "";
}

static const char*
yaml_event_scalar(yaml_event_t *ev)
{
	static char *buf = NULL;
	free(buf);
	asprintf(&buf, "value=%s", ev->data.scalar.value);
	return buf;
}

static const char*
yaml_event_seq_start(yaml_event_t *ev)
{
	return "";
}

static const char*
yaml_event_map_start(yaml_event_t *ev)
{
	return "";
}

static const struct {
	yaml_event_type_t type;
	const char *name;
	const char *(*data_fn)(yaml_event_t *ev);
}
yaml_event_type_strs[] = {
	{ YAML_NO_EVENT,             "no-event",     NULL                  },
	{ YAML_STREAM_START_EVENT,   "stream-start", NULL                  },
	{ YAML_STREAM_END_EVENT,     "stream-end",   NULL                  },
	{ YAML_DOCUMENT_START_EVENT, "doc-start",    &yaml_event_doc_start },
	{ YAML_DOCUMENT_END_EVENT,   "doc-end",      NULL                  },
	{ YAML_ALIAS_EVENT,          "alias",        &yaml_event_alias     },
	{ YAML_SCALAR_EVENT,         "scalar",       &yaml_event_scalar    },
	{ YAML_SEQUENCE_START_EVENT, "seq-start",    &yaml_event_seq_start },
	{ YAML_SEQUENCE_END_EVENT,   "seq-end",      NULL                  },
	{ YAML_MAPPING_START_EVENT,  "map-start",    &yaml_event_map_start },
	{ YAML_MAPPING_END_EVENT,    "map-end",      NULL                  },
};

static void
yaml_event_print(yaml_event_t *ev)
{
	for (size_t i = 0; i < NEM_ARRSIZE(yaml_event_type_strs); i += 1) {
		if (yaml_event_type_strs[i].type == ev->type) {
			const char *(*data_fn)(yaml_event_t*) 
				= yaml_event_type_strs[i].data_fn;
			if (NULL == data_fn) {
				printf("%s\n", yaml_event_type_strs[i].name);
			}
			else {
				printf(
					"%s{%s}\n",
					yaml_event_type_strs[i].name,
					data_fn(ev)
				);
			}
			break;
		}
	}
}

static void
NEM_marshal_yaml_emit(yaml_emitter_t *emitter, yaml_event_t *ev)
{
	if (1 != yaml_emitter_emit(emitter, ev)) {
		NEM_panicf("yaml_emitter_emit failed: %s", emitter->problem);
	}
	if (1 != yaml_emitter_flush(emitter)) {
		NEM_panicf("yaml_emitter_flush failed: %s", emitter->problem);
	}
}

static void
NEM_marshal_yaml_emit_null(yaml_emitter_t *emitter)
{
	yaml_event_t ev;
	yaml_scalar_event_initialize(
		&ev,
		NULL,
		(yaml_char_t*) YAML_NULL_TAG,
		(yaml_char_t*) "~",
		1,
		1,
		1,
		YAML_ANY_SCALAR_STYLE
	);
	NEM_marshal_yaml_emit(emitter, &ev);
}

static int
NEM_marshal_yaml_work(void *vwork, unsigned char *buf, size_t sz)
{
	NEM_marshal_yaml_work_t *work = vwork;
	size_t remain = work->buf_cap - work->buf_len;

	if (remain < sz) {
		while (remain < sz) {
			work->buf_cap += 1024;
			remain += 1024;
		}

		work->buf = NEM_panic_if_null(realloc(work->buf, work->buf_cap));
		bzero(work->buf + work->buf_len, work->buf_cap - work->buf_len);
	}

	memcpy(work->buf + work->buf_len, buf, sz);
	work->buf_len += sz;
	return 1;
}

static NEM_err_t
NEM_marshal_yaml_map(
	const NEM_marshal_map_t *this,
	yaml_emitter_t          *emitter,
	const char              *obj
);

static bool
NEM_marshal_yaml_skip_field(const NEM_marshal_field_t *this, const char *obj)
{
	if ((NEM_MARSHAL_PTR|NEM_MARSHAL_STRING) == this->type) {
		// NB: non-NULL pointers to NULL strings should just be NULL.
		const char **ptr = *(const char***)(obj + this->offset_elem);
		return ptr == NULL || *ptr == NULL;
	}
	else if (NEM_MARSHAL_STRING == (this->type & NEM_MARSHAL_TYPEMASK)) {
		// NB: Don't skip empty strings; NULL/empty have distinct encodings.
		const char *val = *(const char**)(obj + this->offset_elem);
		return val == NULL;
	}
	else if (NEM_MARSHAL_PTR & this->type) {
		const char *ptr = *(const char**)(obj + this->offset_elem);
		return ptr == NULL;
	}

	return false;
}

static NEM_err_t
NEM_marshal_yaml_field(
	const NEM_marshal_field_t *this,
	yaml_emitter_t            *emitter,
	const char                *elem
) {
	char *buf = NULL;
	yaml_event_t ev = {0};

	switch (this->type & NEM_MARSHAL_TYPEMASK) {
#		define NEM_MARSHAL_VISITOR(NTYPE, CTYPE) \
		case NTYPE: \
			asprintf(&buf, "%ld", (int64_t)*(CTYPE*)elem); \
			yaml_scalar_event_initialize( \
				&ev, \
				NULL, \
				NULL, \
				(unsigned char*) buf, \
				strlen(buf), \
				1, \
				1, \
				YAML_PLAIN_SCALAR_STYLE \
			); \
			break;
		NEM_MARSHAL_CASE_VISIT_SINT_TYPES
#		undef NEM_MARSHAL_VISITOR

#		define NEM_MARSHAL_VISITOR(NTYPE, CTYPE) \
		case NTYPE: \
			asprintf(&buf, "%lu", (uint64_t)*(CTYPE*)elem); \
			yaml_scalar_event_initialize( \
				&ev, \
				NULL, \
				NULL, \
				(unsigned char*) buf, \
				strlen(buf), \
				1, \
				1, \
				YAML_PLAIN_SCALAR_STYLE \
			); \
			break;
		NEM_MARSHAL_CASE_VISIT_UINT_TYPES
#		undef NEM_MARSHAL_VISITOR

		case NEM_MARSHAL_BOOL:
			yaml_scalar_event_initialize(
				&ev,
				NULL,
				NULL,
				(unsigned char*)((*(bool*)elem) ? "y" : "n"),
				1,
				1,
				1,
				YAML_PLAIN_SCALAR_STYLE
			);
			break;

		case NEM_MARSHAL_STRUCT:
			return NEM_marshal_yaml_map(this->sub, emitter, elem);

		case NEM_MARSHAL_STRING: {
			unsigned char *val = *(unsigned char**) elem;
			if (NULL == val) {
				NEM_marshal_yaml_emit_null(emitter);
				return NEM_err_none;
			}
			else {
				yaml_scalar_event_initialize(
					&ev,
					NULL,
					NULL,
					val,
					strlen((char*)val),
					1,
					1,
					YAML_ANY_SCALAR_STYLE
				);
			}
			break;
		}

		default:
			NEM_panic("uhh what");
	}

	NEM_marshal_yaml_emit(emitter, &ev);
	free(buf);
	return NEM_err_none;
}

static NEM_err_t
NEM_marshal_yaml_array(
	const NEM_marshal_field_t *this,
	yaml_emitter_t            *emitter,
	const char                *obj
) {
	const char *elem = *(char**)(obj + (this->offset_elem));
	size_t elem_len = *(size_t*)(obj + this->offset_len);
	size_t stride = NEM_marshal_field_stride(this);

	yaml_event_t ev_start;
	yaml_sequence_start_event_initialize(
		&ev_start,
		NULL,
		NULL,
		1,
		YAML_BLOCK_SEQUENCE_STYLE
	);
	NEM_marshal_yaml_emit(emitter, &ev_start);

	for (size_t i = 0; i < elem_len; i += 1) {
		const char *pelem = elem + (i * stride);
		// NB: Array elements are never skipped.
		NEM_err_t err = NEM_marshal_yaml_field(this, emitter, pelem);
		if (!NEM_err_ok(err)) {
			return err;
		}
	}

	yaml_event_t ev_end;
	yaml_sequence_end_event_initialize(&ev_end);
	NEM_marshal_yaml_emit(emitter, &ev_end);

	return NEM_err_none;
}

static NEM_err_t
NEM_marshal_yaml_map(
	const NEM_marshal_map_t *this,
	yaml_emitter_t          *emitter,
	const char              *obj
) {
	yaml_event_t ev_start;
	yaml_mapping_start_event_initialize(
		&ev_start,
		NULL,
		NULL,
		0,
		YAML_BLOCK_MAPPING_STYLE
	);
	NEM_marshal_yaml_emit(emitter, &ev_start);

	for (size_t i = 0; i < this->fields_len; i += 1) {
		const NEM_marshal_field_t *field = &this->fields[i];
		bool is_ptr = field->type & NEM_MARSHAL_PTR;
		bool is_ary = field->type & NEM_MARSHAL_ARRAY;

		if (is_ary && is_ptr) {
			NEM_panic("NEM_marshal_yaml_obj: field with both ARRAY and PTR");
		}
		if (NEM_marshal_yaml_skip_field(field, obj)) {
			continue;
		}

		yaml_event_t ev;
		yaml_scalar_event_initialize(
			&ev,
			NULL,
			NULL,
			(unsigned char*)field->name,
			strlen(field->name),
			1,
			1,
			YAML_PLAIN_SCALAR_STYLE
		);
		NEM_marshal_yaml_emit(emitter, &ev);

		NEM_err_t err = NEM_err_none;
		if (is_ary) {
			err = NEM_marshal_yaml_array(field, emitter, obj);
		}
		else if (is_ptr) {
			char *fieldelem = *(char**)(obj + field->offset_elem);
			if (NULL == fieldelem) {
				NEM_marshal_yaml_emit_null(emitter);
			}
			else {
				err = NEM_marshal_yaml_field(field, emitter, fieldelem);
			}
		}
		else {
			err = NEM_marshal_yaml_field(
				field,
				emitter,
				obj + field->offset_elem
			);
		}

		if (!NEM_err_ok(err)) {
			return err;
		}
	}

	yaml_event_t ev_end;
	yaml_mapping_end_event_initialize(&ev_end);
	NEM_marshal_yaml_emit(emitter, &ev_end);

	return NEM_err_none;
}

NEM_err_t
NEM_marshal_yaml(
	const NEM_marshal_map_t *this,
	void                   **out,
	size_t                  *out_len,
	const void              *elem,
	size_t                   elem_len
) {
	NEM_marshal_yaml_work_t work = {0};

	yaml_emitter_t emitter;
	yaml_emitter_initialize(&emitter);
	yaml_emitter_set_output(&emitter, &NEM_marshal_yaml_work, &work);
	yaml_emitter_set_indent(&emitter, 2);
	yaml_emitter_set_break(&emitter, YAML_LN_BREAK);
	yaml_emitter_set_canonical(&emitter, 0);
	yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
	yaml_emitter_open(&emitter);

	yaml_event_t ev_start;
	yaml_document_start_event_initialize(
		&ev_start,
		NULL,
		NULL,
		NULL,
		1
	);
	NEM_marshal_yaml_emit(&emitter, &ev_start);

	NEM_err_t err = NEM_marshal_yaml_map(this, &emitter, elem);
	if (!NEM_err_ok(err)) {
		free(work.buf);
		return err;
	}

	yaml_event_t ev_end;
	yaml_document_end_event_initialize(&ev_end, 1);
	NEM_marshal_yaml_emit(&emitter, &ev_end);

	yaml_emitter_close(&emitter);
	yaml_emitter_delete(&emitter);

	*out = work.buf;
	*out_len = work.buf_len;
	return NEM_err_none;
}

static NEM_err_t
NEM_unmarshal_yaml_map(
	const NEM_marshal_map_t *this,
	yaml_parser_t           *parser,
	yaml_event_t            *prev_ev,
	char                    *obj
);

static bool
NEM_unmarshal_yaml_bool(const char *str, bool *out)
{
	// UGH. This should probably just use a regex.
	static const struct {
		const char *str;
		bool        val;
	}
	table[] = {
		{ "y",      true  },
		{ "Y",      true  },
		{ "yes",    true  },
		{ "Yes",    true  },
		{ "YES",    true  },
		{ "n",      false },
		{ "N",      false },
		{ "no",     false },
		{ "No",     false },
		{ "NO",     false },
		{ "true",   true  },
		{ "True",   true  },
		{ "TRUE",   true  },
		{ "false",  false },
		{ "False",  false },
		{ "FALSE",  false },
		{ "on",     true  },
		{ "On",     true  },
		{ "ON",     true  },
		{ "off",    false },
		{ "Off",    false },
		{ "OFF",    false },
	};
	for (size_t i = 0; i < NEM_ARRSIZE(table); i += 1) {
		if (!strcmp(str, table[i].str)) {
			*out = table[i].val;
			return true;
		}
	}
	return false;
}

static NEM_err_t
NEM_unmarshal_yaml_field(
	const NEM_marshal_field_t *this,
	yaml_parser_t             *parser,
	yaml_event_t              *prev_ev,
	char                      *elem,
	bool                      *wrote
) {
	bool tmp_wrote;
	if (NULL == wrote) {
		wrote = &tmp_wrote;
	}

	if (YAML_MAPPING_START_EVENT == prev_ev->type) {
		*wrote = true;
		return NEM_unmarshal_yaml_map(this->sub, parser, prev_ev, elem);
	}

	if (YAML_SCALAR_EVENT != prev_ev->type) {
		NEM_panicf("NEM_unmarshal_yaml_field: invalid prev_ev");
	}

	if (NULL != prev_ev->data.scalar.anchor) {
		return NEM_err_static("NEM_unmarshal_yaml: scalar anchors unsupported");
	}
	char *value = (char*)prev_ev->data.scalar.value;

	switch (this->type & NEM_MARSHAL_TYPEMASK) {
#		define NEM_MARSHAL_VISITOR(NTYPE, CTYPE) \
		case NTYPE: { \
			char *end = NULL; \
			int64_t ival = strtoll(value, &end, 10); \
			if (NULL != end && 0 == end[0]) { \
				*wrote = true; \
				*(CTYPE*)elem = (CTYPE) ival; \
			} \
			break; \
		}
		NEM_MARSHAL_CASE_VISIT_SINT_TYPES
#		undef NEM_MARSHAL_VISITOR

#		define NEM_MARSHAL_VISITOR(NTYPE, CTYPE) \
		case NTYPE: { \
			char *end = NULL; \
			uint64_t ival = strtoull(value, &end, 10); \
			if (NULL != end && 0 == end[0]) { \
				*wrote = true; \
				*(CTYPE*)elem = (CTYPE) ival; \
			} \
			break; \
		}
		NEM_MARSHAL_CASE_VISIT_UINT_TYPES
#		undef NEM_MARSHAL_VISITOR

		case NEM_MARSHAL_BOOL:
			*wrote = NEM_unmarshal_yaml_bool(value, (bool*) elem);
			break;

		case NEM_MARSHAL_STRING:
			// XXX: This ... this does not seem correct.
			if (!prev_ev->data.scalar.quoted_implicit && !strcmp(value, "~")) {
				*(char**)elem = NULL;
				*wrote = false;
			}
			else {
				*(char**)elem = strdup(value);
				*wrote = true;
			}
			break;

		case NEM_MARSHAL_BINARY:
		case NEM_MARSHAL_FIXLEN:
			return NEM_err_static("NEM_unmarshal_yaml: unsupported types");

		case NEM_MARSHAL_STRUCT:
			return NEM_err_static(
				"NEM_unmarshal_yaml: expected struct, got scalar"
			);
	}

	return NEM_err_none;
}

static NEM_err_t
NEM_unmarshal_yaml_array(
	const NEM_marshal_field_t *this,
	yaml_parser_t             *parser,
	yaml_event_t              *prev_ev,
	char                      *obj
) {
	if (YAML_SEQUENCE_START_EVENT != prev_ev->type) {
		NEM_panic("NEM_unmarshal_yaml_array: invalid prev_ev");
	}

	char **out_ptr = (char**)(obj + this->offset_elem);
	size_t *out_sz = (size_t*)(obj + this->offset_len);

	NEM_err_t err = NEM_err_none;
	char *buf = NULL;
	size_t buf_cap = 0;
	size_t buf_len = 0;
	size_t stride = NEM_marshal_field_stride(this);
	yaml_event_t ev = {0};

	while (NEM_err_ok(err)) {
		if (!yaml_parser_parse(parser, &ev)) {
			err = NEM_err_static(
				"NEM_unmarshal_yaml: yaml_parser_parse failed"
			);
			continue;
		}
		if (YAML_SEQUENCE_END_EVENT == ev.type) {
			break;
		}

		if (buf_len == buf_cap) {
			buf_cap = buf_cap ? buf_cap * 2 : 4;
			buf = NEM_panic_if_null(realloc(buf, buf_cap * stride));
			bzero(buf + (buf_len * stride), (buf_cap - buf_len) * stride);
		}

		err = NEM_unmarshal_yaml_field(
			this,
			parser,
			&ev,
			buf + (stride * buf_len),
			NULL
		);
		buf_len += 1;

		yaml_event_delete(&ev);
	}

	yaml_event_delete(&ev);

	if (!NEM_err_ok(err)) {
		free(buf);
	}
	else {
		*out_ptr = buf;
		*out_sz = buf_len;
	}

	return err;
}

static NEM_err_t
NEM_unmarshal_yaml_skip_input(yaml_parser_t *parser, yaml_event_t *prev_ev)
{
	NEM_err_t err = NEM_err_none;
	yaml_event_t ev;
	yaml_event_type_t stop_at;

	if (YAML_MAPPING_START_EVENT == prev_ev->type) {
		stop_at = YAML_MAPPING_END_EVENT;
	}
	else if (YAML_SEQUENCE_START_EVENT == prev_ev->type) {
		stop_at = YAML_SEQUENCE_END_EVENT;
	}
	else {
		return NEM_err_none;
	}

	while (NEM_err_ok(err)) {
		if (!yaml_parser_parse(parser, &ev)) {
			return NEM_err_static(
				"NEM_unmarshal_yaml: yaml_parser_parse failed"
			);
		}
		if (YAML_MAPPING_END_EVENT == ev.type) {
			break;
		}
		else if (
			YAML_MAPPING_START_EVENT == ev.type
			|| YAML_SEQUENCE_START_EVENT == ev.type
		) {
			err = NEM_unmarshal_yaml_skip_input(parser, &ev);
		}
		yaml_event_delete(&ev);
	}
	yaml_event_delete(&ev);

	return err;
}

static NEM_err_t
NEM_unmarshal_yaml_map(
	const NEM_marshal_map_t *this,
	yaml_parser_t           *parser,
	yaml_event_t            *prev_ev,
	char                    *obj
) {
	if (YAML_MAPPING_START_EVENT != prev_ev->type) {
		NEM_panic("NEM_unmarshal_yaml_map: invalid prev_ev");
	}

	yaml_event_t key_ev = {0};
	yaml_event_t val_ev = {0};
	NEM_err_t err = NEM_err_none;

	while (NEM_err_ok(err)) {
		if (!yaml_parser_parse(parser, &key_ev)) {
			err = NEM_err_static(
				"NEM_unmarshal_yaml: yaml_parser_parse failed"
			);
			break;
		}
		if (YAML_MAPPING_END_EVENT == key_ev.type) {
			break;
		}
		if (YAML_SCALAR_EVENT != key_ev.type) {
			// XXX: Hook up alias events or something here.
			err = NEM_err_static(
				"NEM_unmarshal_yaml: only scalar keys implemented"
			);
			break;
		}
		if (!yaml_parser_parse(parser, &val_ev)) {
			err = NEM_err_static(
				"NEM_unmarshal_yaml: yaml_parser_parse failed"
			);
			break;
		}

		const NEM_marshal_field_t *field = NULL;
		char *key_name = (char*)key_ev.data.scalar.value;
		for (size_t i = 0; i < this->fields_len; i += 1) {
			if (!strcmp(key_name, this->fields[i].name)) {
				field = &this->fields[i];
				break;
			}
		}
		if (NULL == field) {
			// NB: Handle all no-field input skipping here.
			err = NEM_unmarshal_yaml_skip_input(parser, &val_ev);
		}
		else {
			bool is_array = NEM_MARSHAL_ARRAY & field->type;
			bool is_ptr = NEM_MARSHAL_PTR & field->type;

			if (is_array && is_ptr) {
				err = NEM_err_static(
					"NEM_unmarshal_yaml: ptr to array not allowed"
				);
				break;
			}

			if (is_array) {
				err = NEM_unmarshal_yaml_array(
					field,
					parser,
					&val_ev,
					obj
				);
			}
			else if (is_ptr) {
				char **ptr = (char**)(obj + field->offset_elem);
				bool wrote = false;
				*ptr = NEM_malloc(NEM_marshal_field_stride(field));
				err = NEM_unmarshal_yaml_field(
					field,
					parser,
					&val_ev,
					*ptr,
					&wrote
				);
				if (!NEM_err_ok(err) || !wrote) {
					free(*ptr);
					*ptr = NULL;
				}
			}
			else {
				err = NEM_unmarshal_yaml_field(
					field,
					parser,
					&val_ev,
					obj + field->offset_elem,
					NULL
				);
			}
		}

		yaml_event_delete(&key_ev);
		yaml_event_delete(&val_ev);
	}

	yaml_event_delete(&key_ev);
	yaml_event_delete(&val_ev);
	return err;
}

NEM_err_t
NEM_unmarshal_yaml(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *yaml,
	size_t                   yaml_len
) {
	yaml_parser_t parser;
	if (!yaml_parser_initialize(&parser)) {
		return NEM_err_static(
			"NEM_unmarshal_yaml: yaml_parser_initialize failed"
		);
	}
	yaml_parser_set_input_string(&parser, yaml, yaml_len);
	yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);

	yaml_event_t ev;

	const yaml_event_type_t first_events[] = {
		YAML_STREAM_START_EVENT,
		YAML_DOCUMENT_START_EVENT,
		YAML_MAPPING_START_EVENT,
	};
	for (size_t i = 0; i < NEM_ARRSIZE(first_events); i += 1) {
		if (!yaml_parser_parse(&parser, &ev)) {
			yaml_event_print(&ev);
			return NEM_err_static(
				"NEM_unmarshal_yaml: could not parse initial tokens"
			);
		}
		else if (first_events[i] != ev.type) {
			yaml_event_print(&ev);
			return NEM_err_static(
				"NEM_unmarshal_yaml: unexpected initial token"
			);
		}
	}

	bzero(elem, elem_len);

	NEM_err_t err = NEM_unmarshal_yaml_map(this, &parser, &ev, elem);
	yaml_parser_delete(&parser);

	if (!NEM_err_ok(err)) {
		NEM_unmarshal_free(this, elem, elem_len);
	}

	return err;
}
