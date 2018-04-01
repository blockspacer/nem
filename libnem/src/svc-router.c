#include "nem.h"
#include "nem-svc-router.h"

#define O(F) offsetof(TYPE, F)
#define M(F) NEM_MSIZE(TYPE, F)
#define NAME(t) #t

#define TYPE NEM_svc_router_bind_t
static const NEM_marshal_field_t router_bind_fs[] = {
	{ "port",     NEM_MARSHAL_INT32,  O(port),     -1, NULL },
	{ "proto",    NEM_MARSHAL_STRING, O(proto),    -1, NULL },
	{ "cert_pem", NEM_MARSHAL_STRING, O(cert_pem), -1, NULL },
	{ "key_pem",  NEM_MARSHAL_STRING, O(key_pem),  -1, NULL },
};
const NEM_marshal_map_t NEM_svc_router_bind_m = {
	.fields     = router_bind_fs,
	.fields_len = NEM_ARRSIZE(router_bind_fs),
	.elem_size  = sizeof(TYPE),
	.type_name  = NAME(TYPE),
};
#undef TYPE

#define TYPE NEM_svc_router_register_svc_t
static const NEM_marshal_field_t router_register_svc_fs[] = {
	{ "svc_id", NEM_MARSHAL_UINT16, O(svc_id), -1, NULL },
	{ "inst",   NEM_MARSHAL_STRING, O(inst),   -1, NULL },
};
const NEM_marshal_map_t NEM_svc_router_register_svc_m = {
	.fields     = router_register_svc_fs,
	.fields_len = NEM_ARRSIZE(router_register_svc_fs),
	.elem_size  = sizeof(TYPE),
	.type_name  = NAME(TYPE),
};
#undef TYPE

#define TYPE NEM_svc_router_register_http_t
static const NEM_marshal_field_t router_register_http_fs[] = {
	{ "port", NEM_MARSHAL_INT32,  O(port), -1, NULL },
	{ "host", NEM_MARSHAL_STRING, O(host), -1, NULL },
	{ "base", NEM_MARSHAL_STRING, O(base), -1, NULL },
	{ "inst", NEM_MARSHAL_STRING, O(inst), -1, NULL },
};
const NEM_marshal_map_t NEM_svc_router_register_http_m = {
	.fields     = router_register_http_fs,
	.fields_len = NEM_ARRSIZE(router_register_http_fs),
	.elem_size  = sizeof(TYPE),
	.type_name  = NAME(TYPE),
};
#undef TYPE
