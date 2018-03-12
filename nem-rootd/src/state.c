#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#include "state.h"
#include "nem.h"

static bool is_init = false;
static bool is_root = false;
static int verbose = 0;
static int reload = 0;
static char *rootd_path = NULL;
static char *routerd_path = NULL;
static char *jail_root = NULL;

enum {
	OPT_VERBOSE,
	OPT_RELOAD,
	OPT_ROUTERD_PATH,
	OPT_JAIL_ROOT,
};
static struct option longopts[] = {
	{ "verbose",      optional_argument, &verbose, 'v' },
	{ "reload",       optional_argument, &reload,   0  },
	{ "routerd-path", optional_argument, NULL,      0  },
	{ "jail-root",    optional_argument, NULL,      0  },
	{ 0 },
};

static void
usage()
{
	printf(
		"Usage: %s\n"
		"  --verbose, -v:       be noisy\n"
		"  --reload:            internal usage\n"
		"  --routerd-path=path: set path to nem-routerd\n"
		"  --jail-root=path:    set path to jail root dir\n",
		rootd_path
	);
}

static char*
dupe_arg(int argc, char *argv[])
{
	if (NULL != optarg) {
		return strdup(optarg);
	}
	if (optind < argc) {
		return strdup(argv[optind]);
	}

	// XXX: Not really a good way to handle this.
	usage();
	exit(1);
}

static bool
parse_options(int argc, char *argv[])
{
	rootd_path = strdup(argv[0]);

	char ch;
	int idx;
	while (-1 != (ch = getopt_long(argc, argv, "vr0", longopts, &idx))) {
		switch (ch) {
			case 'v':
			case 0: 
				break;
			default:
				usage();
				return false;
		}
		switch (idx) {
			case OPT_VERBOSE:
				verbose = 1;
				break;
			case OPT_RELOAD:
				reload = 1;
				break;
			case OPT_ROUTERD_PATH:
				routerd_path = dupe_arg(argc, argv);
				break;
			case OPT_JAIL_ROOT:
				jail_root = dupe_arg(argc, argv);
				break;
			default:
				usage();
				return false;
		}
	}

	// XXX: Don't do this here; check that the default values are valid
	// before setting them up.
	if (NULL == routerd_path) {
		routerd_path = strdup("./nem-routerd");
	}
	if (NULL == jail_root) {
		jail_root = strdup("./jails");
	}

	return true;
}

static void
resolve_path(char **path)
{
	char *tmp = *path;
	*path = NEM_panic_if_null(realpath(*path, NULL));
	free(tmp);
}

static bool
check_options()
{
	struct stat sb;

	if (0 != stat(routerd_path, &sb) || !S_ISREG(sb.st_mode)) {
		printf("missing routerd: at %s\n", routerd_path);
		return false;
	}
	// XXX: Check if we can execute.

	if (0 != stat(jail_root, &sb) || !S_ISDIR(sb.st_mode)) {
		printf("missing jailroot: at %s\n", jail_root);
		return false;
	}
	// XXX: Check if we can write.

	resolve_path(&rootd_path);
	resolve_path(&routerd_path);
	resolve_path(&jail_root);
	return true;
}

bool
NEM_rootd_state_init(int argc, char *argv[])
{
	if (1 == getpid()) {
		is_init = true;
	}
	if (0 == geteuid()) {
		is_root = true;
	}
	if (!parse_options(argc, argv)) {
		return false;
	}
	/*
	if (!check_options()) {
		return false;
	}
	*/

	printf(
		"verbose=%d\nreload=%d\nrootd_path=%s\nrouterd_path=%s\njail_root=%s\n",
		verbose,
		reload,
		rootd_path,
		routerd_path,
		jail_root
	);

	return false;
}

void
NEM_rootd_state_close()
{
	free(rootd_path);
	free(jail_root);
	free(routerd_path);
}
