#include <sys/types.h>
#include <sys/event.h>
#include <strings.h>
#include <unistd.h>
#include "nem.h"

static void
NEM_pipechan_on_read(NEM_pipechan_t *this, size_t avail)
{
	if (NULL == this->on_msg) {
		this->ravail = avail;
		return;
	}

	do {
		if (NULL == this->rmsg && avail >= sizeof(NEM_pmsg_t)) {
			NEM_pmsg_t tmpmsg;
			if (0 > read(this->fd_in, &tmpmsg, sizeof(tmpmsg))) {
				// XXX: close?
				NEM_panic("read failed");
			}

			this->rmsg = NEM_msg_alloc(tmpmsg.header_len, tmpmsg.body_len);
			this->rmsg->packed = tmpmsg;
			this->rmsg_off = 0;
			this->rmsg_cap = tmpmsg.header_len + tmpmsg.body_len;
			avail -= sizeof(tmpmsg);
		}
		// NB: Not else if.
		if (NULL != this->rmsg) {
			size_t to_read = avail;
			if (to_read > this->rmsg_cap - this->rmsg_off) {
				to_read = this->rmsg_cap - this->rmsg_off;
			}

			char *ptr = &this->rmsg->appended[this->rmsg_off];
			if (0 > read(this->fd_in, ptr, to_read)) {
				// XXX: Should probably close here.
				NEM_panic("read failed");
			}

			this->rmsg_off += to_read;
			if (this->rmsg_off == this->rmsg_cap) {
				NEM_thunk_invoke(this->on_msg, this->rmsg);
				NEM_msg_free(this->rmsg);
				this->rmsg = NULL;
			}

			avail -= to_read;
		}
	}
	while (this->ravail >= sizeof(NEM_pmsg_t));
}

static void
NEM_pipechan_on_write(NEM_pipechan_t *this, size_t avail)
{
	if (NULL != this->wmsg) {
		size_t cap = sizeof(NEM_pmsg_t);
		size_t off = this->wmsg_off;
		char *ptr = (void*) &this->wmsg->packed;

		if (off >= cap) {
			off -= cap;
			cap = this->wmsg->packed.header_len;
			ptr = this->wmsg->header;
		}
		if (off >= cap) {
			off -= cap;
			cap = this->wmsg->packed.body_len;
			ptr = this->wmsg->body;
		}
		if (off >= cap) {
			NEM_panic("math is wrong");
		}
		cap -= off;
		if (cap > avail) {
			cap = avail;
		}

		if (-1 == write(this->fd_out, ptr + off, cap)) {
			// XXX: This should perhaps close.
			NEM_panic("write failed");
		}

		this->wmsg_off += cap;
		avail -= cap;
		size_t max_cap =
			sizeof(NEM_pmsg_t)
			+ this->wmsg->packed.header_len
			+ this->wmsg->packed.body_len;

		if (this->wmsg_off == max_cap) {
			NEM_msg_free(this->wmsg);
			this->wmsg = NULL;

			if (NULL != this->wqueue) {
				NEM_msglist_t *next = this->wqueue->next;
				this->wmsg = this->wqueue->msg;
				this->wqueue = this->wqueue->next;
				free(next);
			}
		}
	}

	this->wavail = avail;
	if (this->wavail > 0 && NULL != this->wmsg) {
		//return NEM_pipechan_on_write(this, avail);
	}
}

static void
NEM_pipechan_on_kevent(NEM_thunk_t *thunk, void *varg)
{
	NEM_pipechan_t *this = NEM_thunk_ptr(thunk);
	struct kevent *ev = varg;

	if (EVFILT_READ == ev->filter) {
		NEM_pipechan_on_read(this, ev->data);
	}
	else if (EVFILT_WRITE == ev->filter) {
		NEM_pipechan_on_write(this, ev->data);
	}
	else {
		NEM_panicf("NEM_pipechan_on_kevent: unexpected ev %d", ev->filter);
	}
}

NEM_err_t
NEM_pipechan_init(NEM_pipechan_t *this, int kq, int fd_in, int fd_out)
{
	bzero(this, sizeof(*this));
	this->fd_in = fd_in;
	this->fd_out = fd_out;

	NEM_thunk_t *cb = NEM_thunk_new_ptr(
		&NEM_pipechan_on_kevent,
		this
	);

	struct kevent evs[2];
	EV_SET(&evs[0], this->fd_out, EVFILT_WRITE, EV_ADD|EV_CLEAR, 0, 0, cb);
	EV_SET(&evs[1], this->fd_in, EVFILT_READ, EV_ADD, 0, 0, cb);
	if (-1 == kevent(kq, evs, NEM_ARRSIZE(evs), NULL, 0, NULL)) {
		NEM_thunk_free(cb);
		close(fd_in);
		close(fd_out);
		return NEM_err_errno();
	}

	this->on_kevent = cb;

	return NEM_err_none;
}

void
NEM_pipechan_free(NEM_pipechan_t *this)
{
	if (NULL != this->on_msg) {
		NEM_thunk_free(this->on_msg);
	}

	if (NULL != this->on_close) {
		NEM_thunk1_invoke(&this->on_close, NULL);
	}

	if (NULL != this->on_kevent) {
		NEM_thunk_free(this->on_kevent);
	}

	if (NULL != this->wmsg) {
		NEM_msg_free(this->wmsg);
	}

	while (NULL != this->wqueue) {
		NEM_msglist_t *next = this->wqueue->next;
		NEM_msg_free(this->wqueue->msg);
		free(this->wqueue);
		this->wqueue = next;
	}

	if (NULL != this->rmsg) {
		NEM_msg_free(this->rmsg);
	}

	close(this->fd_in);
	close(this->fd_out);
}

void
NEM_pipechan_on_msg(NEM_pipechan_t *this, NEM_thunk_t *cb)
{
	if (NULL != this->on_msg) {
		NEM_thunk_free(this->on_msg);
	}

	this->on_msg = cb;

	if (0 < this->ravail) {
		NEM_pipechan_on_read(this, this->ravail);
	}
}

void
NEM_pipechan_send_msg(NEM_pipechan_t *this, NEM_msg_t *msg)
{
	if (NULL == this->wmsg) {
		this->wmsg = msg;
	}
	else {
		NEM_msglist_t *list = NEM_malloc(sizeof(NEM_msglist_t));
		list->msg = msg;
		list->next = this->wqueue;
		this->wqueue = list;
	}

	if (0 < this->wavail) {
		NEM_pipechan_on_write(this, this->wavail);
	}
}
