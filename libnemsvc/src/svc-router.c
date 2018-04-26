#include "nemsvc.h"
#include "nem.h"
#include "nemsvc-macros.h"

#define TYPE NEM_svc_router_bind_cert_t
static const NEM_marshal_field_t router_bind_cert_fs[] = {
	{ "cert_pem",      NEM_MARSHAL_STRING, O(cert_pem),      -1, NULL },
	{ "key_pem",       NEM_MARSHAL_STRING, O(key_pem),       -1, NULL },
	{ "client_ca_pem", NEM_MARSHAL_STRING, O(client_ca_pem), -1, NULL },
};
MAP(NEM_svc_router_bind_cert_m, router_bind_cert_fs);
#undef TYPE

#define TYPE NEM_svc_router_bind_t
static const NEM_marshal_field_t router_bind_fs[] = {
	{ "port",          NEM_MARSHAL_INT32,  O(port),          -1, NULL },
	{
		"protos",
		NEM_MARSHAL_STRING|NEM_MARSHAL_ARRAY,
		O(protos), O(protos_len),
		NULL
	},
	{
		"certs",
		NEM_MARSHAL_STRUCT|NEM_MARSHAL_ARRAY,
		O(certs),  O(certs_len),
		&NEM_svc_router_bind_cert_m
	},
};
MAP(NEM_svc_router_bind_m, router_bind_fs);
#undef TYPE

#define TYPE NEM_svc_router_register_svc_t
static const NEM_marshal_field_t router_register_svc_fs[] = {
	{ "svc_id", NEM_MARSHAL_UINT16, O(svc_id), -1, NULL },
	{ "inst",   NEM_MARSHAL_STRING, O(inst),   -1, NULL },
};
MAP(NEM_svc_router_register_svc_m, router_register_svc_fs);
#undef TYPE

#define TYPE NEM_svc_router_register_http_t
static const NEM_marshal_field_t router_register_http_fs[] = {
	{ "host", NEM_MARSHAL_STRING, O(host), -1, NULL },
	{ "base", NEM_MARSHAL_STRING, O(base), -1, NULL },
	{ "inst", NEM_MARSHAL_STRING, O(inst), -1, NULL },
};
MAP(NEM_svc_router_register_http_m, router_register_http_fs);
#undef TYPE
