#include "nem.h"

static void
NEM_chan_shutdown(NEM_chan_t *this, NEM_err_t err)
{
	if (!NEM_err_ok(this->err)) {
		return;
	}
	if (NEM_err_ok(err)) {
		NEM_panic("NEM_chan_shutdown: not passed error");
	}

	this->err = err;
	NEM_stream_close(this->stream);

	while (NULL != this->wqueue) {
		NEM_chan_ca ca = {
			.err  = err,
			.chan = this,
			.msg  = this->wqueue->msg,
		};

		if (NULL != this->wqueue->thunk) {
			NEM_thunk1_invoke(&this->wqueue->thunk, &ca);
		}

		// NB: Let the thunk take ownership.
		NEM_msg_free(ca.msg);
		NEM_msglist_t *next = this->wqueue->next;
		free(this->wqueue);
		this->wqueue = next;
	}
	this->wlast = NULL;

	if (NULL != this->on_close) {
		NEM_chan_ca ca = {
			.err  = err,
			.chan = this,
			.msg  = NULL,
		};

		NEM_thunk1_invoke(&this->on_close, &ca);
	}
}

static const char*
NEM_chan_state_string(NEM_chan_state_t st)
{
	static const struct {
		NEM_chan_state_t state;
		const char      *string;
	}
	table[] = {
		{ NEM_CHAN_STATE_MAGIC,    "magic"    },
		{ NEM_CHAN_STATE_HEADERS,  "headers"  },
		{ NEM_CHAN_STATE_BODY,     "body"     },
		{ NEM_CHAN_STATE_FD,       "fd"       },
		{ NEM_CHAN_STATE_DISPATCH, "dispatch" },
	};
	for (size_t i = 0; i < NEM_ARRSIZE(table); i += 1) {
		if (table[i].state == st) {
			return table[i].string;
		}
	}
	return "unknown";
}

static void NEM_chan_read(NEM_chan_t *this);

static void
NEM_chan_on_read(NEM_thunk1_t *thunk, void *varg)
{
	NEM_stream_ca *ca = varg;
	NEM_chan_t *this = NEM_thunk1_ptr(thunk);

	if (!NEM_err_ok(ca->err)) {
		NEM_chan_shutdown(this, ca->err);
		return;
	}

	NEM_chan_read(this);
}

static void
NEM_chan_dispatch(NEM_chan_t *this)
{
	if (NULL == this->on_msg || NEM_CHAN_STATE_DISPATCH != this->rstate) {
		return;
	}

	NEM_chan_ca ca = {
		.err  = NEM_err_none,
		.chan = this,
		.msg  = this->rmsg,
	};

	NEM_thunk_invoke(this->on_msg, &ca);

	if (NULL != ca.msg) {
		// NB: Intentionally providing a hook for the caller
		// to grab the message out from under us, otherwise we'll
		// free it and allocate a new one.
		NEM_msg_free(ca.msg);
	}

	this->rmsg = NULL;
	this->rstate = NEM_CHAN_STATE_MAGIC;
	return NEM_chan_read(this);
}

