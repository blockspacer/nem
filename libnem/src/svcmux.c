#include "nem.h"

void
NEM_svcmux_init(NEM_svcmux_t *this)
{
	bzero(this, sizeof(*this));
	this->refcount = 1;
}

NEM_svcmux_t*
NEM_svcmux_copy(NEM_svcmux_t *this)
{
	if (this->refcount <= 0) {
		NEM_panic("NEM_svcmux_copy: invalid refcount");
	}

	this->refcount += 1;
	return this;
}

static void
NEM_svcmux_free(NEM_svcmux_t *this)
{
	if (this->refcount != 0) {
		NEM_panic("NEM_svcmux_free: invalid refcount");
	}

	for (size_t i = 0; i < this->handlers_len; i += 1) {
		if (NULL != this->handlers[i].thunk) {
			NEM_thunk_free(this->handlers[i].thunk);
		}
	}
	free(this->handlers);
	this->refcount = -1000;
}

void
NEM_svcmux_add_handlers(
	NEM_svcmux_t       *this,
	NEM_svcmux_entry_t *handlers,
	size_t              handlers_len
) {
	size_t max_len = handlers_len + this->handlers_len;
	size_t cur_len = 0;
	NEM_svcmux_entry_t *new_list = NEM_malloc(
		max_len * sizeof(NEM_svcmux_entry_t)
	);

	struct {
		NEM_svcmux_entry_t *entries;
		size_t              entries_len;
	}
	merge_lists[] = {
		// NB: The new handlers take priority.
		{ handlers,       handlers_len       },
		{ this->handlers, this->handlers_len },
	};

	for (size_t l = 0; l < NEM_ARRSIZE(merge_lists); l += 1) {
		NEM_svcmux_entry_t *new_entries = merge_lists[l].entries;
		size_t new_entries_len = merge_lists[l].entries_len;

		for (size_t i = 0; i < new_entries_len; i += 1) {
			bool dupe = false;

			for (size_t j = 0; j < cur_len; j += 1) {
				if (
					new_entries[i].svc_id == new_list[j].svc_id
					&& new_entries[i].cmd_id == new_list[j].cmd_id
				) {
					dupe = true;
					break;
				}
			}
			if (!dupe) {
				new_list[cur_len] = new_entries[i];
				cur_len += 1;
			}
			else if (NULL != new_entries[i].thunk) {
				NEM_thunk_free(new_entries[i].thunk);
			}
		}
	}

	free(this->handlers);
	this->handlers = new_list;
	this->handlers_len = cur_len;
}

void
NEM_svcmux_decref(NEM_svcmux_t *this)
{
	if (0 > this->refcount) {
		NEM_panic("NEM_svcmux_decref: refcount invalid");
	}

	this->refcount -= 1;
	if (0 == this->refcount) {
		NEM_svcmux_free(this);
	}
}

NEM_thunk_t*
NEM_svcmux_resolve(
	NEM_svcmux_t *this,
	uint16_t      svc_id,
	uint16_t      cmd_id
) {
	for (size_t i = 0; i < this->handlers_len; i += 1) {
		if (
			this->handlers[i].svc_id == svc_id
			&& this->handlers[i].cmd_id == cmd_id
		) {
			return this->handlers[i].thunk;
		}
	}

	return NULL;
}
