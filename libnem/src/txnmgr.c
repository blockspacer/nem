#include "nem.h"

static const int NEM_TXN_DEFAULT_TIMEOUT_MS = 15 * 1000;

static void NEM_txnmgr_remove_txn(NEM_txnmgr_t *this, NEM_txn_t *txn);
static void NEM_txnmgr_add_txn(NEM_txnmgr_t *this, NEM_txn_t *txn);
static void NEM_txnmgr_set_timeout(
	NEM_txnmgr_t  *this,
	NEM_txn_t     *txn,
	struct timeval t
);

static inline int NEM_txn_cmp(const void *lhs, const void *rhs);
static inline int NEM_txn_cmp_timeout(const void *lhs, const void *rhs);

SPLAY_PROTOTYPE(NEM_txnin_tree_t, NEM_txnin_t, link, NEM_txn_cmp);
SPLAY_GENERATE(NEM_txnin_tree_t, NEM_txnin_t, link, NEM_txn_cmp);
SPLAY_PROTOTYPE(NEM_txnout_tree_t, NEM_txnout_t, link, NEM_txn_cmp);
SPLAY_GENERATE(NEM_txnout_tree_t, NEM_txnout_t, link, NEM_txn_cmp);
SPLAY_PROTOTYPE(NEM_txn_tree_t, NEM_txn_t, link, NEM_txn_cmp_timeout);
SPLAY_GENERATE(NEM_txn_tree_t, NEM_txn_t, link, NEM_txn_cmp_timeout);

static inline bool
time_is_zero(struct timeval t)
{
	return 0 == t.tv_sec && 0 == t.tv_usec;
}

static inline bool
time_is_less(struct timeval t1, struct timeval t2) {
	if (t1.tv_sec < t2.tv_sec) {
		return true;
	}
	else if (t1.tv_sec == t2.tv_sec && t1.tv_usec < t2.tv_usec) {
		return true;
	}

	return false;
}

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

