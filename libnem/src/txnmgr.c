#include "nem.h"

static void NEM_txnmgr_remove_txn(NEM_txnmgr_t *mgr, NEM_txn_t *txn);

static inline int NEM_txn_cmp(const void *lhs, const void *rhs);

SPLAY_PROTOTYPE(NEM_txnin_tree_t, NEM_txnin_t, link, NEM_txn_cmp);
SPLAY_GENERATE(NEM_txnin_tree_t, NEM_txnin_t, link, NEM_txn_cmp);
SPLAY_PROTOTYPE(NEM_txnout_tree_t, NEM_txnout_t, link, NEM_txn_cmp);
SPLAY_GENERATE(NEM_txnout_tree_t, NEM_txnout_t, link, NEM_txn_cmp);

static inline int
NEM_txn_cmp(const void *vlhs, const void *vrhs)
{
	const NEM_txn_t *lhs = vlhs;
	const NEM_txn_t *rhs = vrhs;

	if (lhs->seq < rhs->seq) {
		return -1;
	}
	else if (lhs->seq > rhs->seq) {
		return 1;
	}
	else {
		return 0;
	}
}

void*
NEM_txn_data(NEM_txn_t *this)
{
	return this->data;
}

void
NEM_txn_set_data(NEM_txn_t *this, void *data)
{
	this->data = data;
}

void
NEM_txn_cancel_err(NEM_txn_t *this, NEM_err_t err)
{
	NEM_txn_t **children = this->children;
	size_t num_children = this->children_len;
	this->children = NULL;
	this->children_len = 0;

	for (size_t i = 0; i < num_children; i += 1) {
		NEM_txn_cancel_err(children[i], err);
	}

	NEM_txn_ca ca = {
		.err  = err,
		.txn  = this,
		.msg  = NULL,
		.done = true,
	};
	if (NULL != this->thunk) {
		NEM_thunk_invoke(this->thunk, &ca);
		NEM_thunk_free(this->thunk);
	}

	NEM_txnmgr_remove_txn(this->mgr, this);
}

void
NEM_txn_cancel(NEM_txn_t *this)
{
	NEM_txn_cancel_err(this, NEM_err_static("transaction cancelled"));
}

void
NEM_txnout_set_timeout(NEM_txnout_t *this, int seconds)
{
	if (-1 == seconds) {
		bzero(&this->base.timeout, sizeof(this->base.timeout));
		return;
	}
	if (seconds < 0) {
		NEM_panic("NEM_txnout_set_timeout: invalid number of seconds");
	}

	gettimeofday(&this->base.timeout, NULL);
	this->base.timeout.tv_sec += seconds;
}

void
NEM_txnin_reply(NEM_txnin_t *this, NEM_msg_t *msg)
{
	msg->packed.seq = this->base.seq;
	msg->packed.flags |= NEM_PMSGFLAG_REPLY;
	NEM_chan_send(
		&this->base.mgr->chan,
		msg,
		NULL
		// XXX: We probably want to have a thing here. But if we do that we'd
		// need refcounts. Might need 'em anyway to properly implement
		// timeouts.
	);
}

void
NEM_txnin_reply_err(NEM_txnin_t *this, NEM_err_t err)
{
	NEM_msghdr_err_t hdrerr = {
		.code   = 1,
		.reason = NEM_err_string(err),
	};
	NEM_msghdr_t hdr = {
		.err = &hdrerr,
	};
	NEM_msg_t *msg = NEM_msg_new(0, 0); 
	NEM_msg_set_header(msg, &hdr);
	NEM_txnin_reply(this, msg);
}

void
NEM_txnin_reply_continue(NEM_txnin_t *this, NEM_msg_t *msg)
{
	msg->packed.flags |= NEM_PMSGFLAG_CONTINUE;
	return NEM_txnin_reply(this, msg);
}

void
NEM_txnmgr_init(NEM_txnmgr_t *this, NEM_stream_t stream)
{
	NEM_chan_init(&this->chan, stream);
	SPLAY_INIT(&this->txns_in);
	SPLAY_INIT(&this->txns_out);
	this->mux = NULL;
	this->seq = 1;
	this->err = NEM_err_none;

	// XXX: Bind an on-close for the channel.
}

static void
NEM_txnmgr_shutdown(NEM_txnmgr_t *this, NEM_err_t err)
{
	if (!NEM_err_ok(this->err)) {
		return;
	}

	this->err = err;

	NEM_txnin_t *txnin = NULL;
	NEM_txnout_t *txnout = NULL;

	// NB: The txn dtors wipe their own entries from the tree, so we don't
	// need to worry about that.
	while (NULL != (txnin = SPLAY_ROOT(&this->txns_in))) {
		NEM_txn_cancel_err(&txnin->base, this->err);
	}
	while (NULL != (txnout = SPLAY_ROOT(&this->txns_out))) {
		NEM_txn_cancel_err(&txnout->base, this->err);
	}

	NEM_chan_free(&this->chan);
}

static void
NEM_txnmgr_remove_txn(NEM_txnmgr_t *this, NEM_txn_t *txn)
{
	if (NEM_TXN_IN == txn->type) {
		SPLAY_REMOVE(NEM_txnin_tree_t, &this->txns_in, (NEM_txnin_t*) txn);
	}
	else {
		SPLAY_REMOVE(NEM_txnout_tree_t, &this->txns_out, (NEM_txnout_t*) txn);
	}
}

void
NEM_txnmgr_close(NEM_txnmgr_t *this)
{
	NEM_txnmgr_shutdown(this, NEM_err_static("txnmgr closed"));
}

void
NEM_txnmgr_free(NEM_txnmgr_t *this)
{
	NEM_txnmgr_shutdown(this, NEM_err_static("txnmgr freed"));
}
