#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>

#include "nem.h"
#include "c-mounts.h"
#include "c-log.h"
#include "c-disk.h"

typedef enum {
	NEM_MOUNT_UFS,
	NEM_MOUNT_NULLFS,
	NEM_MOUNT_DEVFS,
	NEM_MOUNT_UNKNOWN,
}
NEM_mount_type_t;

static const struct {
	NEM_mount_type_t type;
	const char      *name;
}
fstype_names[] = {
	{ NEM_MOUNT_UFS,     "ufs"     },
	{ NEM_MOUNT_NULLFS,  "nullfs"  },
	{ NEM_MOUNT_DEVFS,   "devfs"   },
	{ NEM_MOUNT_UNKNOWN, "unknown" },
};

static NEM_mount_type_t
NEM_mount_type_from_str(const char *str)
{
	for (size_t i = 0; i < NEM_ARRSIZE(fstype_names); i += 1) {
		if (!strcmp(fstype_names[i].name, str)) {
			return fstype_names[i].type;
		}
	}
	return NEM_MOUNT_UNKNOWN;
}

static const char*
NEM_mount_type_str(NEM_mount_type_t ty)
{
	for (size_t i = 0; i < NEM_ARRSIZE(fstype_names); i += 1) {
		if (ty == fstype_names[i].type) {
			return fstype_names[i].name;
		}
	}
	return "unknown";
}

struct NEM_mount_t {
	// NB: These have to be strictly ordered since they need to be
	// unmounted in the reverse order.
	LIST_ENTRY(NEM_mount_t) link;

	NEM_mount_type_t type;
	NEM_disk_t      *disk; // NB: empty for NULLFS mounts.
	char            *source;
	char            *dest;
	bool             owned;
	bool             seen;
};

static int
NEM_mount_cmp(const NEM_mount_t *lhs, const NEM_mount_t *rhs)
{
	if (lhs->type != rhs->type) {
		return (lhs->type > rhs->type) ? 1 : -1;
	}

	int diff = strcmp(lhs->source, rhs->source);
	if (0 != diff) {
		return diff;
	}

	return strcmp(lhs->dest, rhs->dest);
}

static void
NEM_mount_free(NEM_mount_t *this)
{
	if (this->owned) {
		// XXX: unmount.
		NEM_panic("TODO: unmount");
	}

	// XXX: Generalize this a bit?
	if (NULL != this->disk) {
		NEM_disk_free(this->disk);
		this->disk = NULL;
	}

	free(this->source);
	free(this->dest);
}

// XXX: Might wrap this in a struct or something?
typedef LIST_HEAD(NEM_mountlist_t, NEM_mount_t) NEM_mountlist_t;

static NEM_mount_t*
NEM_mountlist_find(NEM_mountlist_t *this, const NEM_mount_t *dummy)
{
	NEM_mount_t *entry;
	LIST_FOREACH(entry, this, link) {
		if (0 == NEM_mount_cmp(entry, dummy)) {
			return entry;
		}
	}
	return NULL;
}

static void
NEM_mountlist_free(NEM_mountlist_t *this)
{
	NEM_mount_t *entry, *tmp;

	// NB: New entries are prependede to the list, so forward iteration
	// is newest -> oldest mounts.
	LIST_FOREACH_SAFE(entry, this, link, tmp) {
		NEM_mount_free(entry);
		LIST_REMOVE(entry, link);
		free(entry);
	}
	
	bzero(this, sizeof(*this));
}

static NEM_mountlist_t static_mounts = {0};

static NEM_err_t
NEM_mountlist_rescan(NEM_mountlist_t *this)
{
	NEM_mount_t *entry = NULL, *tmp = NULL;
	LIST_FOREACH(entry, this, link) {
		entry->seen = false;
	}

	int num_mounts = getfsstat(NULL, 0, MNT_WAIT);
	if (-1 == num_mounts) {
		return NEM_err_errno();
	}

	size_t len = (size_t) num_mounts * sizeof(struct statfs);
	struct statfs *mnts = NEM_malloc(len);
	num_mounts = getfsstat(mnts, len, MNT_WAIT);
	if (-1 == num_mounts) {
		free(mnts);
		return NEM_err_errno();
	}

	for (size_t i = 0; i < num_mounts; i += 1) {
		NEM_mount_type_t ty = NEM_mount_type_from_str(mnts[i].f_fstypename);

		NEM_mount_t dummy = {
			.type   = ty,
			.source = mnts[i].f_mntfromname,
			.dest   = mnts[i].f_mntonname,
		};
		NEM_mount_t *entry = NEM_mountlist_find(this, &dummy);
		if (NULL == entry) {
			entry = NEM_malloc(sizeof(NEM_mount_t));
			entry->type = ty;
			entry->source = strdup(mnts[i].f_mntfromname);
			entry->dest = strdup(mnts[i].f_mntonname);
			entry->owned = false;
			NEM_disk_init_device(&entry->disk, entry->source);
			LIST_INSERT_HEAD(this, entry, link);
		}

		entry->seen = true;
	}
	free(mnts);

	LIST_FOREACH_SAFE(entry, this, link, tmp) {
		if (!entry->seen) {
			NEM_mount_free(entry);
			LIST_REMOVE(entry, link);
			free(entry);
		}
	}

	return NEM_err_none;
}

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	if (!LIST_EMPTY(&static_mounts)) {
		NEM_panicf("mounts: corrupt static_mounts during setup");
	}

	NEM_logf(COMP_MOUNTS, "setup");

	NEM_err_t err = NEM_mountlist_rescan(&static_mounts);
	if (!NEM_err_ok(err)) {
		NEM_mountlist_free(&static_mounts);
		return err;
	}

	NEM_mount_t *mount;
	LIST_FOREACH(mount, &static_mounts, link) {
		NEM_logf(
			COMP_MOUNTS,
			"  - %8.8s:%s %s -> %s",
			NEM_mount_type_str(mount->type),
			mount->owned ? "" : " (foreign)",
			NEM_disk_dbg_string(mount->disk), //mount->source,
			mount->dest
		);
	}

	return NEM_err_none;
}

static void
teardown(NEM_app_t *app)
{
	NEM_logf(COMP_MOUNTS, "teardown");
	NEM_mountlist_free(&static_mounts);
}

const NEM_app_comp_t NEM_rootd_c_mounts = {
	.name     = "mounts",
	.setup    = &setup,
	.teardown = &teardown,
};
