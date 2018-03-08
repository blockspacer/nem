#include <sys/types.h>
#include <libgeom.h>
#include <devstat.h>
#include <stdio.h>

#include "nem.h"

static void
printconfig(struct gconf *cfg)
{
	struct gconfig *entry;

	LIST_FOREACH(entry, cfg, lg_config) {
		printf("   '%s' = '%s'\n", entry->lg_name, entry->lg_val);
	}
}

int
main(int argc, char *argv[])
{
	struct gmesh tree;
	if (0 != geom_gettree(&tree)) {
		NEM_panicf_errno("geom_gettree");
	}

	if (0 != geom_stats_open()) {
		NEM_panicf_errno("geom_stats_open");
	}

	void *snap = geom_stats_snapshot_get();
	if (NULL == snap) {
		NEM_panicf_errno("geom_stats_snapshot_get");
	}

	struct devstat *entry;
	while (NULL != (entry = geom_stats_snapshot_next(snap))) {
		struct gident *id = geom_lookupid(&tree, entry->id);
		if (NULL == id || ISPROVIDER != id->lg_what) {
			continue;
		}

		struct gprovider *prov = id->lg_ptr;
		struct ggeom *geom = prov->lg_geom;
		struct gclass *cls = geom->lg_class;

		if (argc > 1 && strcmp(argv[1], cls->lg_name)) {
			continue;
		}

		printf("provider\n");
		printf("   name = %s\n", prov->lg_name);
		printf("   size = %lu\n", prov->lg_mediasize);
		printconfig(&prov->lg_config);
		printf("geom\n");
	   	printf("   rank = %u\n", geom->lg_rank);
		printf("   name = %s\n", geom->lg_name);
		printconfig(&geom->lg_config);
		printf("class\n");
		printf("   name = %s\n", cls->lg_name);
		printconfig(&cls->lg_config);
		printf("\n");
	}

	geom_stats_snapshot_free(snap);
	geom_deletetree(&tree);
}
