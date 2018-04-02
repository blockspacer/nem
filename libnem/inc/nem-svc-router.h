#pragma once

// NEM_svc_router_bind_t is a request that passes a FD to the router 
// daemon to accept against. The router daemon shouldn't bind any sockets
// itself; instead it should just get the FDs via this message from
// the host.
typedef struct {
	// port is the port this socket is bound on. It's supplementary.
	int32_t     port;

	// proto is which protocol should be on the other end. Right now we
	// support three protocols:
	// 
	//  * http: can be registered against with NEM_svc_router_register_http_t.
	//  * nem: can be registered against with NEM_svc_router_register_svc_t.
	//  * http+nem: both http and nem.
	//
	// Incoming non-TLS requests to "http" protocol connections will be
	// redirected to "https". "nem" protocol must always be over TLS.
	const char *proto;

	// cert_pem/key_pem, if set, makes this a TLS connection. Both of these
	// must be set or unset.
	const char *cert_pem;
	const char *key_pem;

	// client_ca_pem, if set, makes this TLS connection require client
	// authentication with a cert that validates against this CA. If this
	// is set, cert_pem/key_pem must also be set.
	const char *client_ca_pem;
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
