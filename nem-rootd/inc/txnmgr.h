#pragma once
#include <sys/types.h>
#include <sys/tree.h>
#include "nem.h"

// NEM_rootd_txn_t is a stub holding an outgoing transaction. It's used for
// internal bookkeeping.
typedef struct NEM_rootd_txn_t {
	SPLAY_ENTRY(NEM_rootd_txn_t) link;

	uint64_t      seq_id;
	NEM_thunk1_t *cb;
}
NEM_rootd_txn_t;

static inline int
NEM_rootd_txn_cmp(const void *vlhs, const void *vrhs)
{
	const NEM_rootd_txn_t *lhs = vlhs;
	const NEM_rootd_txn_t *rhs = vrhs;
	return (lhs->seq_id < rhs->seq_id)
		? -1
		: (lhs->seq_id > rhs->seq_id ? 1 : 0);
}

typedef SPLAY_HEAD(NEM_rootd_txntree_t, NEM_rootd_txn_t) NEM_rootd_txntree_t;
SPLAY_PROTOTYPE(
	NEM_rootd_txntree_t,
	NEM_rootd_txn_t,
	link,
	NEM_rootd_txn_cmp
);

typedef struct {
	NEM_err_t  err;
	NEM_msg_t *msg;
	bool       done;
}
NEM_rootd_txn_ca;

typedef struct {
	NEM_chan_t   *chan;
	uint64_t      next_seq;
	NEM_thunk_t  *on_req;
	NEM_thunk1_t *on_close;
	bool          closed;

	NEM_rootd_txntree_t txns;
}
NEM_rootd_txnmgr_t;

// NEM_rootd_txnmgr_init initializes a new transaction manager context with
// the given stream. The transaction manager keeps track of request/response
// pairs and lets the caller just deal with thunks, in addition to mapping
// incoming requests to registered per-service/command thunks. on_req is called
// whenever an incoming request is received and must be set. on_req is passed
// a NEM_chan_ca (!!!!) rather than a NEM_rootd_txn_ca.
// 
// This does not free the passed chan! The caller still owns it.
void NEM_rootd_txnmgr_init(
	NEM_rootd_txnmgr_t *this,
	NEM_chan_t         *chan,
	NEM_thunk_t        *on_req
);

// NEM_rootd_txnmgr_free closes out the txnmgr, freeing the backing channel.
// All pending transactions are cancelled and their callbacks invoked.
void NEM_rootd_txnmgr_free(NEM_rootd_txnmgr_t *this);

// NEM_rootd_txnmgr_on_close sets a handler to be invoked when the manager's
// channel has closed. Prior to this, all pending transactions will receive
// error messages.
NEM_err_t NEM_rootd_txnmgr_on_close(
	NEM_rootd_txnmgr_t *this,
	NEM_thunk1_t       *thunk
);

// NEM_rootd_txnmgr_req1 sends a request and invokes the passed thunk when
// a response is received. The thunk is passed a NEM_rootd_txn_ca and is 
// always invoked.
void NEM_rootd_txnmgr_req1(
	NEM_rootd_txnmgr_t *this,
	NEM_msg_t          *msg,
	NEM_thunk1_t       *cb
);
