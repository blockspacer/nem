#pragma once

static const uint16_t
	NEM_cmdid_router_bind          = 1,
	NEM_cmdid_router_register_svc  = 2,
	NEM_cmdid_router_register_http = 3;

// NEM_svc_router_bind_cert_t contains the PEM-encoded certificate info
// for a TLS listener.
typedef struct {
	const char *cert_pem;
	const char *key_pem;
	const char *client_ca_pem;
}
NEM_svc_router_bind_cert_t;
extern const NEM_marshal_map_t NEM_svc_router_bind_cert_m;

// NEM_svc_router_bind_t is a multi-message request that passes a set of
// fds to bind on. Each fd can have multiple protocols attached to it. 
// Key/certs can additionally be supplied for TLS connections.
typedef struct {
	// port is the port this socket is bound on. It's supplementary.
	int32_t port;

	// protos should contain only "http" "https" "nem". https/nem imply that
	// certs should be a non-empty array.
	const char **protos;
	size_t       protos_len;

	NEM_svc_router_bind_cert_t *certs;
	size_t                      certs_len;
}
NEM_svc_router_bind_t;
extern const NEM_marshal_map_t NEM_svc_router_bind_m;

// NEM_svc_router_register_svc_t registers a service against the router.
// The router will forward messages received for the passed svc_id/inst
// pair to the attached socket, or to the sending socket if an fd is
// not attached. If inst is NULL, all messages matching the svc_id will
// be forwarded.
typedef struct {
	uint16_t    svc_id;
	const char *inst;
}
NEM_svc_router_register_svc_t;
extern const NEM_marshal_map_t NEM_svc_router_register_svc_m;

// NEM_svc_router_register_http_t registers a HTTP service against the
// router. It _must_ be passed a FD over which requests are sent. Multiple
// duplicate entries can be registered; incoming matching requests will
// be dispatched to matching entries round-robin.
//
// An error is returned if there is no TLS cert registered for the given
// host, as all non-TLS HTTP requests are redirected to HTTPS.
typedef struct {
	const char *host;
	const char *base;
	const char *inst;
}
NEM_svc_router_register_http_t;
extern const NEM_marshal_map_t NEM_svc_router_register_http_m;
