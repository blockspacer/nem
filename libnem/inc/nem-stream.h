#pragma once

typedef struct {
	NEM_err_t (*read)(void *vthis, void *buf, size_t len, NEM_thunk1_t *cb);
	NEM_err_t (*write)(void *vthis, void *buf, size_t len, NEM_thunk1_t *cb);
	NEM_err_t (*read_fd)(void *vthis, int *fdout);
	NEM_err_t (*write_fd)(void *vthis, int fd);
	NEM_err_t (*close)(void *vthis);
	NEM_err_t (*on_close)(void *vthis, NEM_thunk1_t *cb);
}
NEM_stream_vt;

typedef struct {
	const NEM_stream_vt *vt;
	void                *this;
}
NEM_stream_t;

typedef struct {
	NEM_err_t err;
	NEM_stream_t stream;
}
NEM_stream_ca;

//
// Inline dispatch
//

// NEM_stream_read reads from the specified stream into the provided buffer.
// Reads cannot be interleaved -- if there's already a read scheduled this
// will return an error. The passed thunk is always freed, but if an error
// is returned it is not called.
static inline NEM_err_t
NEM_stream_read(NEM_stream_t this, void *buf, size_t len, NEM_thunk1_t *cb)
{
	return this.vt->read(this.this, buf, len, cb);
}

// NEM_stream_write has the same semantics as NEM_stream_read, except it
// writes to the stream. Reads and writes may be interleaved separately.
static inline NEM_err_t
NEM_stream_write(NEM_stream_t this, void *buf, size_t len, NEM_thunk1_t *cb)
{
	return this.vt->write(this.this, buf, len, cb);
}

static inline NEM_err_t
NEM_stream_read_fd(NEM_stream_t this, int *fdout)
{
	return this.vt->read_fd(this.this, fdout);
}

static inline NEM_err_t
NEM_stream_write_fd(NEM_stream_t this, int fd)
{
	return this.vt->write_fd(this.this, fd);
}

static inline NEM_err_t
NEM_stream_close(NEM_stream_t this)
{
	return this.vt->close(this.this);
}

static inline void
NEM_stream_on_close(NEM_stream_t this, NEM_thunk1_t *cb)
{
	this.vt->on_close(this.this, cb);
}
