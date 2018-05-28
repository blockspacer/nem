#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

#include "nem.h"
#include "args.h"

static NEM_hostd_args_t static_args;
bool NEM_hostd_args_testing = false;

enum {
	OPT_VERBOSE,
	OPT_CONFIG
};
static struct option longopts[] = {
	{ "verbose", no_argument,       NULL, 1 },
	{ "config",  required_argument, NULL, 1 },
	{ NULL,      0,                 NULL, 0 },
};

static void
usage()
{
	if (!NEM_hostd_args_testing) {
		printf(
			"Usage: %s\n"
			" --verbose:     be noisy\n"
			" --config=path: path to config\n",
			static_args.own_path
		);
	}
}

static NEM_err_t
parse_options(NEM_hostd_args_t *args, int argc, char *argv[])
{
	bzero(args, sizeof(*args));
	args->own_path = strdup(argv[0]);

	char ch = 0;
	int idx = 0;
	while (-1 != (ch = getopt_long(argc, argv, "vc", longopts, &idx))) {
		if (1 != ch) {
			usage();
			return NEM_err_static("parse_options: invalid option");
		}
		switch (idx) {
			case OPT_VERBOSE:
				args->verbose = true;
				break;
			case OPT_CONFIG:
				if (NULL == optarg || '\0' == optarg[0]) {
					usage();
					return NEM_err_static("parse_options: config not provided");
				}
				args->config_path = strdup(optarg);
				break;
		}
	}

	return NEM_err_none;
}

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	return parse_options(&static_args, argc, argv);
}

static void
teardown()
{
	free((void*)static_args.own_path);
}

const NEM_app_comp_t NEM_hostd_c_args = {
	.name     = "args",
	.setup    = &setup,
	.teardown = &teardown,
};

const NEM_hostd_args_t*
NEM_hostd_args()
{
	return &static_args;
}
