#pragma once

typedef enum {
	NEM_CHAN_STATE_MAGIC,
	NEM_CHAN_STATE_HEADERS,
	NEM_CHAN_STATE_BODY,
	NEM_CHAN_STATE_FD,
	NEM_CHAN_STATE_DISPATCH,
}
NEM_chan_state_t;

typedef struct {
	NEM_stream_t  stream;
	NEM_thunk_t  *on_msg;
	NEM_thunk1_t *on_close;

	NEM_msg_t     *wmsg;
	NEM_msglist_t *wqueue;

	NEM_pmsg_t pmsg;
	NEM_msg_t *rmsg;

	NEM_err_t        err;
	NEM_chan_state_t rstate;
	NEM_chan_state_t wstate;
}
NEM_chan_t;

typedef struct {
	NEM_err_t   err;
	NEM_chan_t *chan;
	NEM_msg_t  *msg;
}
NEM_chan_ca;

void NEM_chan_init(NEM_chan_t *this, NEM_stream_t stream);
void NEM_chan_free(NEM_chan_t *this);

void NEM_chan_send(NEM_chan_t *this, NEM_msg_t *msg);
void NEM_chan_on_msg(NEM_chan_t *this, NEM_thunk_t *cb);
void NEM_chan_on_close(NEM_chan_t *this, NEM_thunk1_t *cb);

