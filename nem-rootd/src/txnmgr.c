#include "nem.h"
#include "txnmgr.h"
#include "state.h"

SPLAY_GENERATE(
	NEM_rootd_txntree_t,
	NEM_rootd_txn_t,
	link,
	NEM_rootd_txn_cmp
);

static void
NEM_rootd_txnmgr_shutdown(NEM_rootd_txnmgr_t *this, NEM_err_t err)
{
	if (this->closed) {
		return;
	}

	this->closed = true;

	NEM_rootd_txn_ca ca = {
		.err  = err,
		.done = true,
	};

	NEM_rootd_txn_t *txn;
	while (NULL != (txn = SPLAY_MIN(NEM_rootd_txntree_t, &this->txns))) {
		SPLAY_REMOVE(NEM_rootd_txntree_t, &this->txns, txn);
		NEM_thunk1_invoke(&txn->cb, &ca);
		free(txn);
	}

	NEM_thunk_free(this->on_req);

	if (NULL != this->on_close) {
		NEM_thunk1_invoke(&this->on_close, &ca);
	}
}

static void
NEM_rootd_cancel_incoming(NEM_rootd_txnmgr_t *this, NEM_msg_t *inc)
{
	NEM_msg_t *cancel = NEM_msg_new_reply(inc, 0, 0);
	cancel->packed.flags |= NEM_PMSGFLAG_CANCEL;
	// XXX: Might need to put NEM_PMSGFLAG_ROUTE in here if it's in the
	// incoming request, not sure how those semantics are going to work yet.
	// XXX: Want to put an error message in the header, but the header wire
	// format isn't sorted out yet.
	NEM_chan_send(this->chan, cancel);
}

static void
NEM_rootd_txnmgr_on_msg(NEM_thunk_t *thunk, void *varg)
{
	NEM_rootd_txnmgr_t *this = NEM_thunk_ptr(thunk);
	NEM_chan_ca *ca = varg;

	if (!NEM_err_ok(ca->err)) {
		NEM_rootd_txnmgr_shutdown(this, ca->err);
		return;
	}
	// Not a reply -> forward the message.
	if (!(ca->msg->packed.flags & NEM_PMSGFLAG_REPLY)) {
		NEM_thunk_invoke(this->on_req, ca);
		return;
	}
	// XXX: We don't currently support continue requests.
	if ((ca->msg->packed.flags & NEM_PMSGFLAG_CONTINUE)) {
		NEM_rootd_cancel_incoming(this, ca->msg);
		return;
	}

	NEM_rootd_txn_t dummy = {
		.seq_id = ca->msg->packed.seq,
	};

	NEM_rootd_txn_t *txn = SPLAY_FIND(
		NEM_rootd_txntree_t,
		&this->txns, &dummy
	);

	if (NULL == txn) {
		if (NEM_rootd_verbose()) {
			printf(
				"txn: unmatched reply, seq=%lu, service=%s, command=%s\n",
				ca->msg->packed.seq,
				NEM_svcid_to_string(ca->msg->packed.service_id),
				NEM_cmdid_to_string(
					ca->msg->packed.service_id,
					ca->msg->packed.command_id
				)
			);
		}
		return;
	}

	SPLAY_REMOVE(NEM_rootd_txntree_t, &this->txns, txn);

	NEM_rootd_txn_ca tca = {
		.err  = NEM_err_none,
		.msg  = ca->msg,
		.done = true,
	};
	NEM_thunk1_invoke(&txn->cb, &tca);
	free(txn);
}

void
NEM_rootd_txnmgr_init(
	NEM_rootd_txnmgr_t *this,
	NEM_chan_t         *chan,
	NEM_thunk_t        *on_req
) {
	bzero(this, sizeof(*this));
	this->chan = chan;
	this->next_seq = 1;
	this->closed = false;
	this->on_req = on_req;
	SPLAY_INIT(&this->txns);

	NEM_chan_on_msg(this->chan, NEM_thunk_new_ptr(
		&NEM_rootd_txnmgr_on_msg,
		this
	));
}

void
NEM_rootd_txnmgr_free(NEM_rootd_txnmgr_t *this)
{
	NEM_rootd_txnmgr_shutdown(
		this,
		NEM_err_static("NEM_rootd_txnmgr_free: killing in-flight txns")
	);
}

NEM_err_t
NEM_rootd_txnmgr_on_close(NEM_rootd_txnmgr_t *this, NEM_thunk1_t *thunk)
{
	if (NULL != this->on_close) {
		NEM_thunk1_discard(&this->on_close);
		return NEM_err_static("NEM_rootd_txnmgr_on_close: already bound");
	}
	if (this->closed) {
		// NB: This is kind of wonky; this works around a potential race
		// condition wherein the channel has data as soon as we initialize it;
		// there's a fast path there where the callback will be immediately
		// invoked in that case. We might get a message between initializing
		// the channel and setting the callback, so be sure that's hooked up.
		NEM_rootd_txn_ca ca = {
			.err = NEM_err_static("NEM_rootd_txnmgr_on_close: already closed"),
		};
		NEM_thunk1_invoke(&thunk, &ca);
		return NEM_err_static("NEM_rootd_txnmgr_on_close: already closed");
	}

	this->on_close = thunk;
	return NEM_err_none;
}

void
NEM_rootd_txnmgr_req1(
	NEM_rootd_txnmgr_t *this,
	NEM_msg_t          *msg,
	NEM_thunk1_t       *cb
) {
	if (this->closed) {
		NEM_rootd_txn_ca ca = {
			.err = NEM_err_static("NEM_rootd_txnmgr_req1: chan closed"),
		};
		NEM_thunk1_invoke(&cb, &ca);
		NEM_msg_free(msg);
		return;
	}

	NEM_rootd_txn_t *txn = NEM_malloc(sizeof(NEM_rootd_txn_t));
	txn->seq_id = this->next_seq;
	txn->cb = cb;
	SPLAY_INSERT(NEM_rootd_txntree_t, &this->txns, txn);

	msg->packed.seq = this->next_seq;
	this->next_seq += 1;

	NEM_chan_send(this->chan, msg);
}
