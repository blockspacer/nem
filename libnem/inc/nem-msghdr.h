#pragma once

typedef struct {
	int64_t     code;
	const char *reason;
}
NEM_msghdr_err_t;
extern const NEM_marshal_map_t NEM_msghdr_err_m;

typedef struct {
	const char *cluster;
	const char *host;
	const char *inst;
	const char *obj;
}
NEM_msghdr_route_t;
extern const NEM_marshal_map_t NEM_msghdr_route_m;

typedef struct {
	uint32_t timeout_ms;
}
NEM_msghdr_time_t;
extern const NEM_marshal_map_t NEM_msghdr_time_m;

typedef struct {
	NEM_msghdr_err_t   *err;
	NEM_msghdr_route_t *route;
	NEM_msghdr_time_t  *time;
	NEM_ALIGN char data[];
}
NEM_msghdr_t;
extern const NEM_marshal_map_t NEM_msghdr_m;

NEM_err_t NEM_msghdr_new(NEM_msghdr_t **hdr, const void *bs, size_t len);
NEM_err_t NEM_msghdr_pack(NEM_msghdr_t *hdr, void **bs, size_t *len);
void NEM_msghdr_free(NEM_msghdr_t *hdr);
