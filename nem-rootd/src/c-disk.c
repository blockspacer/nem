#include <sys/types.h>
#include <sys/mdioctl.h>
#include <sys/ioctl.h>
#include <libgeom.h>
#include <limits.h>

#include "nem.h"
#include "c-disk.h"
#include "c-log.h"
#include "c-config.h"
#include "utils.h"

typedef struct NEM_mdlist_t NEM_mdlist_t;;

typedef struct {
	NEM_mdlist_t *list;

	int    unit;
	int    refcount;
	size_t len;
	char  *device;
	char  *source; // NB: May be NULL (mem).
	bool   owned;
	bool   writable;
	bool   mounted;
	bool   seen; // NB: used for mark/sweep purposes.
}
NEM_disk_md_t;

struct NEM_mdlist_t {
	// NB: ordering isn't important here; these are individually
	// refcounted and can't actually be released until they are no more
	// consumers referencing them.
	NEM_disk_md_t **mds;
	size_t          mds_len;
	size_t          mds_cap;
};

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

static NEM_disk_md_t*
NEM_md_alloc(int unit)
{
	NEM_disk_md_t *this = NEM_malloc(sizeof(NEM_disk_md_t));
	this->unit = unit;
	asprintf(&this->device, "md%d", unit);
	return this;
}

static void
NEM_md_free_internal(NEM_disk_md_t *this)
{
	if (this->owned) {
		int ctlfd = open("/dev/" MDCTL_NAME, O_RDWR|O_CLOEXEC);
		if (0 > ctlfd) {
			NEM_panicf_errno("NEM_md_free_internal: unable to open mdctl");
		}

		struct md_ioctl params = {
			.md_unit = this->unit,
		};

		int ioret = ioctl(ctlfd, MDIOCDETACH, &params);
		close(ctlfd);

		if (-1 == ioret) {
			NEM_panicf_errno("NEM_md_free_internal: unable to detach");
		}
	}

	free(this->device);
	free(this->source);
	free(this);
}

static void NEM_mdlist_remove_md(NEM_mdlist_t *list, NEM_disk_md_t *md);

/*
 * NEM_disk_t implementation for NEM_disk_md_t
 */

NEM_disk_type_t
NEM_disk_md_type(void *vthis)
{
	return NEM_DISK_VIRTUAL;
}

const char*
NEM_disk_md_device(void *vthis)
{
	NEM_disk_md_t *this = vthis;
	return this->device;
}

void
NEM_disk_md_free(void *vthis)
{
	NEM_disk_md_t *this = vthis;

	if (0 == this->refcount) {
		NEM_panic("NEM_md_free: corrupt refcount");
	}

	this->refcount -= 1;
	if (0 == this->refcount) {
		NEM_mdlist_remove_md(this->list, this);
		NEM_md_free_internal(this);
	}
}

bool
NEM_disk_md_mounted(void *vthis)
{
	NEM_disk_md_t *this = vthis;
	return this->mounted;
}

bool
NEM_disk_md_readonly(void *vthis)
{
	NEM_disk_md_t *this = vthis;
	return !this->writable;
}

const NEM_disk_vt NEM_disk_md_vt = {
	.type     = &NEM_disk_md_type,
	.device   = &NEM_disk_md_device,
	.free     = &NEM_disk_md_free,
	.mounted  = &NEM_disk_md_mounted,
	.readonly = &NEM_disk_md_readonly,
};

/*
 * NEM_mdlist_t methods -- XXX: Just bother with NEM_disk_t's.
 */

static NEM_disk_md_t*
NEM_mdlist_by_unit(NEM_mdlist_t *this, int unit)
{
	for (size_t i = 0; i < this->mds_len; i += 1) {
		if (this->mds[i]->unit == unit) {
			return this->mds[i];
		}
	}
	return NULL;
}

static NEM_disk_md_t*
NEM_mdlist_add_unit(NEM_mdlist_t *this, int unit)
{
	if (NULL != NEM_mdlist_by_unit(this, unit)) {
		NEM_panicf("NEM_mdlist_add_unit: refusing to duplicate a unit");
	}

	if (this->mds_len == this->mds_cap) {
		this->mds_cap = (this->mds_cap == 0) ? 1 : this->mds_cap * 2;
		this->mds = NEM_panic_if_null(realloc(
			this->mds,
			sizeof(NEM_disk_md_t*) * this->mds_cap
		));
	}

	NEM_disk_md_t *md = NEM_md_alloc(unit);
	md->list = this;
	this->mds[this->mds_len] = md;
	this->mds_len += 1;
	return md;
}