static inline int
NEM_txn_cmp_timeout(const void *vlhs, const void *vrhs)
{
	const NEM_txn_t *lhs = vlhs;
	const NEM_txn_t *rhs = vrhs;

	if (lhs->timeout.tv_sec < rhs->timeout.tv_sec) {
		return -1;
	}
	else if (lhs->timeout.tv_sec > rhs->timeout.tv_sec) {
		return 1;
	}
	else if (lhs->timeout.tv_usec < rhs->timeout.tv_usec) {
		return -1;
	}
	else if (lhs->timeout.tv_usec > rhs->timeout.tv_usec) {
		return 1;
	}
	
	return 0;
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
			this->messages[i] = NULL;
		}
		else {
			NEM_panic("uhh what");
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

static void
NEM_txn_cancel_internal(NEM_txn_t *this, NEM_msg_t *msg, NEM_err_t err)
{
	// NB: NEM_txn_cancel_err and this both call NEM_txn_cancel_internal
	// on all children.
	if (this->cancelled) {
		return;
	}

	// NB: This cancels but does not free the underlying transaction.
	// We can't actually free the transaction after giving it to the
	// application code because they might have a dangling reference to
	// it. Instead, flag it as 'cancelled' and just no-op all the
	// operations against it.
	this->cancelled = true;

	for (size_t i = 0; i < this->children_len; i += 1) {
		NEM_txn_cancel_internal(this->children[i], NULL, err);
	}

	// XXX: If this is an outgoing transaction, we should tell the remote
	// that we're not wanting anything else. If this is an incoming
	// transaction that we're cancelling, we should probably tell the 
	// remote (since NEM_txn_cancel is called on our side by timeout,
	// shutdown, or application code).

	NEM_txn_ca ca = {
		.err    = err,
		.txnin  = (NEM_TXN_IN == this->type) ? (NEM_txnin_t*)this : NULL,
		.txnout = (NEM_TXN_OUT == this->type) ? (NEM_txnout_t*)this : NULL,
		.mgr    = this->mgr,
		.msg    = msg,
		.done   = true,
	};
	if (NULL != this->thunk) {
		NEM_thunk_invoke(this->thunk, &ca);
	}
}

void
NEM_txnout_cancel_err(NEM_txnout_t *this, NEM_err_t err)
{
	// NB: This is an exported function, so explicitly free.
	NEM_txn_cancel_internal(&this->base, NULL, err);

	for (size_t i = 0; i < this->base.children_len; i += 1) {
		// XXX: How does this work?
		//NEM_txnout_cancel_err(this->base.children[i], err);
	}

	NEM_txn_free(&this->base);
}

void
NEM_txnout_cancel(NEM_txnout_t *this)
{
	NEM_txnout_cancel_err(this, NEM_err_static("transaction cancelled"));
}

static void
NEM_txn_set_timeout(NEM_txn_t *this, int ms)
{
	if (this->cancelled) {
		return;
	}

	struct timeval new_time = {0};
	if (-1 == ms) {
		/* zero */
	}
	else if (0 > ms) {
		NEM_panic("NEM_txnout_set_timeout: invalid number of ms");
	}
	else {
		gettimeofday(&new_time, NULL);
		new_time.tv_sec += ms / 1000;
		new_time.tv_usec += (ms % 1000) * 1000;

		// NB: Handle overflow for canonicalized values.
		new_time.tv_sec += new_time.tv_usec / (1000 * 1000);
		new_time.tv_usec = new_time.tv_usec % (1000 * 1000);
	}

	NEM_txnmgr_set_timeout(this->mgr, this, new_time);
}

static void
NEM_txnin_set_timeout(NEM_txnin_t *this, int ms)
{
	NEM_txn_set_timeout(&this->base, ms);
}

void
NEM_txnout_set_timeout(NEM_txnout_t *this, int ms)
{
	NEM_txn_set_timeout(&this->base, ms);
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
	// XXX: Sending on a closed channel generates an error, right?
	bool done = 0 == (msg->packed.flags & NEM_PMSGFLAG_CONTINUE);
	msg->packed.seq = this->base.seq;

	if (!time_is_zero(this->base.timeout)) {
		// Explicitly set timeout information.
		struct timeval tv = {0};
		gettimeofday(&tv, NULL);

		int64_t remaining = 0;
		remaining += (this->base.timeout.tv_sec - tv.tv_sec) * 1000;
		remaining += (this->base.timeout.tv_usec - tv.tv_usec) / 1000;
		if (0 > remaining) {
			NEM_panic("already timed out"); // XXX: fix this.
		}

		// XXX: Could use a helper or something to simplify this, but it'd
		// have to be a macro or something which is kind of gross.
		NEM_msghdr_time_t timehdr = {
			.timeout_ms = remaining,
		};

		NEM_msghdr_t *hdr = NEM_msg_header(msg);
		NEM_msghdr_t new_hdr = {0};
		if (NULL != hdr) {
			new_hdr = *hdr;
		}
		new_hdr.time = &timehdr;
		NEM_msg_set_header(msg, &new_hdr);
		NEM_msghdr_free(hdr);
	}

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

	// NB: shutdown happens when the underlying stream breaks. We can't
	// really control when this happens, but there might be dangling refs
	// to the bits and bobbles. Go through and cancel everything. This leaves
	// the actual transactions intact (and in-tree) so they can be removed
	// on free.
	SPLAY_FOREACH(txnin, NEM_txnin_tree_t, &this->txns_in) {
		NEM_txn_cancel_internal(&txnin->base, NULL, this->err);
	}
	SPLAY_FOREACH(txnout, NEM_txnout_tree_t, &this->txns_out) {
		NEM_txn_cancel_internal(&txnout->base, NULL, this->err);
	}
	// NB: The timer tree is automatically cleared out when all the
	// transactions are removed (since NEM_txn_free removes them from the
	// timeout tree).

	NEM_chan_free(&this->chan);
	NEM_timer_free(&this->timer);

	if (NULL != this->mux) {
		NEM_svcmux_unref(this->mux);
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
	else if (txnout->base.cancelled) {
		// We cancelled on our side; maybe we should notify the remote?
		return;
	}
	if (NULL == txnout->base.thunk) {
		// Uhh. What?
		NEM_panic("NEM_txnmgr_on_reply: transaction has no handler?");
	}

	bool done = 0 == (msg->packed.flags & NEM_PMSGFLAG_CONTINUE);
	NEM_err_t err = NEM_err_none;

	// XXX: Should maybe claim the message here instead of below?

	if ((msg->packed.flags & NEM_PMSGFLAG_CANCEL)) {
		err = NEM_err_static("remote cancelled transaction");
		NEM_txn_cancel_internal(&txnout->base, msg, err);
		return;
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
		if (txnin->base.cancelled) {
			// XXX: We might consider notifying the remote gratitously.
			// NB: The message is still owned and freed by NEM_chan_t.
			return;
		}

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
		txnin->base.type = NEM_TXN_IN;
		txnin->service_id = msg->packed.service_id;
		txnin->command_id = msg->packed.command_id;
		NEM_txnmgr_add_txn(this, &txnin->base);

		NEM_msghdr_t *hdr = NEM_msg_header(msg);
		if (NULL != hdr && NULL != hdr->time) {
			NEM_txnin_set_timeout(txnin, hdr->time->timeout_ms);
		}
		NEM_msghdr_free(hdr);
	}

	bool done = 0 == (msg->packed.flags & NEM_PMSGFLAG_CONTINUE);
	NEM_err_t err = NEM_err_none;

	// XXX: Maybe claim the message here?

	if ((msg->packed.flags & NEM_PMSGFLAG_CANCEL)) {
		// They're cancelling their request.
		err = NEM_err_static("remote cancelled transaction");
		NEM_txn_cancel_internal(&txnin->base, msg, err);
		return;
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
}

static void
NEM_txnmgr_on_close(NEM_thunk1_t *thunk, void *varg)
{
	NEM_txnmgr_t *this = NEM_thunk1_ptr(thunk);
	NEM_chan_ca *ca = varg;
	NEM_txnmgr_shutdown(this, ca->err);
}

static void
NEM_txnmgr_on_timer(NEM_thunk_t *thunk, void *varg)
{
	NEM_txnmgr_t *this = NEM_thunk_ptr(thunk);

	struct timeval now;
	gettimeofday(&now, NULL);

	NEM_txn_t *txn = NULL;
	NEM_err_t err = NEM_err_static("transaction timeout");

	while (NULL != (txn = SPLAY_MIN(NEM_txn_tree_t, &this->timeouts))) {
		if (time_is_less(txn->timeout, now)) {
			NEM_txn_cancel_internal(txn, NULL, err);
			SPLAY_REMOVE(NEM_txn_tree_t, &this->timeouts, txn);
		}
	}

	if (NULL != txn) {
		NEM_timer_set_abs(&this->timer, txn->timeout);
	}
}

void
NEM_txnmgr_init(NEM_txnmgr_t *this, NEM_stream_t stream, NEM_app_t *app)
{
	NEM_chan_init(&this->chan, stream);
	NEM_timer_init(&this->timer, app, NEM_thunk_new_ptr(
		&NEM_txnmgr_on_timer,
		this
	));
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

	if (!time_is_zero(txn->timeout)) {
		SPLAY_REMOVE(NEM_txn_tree_t, &this->timeouts, txn);
	}
}

static void
NEM_txnmgr_add_txn(NEM_txnmgr_t *this, NEM_txn_t *txn)
{
	txn->mgr = this;

	if (NEM_TXN_IN == txn->type) {
		SPLAY_INSERT(NEM_txnin_tree_t, &this->txns_in, (NEM_txnin_t*) txn);
	}
	else {
		SPLAY_INSERT(NEM_txnout_tree_t, &this->txns_out, (NEM_txnout_t*) txn);
	}

	if (!time_is_zero(txn->timeout)) {
		SPLAY_INSERT(NEM_txn_tree_t, &this->timeouts, txn);
		if (txn == SPLAY_MIN(NEM_txn_tree_t, &this->timeouts)) {
			NEM_timer_set_abs(&this->timer, txn->timeout);
		}
	}
}

static void
NEM_txnmgr_set_timeout(NEM_txnmgr_t *this, NEM_txn_t *txn, struct timeval t)
{
	if (!time_is_zero(txn->timeout)) {
		SPLAY_REMOVE(NEM_txn_tree_t, &this->timeouts, txn);
	}

	txn->timeout = t;

	if (!time_is_zero(txn->timeout)) {
		SPLAY_INSERT(NEM_txn_tree_t, &this->timeouts, txn);
	}

	NEM_txn_t *min = SPLAY_MIN(NEM_txn_tree_t, &this->timeouts);
	if (NULL != min) {
		NEM_timer_set_abs(&this->timer, min->timeout);
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

	// NB: At this point, we're ensured that nothing is hanging on to txn
	// pointers, so go through and remove any remaining transactions.
	NEM_txnin_t *txnin = NULL;
	NEM_txnout_t *txnout = NULL;

	// NB: NEM_txn_free removes the bits from the tree.
	while (NULL != (txnin = SPLAY_MIN(NEM_txnin_tree_t, &this->txns_in))) {
		NEM_txn_free(&txnin->base);
	}
	while (NULL != (txnout = SPLAY_MIN(NEM_txnout_tree_t, &this->txns_out))) {
		NEM_txn_free(&txnout->base);
	}
}

void
NEM_txnmgr_set_mux(NEM_txnmgr_t *this, NEM_svcmux_t *mux)
{
	if (NULL != this->mux) {
		NEM_svcmux_unref(this->mux);
	}
	if (NULL == mux) {
		NEM_panic("NEM_txnmgr_set_mux: cannot set a NULL mux");
	}

	bool new = (NULL == this->mux) && (NULL != mux);
	this->mux = NEM_svcmux_ref(mux);

	if (new) {
		NEM_chan_on_msg(&this->chan, NEM_thunk_new_ptr(
			&NEM_txnmgr_on_msg,
			this
		));
	}
}

NEM_txnout_t*
NEM_txnmgr_req(NEM_txnmgr_t *this, NEM_txnin_t *parent, NEM_thunk_t *thunk)
{
	if (NULL == thunk) {
		NEM_panic("NEM_txnmgr_req: cannot have a NULL thunk");
	}
	if (!NEM_err_ok(this->err)) {
		NEM_panic("XXX need to handle the error here"); // XXX
	}

	NEM_txnout_t *txnout = NEM_malloc(sizeof(NEM_txnout_t));
	txnout->base.type = NEM_TXN_OUT;
	txnout->base.seq = ++this->seq;
	txnout->base.thunk = thunk;
	if (NULL != parent) {
		txnout->base.timeout = parent->base.timeout;
	}
	NEM_txnmgr_add_txn(this, &txnout->base);

	if (NULL == parent) {
		NEM_txnout_set_timeout(txnout, NEM_TXN_DEFAULT_TIMEOUT_MS);
	}

	return txnout;
}

void
NEM_txnmgr_req1(
	NEM_txnmgr_t *this,
	NEM_txnin_t  *parent,
	NEM_msg_t    *msg,
	NEM_thunk_t  *thunk
) {
	NEM_txnout_t *txnout = NEM_txnmgr_req(this, parent, thunk);
	NEM_txnout_req(txnout, msg);
}
