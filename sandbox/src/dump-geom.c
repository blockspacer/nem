#include <sys/types.h>
#include <libgeom.h>
#include <devstat.h>
#include <stdio.h>

#include "nem.h"

static void printgeom(struct ggeom *geom, const char *prefix);

static void
printconfig(struct gconf *cfg, const char *prefix)
{
	struct gconfig *entry;

	LIST_FOREACH(entry, cfg, lg_config) {
		printf("%s'%s' = '%s'\n", prefix, entry->lg_name, entry->lg_val);
	}
}

static void
printprovider(struct gprovider *prov, const char *prefix)
{
	char *new_prefix;
	asprintf(&new_prefix, "%s  ", prefix);
	printf("%sPROVIDER: %s %p\n", prefix, prov->lg_name, prov->lg_id);
	printconfig(&prov->lg_config, prefix);

	printgeom(prov->lg_geom, new_prefix);
	free(new_prefix);
}

static void
printconsumer(struct gconsumer *cons, const char *prefix)
{
	if (strlen(prefix) > 24) {
		return;
	}

	char *new_prefix;
	asprintf(&new_prefix, "%s  ", prefix);
	printf("%sCONSUMER %p\n", prefix, cons->lg_id);
	printconfig(&cons->lg_config, prefix);

	printprovider(cons->lg_provider, new_prefix);
	free(new_prefix);
}

static void
printgeom(struct ggeom *geom, const char *prefix)
{
	char *new_prefix;
	asprintf(&new_prefix, "%s  ", prefix);

	printf("%sGEOM: %s\n", prefix, geom->lg_name);
	printf("%srank: %d\n", prefix, geom->lg_rank);
	printf("%sclass: %s\n", prefix, geom->lg_class->lg_name);

	struct gprovider *prov;
	LIST_FOREACH(prov, &geom->lg_provider, lg_provider) {
		printf("%sPROVIDER: %s\n", new_prefix, prov->lg_name);
	}

	struct gconsumer *cons;
	LIST_FOREACH(cons, &geom->lg_consumer, lg_consumer) {
		printconsumer(cons, new_prefix);
	}

	free(new_prefix);
}

static void
printclass(struct gclass *cls)
{
	struct ggeom *geom = NULL;
	LIST_FOREACH(geom, &cls->lg_geom, lg_geom) {
		printgeom(geom, "");
		printf("\n");
	}
}

int
main(int argc, char *argv[])
{
	struct gmesh tree;
	if (0 != geom_gettree(&tree)) {
		NEM_panicf_errno("geom_gettree");
	}

	struct gclass *cls;
	LIST_FOREACH(cls, &tree.lg_class, lg_class) {
		printclass(cls);
	}

	geom_deletetree(&tree);
}
