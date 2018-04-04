#pragma once

typedef struct NEM_txnmgr_t NEM_txnmgr_t;
typedef struct NEM_txn_t NEM_txn_t;

// NEM_txn_t represents a transaction. A transaction encapsulates the data
// around a set of messages -- either incoming or outgoing. Transactions
// are hierarchical -- they can be bound to a parent transaction -- so that
// cancelling the parent transaction propagates down to all children.
struct NEM_txn_t {
	uint64_t       seq;

	size_t        children_len;
	NEM_txn_t   **children;
	NEM_txnmgr_t *mgr;

	size_t        messages_len;
	NEM_msg_t   **messsages;

	// NB: This isn't a NEM_thunk1_t because we might get multiple replies.
	// The caller should check NEM_txn_ca.done to determine whether or not
	// there will be additional calls.
	NEM_thunk_t  *thunk;
};

// NEM_txnin_t is a specialization of NEM_txn_t that represents an incoming
// request.
typedef struct NEM_txnin_t {
	NEM_txn_t                base;
	SPLAY_ENTRY(NEM_txnin_t) link;
}
NEM_txnin_t;

// NEM_txnout_t is a specialization of NEM_txn_t that represents an outgoing
// request.
typedef struct NEM_txnout_t {
	NEM_txn_t                 base;
	struct timeval            timeout;
	SPLAY_ENTRY(NEM_txnout_t) link;
}
NEM_txnout_t;

// NEM_txn_data returns the data pointer previously set by NEM_txn_set_data.
void *NEM_txn_data(NEM_txn_t *txn);

// NEM_txn_set_data binds arbitrary data with this NEM_txn_t. The data can
// be retrieved with NEM_txn_data.
void NEM_txn_set_data(NEM_txn_t *txn, void *data);

// NEM_txn_cancel aborts the entire transaction, removing it and every
// child transaction from their respective managers. This invalidates the
// transaction object. For outgoing transactions, this invokes the callback
// before freeing the transaction.
void NEM_txn_cancel(NEM_txn_t *txn);

// NEM_txnout_set_timeout sets the timeout in seconds for this transaction. Once
// the period is expired, the transaction (and all children) are automatically
// cancelled. This should be set on the top-level transaction -- when children
// transactions are associated they make a copy of the parent's absolute
// timeout value.
void NEM_txnout_set_timeout(NEM_txn_t *txn, int seconds);

// NEM_txnin_reply finalizes the transaction and sends the provided message.
// This invalidates the transaction object.
void NEM_txnin_reply(NEM_txn_t *txn, NEM_msg_t *msg);

// NEM_txnin_reply_continue sends a message without invalidating the
// transaction object.
void NEM_txnin_reply_continue(NEM_txn_t *txn, NEM_msg_t *msg);

// NEM_txntree_t is a tree of transactions, ordered by seqid.
typedef SPLAY_HEAD(NEM_txnin_tree_t, NEM_txnin_t) NEM_txnin_tree_t;
typedef SPLAY_HEAD(NEM_txnout_tree_t, NEM_txnout_t) NEM_txnout_tree_t;

// NEM_svcmux_entry_t for internal use.
typedef struct {
	uint16_t     svc_id;
	uint16_t     cmd_id;
	NEM_thunk_t *thunk;
}
NEM_svcmux_entry_t;

// NEM_svcmux_t is a service multiplexer. It contains a set of svc/cmd to
// thunk mappings. When an incoming request is received that matches a
// registered entry, a new NEM_txnin_t is created and passed to the thunk
// via a NEM_txn_ca. If done isn't set, the NEM_txn_t.thunk should be set
// and will be invoked when more incoming messages are received.
//
// NEM_svcmux_t's are refcounted and can be shared between multiple
// NEM_txnmgr_t's. The handlers aren't notified when they go away -- the
// thunk is just freed -- so they shouldn't be bound to any dynamic
// allocations (use thunk-inline allocations if really needed).
typedef struct {
	NEM_svcmux_entry_t *handlers;
	size_t              handlers_len;
	int                 refcount;
}
NEM_svcmux_t;