static void
NEM_chan_read(NEM_chan_t *this)
{
	NEM_err_t err;

	switch (this->rstate) {
		case NEM_CHAN_STATE_MAGIC:
			this->rstate = NEM_CHAN_STATE_BODY;
			err = NEM_stream_read(
				this->stream,
				&this->pmsg,
				sizeof(this->pmsg),
				NEM_thunk1_new_ptr(
					&NEM_chan_on_read,
					this
				)
			);
			if (!NEM_err_ok(err)) {
				return NEM_chan_shutdown(this, err);
			}
			break;

		case NEM_CHAN_STATE_HEADERS:
			// NB: The body and headers are coalesed into a single alloc.
			NEM_panic("NEM_chan_read: invalid rstate");

		case NEM_CHAN_STATE_BODY:
			err = NEM_pmsg_validate(&this->pmsg);
			if (!NEM_err_ok(err)) {
				return NEM_chan_shutdown(this, err);
			}

			this->rstate = NEM_CHAN_STATE_FD;
			this->rmsg = NEM_msg_new(
				this->pmsg.header_len,
				this->pmsg.body_len
			);

			this->rmsg->packed = this->pmsg;
			bzero(&this->pmsg, sizeof(this->pmsg));

			err = NEM_stream_read(
				this->stream,
				this->rmsg->appended,
				this->rmsg->packed.header_len + this->rmsg->packed.body_len,
				NEM_thunk1_new_ptr(
					&NEM_chan_on_read,
					this
				)
			);
			if (!NEM_err_ok(err)) {
				return NEM_chan_shutdown(this, err);
			}
			break;

		case NEM_CHAN_STATE_FD:
			if ((this->rmsg->packed.flags & NEM_PMSGFLAG_FD)) {
				int fd;
				err = NEM_stream_read_fd(this->stream, &fd);
				if (!NEM_err_ok(err)) {
					return NEM_chan_shutdown(this, err);
				}
				err = NEM_msg_set_fd(this->rmsg, fd);
				if (!NEM_err_ok(err)) {
					return NEM_chan_shutdown(this, err);
				}
			}
			this->rstate = NEM_CHAN_STATE_DISPATCH;
			// Fallthrough.

		case NEM_CHAN_STATE_DISPATCH:
			return NEM_chan_dispatch(this);
	}
}

static void NEM_chan_write(NEM_chan_t *this);

static void
NEM_chan_on_write(NEM_thunk1_t *thunk, void *varg)
{
	NEM_stream_ca *ca = varg;
	NEM_chan_t *this = NEM_thunk1_ptr(thunk);

	if (!NEM_err_ok(ca->err)) {
		NEM_chan_shutdown(this, ca->err);
		return;
	}

	NEM_chan_write(this);
}

static void
NEM_chan_write(NEM_chan_t *this)
{
	size_t len;
	NEM_err_t err;
	NEM_msg_t *msg = this->wqueue->msg;

	switch (this->wstate) {
		case NEM_CHAN_STATE_MAGIC:
			this->wstate = NEM_CHAN_STATE_HEADERS;
			len = sizeof(msg->packed);
			if ((msg->flags & NEM_MSGFLAG_HEADER_INLINE)) {
				len += msg->packed.header_len;
				this->wstate = NEM_CHAN_STATE_BODY;

				if ((msg->flags & NEM_MSGFLAG_BODY_INLINE)) {
					len += msg->packed.body_len;
					this->wstate = NEM_CHAN_STATE_FD;
				}
			}

			err = NEM_stream_write(
				this->stream,
				&msg->packed,
				len,
				NEM_thunk1_new_ptr(
					&NEM_chan_on_write,
					this
				)
			);
			if (!NEM_err_ok(err)) {
				return NEM_chan_shutdown(this, err);
			}
			return;

		case NEM_CHAN_STATE_HEADERS:
			this->wstate = NEM_CHAN_STATE_BODY;
			len = msg->packed.header_len;
			if (0 < len) {
				err = NEM_stream_write(
					this->stream,
					msg->header,
					len,
					NEM_thunk1_new_ptr(
						&NEM_chan_on_write,
						this
					)
				);
				if (!NEM_err_ok(err)) {
					return NEM_chan_shutdown(this, err);
				}
				return;
			}
			// Fallthrough
		
		case NEM_CHAN_STATE_BODY:
			this->wstate = NEM_CHAN_STATE_FD;
			len = msg->packed.body_len;
			if (0 < len) {
				err = NEM_stream_write(
					this->stream,
					msg->body,
					len,
					NEM_thunk1_new_ptr(
						&NEM_chan_on_write,
						this
					)
				);
				if (!NEM_err_ok(err)) {
					return NEM_chan_shutdown(this, err);
				}
				return;
			}
			// Fallthrough

		case NEM_CHAN_STATE_FD: {
			this->wstate = NEM_CHAN_STATE_DISPATCH;
			if ((msg->flags & NEM_MSGFLAG_HAS_FD)) {
				err = NEM_stream_write_fd(this->stream, msg->fd);
				if (!NEM_err_ok(err)) {
					return NEM_chan_shutdown(this, err);
				}
			}

			// Pop off the just-written message. NEM_stream_write_fd is 
			// synchronous, so this should be fine.
			NEM_msglist_t *next = this->wqueue->next;
			NEM_chan_ca ca = {
				.err  = NEM_err_none,
				.chan = this,
				.msg  = this->wqueue->msg,
			};
			if (NULL != this->wqueue->thunk) {
				NEM_thunk1_invoke(&this->wqueue->thunk, &ca);
			}
			// NB: Let the thunk take ownership of the message.
			NEM_msg_free(ca.msg);

			free(this->wqueue);
			this->wqueue = next;
			if (NULL == next) {
				this->wlast = NULL;
			}
			
			// Fallthrough
		}

		case NEM_CHAN_STATE_DISPATCH:
			if (NULL != this->wqueue) {
				this->wstate = NEM_CHAN_STATE_MAGIC;
				return NEM_chan_write(this);
			}
	}
}

