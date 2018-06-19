#include <sys/types.h>
#include <sys/mdioctl.h>
#include <sys/ioctl.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <libgeom.h>
#include <limits.h>

#include "nem.h"

/*
 * helper functions for reading values out of libgeom configs.
 */

static const char*
geom_config_str(const struct gconf *cfg, const char *key)
{
	struct gconfig *entry = NULL;
	LIST_FOREACH(entry, cfg, lg_config) {
		if (!strcmp(key, entry->lg_name)) {
			return entry->lg_val;
		}
	}
	return NULL;
}

static long
geom_config_long(const struct gconf *cfg, const char *key)
{
	char *end = NULL;
	long val = strtol(
		NEM_panic_if_null((void*)geom_config_str(cfg, key)),
		&end,
		10
	);
	if (end == NULL || end[0] != '\0') {
		NEM_panicf("geom_config_int: bad value for key %s", key);
	}
	return val;
}

static int
geom_config_int(const struct gconf *cfg, const char *key)
{
	long val = geom_config_long(cfg, key);
	if (val > INT_MAX || val < INT_MIN) {
		NEM_panicf("geom_config_int: value exceeds int bounds");
	}
	return (int)val;
}

/*
 * NEM_disk_base_t is the base class for all disk types. It provides basic
 * accessors that are common to all disks (aka geom providers). This is a bit
 * intertwined with NEM_disk_set_t, which is a set of NEM_disk_base_t's.
 */

typedef struct NEM_disk_set_t NEM_disk_set_t;
typedef struct NEM_disk_base_t NEM_disk_base_t;

// NEM_disk_base_vt defines some common internal methods.
typedef struct {
	// geom_class should be a singular string that indicates which geom class
	// this type maps to.
	const char *geom_class;

	// new_geom should allocate, initialize, and return a new instance of the
	// disk from the given geom.
	NEM_err_t (*new_geom)(
		NEM_disk_set_t*,
		const struct ggeom*,
		NEM_disk_base_t**
	);

	// free should release any resources used by this disk, including any
	// kernel-side resources if this disk is owned by us.
	NEM_err_t (*free)(NEM_disk_base_t*);
}
NEM_disk_base_vt;

struct NEM_disk_base_t {
	LIST_ENTRY(NEM_disk_base_t) entry;
	const NEM_disk_base_vt     *vt;

	NEM_disk_set_t *set;
	char           *name;
	int             refcount;
	bool            seen;
	bool            foreign;
};

// NEM_disk_set_t provides actual ownership of the disks. The disks list is
// ordered from newest -> oldest.
struct NEM_disk_set_t {
	LIST_HEAD(, NEM_disk_base_t) disks;
};

static int
NEM_disk_base_cmp_name(const void *vlhs, const void *vrhs)
{
	const NEM_disk_base_t *lhs = vlhs;
	const NEM_disk_base_t *rhs = vrhs;
	return strcmp(lhs->name, rhs->name);
}

static void
NEM_disk_base_init(
	NEM_disk_base_t *this,
	NEM_disk_set_t  *set,
	const char      *name
) {
	bzero(this, sizeof(*this));
	this->set = set;
	this->name = strdup(name);
	this->refcount = 1;

	LIST_INSERT_HEAD(&set->disks, this, entry);
}

static void
NEM_disk_base_init_geom(
	NEM_disk_base_t    *this,
	NEM_disk_set_t     *set,
	const struct ggeom *geom
) {
	NEM_disk_base_init(this, set, geom->lg_name);
	this->foreign = true;
}

static void
NEM_disk_base_free(NEM_disk_base_t *this)
{
	free(this->name);
	LIST_REMOVE(this, entry);
}

/*
 * NEM_disk_md_t is an md-backed disk.
 */

typedef struct {
	NEM_disk_base_t base;
	char           *file;
	char           *access;
	enum md_types   type;
	size_t          length;
	int             unit;
}
NEM_disk_md_t;

static const NEM_disk_base_vt NEM_disk_md_vt;

