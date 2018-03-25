#include "svclist.h"

void
NEM_rootd_svclist_init(NEM_rootd_svclist_t *this)
{
	bzero(this, sizeof(*this));
}

void
NEM_rootd_svclist_free(NEM_rootd_svclist_t *this)
{
	free(this->defs);
}

void
NEM_rootd_svclist_add(
	NEM_rootd_svclist_t *this,
	NEM_rootd_svcdef_t  *def
) {
	this->len += 1;
	this->defs = NEM_panic_if_null(realloc(
		this->defs,
		sizeof(NEM_rootd_svcdef_t*) * this->len
	));
	this->defs[this->len - 1] = def;
}

bool
NEM_rootd_svclist_dispatch(
	NEM_rootd_svclist_t *this,
	NEM_msg_t           *msg,
	NEM_chan_t          *chan
) {
	for (size_t i = 0; i < this->len; i += 1) {
		bool done = NEM_rootd_svcdef_dispatch(
			this->defs[i],
			msg,
			chan
		);
		if (done) {
			return true;
		}
	}

	return false;
}
