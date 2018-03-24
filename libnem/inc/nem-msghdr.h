#pragma once

typedef struct {
	int64_t     code;
	const char *reason;
}
NEM_msghdr_err_t;

typedef struct {
	const char *cluster;
	const char *host;
	const char *inst;
	const char *obj;
}
NEM_msghdr_route_t;

typedef struct {
	NEM_msghdr_err_t   *err;
	NEM_msghdr_route_t *route;
	NEM_ALIGN char data[];
}
NEM_msghdr_t;

NEM_err_t NEM_msghdr_new(NEM_msghdr_t **hdr, const void *bs, size_t len);
NEM_err_t NEM_msghdr_pack(NEM_msghdr_t *hdr, void **bs, size_t *len);
void NEM_msghdr_free(NEM_msghdr_t *hdr);
