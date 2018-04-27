#include "nemsvc.h"
#include "nem.h"
#include "nem-marshal-macros.h"

#define TYPE NEM_svc_host_bind_req_t
static const NEM_marshal_field_t host_bind_req_fs[] = {
	{ "port", NEM_MARSHAL_UINT16, O(port), -1, NULL },
};
MAP(NEM_svc_host_bind_req_m, host_bind_req_fs);
#undef TYPE

#define TYPE NEM_svc_host_bind_res_t
static const NEM_marshal_field_t host_bind_res_fs[] = {
};
MAP(NEM_svc_host_bind_res_m, host_bind_res_fs);
#undef TYPE

#define TYPE NEM_svc_host_connect_req_t
static const NEM_marshal_field_t host_connect_req_fs[] = {
	{ "port", NEM_MARSHAL_UINT16, O(port), -1, NULL },
	{ "addr", NEM_MARSHAL_STRING, O(addr), -1, NULL },
};
MAP(NEM_svc_host_connect_req_m, host_connect_req_fs);
#undef TYPE

#define TYPE NEM_svc_host_connect_res_t
static const NEM_marshal_field_t host_connect_res_fs[] = {
};
MAP(NEM_svc_host_connect_res_m, host_connect_res_fs);
#undef TYPE
