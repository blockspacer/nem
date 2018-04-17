#include "nem.h"

NEM_err_t
NEM_msghdr_new(NEM_msghdr_t **out, const void *bs, size_t len)
{
	NEM_msghdr_t *this = NEM_malloc(sizeof(NEM_msghdr_t));
	NEM_err_t err = NEM_unmarshal_bson(
		&NEM_msghdr_m,
		this,
		sizeof(*this),
		bs,
		len
	);
	if (!NEM_err_ok(err)) {
		free(this);
		return err;
	}

	if (
		NULL != this->err
		&& NULL != this->err->reason
		&& 0 == this->err->reason[0]
	) {
		free((char*)this->err->reason);
		this->err->reason = NULL;
	}

	*out = this;
	return NEM_err_none;
}

void
NEM_msghdr_free(NEM_msghdr_t *hdr)
{
	if (NULL != hdr) {
		NEM_unmarshal_free(&NEM_msghdr_m, hdr, sizeof(*hdr));
		free(hdr);
	}
}

NEM_err_t
NEM_msghdr_pack(NEM_msghdr_t *hdr, void **out, size_t *outlen)
{
	return NEM_marshal_bson(&NEM_msghdr_m, out, outlen, hdr, sizeof(*hdr));
}

#define O(F) offsetof(TYPE, F)
#define M(F) NEM_MSIZE(TYPE, F)
#define NAME(t) #t

#define TYPE NEM_msghdr_err_t
static const NEM_marshal_field_t msghdr_err_fs[] = {
	{ "code",   NEM_MARSHAL_INT64,  O(code),   -1, NULL },
	{ "reason", NEM_MARSHAL_STRING, O(reason), -1, NULL },
};
const NEM_marshal_map_t NEM_msghdr_err_m = {
	.fields     = msghdr_err_fs,
	.fields_len = NEM_ARRSIZE(msghdr_err_fs),
	.elem_size  = sizeof(TYPE),
	.type_name  = NAME(TYPE),
};
#undef TYPE

#define TYPE NEM_msghdr_route_t
static const NEM_marshal_field_t msghdr_route_fs[] = {
	{ "cluster", NEM_MARSHAL_STRING, O(cluster), -1, NULL },
	{ "host",    NEM_MARSHAL_STRING, O(host),    -1, NULL },
	{ "inst",    NEM_MARSHAL_STRING, O(inst),    -1, NULL },
	{ "obj",     NEM_MARSHAL_STRING, O(obj),     -1, NULL },
};
const NEM_marshal_map_t NEM_msghdr_route_m = {
	.fields     = msghdr_route_fs,
	.fields_len = NEM_ARRSIZE(msghdr_route_fs),
	.elem_size  = sizeof(TYPE),
	.type_name  = NAME(TYPE),
};
#undef TYPE

#define TYPE NEM_msghdr_time_t
static const NEM_marshal_field_t msghdr_time_fs[] = {
	{ "timeout_ms", NEM_MARSHAL_UINT32, O(timeout_ms), -1, NULL },
};
const NEM_marshal_map_t NEM_msghdr_time_m = {
	.fields     = msghdr_time_fs,
	.fields_len = NEM_ARRSIZE(msghdr_time_fs),
	.elem_size  = sizeof(TYPE),
	.type_name  = NAME(TYPE),
};
#undef TYPE

#define TYPE NEM_msghdr_t
static const NEM_marshal_field_t msghdr_fs[] = {
	{ "err",   NEM_MARSHAL_STRUCTPTR, O(err),   -1, &NEM_msghdr_err_m   },
	{ "route", NEM_MARSHAL_STRUCTPTR, O(route), -1, &NEM_msghdr_route_m },
	{ "time",  NEM_MARSHAL_STRUCTPTR, O(time),  -1, &NEM_msghdr_time_m  },
};
const NEM_marshal_map_t NEM_msghdr_m = {
	.fields     = msghdr_fs,
	.fields_len = NEM_ARRSIZE(msghdr_fs),
	.elem_size  = sizeof(TYPE),
	.type_name  = NAME(TYPE),
};
#undef TYPE