static const struct {
	enum md_types type;
	const char  *str;
}
md_type_strs[] = {
	{ MD_MALLOC,  "malloc"  },
	{ MD_PRELOAD, "preload" },
	{ MD_VNODE,   "vnode"   },
	{ MD_SWAP,    "swap"    },
	{ MD_NULL,    "null"    },
};

static enum md_types
md_type_from_str(const char *str)
{
	for (size_t i = 0; i < NEM_ARRSIZE(md_type_strs); i += 1) {
		if (!strcmp(md_type_strs[i].str, str)) {
			return md_type_strs[i].type;
		}
	}
	return MD_NULL;
}

static const char*
md_type_to_str(enum md_types type)
{
	for (size_t i = 0; i < NEM_ARRSIZE(md_type_strs); i += 1) {
		if (md_type_strs[i].type == type) {
			return md_type_strs[i].str;
		}
	}
	return "null";
}

static NEM_err_t
NEM_disk_md_ctl(int cmd, struct md_ioctl *params)
{
	int ctlfd = open("/dev/" MDCTL_NAME, O_RDWR|O_CLOEXEC);
	if (0 > ctlfd) {
		return NEM_err_errno();
	}

	NEM_err_t err = NEM_err_none;
	if (-1 == ioctl(ctlfd, cmd, &params)) {
		err = NEM_err_errno();
	}

	close(ctlfd);
	return err;
}

static NEM_err_t
NEM_disk_md_init_geom(
	NEM_disk_md_t      *this,
	NEM_disk_set_t     *set,
	const struct ggeom *geom
) {
	const struct gprovider *prov = LIST_FIRST(&geom->lg_provider);
	// XXX: assert that the list has exactly one entry?
	
	NEM_disk_base_init_geom(&this->base, set, geom);

	const char *file = geom_config_str(&prov->lg_config, "file");
	this->file = (NULL != file) ? strdup(file) : NULL;
	this->access = strdup(geom_config_str(&prov->lg_config, "access"));
	this->type = md_type_from_str(geom_config_str(&prov->lg_config, "type"));
	this->length = geom_config_long(&prov->lg_config, "length");
	this->unit = geom_config_int(&prov->lg_config, "unit");

	return NEM_err_none;
}

static NEM_err_t
NEM_disk_md_new_geom(
	NEM_disk_set_t     *set,
	const struct ggeom *geom,
	NEM_disk_base_t   **out
) {
	NEM_disk_md_t *this = NEM_malloc(sizeof(*this));
	NEM_err_t err = NEM_disk_md_init_geom(this, set, geom);
	if (NEM_err_ok(err)) {
		*out = &this->base;
	}
	else {
		free(this);
	}
	return err;
}

static NEM_err_t
NEM_disk_md_init_mem(NEM_disk_md_t *this, NEM_disk_set_t *set, size_t len)
{
	struct md_ioctl params = {
		.md_version   = MDIOVERSION,
		.md_type      = MD_MALLOC,
		.md_mediasize = len,
		.md_options   = MD_CLUSTER | MD_AUTOUNIT,
	};

	NEM_err_t err = NEM_disk_md_ctl(MDIOCATTACH, &params);
	if (!NEM_err_ok(err)) {
		return err;
	}

	this->file = NULL;
	this->access = strdup("read-write");
	this->type = MD_MALLOC;
	this->length = len;
	this->unit = params.md_unit;
	return NEM_err_none;
}

static NEM_err_t
NEM_disk_md_init_file(
	NEM_disk_md_t  *this,
	NEM_disk_set_t *set,
	const char     *file,
	bool            ro
) {
	struct stat sb;
	if (0 != stat(file, &sb)) {
		return NEM_err_errno();
	}
	if (!S_ISREG(sb.st_mode)) {
		return NEM_err_static("NEM_disk_md_new_file: not a file");
	}

	struct md_ioctl params = {
		.md_version   = MDIOVERSION,
		.md_type      = MD_VNODE,
		.md_mediasize = sb.st_size,
		.md_options   = MD_CLUSTER | MD_AUTOUNIT,
	};
	if (ro) {
		params.md_options |= MD_READONLY;
	}

	NEM_err_t err = NEM_disk_md_ctl(MDIOCATTACH, &params);
	if (!NEM_err_ok(err)) {
		return err;
	}

	this->file = strdup(file);
	this->access = strdup(ro ? "read-only" : "read-write");
	this->type = MD_VNODE;
	this->length = sb.st_size;
	this->unit = params.md_unit;
	return NEM_err_none;
}