// NEM_svcmux_init initializes a new NEM_svcmux_t with a refcount of 1.
// After being passed to a NEM_txnmgr_t (or whatever) it should be decref'd.
void NEM_svcmux_init(NEM_svcmux_t *this);

// NEM_svcmux_add_handlers adds an array of handlers into the svcmux. Existing
// svc/cmd handlers are overriden.
void NEM_svcmux_add_handlers(
	NEM_svcmux_t       *this,
	NEM_svcmux_entry_t *entries,
	size_t              entries_len
);

// NEM_svcmux_decref decrements the refcount of the NEM_svcmux_t. When the
// refcount is 0, the mux is freed.
void NEM_svcmux_decref(NEM_svcmux_t *this);

// NEM_svcmux_merge copies all commands from the src mux into the this
// mux. Commands from src override those in this. A NULL NEM_svcmux_t
// is valid and means "no transactions are handled".
// XXX: Is that the right behavior?
void NEM_svcmux_merge(NEM_svcmux_t *this, const NEM_svcmux_t *src);

// NEM_txnmgr_t wraps a NEM_chan_t and provides a transactional interface
// over it. It handles assignment of sequence ids and can delegate request
// dispatching.
struct NEM_txnmgr_t {
	NEM_chan_t        chan;
	NEM_txnin_tree_t  txns_in;
	NEM_txnout_tree_t txns_out;
	NEM_svcmux_t     *mux;
	uint64_t          seq;
	NEM_err_t         err;
};

// NEM_txnmgr_init initializes the txnmgr with the specified stream. The
// stream becomes owned by this txnmgr and will be freed when the txnmgr
// is closed.
void NEM_txnmgr_init(NEM_txnmgr_t *this, NEM_stream_t stream);
void NEM_txnmgr_free(NEM_txnmgr_t *this);

// NEM_txnmgr_set_mux replaces the existing mux with the specified one.
void NEM_txnmgr_set_mux(NEM_txnmgr_t *this, NEM_svcmux_t *mux);

// NEM_txnmgr_req initiates a request against the connection underlying
// the NEM_txnmgr_t. If a parent NEM_txn_t is provided, the returned txn
// is added as a child (and will be cancelled if the parent is cancelled).
// The thunk will be passed a NEM_txn_ca for each incoming message for
// the outgoing request.
NEM_txnout_t* NEM_txnmgr_req(
	NEM_txnmgr_t *this,
	NEM_txn_t    *parent,
	NEM_thunk_t  *thunk
);

// NEM_txnmgr_close cancels all inflight transactions. Their callbacks will
// be invoked with done+err set. It closes the underlying channel and 
// so forth.
void NEM_txnmgr_close(NEM_txnmgr_t *this);

typedef struct {
	NEM_err_t  err;
	NEM_txn_t *txn;
	NEM_msg_t *msg;
	bool       done;
}
NEM_txn_ca;

//
// Inline static dispatch functions
//

static inline void*
NEM_txnin_data(NEM_txnin_t *txn)
{
	return NEM_txn_data(&txn->base);
}

static inline void*
NEM_txnout_data(NEM_txnout_t *txn)
{
	return NEM_txn_data(&txn->base);
}

static inline void
NEM_txnin_set_data(NEM_txnin_t *txn, void *data)
{
	return NEM_txn_set_data(&txn->base, data);
}

static inline void
NEM_txnout_set_data(NEM_txnout_t *txn, void *data)
{
	return NEM_txn_set_data(&txn->base, data);
}

static inline void
NEM_txnin_cancel(NEM_txnin_t *txn)
{
	return NEM_txn_cancel(&txn->base);
}

static inline void
NEM_txnout_cancel(NEM_txnout_t *txn)
{
	return NEM_txn_cancel(&txn->base);
}
