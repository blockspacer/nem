#pragma once
#include "nem.h"

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

typedef struct {
	NEM_msg_t *msg;
}
NEM_rootd_cmd_ca;

typedef struct {
	NEM_rootd_cmdtree_t tree;
	NEM_thunk_t        *on_unmatched;
}
NEM_rootd_svcmgr_t;

void NEM_rootd_svcmgr_init(NEM_rootd_svcmgr_t *this);
void NEM_rootd_svcmgr_free(NEM_rootd_svcmgr_t *this);

void NEM_rootd_svcmgr_on_unmatched(NEM_rootd_svcmgr_t *this, NEM_thunk_t *cb);
bool NEM_rootd_svcmgr_dispatch(NEM_rootd_svcmgr_t *this, NEM_msg_t *msg);

NEM_thunk_t* NEM_rootd_svcmgr_dispatch_thunk(NEM_rootd_svcmgr_t *this);

NEM_err_t NEM_rootd_svcmgr_add(
	NEM_rootd_svcmgr_t *this,
	uint16_t            svc_id,
	uint16_t            cmd_id,
	NEM_thunk_t        *cb
);
