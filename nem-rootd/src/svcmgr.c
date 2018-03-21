#include "svcmgr.h"

RB_GENERATE(
	NEM_rootd_cmdtree_t,
	NEM_rootd_cmd_t,
	link,
	NEM_rootd_cmd_cmp
);

void
NEM_rootd_svcmgr_init(NEM_rootd_svcmgr_t *this)
{
	bzero(this, sizeof(*this));
	RB_INIT(&this->tree);
}

void
NEM_rootd_svcmgr_free(NEM_rootd_svcmgr_t *this)
{
	NEM_rootd_cmd_t *cmd = NULL, *tmp = NULL;

	RB_FOREACH_SAFE(cmd, NEM_rootd_cmdtree_t, &this->tree, tmp) {
		RB_REMOVE(NEM_rootd_cmdtree_t, &this->tree, cmd);
		NEM_thunk_free(cmd->cb);
		free(cmd);
	}

	if (NULL != this->on_unmatched) {
		NEM_thunk_free(this->on_unmatched);
	}
}

void
NEM_rootd_svcmgr_on_unmatched(NEM_rootd_svcmgr_t *this, NEM_thunk_t *cb)
{
	if (NULL != this->on_unmatched) {
		NEM_thunk_free(this->on_unmatched);
	}
	this->on_unmatched = cb;
}

bool
NEM_rootd_svcmgr_dispatch(NEM_rootd_svcmgr_t *this, NEM_msg_t *msg)
{
	NEM_rootd_cmd_t dummy = {
		.svc_id = msg->packed.service_id,
		.cmd_id = msg->packed.command_id,
	};

	NEM_rootd_cmd_t *cmd = RB_FIND(NEM_rootd_cmdtree_t, &this->tree, &dummy);
	NEM_rootd_cmd_ca ca = {
		.msg = msg,
	};

	if (NULL != cmd) {
		NEM_thunk_invoke(cmd->cb, &ca);
		return true;
	}
	if (NULL != this->on_unmatched) {
		NEM_thunk_invoke(this->on_unmatched, &ca);
		return true;
	}

	return false;
}

static void
NEM_rootd_svcmgr_dispatch_cb(NEM_thunk_t *thunk, void *varg)
{
	NEM_rootd_svcmgr_t *this = NEM_thunk_ptr(thunk);
	NEM_rootd_cmd_ca *ca = varg;
	NEM_rootd_svcmgr_dispatch(this, ca->msg);
}

NEM_thunk_t*
NEM_rootd_svcmgr_dispatch_thunk(NEM_rootd_svcmgr_t *this)
{
	return NEM_thunk_new_ptr(
		&NEM_rootd_svcmgr_dispatch_cb,
		this
	);
}

NEM_err_t
NEM_rootd_svcmgr_add(
	NEM_rootd_svcmgr_t *this,
	uint16_t            svc_id,
	uint16_t            cmd_id,
	NEM_thunk_t        *cb
) {
	NEM_rootd_cmd_t *cmd = NEM_malloc(sizeof(NEM_rootd_cmd_t));
	cmd->svc_id = svc_id;
	cmd->cmd_id = cmd_id;
	cmd->cb = cb;

	NEM_rootd_cmd_t *old = RB_FIND(NEM_rootd_cmdtree_t, &this->tree, cmd);
	if (NULL != old) {
		free(cmd);
		NEM_thunk_free(cb);
		return NEM_err_static("NEM_rootd_svcmgr_add: dupe command added");
	}

	RB_INSERT(NEM_rootd_cmdtree_t, &this->tree, cmd);
	return NEM_err_none;
}
