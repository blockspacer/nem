#pragma once

typedef enum {
	NEM_CHAN_STATE_MAGIC,
	NEM_CHAN_STATE_HEADERS,
	NEM_CHAN_STATE_BODY,
	NEM_CHAN_STATE_FD,
	NEM_CHAN_STATE_DISPATCH,
}
NEM_chan_state_t;

// NEM_chan_t implements the NEM framing protocol on top of a NEM_stream_t
// and provides a message-based interface. This lets the application work in
// terms of messages across a bidirectional channel rather than mucking with
// raw reads/writes. It does not assume ownership of the underlying stream
typedef struct {
	NEM_stream_t  stream;
	NEM_thunk_t  *on_msg;
	NEM_thunk1_t *on_close;

	NEM_msglist_t *wqueue;
	NEM_msglist_t *wlast;

	NEM_pmsg_t pmsg;
	NEM_msg_t *rmsg;

	NEM_err_t        err;
	NEM_chan_state_t rstate;
	NEM_chan_state_t wstate;
}
NEM_chan_t;

// NEM_chan_ca is the callback argument struct passed to the thunks used by
// a NEM_chan_t.
typedef struct {
	NEM_err_t   err;
	NEM_chan_t *chan;
	NEM_msg_t  *msg;
}
NEM_chan_ca;

// NEM_chan_init initializes a new chan. It does not assume ownership of 
// the stream -- the caller is responsible for maintaining the allocation.
// The stream should not be closed directly, rather, closing the channel
// will close the stream.
void NEM_chan_init(NEM_chan_t *this, NEM_stream_t stream);

// NEM_chan_free closes the underlying stream and frees the NEM_chan_t's
// members. If the stream wasn't already closed, the on-close handler is
// invoked.
void NEM_chan_free(NEM_chan_t *this);

// NEM_chan_send assumes ownership of the heap-allocated msg object and
// sends it across the network. If cb is non-null, it is called when
// the message is sent across the wire. The cb is passed a NEM_chan_ca
// and may NULL the msg field if it wants to assume ownership of the
// message.
void NEM_chan_send(NEM_chan_t *this, NEM_msg_t *msg, NEM_thunk1_t *cb);

// NEM_chan_on_msg sets a singluar callback to invoke when a message is
// received from the channel. The message is freed after the callback
// returns, but may be kept around by NULL'ing the msg field of the ca.
void NEM_chan_on_msg(NEM_chan_t *this, NEM_thunk_t *cb);

// NEM_chan_on_close sets a singular callback to invoke when the stream is
// closed. If the stream is already closed, this is invoked immediately.
void NEM_chan_on_close(NEM_chan_t *this, NEM_thunk1_t *cb);

