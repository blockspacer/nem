#pragma once

// NEM_dial_unix connects to a listening unix socket. If there's an error
// it's passed to the provided thunk (which is always called). The thunk is
// passed a NEM_dial_ca.
void NEM_dial_unix(int kq, const char *path, NEM_thunk1_t *on_stream);

// NEM_dial_tcp connects to the specified TCP address. If there's an error
// it's passed to the provided thunk (always called too). The thunk is passed
// a NEM_dial_ca.
void NEM_dial_tcp(
	int           kq,
	int           port,
	const char   *addr,
	NEM_thunk1_t *on_stream
);

typedef struct {
	NEM_err_t    err;
	NEM_stream_t stream;
}
NEM_dial_ca;
