#pragma once

typedef struct {
	int32_t port;
	char   *cert_pem;
	char   *key_pem;
}
NEM_svc_router_bind_t;
extern const NEM_marshal_map_t NEM_svc_router_bind_m;

typedef struct {
	uint16_t svc_id;
	char    *inst;
}
NEM_svc_router_register_svc_t;
extern const NEM_marshal_map_t NEM_svc_router_register_svc_m;

typedef struct {
	char *host;
	char *base;
	char *inst;
}
NEM_svc_router_register_http_t;
extern const NEM_marshal_map_t NEM_svc_router_register_http_m;
