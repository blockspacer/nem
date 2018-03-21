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

static void
NEM_rootd_svcmgr_next_cb(NEM_thunk_t *thunk, void *varg)
{
	NEM_rootd_svcmgr_t *this = NEM_thunk_ptr(thunk);
	NEM_rootd_cmd_ca *ca = varg;
	*ca->handled = NEM_rootd_svcmgr_dispatch(this, ca->msg, ca->data);
}

void
NEM_rootd_svcmgr_set_next(NEM_rootd_svcmgr_t *this, NEM_rootd_svcmgr_t *next)
{
	NEM_rootd_svcmgr_on_unmatched(this, NEM_thunk_new_ptr(
		&NEM_rootd_svcmgr_next_cb,
		next
	));
}

bool
NEM_rootd_svcmgr_dispatch(NEM_rootd_svcmgr_t *this, NEM_msg_t *msg, void *data)
{
	NEM_rootd_cmd_t dummy = {
		.svc_id = msg->packed.service_id,
		.cmd_id = msg->packed.command_id,
	};

	NEM_rootd_cmd_t *cmd = RB_FIND(NEM_rootd_cmdtree_t, &this->tree, &dummy);

	bool ret = false;
	NEM_rootd_cmd_ca ca = {
		.msg     = msg,
		.handled = &ret,
		.data    = data,
	};

	if (NULL != cmd) {
		ret = true;
		NEM_thunk_invoke(cmd->cb, &ca);
		return ret;
	}
	if (NULL != this->on_unmatched) {
		NEM_thunk_invoke(this->on_unmatched, &ca);
		return ret;
	}

	return false;
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
