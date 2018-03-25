#pragma once
#include "nem.h"

// NEM_rootd_cmd_t is a svc_id/cmd_id/thunk tuple that's used for command
// dispatching bookkeeping. It shouldn't be used for anything.
typedef struct NEM_rootd_cmd_t {
	RB_ENTRY(NEM_rootd_cmd_t) link;

	uint16_t     svc_id;
	uint16_t     cmd_id;
	NEM_thunk_t *cb;
}
NEM_rootd_cmd_t;

static inline int
NEM_rootd_cmd_cmp(const void *vlhs, const void *vrhs)
{
	const NEM_rootd_cmd_t *lhs = vlhs;
	const NEM_rootd_cmd_t *rhs = vrhs;
	uint32_t lhs_val = (lhs->svc_id << 2) | lhs->cmd_id;
	uint32_t rhs_val = (rhs->svc_id << 2) | rhs->cmd_id;
	return (lhs_val < rhs_val)
		? -1
		: (lhs_val > rhs_val ? 1 : 0);
}

typedef RB_HEAD(NEM_rootd_cmdtree_t, NEM_rootd_cmd_t) NEM_rootd_cmdtree_t;
RB_PROTOTYPE(
	NEM_rootd_cmdtree_t,
	NEM_rootd_cmd_t,
	link,
	NEM_rootd_cmd_cmp
);

// NEM_rootd_msg_ca is the callback arg passed out from NEM_rootd_svcdef_t
// to things waiting for messages.
typedef struct {
	NEM_msg_t *msg;
	bool      *handled;
	void      *data;
}
NEM_rootd_cmd_ca;

// NEM_rootd_svcdef_t is a service dispatch manager. It keeps a handle on a
// pile of registered services and lets you dispatch messages straight to
// those service callbacks. It's hierarchical -- you can attach up to one
// child to each NEM_rootd_svcdef_t which receives the messages when there's
// no matching entry in the parent.
typedef struct {
	NEM_rootd_cmdtree_t tree;
	NEM_thunk_t        *on_unmatched;
}
NEM_rootd_svcdef_t;

void NEM_rootd_svcdef_init(NEM_rootd_svcdef_t *this);
void NEM_rootd_svcdef_free(NEM_rootd_svcdef_t *this);

// NEM_rootd_svcdef_on_unmatched sets a thunk which is invoked with a
// NEM_rootd_cmd_ca whenever an incoming message isn't matched. The return
// value of the corresponding NEM_rootd_svcdef_dispatch can be set via the
// ca->handled pointer.
void NEM_rootd_svcdef_on_unmatched(NEM_rootd_svcdef_t *this, NEM_thunk_t *cb);

// NEM_rootd_svcdef_dispatch dispatches a message. It returns true iff there
// was a matching handler. The data is passed through I'm not sure what needs
// to go there yet
// XXX: Figure out what the fuck data needs to be and thread it through.
bool NEM_rootd_svcdef_dispatch(
	NEM_rootd_svcdef_t *this,
	NEM_msg_t          *msg,
	void               *data
);

// NEM_rootd_svcdef_add registers a service/command/thunk tuple.
NEM_err_t NEM_rootd_svcdef_add(
	NEM_rootd_svcdef_t *this,
	uint16_t            svc_id,
	uint16_t            cmd_id,
	NEM_thunk_t        *cb
);
