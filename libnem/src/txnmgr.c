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

static void
NEM_txn_add_msg(NEM_txn_t *this, NEM_msg_t *msg)
{
	this->messages_len += 1;
	this->messages = NEM_panic_if_null(
		realloc(this->messages, sizeof(NEM_msg_t*) * this->messages_len)
	);
	this->messages[this->messages_len - 1] = msg;
}

static void
NEM_txn_add_child(NEM_txn_t *this, NEM_txn_t *child)
{
	this->children_len += 1;
	this->children = NEM_panic_if_null(
		realloc(this->children, sizeof(NEM_txn_t*) * this->children_len)
	);
	this->children[this->children_len - 1] = child;
}

static void
NEM_txn_free(NEM_txn_t *this)
{
	for (size_t i = 0; i < this->children_len; i += 1) {
		NEM_txn_free(this->children[i]);
	}

	for (size_t i = 0; i < this->messages_len; i += 1) {
		NEM_msg_t *msg = this->messages[i];
		if (NULL != msg) {
			NEM_msg_free(msg);
		}
	}
	free(this->messages);

	if (NULL != this->thunk) {
		NEM_thunk_free(this->thunk);
	}

	NEM_txnmgr_remove_txn(this->mgr, this);
	free(this);
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
		.err    = err,
		.txnin  = (NEM_TXN_IN == this->type) ? (NEM_txnin_t*)this : NULL,
		.txnout = (NEM_TXN_OUT == this->type) ? (NEM_txnout_t*)this : NULL,
		.mgr    = this->mgr,
		.msg    = NULL,
		.done   = true,
	};
	if (NULL != this->thunk) {
		NEM_thunk_invoke(this->thunk, &ca);
	}

	NEM_txn_free(this);
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
	bool done = 0 == (msg->packed.flags & NEM_PMSGFLAG_CONTINUE);

	if (this->base.cancelled) {
		NEM_msg_free(msg);
	}
	else {
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

	if (done) {
		NEM_txn_free(&this->base);
	}
}

