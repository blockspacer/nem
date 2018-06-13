#include <sys/types.h>
#include <libgeom.h>
#include <limits.h>

#include "nem.h"
#include "c-md.h"
#include "c-log.h"

struct NEM_md_t {
	int   unit;
	int   refcount;
	char *source; // NB: May be NULL (mem).
	char *dest;   // NB: May be NULL (unmounted).
	bool  owned;
	bool  writable;
	bool  mounted;
	bool  seen; // NB: used for mark/sweep purposes.
};

typedef struct {
	// NB: ordering isn't important here; these are individually
	// refcounted and can't actually be released until they are no more
	// consumers referencing them.
	NEM_md_t *mds;
	size_t    mds_len;
	size_t    mds_cap;
}
NEM_mdlist_t;

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

static void
NEM_md_free(NEM_md_t *this)
{
	free(this->source);
	free(this->dest);
}

static NEM_md_t*
NEM_mdlist_by_unit(NEM_mdlist_t *this, int unit)
{
	for (size_t i = 0; i < this->mds_len; i += 1) {
		if (this->mds[i].unit == unit) {
			return &this->mds[i];
		}
	}
	return NULL;
}

static NEM_md_t*
NEM_mdlist_by_dest(NEM_mdlist_t *this, const char *dest)
{
	// XXX: realpath(dest)? from geom it's always correct and immutable.
	for (size_t i = 0; i < this->mds_len; i += 1) {
		if (NULL != this->mds[i].dest && !strcmp(this->mds[i].dest, dest)) {
			return &this->mds[i];
		}
	}
	return NULL;
}

static NEM_md_t*
NEM_mdlist_add_unit(NEM_mdlist_t *this, int unit)
{
	if (NULL != NEM_mdlist_by_unit(this, unit)) {
		NEM_panicf("NEM_mdlist_add_unit: refusing to duplicate a unit");
	}

	if (this->mds_len == this->mds_cap) {
		this->mds_cap *= 2;
		this->mds = NEM_panic_if_null(realloc(
			this->mds,
			sizeof(NEM_mdlist_t) * this->mds_cap
		));
	}

	NEM_md_t *md = &this->mds[this->mds_len];
	bzero(md, sizeof(*md));
	this->mds_len += 1;
	md->unit = unit;
	return md;
}

static void
NEM_mdlist_remove_md(NEM_mdlist_t *this, NEM_md_t *md)
{
	if (
		0 == this->mds_len
		|| md < this->mds
		|| md > this->mds + this->mds_len
	) {
		NEM_panicf("NEM_mdlist_remove_md: removal oob");
	}

	off_t diff = md - this->mds;
	size_t idx = diff / sizeof(*md);
	this->mds_len -= 1;
	if (idx < this->mds_len) {
		*md = this->mds[this->mds_len];
		bzero(&this->mds[this->mds_len], sizeof(*md));
	}
}

static void
NEM_mdlist_clear(NEM_mdlist_t *this)
{
	for (size_t i = 0; i < this->mds_len; i += 1) {
		NEM_md_free(&this->mds[i]);
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

	NEM_md_t *md = NEM_mdlist_by_unit(this, unit);
	if (NULL != md) {
		if (
			(NULL == file && strcmp(file, md->source))
			|| (NULL != file && NULL != md->source)
		) {
			NEM_logf(
				COMP_MOUNTS,
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
	return NEM_err_none;
}

static NEM_err_t
NEM_mdlist_rescan(NEM_mdlist_t *this)
{
	for (size_t i = 0; i < this->mds_len; i +=1 ) {
		this->mds[i].seen = false;
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
		NEM_md_t *md = &this->mds[i];
		if (!md->seen) {
			NEM_mdlist_remove_md(this, md);
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
setup(NEM_app_t *app, int argc, char *argv[])
{
	if (0 != static_mdlist.mds_len) {
		NEM_panicf("mounts: corrupt static_mdlist during setup");
	}

	NEM_logf(COMP_MD, "setup");
	NEM_err_t err = NEM_mdlist_rescan(&static_mdlist);
	if (!NEM_err_ok(err)) {
		NEM_mdlist_clear(&static_mdlist);
	}

	for (size_t i = 0; i < static_mdlist.mds_len; i += 1) {
		NEM_md_t *md = &static_mdlist.mds[i];
		NEM_logf(
			COMP_MD,
			"  md%d: (%s, %s) %s -> %s",
			md->unit,
			md->writable ? "rw" : "ro",
			md->owned ? "ours" : "foreign",
			md->source != NULL ? md->source : "(mem)",
			md->mounted
				? (md->dest != NULL ? md->dest : "(unknown)")
				: "unmounted"
		);
	}

	return err;
}

static void
teardown(NEM_app_t *app)
{
	NEM_logf(COMP_MD, "teardown");
}

const NEM_app_comp_t NEM_rootd_c_md = {
	.name     = "md",
	.setup    = &setup,
	.teardown = &teardown,
};
