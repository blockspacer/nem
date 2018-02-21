#pragma once

typedef struct {
	int fd_in;
	int fd_out;

	NEM_thunk_t  *on_kevent;
	NEM_thunk_t  *on_msg;
	NEM_thunk1_t *on_close;

	NEM_msg_t     *wmsg;
	size_t         wmsg_off;
	NEM_msglist_t *wqueue;
	size_t         wavail;

	NEM_msg_t *rmsg;
	size_t     rmsg_off;
	size_t     rmsg_cap;
	size_t     ravail;
}
NEM_pipechan_t;

NEM_err_t NEM_pipechan_init(
	NEM_pipechan_t *this,
	int kq,
	int fd_in,
	int fd_out
);
void NEM_pipechan_free(NEM_pipechan_t *this);
void NEM_pipechan_on_msg(NEM_pipechan_t *this, NEM_thunk_t *cb);
void NEM_pipechan_send_msg(NEM_pipechan_t *this, NEM_msg_t *msg);
