#pragma once
#include "svcdef.h"

typedef struct {
	NEM_rootd_svcdef_t **defs;
	size_t               len;
}
NEM_rootd_svclist_t;

void NEM_rootd_svclist_init(NEM_rootd_svclist_t *this);
void NEM_rootd_svclist_free(NEM_rootd_svclist_t *this);
void NEM_rootd_svclist_add(
	NEM_rootd_svclist_t *this,
	NEM_rootd_svcdef_t  *def
);

bool NEM_rootd_svclist_dispatch(
	NEM_rootd_svclist_t *this,
	NEM_msg_t           *msg,
	NEM_chan_t          *chan
);
