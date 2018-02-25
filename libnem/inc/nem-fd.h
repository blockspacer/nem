#pragma once

typedef struct {
	int           fd_in;
	int           fd_out;
	NEM_thunk_t  *on_kevent;
	NEM_thunk1_t *on_close;

	size_t        wcap;
	char         *wbuf;
	size_t        wavail;
	NEM_thunk1_t *on_write;

	size_t        rcap;
	char         *rbuf;
	size_t        ravail;
	NEM_thunk1_t *on_read;

	bool running;
}
NEM_fd_t;

extern const NEM_stream_vt NEM_fd_stream_vt;

NEM_err_t NEM_fd_init(NEM_fd_t *this, int kq, int fd);
NEM_err_t NEM_fd_init2(NEM_fd_t *this, int kq, int fd_in, int fd_out);
void NEM_fd_free(NEM_fd_t *this);

NEM_err_t NEM_fd_init_pipe(NEM_fd_t *this, NEM_fd_t *that, int kq);
NEM_err_t NEM_fd_init_unix(NEM_fd_t *this, NEM_fd_t *that, int kq);

NEM_stream_t NEM_fd_as_stream(NEM_fd_t *this);

NEM_err_t NEM_fd_read(NEM_fd_t *this, void *buf, size_t len, NEM_thunk1_t *cb);
NEM_err_t NEM_fd_write(NEM_fd_t *this, void *buf, size_t len, NEM_thunk1_t *cb);

void NEM_fd_close(NEM_fd_t *this);
void NEM_fd_on_close(NEM_fd_t *this, NEM_thunk1_t *cb);

NEM_err_t NEM_fd_read_fd(NEM_fd_t *this, int *fdout);
NEM_err_t NEM_fd_write_fd(NEM_fd_t *this, int fd);
