#include "nem.h"
#include "c-mounts.h"
#include "c-log.h"
#include "c-md.h"

typedef enum {
	NEM_MOUNT_MDRO,
	NEM_MOUNT_MDRW,
	NEM_MOUNT_NULL,
}
NEM_mount_type_t;

struct NEM_mount_t {
	// NB: These have to be strictly ordered.
	LIST_ENTRY(NEM_mount_t) link;

	NEM_mount_type_t type;
	NEM_md_t        *md; // NB: non-NULL for MD mounts.
	char            *source;
	char            *dest;
	bool             owned;
};

typedef LIST_HEAD(NEM_mountlist_t, NEM_mount_t) NEM_mountlist_t;
static NEM_mountlist_t static_mounts = {0};

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	if (!LIST_EMPTY(&static_mounts)) {
		NEM_panicf("mounts: corrupt static_mounts during setup");
	}

	NEM_logf(COMP_MOUNTS, "setup");
	return NEM_err_none;
}

static void
teardown(NEM_app_t *app)
{
	NEM_logf(COMP_MOUNTS, "teardown");
}

const NEM_app_comp_t NEM_rootd_c_mounts = {
	.name     = "mounts",
	.setup    = &setup,
	.teardown = &teardown,
};
