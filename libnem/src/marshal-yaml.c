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
	int type = this->type & NEM_MARSHAL_TYPEMASK;
	if (NEM_MARSHAL_STRING == type) {
		const char *val = *(const char**)(obj + this->offset_elem);
		return val == NULL || val[0] == 0;
	}
	if (NEM_MARSHAL_PTR & this->type) {
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
			yaml_scalar_event_initialize(
				&ev,
				NULL,
				NULL,
				val ? val : (unsigned char*) "",
				val ? strlen((char*)val) : 0,
				1,
				1,
				YAML_ANY_SCALAR_STYLE
			);
			break;
		}

		default:
			NEM_panic("uhh what");
	}

	if (ev.type == 0) {
		NEM_panic("fuck off");
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
			err = NEM_marshal_yaml_field(field, emitter, fieldelem);
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

NEM_err_t
NEM_unmarshal_yaml(
	const NEM_marshal_map_t *this,
	void                    *elem,
	size_t                   elem_len,
	const void              *json,
	size_t                   json_len
) {
	return NEM_err_static("TODO");
}
