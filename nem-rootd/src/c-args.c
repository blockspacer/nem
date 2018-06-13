#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

#include "nem.h"
#include "c-args.h"
#include "utils.h"

static bool is_init = false;
static bool is_root = false;
static int verbose = 0;
static int reload = 0;
static char *rootd_path = NULL;
static char *config_path = NULL;

enum {
	OPT_VERBOSE,
	OPT_CONFIG,
	OPT_RELOAD,
};
static struct option longopts[] = {
	{ "verbose",   optional_argument, &verbose, 'v' },
	{ "config",    required_argument, NULL,     'c' },
	{ "reload",    optional_argument, &reload,  'r' },
	{ 0 },
};

static void
usage()
{
	if (!NEM_rootd_testing) {
		fprintf(
			stderr,
			"Usage: %s\n"
			"  --verbose, -v:    be noisy\n"
			"  --reload:         internal usage\n"
			"  --config=path:    path to config.yaml\n",
			rootd_path
		);
	}
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

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	if (1 == getpid()) {
		is_init = true;
	}
	if (0 == geteuid()) {
		is_root = true;
	}
	rootd_path = strdup(argv[0]);

	char ch = 0;
	int idx = 0;
	while (-1 != (ch = getopt_long(argc, argv, "vr0", longopts, &idx))) {
		switch (ch) {
			case 'v':
				break;
			case 'c':
				break;
			case 0: 
				break;
			default:
				usage();
				return NEM_err_static("args: invalid option");
		}
		switch (idx) {
			case OPT_VERBOSE:
				verbose = 1;
				break;
			case OPT_RELOAD:
				reload = 1;
				break;
			case OPT_CONFIG:
				config_path = dupe_arg(argc, argv);
				break;
			default:
				usage();
				return NEM_err_static("parse_options: invalid option");
		}
	}

	if (NULL == config_path) {
		config_path = strdup("./config.yaml");
	}

	return NEM_err_none;
}

static void
teardown()
{
	free(rootd_path);
	free(config_path);
}

const NEM_app_comp_t NEM_rootd_c_args = {
	.name     = "args",
	.setup    = &setup,
	.teardown = &teardown,
};

bool
NEM_rootd_verbose()
{
	return verbose;
}

const char*
NEM_rootd_config_path()
{
	return config_path;
}

const char*
NEM_rootd_own_path()
{
	return rootd_path;
}

