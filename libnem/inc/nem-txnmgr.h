#pragma once

typedef struct NEM_txnmgr_t NEM_txnmgr_t;
typedef struct NEM_txn_t NEM_txn_t;

typedef enum {
	NEM_TXN_IN,
	NEM_TXN_OUT,
}
NEM_txntype_t;

// NEM_txn_t represents a transaction. A transaction encapsulates the data
// around a set of messages -- either incoming or outgoing. Transactions
// are hierarchical -- they can be bound to a parent transaction -- so that
// cancelling the parent transaction propagates down to all children.
struct NEM_txn_t {
	uint64_t       seq;
	struct timeval timeout;
	NEM_txntype_t  type;
	bool           cancelled;

	size_t        children_len;
	NEM_txn_t   **children;
	NEM_txnmgr_t *mgr;
	void         *data;

	size_t        messages_len;
	NEM_msg_t   **messages;
	NEM_thunk_t  *thunk;
};

// NEM_txnin_t is a specialization of NEM_txn_t that represents an incoming
// request.
typedef struct NEM_txnin_t {
	NEM_txn_t                base;
	SPLAY_ENTRY(NEM_txnin_t) link;
	uint16_t                 service_id;
	uint16_t                 command_id;
}
NEM_txnin_t;

// NEM_txnout_t is a specialization of NEM_txn_t that represents an outgoing
// request.
typedef struct NEM_txnout_t {
	NEM_txn_t                 base;
	SPLAY_ENTRY(NEM_txnout_t) link;
}
NEM_txnout_t;

// NEM_txn_data returns the data pointer previously set by NEM_txn_set_data.
void *NEM_txn_data(NEM_txn_t *this);

// NEM_txn_set_data binds arbitrary data with this NEM_txn_t. The data can
// be retrieved with NEM_txn_data.
void NEM_txn_set_data(NEM_txn_t *this, void *data);

// NEM_txn_cancel aborts the entire transaction, removing it and every
// child transaction from their respective managers. This invalidates the
// transaction object. For outgoing transactions, this invokes the callback
// before freeing the transaction.
void NEM_txn_cancel(NEM_txn_t *this);
void NEM_txn_cancel_err(NEM_txn_t *this, NEM_err_t err);

// NEM_txnout_set_timeout sets the timeout in seconds for this transaction. Once
// the period is expired, the transaction (and all children) are automatically
// cancelled. This should be set on the top-level transaction -- when children
// transactions are associated they make a copy of the parent's absolute
// timeout value.
// 
// Setting -1 seconds makes the timeout infinite.
void NEM_txnout_set_timeout(NEM_txnout_t *this, int seconds);

// NEM_txnin_reply finalizes the transaction and sends the provided message.
// This invalidates the transaction object.
void NEM_txnin_reply(NEM_txnin_t *this, NEM_msg_t *msg);

// NEM_txnin_reply_err finalizes the transaction and sends the error.
void NEM_txnin_reply_err(NEM_txnin_t *this, NEM_err_t err);

// NEM_txnin_reply_continue sends a message without invalidating the
// transaction object.
void NEM_txnin_reply_continue(NEM_txnin_t *this, NEM_msg_t *msg);

// NEM_txnout_req finializes the outgoing transaction and sends the 
// provided message.
void NEM_txnout_req(NEM_txnout_t *this, NEM_msg_t *msg);

// NEM_txnout_req_continue sends another message as part of the given
// transaction. It does not finalize the transaction (NEM_txnout_req must
// eventually be called).
void NEM_txnout_req_continue(NEM_txnout_t *this, NEM_msg_t *msg);

// NEM_txn*_tree_t is a tree of transactions, ordered by seqid.
typedef SPLAY_HEAD(NEM_txnin_tree_t, NEM_txnin_t) NEM_txnin_tree_t;
typedef SPLAY_HEAD(NEM_txnout_tree_t, NEM_txnout_t) NEM_txnout_tree_t;

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
// This adds a ref to the passed in mux to keep it alive.
void NEM_txnmgr_set_mux(NEM_txnmgr_t *this, NEM_svcmux_t *mux);

// NEM_txnmgr_req initiates a request against the connection underlying
// the NEM_txnmgr_t. If a parent NEM_txn_t is provided, the returned txn
// is added as a child (and will be cancelled if the parent is cancelled).
// The thunk will be passed a NEM_txn_ca for each incoming message for
// the outgoing request. The returned transaction is valid until the thunk
// is invoked with ca.done.
NEM_txnout_t* NEM_txnmgr_req(
	NEM_txnmgr_t *this,
	NEM_txn_t    *parent,
	NEM_thunk_t  *thunk
);

// NEM_txnmgr_req1 is a shortcut for sending a transaction with a single
// transaction. The NEM_txnout_t isn't returned, so transactions sent this
// way can't be explicitly cancelled (though cancelling the parent
// transaction should still cancel it).
void NEM_txnmgr_req1(
	NEM_txnmgr_t *this,
	NEM_txn_t    *parent,
	NEM_msg_t    *msg,
	NEM_thunk_t  *thunk
);

// NEM_txnmgr_close cancels all inflight transactions. Their callbacks will
// be invoked with done+err set. It closes the underlying channel and 
// so forth.
void NEM_txnmgr_close(NEM_txnmgr_t *this);

typedef struct {
	NEM_err_t     err;
	NEM_txnin_t  *txnin;
	NEM_txnout_t *txnout;
	NEM_txnmgr_t *mgr;
	NEM_msg_t    *msg;
	bool          done;
}
NEM_txn_ca;

//
// Inline static dispatch functions
//

static inline void*
NEM_txnin_data(NEM_txnin_t *this)
{
	return NEM_txn_data(&this->base);
}

static inline void*
NEM_txnout_data(NEM_txnout_t *this)
{
	return NEM_txn_data(&this->base);
}

static inline void
NEM_txnin_set_data(NEM_txnin_t *this, void *data)
{
	return NEM_txn_set_data(&this->base, data);
}

static inline void
NEM_txnout_set_data(NEM_txnout_t *this, void *data)
{
	return NEM_txn_set_data(&this->base, data);
}

static inline void
NEM_txnin_cancel(NEM_txnin_t *this)
{
	return NEM_txn_cancel(&this->base);
}

static inline void
NEM_txnout_cancel(NEM_txnout_t *this)
{
	return NEM_txn_cancel(&this->base);
}