static void
NEM_mdlist_remove_md(NEM_mdlist_t *this, NEM_disk_md_t *md)
{
	size_t idx;
	for (size_t i = 0; i < this->mds_len; i += 1) {
		if (md == this->mds[i]) {
			idx = i;
			goto remove;
		}
	}
	NEM_panicf("NEM_mdlist_remove_md: removal oob");
remove:
	this->mds_len -= 1;
	if (idx < this->mds_len) {
		this->mds[idx] = this->mds[this->mds_len];
	}
}

static void
NEM_mdlist_clear(NEM_mdlist_t *this)
{
	for (size_t i = 0; i < this->mds_len; i += 1) {
		NEM_md_free_internal(this->mds[i]);
	}

	free(this->mds);
	bzero(this, sizeof(*this));
}

static NEM_err_t
NEM_mdlist_add_provider(NEM_mdlist_t *this, const struct gprovider *prov)
{
	const char *type = geom_config_str(&prov->lg_config, "type");
	const char *access = geom_config_str(&prov->lg_config, "access");
	const char *file = geom_config_str(&prov->lg_config, "file");
	int unit = geom_config_int(&prov->lg_config, "unit");
	long len = geom_config_long(&prov->lg_config, "length");
	//size_t len = geom_config_long(&prov->lg_config, "length");
	bool rw = !strcmp(access, "read-write");
	bool mounted = false;

	// NB: Ignore undocumented md types (e.g. "preload");
	if (0 != strcmp(type, "vnode") && 0 != strcmp(type, "malloc")) {
		return NEM_err_none;
	}

	// NB: Explicitly not going to support images with partitions (keep it
	// simple), so we should either have a LABEL or VFS consumer.
	struct gconsumer *cons = NULL;
	int num_vfs = 0;
	int num_part = 0;
	LIST_FOREACH(cons, &prov->lg_consumers, lg_consumers) {
		if (NULL == cons->lg_geom || NULL == cons->lg_geom->lg_class) {
			// ????
			continue;
		}
		else if (0 == strcmp("VFS", cons->lg_geom->lg_class->lg_name)) {
			num_vfs += 1;
		}
		else if (0 == strcmp("PART", cons->lg_geom->lg_class->lg_name)) {
			num_part += 1;
		}
	}
	if (0 != num_part) {
		return NEM_err_none;
	}
	if (0 != num_vfs) {
		// NB: mounted!
		mounted = true;
	}

	NEM_disk_md_t *md = NEM_mdlist_by_unit(this, unit);
	if (NULL != md) {
		if (
			(NULL == file && strcmp(file, md->source))
			|| (NULL != file && NULL != md->source)
		) {
			NEM_logf(
				COMP_DISK,
				"underlying md changed wtf?\n'%s' -> '%s'",
				md->source,
				file
			);
			NEM_mdlist_remove_md(this, md);
			md = NULL;
		}
	}
	if (NULL == md) {
		// NB: Create and set the immutable fields.
		md = NEM_mdlist_add_unit(this, unit);
		md->owned = false;
		md->source = (NULL == file) ? NULL : strdup(file);
	}

	// Then update the fields that _are_ mutable.
	md->writable = rw;
	md->mounted = mounted;
	md->seen = true;
	md->len = len;
	return NEM_err_none;
}

static NEM_err_t
NEM_mdlist_rescan(NEM_mdlist_t *this)
{
	for (size_t i = 0; i < this->mds_len; i +=1 ) {
		this->mds[i]->seen = false;
	}

	struct gmesh tree;
	if (0 != geom_gettree(&tree)) {
		return NEM_err_errno();
	}

	struct gclass *cls = NULL;
	struct ggeom *geom = NULL;
	struct gprovider *prov = NULL;
	LIST_FOREACH(cls, &tree.lg_class, lg_class) {
		if (0 != strcmp("MD", cls->lg_name)) {
			continue;
		}

		LIST_FOREACH(geom, &cls->lg_geom, lg_geom) {
			if (1 != geom->lg_rank) {
				continue;
			}

			// md devices! should really only be one per geom.
			LIST_FOREACH(prov, &geom->lg_provider, lg_provider) {
				NEM_err_t err = NEM_mdlist_add_provider(this, prov);
				if (!NEM_err_ok(err)) {
					return err;
				}
			}
		}
	}

	for (size_t i = 0; i < this->mds_len;) {
		NEM_disk_md_t *md = this->mds[i];
		if (!md->seen) {
			NEM_mdlist_remove_md(this, md);
			NEM_md_free_internal(md);
		}
		else {
			i += 1;
		}
	}

	geom_deletetree(&tree);
	return NEM_err_none;
}

static NEM_mdlist_t static_mdlist = {0};

