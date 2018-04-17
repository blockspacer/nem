#pragma once

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
// After being passed to a NEM_txnmgr_t (or whatever) it should be unref'd.
void NEM_svcmux_init(NEM_svcmux_t *this);

// NEM_svcmux_add_handlers adds an array of handlers into the svcmux. Existing
// svc/cmd handlers are overriden. If an entry has a NULL thunk, it removes an
// existing entry. If there are duplicate handlers in the entries list, the
// ones earlier have priority.
void NEM_svcmux_add_handlers(
	NEM_svcmux_t       *this,
	NEM_svcmux_entry_t *entries,
	size_t              entries_len
);

// NEM_svcmux_ref increments the refcount for NEM_svcmux_t.
NEM_svcmux_t* NEM_svcmux_ref(NEM_svcmux_t *this);

// NEM_svcmux_unref decrements the refcount of the NEM_svcmux_t. When the
// refcount is 0, the mux is freed.
void NEM_svcmux_unref(NEM_svcmux_t *this);

// NEM_svcmux_resolve returns the thunk associated with the specified 
// svc_id/cmd_id.
NEM_thunk_t* NEM_svcmux_resolve(
	NEM_svcmux_t *this,
	uint16_t      svc_id,
	uint16_t      cmd_id
);
