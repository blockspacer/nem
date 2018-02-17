#pragma once

typedef struct {
	// send_msg should send a message across the channel. The channel should
	// handle any queueing internally -- messages do not need to be sent
	// in-order.
	void(*send_msg)(void*, NEM_msg_t *msg);

	// on_msg binds a callback which is invoked whenever a message is received.
	// The callback is passed a NEM_msg_t* as the argument. The callback is
	// automatically freed when the channel is closed. Calling this subsequent
	// times unbinds and frees the previous callback.
	void(*on_msg)(void*, NEM_thunk_t *thunk);

	// on_close binds a callback which is invoked when the channel is closed
	// (either explicitly via the close method or by the remote).
	void(*on_close)(void*, NEM_thunk1_t *thunk);

	// close releases all resources used by the channel. If on_close has not
	// yet been called, it is called. After calling, the channel cannot be 
	// used anymore.
	void(*close)(void*);
}
NEM_chan_vt;

// NEM_chan_t is a generalized bidirectional channel that sends and receives
// messages. As with all dynamic types in NEM, it should always be passed
// around by-value as it refers to the underlying type via pointer.
typedef struct {
	const NEM_chan_vt *vt;
	void *this;
}
NEM_chan_t;

//
// Inline dispatch methods for NEM_chan_t.
//

static inline void
NEM_chan_send_msg(NEM_chan_t this, NEM_msg_t *msg)
{
	this.vt->send_msg(this.this, msg);
}

static inline void
NEM_chan_on_msg(NEM_chan_t this, NEM_thunk_t *thunk)
{
	this.vt->on_msg(this.this, thunk);
}

static inline void
NEM_chan_on_close(NEM_chan_t this, NEM_thunk1_t *thunk)
{
	this.vt->on_close(this.this, thunk);
}

static inline void
NEM_chan_close(NEM_chan_t this)
{
	this.vt->close(this.this);
}
