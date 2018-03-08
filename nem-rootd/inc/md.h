#pragma once
#include "nem.h"

// NEM_md_t is a controls an md-backed filesystem. This is system-level
// shenanigans, so everything is centralized in a manager and ref-counted.
typedef struct {
	const char *file;
	int         unit;
}
NEM_md_t;

// NEM_mdmgr_init initializes the centralized portions of the md subsystem
// and looks at the initial machine state. If something looks weird (e.g.
// there are already a bunch of md's allocated) it'll barf on you.
NEM_err_t NEM_mdmgr_init();

// NEM_mdmgr_free explicitly frees all md devices we've allocated. This should
// be called via atexit, but who knows.
void NEM_mdmgr_free();

// NEM_md_init initializes an md device with the given path/ro bit. An image
// cannot be opened multiple times rw (but multiple images can be open ro).
// Duplicate images are backed by the same md device, this internally does
// refcounting.
NEM_err_t NEM_md_init(NEM_md_t *this, const char *path, bool ro);

// NEM_md_free frees an md device.
void NEM_md_free(NEM_md_t *this);