void
NEM_chan_init(NEM_chan_t *this, NEM_stream_t stream)
{
	bzero(this, sizeof(*this));
	this->stream = stream;
	this->rstate = NEM_CHAN_STATE_MAGIC;
	this->wstate = NEM_CHAN_STATE_DISPATCH;
	this->err = NEM_err_none;

	NEM_chan_read(this);
}

void
NEM_chan_free(NEM_chan_t *this)
{
	NEM_chan_shutdown(this, NEM_err_static("NEM_chan_free invoked"));

	if (NULL != this->rmsg) {
		NEM_msg_free(this->rmsg);
	}

	if (NULL != this->on_msg) {
		NEM_thunk_free(this->on_msg);
	}
}

void
NEM_chan_send(NEM_chan_t *this, NEM_msg_t *msg, NEM_thunk1_t *cb)
{
	if (!NEM_err_ok(this->err)) {
		NEM_chan_ca ca = {
			.err  = this->err,
			.chan = this,
			.msg  = msg,
		};
		if (NULL != cb) {
			NEM_thunk1_invoke(&cb, &ca);
		}
		NEM_msg_free(ca.msg);
		return;
	}

	NEM_msglist_t *list = NEM_malloc(sizeof(NEM_msglist_t));
	list->msg = msg;
	list->thunk = cb;

	if (NULL == this->wqueue) {
		if (NEM_CHAN_STATE_DISPATCH != this->wstate) {
			NEM_panic("NEM_chan_send: state machine corrupt");
		}
		this->wqueue = list;
		this->wlast = list;
		NEM_chan_write(this);
	}
	else {
		this->wlast->next = list;
		this->wlast = list;
	}
}

void
NEM_chan_on_msg(NEM_chan_t *this, NEM_thunk_t *cb)
{
	if (!NEM_err_ok(this->err)) {
		NEM_thunk_free(cb);
		return;
	}

	if (NULL != this->on_msg) {
		NEM_thunk_free(this->on_msg);
	}

	this->on_msg = cb;
	NEM_chan_dispatch(this);
}

void
NEM_chan_on_close(NEM_chan_t *this, NEM_thunk1_t *cb)
{
	if (!NEM_err_ok(this->err)) {
		NEM_chan_ca ca = {
			.err  = this->err,
			.chan = this,
			.msg  = NULL,
		};

		NEM_thunk1_invoke(&cb, &ca);
		return;
	}

	if (NULL != this->on_close) {
		NEM_thunk1_discard(&this->on_close);
	}

	this->on_close = cb;
}
