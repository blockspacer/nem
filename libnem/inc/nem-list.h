#pragma once

// NEM_list_vt is the virtual table for a listening socket. The listener
// is initialized with a NEM_thunk_t that is passed a NEM_list_ca for each
// connecting stream. NEM_list_vt has a single method -- close -- that closes
// the underlying connection and prevents further incoming connections. The
// listener is never closed prematurely.
typedef struct {
	void (*close)(void *vthis);
}
NEM_list_vt;

// NEM_list_t is an instance with a NEM_list_vt.
typedef struct {
	const NEM_list_vt *vt;
	void              *this;
}
NEM_list_t;

// NEM_list_init_unix initializes a unix domain socket listening on the
// given port. The socket is removed when the listener is closed. The thunk
// is passed a NEM_list_ca for each connecting stream.
NEM_err_t NEM_list_init_unix(
	NEM_list_t  *this,
	int          kq,
	const char  *path,
	NEM_thunk_t *on_stream
);

// NEM_list_init_tcp binds a TCP socket on the given port/addr.
NEM_err_t NEM_list_init_tcp(
	NEM_list_t  *this,
	int          kq,
	int          port,
	const char  *addr,
	NEM_thunk_t *on_stream
);

typedef struct {
	NEM_err_t    err;
	NEM_list_t   list;
	NEM_stream_t stream;
}
NEM_list_ca;

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

// 
// Static dispatch methods
//

static inline void
NEM_list_close(NEM_list_t this)
{
	this.vt->close(this.this);
}
