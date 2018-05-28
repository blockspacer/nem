#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>

#include "nem.h"
#include "args.h"

static NEM_hostd_args_t static_args = {0};
extern int optind, optreset;
bool NEM_hostd_args_testing = false;

enum {
	OPT_VERBOSE,
	OPT_CONFIG
};
static struct option longopts[] = {
	{ "verbose", no_argument,       NULL, 'v' },
	{ "config",  required_argument, NULL, 'c' },
	{ NULL,      0,                 NULL, 0   },
};

static void
NEM_hostd_args_free(NEM_hostd_args_t *this)
{
	free((char*)this->own_path);
	free((char*)this->config_path);
	bzero(this, sizeof(*this));
}

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
	if (argc == 0) {
		return NEM_err_static("args: no args??");
	}
	if (args->initialized) {
		NEM_panic("args: setup called twice?");
	}

	args->initialized = true;
	args->own_path = strdup(argv[0]);

	char ch = 0;
	int idx = 0;
	optind = 1;
	optreset = 1;

	while (-1 != (ch = getopt_long(argc, argv, "vc", longopts, &idx))) {
		switch (ch) {
			default:
				usage();
				NEM_hostd_args_free(args);
				return NEM_err_static("args: invalid option");

			case 'v':
				args->verbose = true;
				break;
			case 'c':
				if (NULL == optarg || '\0' == optarg[0]) {
					usage();
					NEM_hostd_args_free(args);
					return NEM_err_static("args: config not provided");
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
	NEM_hostd_args_free(&static_args);
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
