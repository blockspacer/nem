#pragma once

static const uint16_t
	NEM_cmdid_host_bind    = 1,
	NEM_cmdid_host_connect = 2;

typedef struct {
	uint16_t port;
}
NEM_svc_host_bind_req_t;
extern const NEM_marshal_map_t NEM_svc_host_bind_req_m;

typedef struct {
}
NEM_svc_host_bind_res_t;
extern const NEM_marshal_map_t NEM_svc_host_bind_res_m;

typedef struct {
	uint16_t    port;
	const char *addr;
}
NEM_svc_host_connect_req_t;
extern const NEM_marshal_map_t NEM_svc_host_connect_req_m;

typedef struct {
}
NEM_svc_host_connect_res_t;
extern const NEM_marshal_map_t NEM_svc_host_connect_res_m;
