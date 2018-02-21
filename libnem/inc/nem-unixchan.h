#pragma once

typedef struct {
	int kq;
	int fd_s;
	int fd_c1;
	int fd_c2;
	char *fd_path;

	bool ready;

	NEM_thunk_t  *on_kevent;
	NEM_thunk_t  *on_msg;
	NEM_thunk1_t *on_ready;
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
NEM_unixchan_t;

NEM_err_t NEM_unixchan_init(
	NEM_unixchan_t *this,
	int             kq,
	NEM_thunk1_t   *on_ready
);
void NEM_unixchan_free(NEM_unixchan_t *this);
void NEM_unixchan_on_msg(NEM_unixchan_t *this, NEM_thunk_t *cb);
void NEM_unixchan_send_msg(NEM_unixchan_t *this, NEM_msg_t *msg);

int NEM_unixchan_remote_fd(NEM_unixchan_t *this);
