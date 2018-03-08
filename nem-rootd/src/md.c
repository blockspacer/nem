#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mdioctl.h>
#include <sys/param.h>
#include <sys/tree.h>
#include <libgeom.h>
#include <devstat.h>
#include <stdlib.h>
#include <strings.h>
#include <fcntl.h>

#include "md.h"

typedef struct NEM_mdentry_t {
	RB_ENTRY(NEM_mdentry_t) link;

	NEM_md_t md;
	int      refcount;
	bool     ro;
	bool     external;
}
NEM_mdentry_t;

typedef RB_HEAD(NEM_mdtree_t, NEM_mdentry_t) NEM_mdtree_t;

static inline int
NEM_mdentry_cmp(const void *vlhs, const void *vrhs)
{
	const NEM_mdentry_t *lhs = vlhs;
	const NEM_mdentry_t *rhs = vrhs;

	return strcmp(lhs->md.file, rhs->md.file);
}

RB_PROTOTYPE(NEM_mdtree_t, NEM_mdentry_t, link, NEM_mdentry_cmp);
RB_GENERATE(NEM_mdtree_t, NEM_mdentry_t, link, NEM_mdentry_cmp);

typedef enum {
	NEM_MDMGR_STATE_UNINIT,
	NEM_MDMGR_STATE_INIT,
	NEM_MDMGR_STATE_FREED,
}
NEM_mdmgr_state_t;

static NEM_mdtree_t NEM_mdmgr_tree = RB_INITIALIZER(&NEM_mdmgr_tree);
static NEM_mdmgr_state_t NEM_mdmgr_state = NEM_MDMGR_STATE_UNINIT;

static NEM_mdentry_t*
NEM_mdmgr_find(const char *file)
{
	NEM_mdentry_t dummy = {
		.md = {
			.file = file,
		},
	};

	return RB_FIND(NEM_mdtree_t, &NEM_mdmgr_tree, &dummy);
}

static const char*
NEM_geom_cfg(struct gconf *cfg, const char *name, const char *def)
{
	struct gconfig *entry;

	LIST_FOREACH(entry, cfg, lg_config) {
		if (!strcmp(entry->lg_name, name)) {
			return entry->lg_val;
		}
	}

	return def;
}

NEM_err_t
NEM_mdmgr_init()
{
	if (NEM_MDMGR_STATE_UNINIT != NEM_mdmgr_state) {
		return NEM_err_static("NEM_mdmgr_init: init/free already called");
	}

	NEM_err_t err = NEM_err_none;

	struct gmesh tree;
	void *snap = NULL;

	if (0 != geom_gettree(&tree)) {
		err = NEM_err_errno();
		goto done;
	}
	if (0 != geom_stats_open()) {
		err = NEM_err_errno();
		goto done;
	}

	snap = geom_stats_snapshot_get();
	if (NULL == snap) {
		err = NEM_err_errno();
		goto done;
	}

	struct devstat *dstat;
	while (NULL != (dstat = geom_stats_snapshot_next(snap))) {
		struct gident *id = geom_lookupid(&tree, dstat->id);
		if (NULL == id || ISPROVIDER != id->lg_what) {
			continue;
		}

		struct gprovider *prov = id->lg_ptr;
		struct ggeom *geom = prov->lg_geom;
		struct gclass *cls = geom->lg_class;

		// Not an md device.
		if (strcmp("MD", cls->lg_name)) {
			continue;
		}

		// Only care about vnodes.
		if (strcmp("vnode", NEM_geom_cfg(&prov->lg_config, "type", ""))) {
			continue;
		}

		// Then pull out the fields. Really being overly pedantic here, but
		// the geom data is just stored in XML blobs so ... yeah. If we can't
		// successfully parse there's something horribly wrong.
		const char *file = NEM_geom_cfg(&prov->lg_config, "file", NULL);
		if (NULL == file) {
			err = NEM_err_static("NEM_mdmgr_init: md vnode has NULL file");
			goto done;
		}

		const char *access = NEM_geom_cfg(&prov->lg_config, "access", NULL);
		if (NULL == file) {
			err = NEM_err_static("NEM_mdmgr_init: md vnode has NULL file");
			goto done;
		}
		bool read_only = !strcmp("read-write", access);

		const char *unit_str = NEM_geom_cfg(&prov->lg_config, "unit", NULL);
		if (NULL == file) {
			err = NEM_err_static("NEM_mdmgr_init: md vnode has NULL file");
			goto done;
		}
		char *endptr = NULL;
		long unit = strtol(unit_str, &endptr, 10);
		if (unit_str[0] == 0 || endptr[0] != '0') {
			err = NEM_err_static("NEM_mdmgr_init: md unit not an int");
			goto done;
		}

		// Double-check that this path doesn't exist. If there are multiple
		// mds configured with the same path it breaks our assumptions.
		if (NULL != NEM_mdmgr_find(file)) {
			err = NEM_err_static("NEM_mdmgr_init: dupe md for a given file");
			goto done;
		}

		// Then insert it into the treeeee.
		NEM_mdentry_t *entry = NEM_malloc(sizeof(NEM_mdentry_t));
		entry->refcount = 0;
		entry->ro = read_only;
		entry->external = true;
		entry->md.file = strdup(file);
		entry->md.unit = (int) unit;
		RB_INSERT(NEM_mdtree_t, &NEM_mdmgr_tree, entry);
	}

done:
	if (NULL != snap) {
		geom_stats_snapshot_free(snap);
	}

	geom_deletetree(&tree);

	if (!NEM_err_ok(err)) {
		NEM_mdentry_t *entry;
		while (NULL != (entry = RB_ROOT(&NEM_mdmgr_tree))) {
			free((char*) entry->md.file);
			free(entry);
		}
		NEM_mdmgr_state = NEM_MDMGR_STATE_FREED;
	}
	else {
		NEM_mdmgr_state = NEM_MDMGR_STATE_INIT;
	}

	return err;
}

void
NEM_mdmgr_free()
{
	NEM_panic("TODO");
}

NEM_err_t
NEM_md_init(NEM_md_t *this, const char *path, bool ro)
{
	NEM_panic("TODO");
}

void
NEM_md_free(NEM_md_t *this)
{
	NEM_panic("TODO");
}