void
NEM_txnin_reply_err(NEM_txnin_t *this, NEM_err_t err)
{
	// XXX: Should have serializable errors or something.
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
NEM_txnout_req(NEM_txnout_t *this, NEM_msg_t *msg)
{
	bool done = 0 == (msg->packed.flags & NEM_PMSGFLAG_CONTINUE);
	msg->packed.seq = this->base.seq;

	if (this->base.cancelled) {
		done = true;
		NEM_msg_free(msg);
		return;
	}

	NEM_chan_send(&this->base.mgr->chan, msg, NULL);
	// XXX: We'd want to maybe clear a timeout in that NULL callback.

	if (done) {
		// XXX: Do we set a timeout here for receiving a response?
	}
}

void
NEM_txnout_req_continue(NEM_txnout_t *this, NEM_msg_t *msg)
{
	msg->packed.flags |= NEM_PMSGFLAG_CONTINUE;
	NEM_txnout_req(this, msg);
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

	if (NULL != this->mux) {
		NEM_svcmux_decref(this->mux);
	}
}

static void
NEM_txnmgr_on_reply(NEM_txnmgr_t *this, NEM_chan_ca *chan_ca)
{
	// NB: This is called when we have an incoming message that's marked as
	// a reply. There should be a matching txnout entry.
	NEM_msg_t *msg = chan_ca->msg;
	NEM_txnout_t dummy = {
		.base = {
			.seq = msg->packed.seq,
		},
	};

	NEM_txnout_t *txnout = SPLAY_FIND(
		NEM_txnout_tree_t,
		&this->txns_out,
		&dummy
	);
	if (NULL == txnout) {
		// NB: No matching transaction. We can't reply to a reply, so it's
		// impossible to cancel.
		// XXX: When something's sending us garbage that we didn't ask for
		// what do we do.
		return;
	}
	if (NULL == txnout->base.thunk) {
		// Uhh. What?
		NEM_panic("NEM_txnmgr_on_reply: transaction has no handler?");
	}

	bool done = 0 == (msg->packed.flags & NEM_PMSGFLAG_CONTINUE);
	NEM_err_t err = NEM_err_none;

	if ((msg->packed.flags & NEM_PMSGFLAG_CANCEL)) {
		txnout->base.cancelled = true;
		done = true;
		err = NEM_err_static("remote cancelled transaction");
	}

	NEM_msghdr_t *hdr = NULL;
	if (NEM_err_ok(err) && 0 < msg->packed.header_len) {
		hdr = NEM_msg_header(msg);
		if (NULL != hdr && NULL != hdr->err) {
			// XXX: UNSAFE LIFETIMES HERE
			err = NEM_err_static(hdr->err->reason);
		}
	}

	NEM_txn_ca ca = {
		.err    = err,
		.txnout = txnout,
		.mgr    = this,
		.msg    = msg,
		.done   = done,
	};

	NEM_txn_add_msg(&txnout->base, msg);
	chan_ca->msg = NULL; // NB: Claim ownership over this message.
	NEM_thunk_invoke(txnout->base.thunk, &ca);

	if (NULL != hdr) {
		NEM_msghdr_free(hdr);
	}

	if (done) {
		NEM_txn_free(&txnout->base);
	}
}

static void
NEM_txnmgr_on_req(NEM_txnmgr_t *this, NEM_chan_ca *chan_ca)
{
	// NB: This is called when we have an incoming message that's the start of
	// or part of an incoming request.
	NEM_msg_t *msg = chan_ca->msg;

	// NB: Find an existing txnin if we've got one. The cached service/command
	// ids override anything in the message if there's a matching seq.
	NEM_txnin_t dummy = {
		.base = {
			.seq = msg->packed.seq,
		},
	};
	NEM_txnin_t *txnin = SPLAY_FIND(
		NEM_txnin_tree_t,
		&this->txns_in,
		&dummy
	);
	if (NULL != txnin) {
		msg->packed.service_id = txnin->service_id;
		msg->packed.command_id = txnin->command_id;
	}

	// NB: Handlers can be removed at runtime; there isn't much that can be
	// done about this since the thunks are owned by the svcmux. So whenever
	// a message comes in, we need to re-resolve the handler against the mux
	// (which should be pretty fast).
	NEM_thunk_t *handler = NEM_svcmux_resolve(
		this->mux,
		msg->packed.service_id,
		msg->packed.command_id
	);
	if (NULL == handler) {
		// XXX: This shouldn't be using a generic error.
		NEM_msghdr_err_t err = {
			.code   = 1,
			.reason = "no handler",
		};
		NEM_msghdr_t hdr = {
			.err = &err,
		};
		NEM_msg_t *reply = NEM_msg_new_reply(msg, 0, 0);
		NEM_msg_set_header(reply, &hdr);
		NEM_chan_send(&this->chan, reply, NULL);
		return;
	}

	if (NULL == txnin) {
		// NB: If this is a cancel request for a transaction we don't have
		// a record for ... just ... ignore it.
		if (msg->packed.flags & NEM_PMSGFLAG_CANCEL) {
			return;
		}

		txnin = NEM_malloc(sizeof(NEM_txnin_t));
		txnin->base.seq = msg->packed.seq;
		txnin->base.mgr = this;
		txnin->base.type = NEM_TXN_IN;
		txnin->service_id = msg->packed.service_id;
		txnin->command_id = msg->packed.command_id;
		SPLAY_INSERT(NEM_txnin_tree_t, &this->txns_in, txnin);
	}

	bool done = 0 == (msg->packed.flags & NEM_PMSGFLAG_CONTINUE);
	NEM_err_t err = NEM_err_none;

	if ((msg->packed.flags & NEM_PMSGFLAG_CANCEL)) {
		// They're cancelling their request.
		txnin->base.cancelled = true;
		done = true;
		err = NEM_err_static("remote cancelled transaction");
	}

	NEM_txn_ca ca = {
		.err   = err,
		.txnin = txnin,
		.mgr   = this,
		.msg   = msg,
		.done  = done,
	};

	NEM_txn_add_msg(&txnin->base, msg);
	chan_ca->msg = NULL; // NB: Claim ownership of this message.
	NEM_thunk_invoke(handler, &ca);

	// NB: The transaction doesn't get freed until the handler sends the
	// final reply.
	// XXX: We should probably set up a timeout here or something maybe? to
	// automatically reply after a period? Maybe it's better to have all that
	// logic on the client-side.
}

static void
NEM_txnmgr_on_msg(NEM_thunk_t *thunk, void *varg)
{
	NEM_chan_ca *ca = varg;
	NEM_msg_t *msg = ca->msg;
	NEM_txnmgr_t *this = NEM_thunk_ptr(thunk);

	if (!NEM_err_ok(ca->err)) {
		NEM_txnmgr_shutdown(this, ca->err);
		return;
	}

	if (NEM_PMSGFLAG_REPLY & msg->packed.flags) {
		NEM_txnmgr_on_reply(this, ca);
	}
	else {
		NEM_txnmgr_on_req(this, ca);
	}

	// XXX: We should probably set a timeout here to detect failures
	// to continue/reply.
	
	// XXX: We should probably check txnin->base.thunk here to ensure that
	// the thing's been set up to process additional messages.
}

static void
NEM_txnmgr_on_close(NEM_thunk1_t *thunk, void *varg)
{
	NEM_txnmgr_t *this = NEM_thunk1_ptr(thunk);
	NEM_chan_ca *ca = varg;
	NEM_txnmgr_shutdown(this, ca->err);
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

	NEM_chan_on_close(&this->chan, NEM_thunk1_new_ptr(
		&NEM_txnmgr_on_close, this
	));
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

void
NEM_txnmgr_set_mux(NEM_txnmgr_t *this, NEM_svcmux_t *mux)
{
	if (NULL != this->mux) {
		NEM_svcmux_decref(this->mux);
	}
	if (NULL == mux) {
		NEM_panic("NEM_txnmgr_set_mux: cannot set a NULL mux");
	}

	bool new = (NULL == this->mux) && (NULL != mux);
	this->mux = NEM_svcmux_copy(mux);

	if (new) {
		NEM_chan_on_msg(&this->chan, NEM_thunk_new_ptr(
			&NEM_txnmgr_on_msg,
			this
		));
	}
}

NEM_txnout_t*
NEM_txnmgr_req(NEM_txnmgr_t *this, NEM_txn_t *parent, NEM_thunk_t *thunk)
{
	if (NULL == thunk) {
		NEM_panic("NEM_txnmgr_req: cannot have a NULL thunk");
	}

	NEM_txnout_t *txnout = NEM_malloc(sizeof(NEM_txnout_t));
	txnout->base.mgr = this;
	txnout->base.type = NEM_TXN_OUT;
	txnout->base.seq = ++this->seq;
	txnout->base.thunk = thunk;

	SPLAY_INSERT(NEM_txnout_tree_t, &this->txns_out, txnout);

	return txnout;
}

void
NEM_txnmgr_req1(
	NEM_txnmgr_t *this,
	NEM_txn_t    *parent,
	NEM_msg_t    *msg,
	NEM_thunk_t  *thunk
) {
	NEM_txnout_t *txnout = NEM_txnmgr_req(this, parent, thunk);
	NEM_txnout_req(txnout, msg);
}
