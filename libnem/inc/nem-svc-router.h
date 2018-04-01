#pragma once

typedef struct {
	int32_t     port;
	const char *proto;
	const char *cert_pem;
	const char *key_pem;
}
NEM_svc_router_bind_t;
extern const NEM_marshal_map_t NEM_svc_router_bind_m;

typedef struct {
	uint16_t    svc_id;
	const char *inst;
}
NEM_svc_router_register_svc_t;
extern const NEM_marshal_map_t NEM_svc_router_register_svc_m;

typedef struct {
	int32_t     port;
	const char *host;
	const char *base;
	const char *inst;
}
NEM_svc_router_register_http_t;
extern const NEM_marshal_map_t NEM_svc_router_register_http_m;