static NEM_err_t
NEM_disk_md_destroy(NEM_disk_md_t *this)
{
	struct md_ioctl params = {
		.md_version = MDIOVERSION,
		.md_unit    = this->unit,
	};

	return NEM_disk_md_ctl(MDIOCDETACH, &params);
}

static NEM_err_t
NEM_disk_md_free(NEM_disk_base_t *vthis)
{
	NEM_disk_md_t *this = (NEM_disk_md_t*) vthis;
	NEM_err_t err = NEM_err_none;
	if (!this->base.foreign) {
		err = NEM_disk_md_destroy(this);
	}

	free(this->file);
	free(this->access);
	NEM_disk_base_free(&this->base);
	free(this);
	return err;
}

static const NEM_disk_base_vt NEM_disk_md_vt = {
	.geom_class = "MD",
	.new_geom   = &NEM_disk_md_new_geom,
	.free       = &NEM_disk_md_free,
};

/*
 * NEM_disk_ad_t is a disk-backed geom provider.
 */

typedef struct {
	NEM_disk_base_t base;
	const char     *descr;
	const char     *lunid;
	const char     *ident;
	int             rotation_rate;
	int             fw_sectors;
	int             fw_heads;
}
NEM_disk_ad_t;

/*
 * NEM_disk_part_t is a part-backed geom provider.
 */

typedef struct {
	NEM_disk_base_t base;
	const char     *type;
	int             raw_type;
	size_t          length;
	size_t          offset;
	size_t          start;
	size_t          end;
}
NEM_disk_part_t;

/*
 * NEM_disk_eli_t is an eli-backed geom provider.
 */

typedef struct {
	NEM_disk_base_t base;
}
NEM_disk_eli_t;

/*
 * NEM_disk_set_t methods.
 */

void
NEM_disk_set_init(NEM_disk_set_t *this)
{
	LIST_INIT(&this->disks);
}

static const NEM_disk_base_vt *disk_vts[] = {
	&NEM_disk_md_vt,
};

NEM_err_t
NEM_disk_set_rescan(NEM_disk_set_t *this)
{
	// Mark everything unseen.
	NEM_disk_base_t *entry = NULL, *tmp = NULL;
	LIST_FOREACH(entry, &this->disks, entry) {
		entry->seen = false;
	}

	struct gmesh tree;
	if (0 != geom_gettree(&tree)) {
		return NEM_err_errno();
	}

	struct gclass *cls = NULL;
	struct ggeom *geom = NULL;

	LIST_FOREACH(cls, &tree.lg_class, lg_class) {
		const NEM_disk_base_vt *vt = NULL;
		for (size_t i = 0; i < NEM_ARRSIZE(disk_vts); i += 1) {
			if (0 == strcmp(disk_vts[i]->geom_class, cls->lg_name)) {
				vt = disk_vts[i];
				break;
			}
		}
		if (NULL == vt) {
			continue;
		}

		LIST_FOREACH(geom, &cls->lg_geom, lg_geom) {
			NEM_disk_base_t *disk = NULL;
			// XXX: THIS IS WRONG! Need to find the existing entry if
			// we've got it.
			NEM_err_t err = vt->new_geom(this, geom, &disk);
			if (!NEM_err_ok(err)) {
				// XXX: Cleanup?
				return err;
			}

			disk->seen = true;
		}
	}

	// Then free anything that's no longer here.
	LIST_FOREACH_SAFE(entry, &this->disks, entry, tmp) {
	}

	return NEM_err_none;
}

void
NEM_disk_set_free(NEM_disk_set_t *this)
{
}

int
main(int argc, char *argv[])
{
	printf("hello\n");
}