static NEM_err_t
NEM_disk_md_new(NEM_disk_md_t **pout, struct md_ioctl *params)
{
	NEM_mdlist_t *this = &static_mdlist;

	int ctlfd = open("/dev/" MDCTL_NAME, O_RDWR|O_CLOEXEC);
	if (0 > ctlfd) {
		return NEM_err_errno();
	}

	int ioret = ioctl(ctlfd, MDIOCATTACH, params);
	close(ctlfd);
	if (-1 == ioret) {
		return NEM_err_errno();
	}

	NEM_disk_md_t *out = NEM_mdlist_add_unit(this, params->md_unit);
	out->refcount = 1;
	out->len = params->md_mediasize;
	out->owned = true;
	*pout = out;
	return NEM_err_none;
}

NEM_err_t
NEM_disk_init(NEM_disk_t *out, const char *device)
{
	NEM_panic("TODO");
}

NEM_err_t
NEM_disk_init_mem(NEM_disk_t *out, size_t len)
{
	NEM_mdlist_t *this = &static_mdlist;
	NEM_disk_md_t *md = NULL;

	for (size_t i = 0; i < this->mds_len; i += 1) {
		md = this->mds[i];
		if (
			0 != md->refcount
			|| md->mounted
			|| NULL != md->source
			|| !md->writable
		) {
			continue;
		}

		if (md->len == len) {
			md->refcount += 1;
			out->vt = &NEM_disk_md_vt;
			out->this = md;
			return NEM_err_none;
		}
	}

	if (!NEM_rootd_is_root()) {
		return NEM_err_static("NEM_disk_md_new_mem: need root");
	}

	struct md_ioctl params = {
		.md_version   = MDIOVERSION,
		.md_type      = MD_MALLOC,
		.md_mediasize = len,
		.md_options   = MD_CLUSTER | MD_AUTOUNIT,
	};

	NEM_err_t err = NEM_disk_md_new(&md, &params);
	if (NEM_err_ok(err)) {
		md->writable = true;
		out->vt = &NEM_disk_md_vt;
		out->this = md;
	}

	return err;
}

NEM_err_t
NEM_disk_init_file(NEM_disk_t *out, const char *path, bool ro)
{
	NEM_mdlist_t *this = &static_mdlist;
	NEM_disk_md_t *md = NULL;

	char *tmp_path = strdup(path);
	NEM_err_t err = NEM_path_abs(&tmp_path);
	if (!NEM_err_ok(err)) {
		free(tmp_path);
		return err;
	}

	for (size_t i = 0; i < this->mds_len; i += 1) {
		md = this->mds[i];
		if (
			0 != md->refcount
			|| md->mounted
			|| NULL == md->source
			|| ro == md->writable
		) {
			continue;
		}

		if (0 == strcmp(md->source, path)) {
			md->refcount += 1;
			out->vt = &NEM_disk_md_vt;
			out->this = md;
			return NEM_err_none;
		}
	}

	if (!NEM_rootd_is_root()) {
		return NEM_err_static("NEM_disk_md_new_file: need root");
	}

	struct stat sb;
	if (0 != stat(path, &sb)) {
		return NEM_err_errno();
	}
	if (!S_ISREG(sb.st_mode)) {
		return NEM_err_static("NEM_disk_md_new_file: not a file");
	}

	struct md_ioctl params = {
		.md_version   = MDIOVERSION,
		.md_type      = MD_VNODE,
		.md_options   = MD_CLUSTER | MD_AUTOUNIT,
		.md_mediasize = sb.st_size,
		.md_file      = (char*) path,
	};
	if (ro) {
		params.md_options |= MD_READONLY;
	}

	err = NEM_disk_md_new(&md, &params);
	if (NEM_err_ok(err)) {
		md->writable = !ro;
		md->source = strdup(path);
		out->vt = &NEM_disk_md_vt;
		out->this = md;
	}
	return err;
}

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	if (0 != static_mdlist.mds_len) {
		NEM_panicf("mounts: corrupt static_mdlist during setup");
	}

	NEM_logf(COMP_DISK, "setup");
	NEM_err_t err = NEM_mdlist_rescan(&static_mdlist);
	if (!NEM_err_ok(err)) {
		NEM_mdlist_clear(&static_mdlist);
	}

	for (size_t i = 0; i < static_mdlist.mds_len; i += 1) {
		NEM_disk_md_t *md = static_mdlist.mds[i];
		NEM_logf(
			COMP_DISK,
			"  %s: (%s, %s%s) %.40s...",
			md->device,
			md->writable ? "rw" : "ro",
			md->owned ? "ours" : "foreign",
			md->mounted ? ", mounted" : "",
			md->source != NULL ? md->source : "(mem)"
		);
	}

	return err;
}

static void
teardown(NEM_app_t *app)
{
	NEM_logf(COMP_DISK, "teardown");
}

const NEM_app_comp_t NEM_rootd_c_disk_md = {
	.name     = "disk-md",
	.setup    = &setup,
	.teardown = &teardown,
};
