#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#include "nem.h"
#include "state.h"
#include "utils.h"

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
	{ "verbose",   optional_argument, &verbose, 'v' },
	{ "reload",    optional_argument, &reload,   0  },
	{ "routerd",   optional_argument, NULL,      0  },
	{ "jail-root", optional_argument, NULL,      0  },
	{ 0 },
};

static void
usage()
{
	printf(
		"Usage: %s\n"
		"  --verbose, -v:    be noisy\n"
		"  --reload:         internal usage\n"
		"  --routerd=path:   set path to nem-routerd\n"
		"  --jail-root=path: set path to jail root dir\n",
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

static NEM_err_t
parse_options(int argc, char *argv[])
{
	rootd_path = strdup(argv[0]);

	char ch = 0;
	int idx = 0;
	while (-1 != (ch = getopt_long(argc, argv, "vr0", longopts, &idx))) {
		switch (ch) {
			case 'v':
			case 0: 
				break;
			default:
				usage();
				return NEM_err_static("parse_options: invalid option");
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
				return NEM_err_static("parse_options: invalid option");
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

	return NEM_err_none;
}

static mode_t
get_rights_stat(struct stat *sb)
{
	mode_t mask = S_IRWXO;

	if (getuid() == sb->st_uid) {
		mask |= S_IRWXU;
	}
	if (getgid() == sb->st_gid) {
		mask |= S_IRWXG;
	}

	return sb->st_mode & mask;
}

static bool
can_exe_stat(struct stat *sb)
{
	mode_t mode = get_rights_stat(sb);
	return 0 != (mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

static bool
can_write_stat(struct stat *sb)
{
	mode_t mode = get_rights_stat(sb);
	return 0 != (mode & (S_IWUSR | S_IWGRP | S_IWOTH));
}

static NEM_err_t
check_options()
{
	struct stat sb;

	if (0 != stat(routerd_path, &sb) || !S_ISREG(sb.st_mode)) {
		if (verbose) {
			printf("ERROR: missing routerd: at %s\n", routerd_path);
		}
		return NEM_err_static("check_options: invalid routerd path");
	}
	if (!can_exe_stat(&sb)) {
		return NEM_err_static("check_options: routerd path not executable");
	}

	if (0 != stat(jail_root, &sb) || !S_ISDIR(sb.st_mode)) {
		if (verbose) {
			printf("ERROR: missing jailroot: at %s\n", jail_root);
		}
		return NEM_err_static("check_options: missing or invalid jail-root");
	}
	if (!can_write_stat(&sb)) {
		return NEM_err_static("check_options: jail-root not writable");
	}

	NEM_panic_if_err(NEM_path_abs(&rootd_path));
	NEM_panic_if_err(NEM_path_abs(&routerd_path));
	NEM_panic_if_err(NEM_path_abs(&jail_root));

	return NEM_err_none;
}

NEM_err_t
NEM_rootd_state_init(int argc, char *argv[])
{
	if (1 == getpid()) {
		is_init = true;
	}
	if (0 == geteuid()) {
		is_root = true;
	}
	NEM_err_t err = parse_options(argc, argv);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = check_options();
	if (!NEM_err_ok(err)) {
		return err;
	}

	if (verbose) {
		printf(
			"nem-rootd starting\n"
			"   pid        = %d\n"
			"   reload     = %d\n"
			"   rootd-path = %s\n"
			"   routerd    = %s\n"
			"   jail-root  = %s\n",
			getpid(),
			reload,
			rootd_path,
			routerd_path,
			jail_root
		);
	}

	return NEM_err_none;
}

void
NEM_rootd_state_close()
{
	free(rootd_path);
	free(jail_root);
	free(routerd_path);
}

bool
NEM_rootd_verbose()
{
	return verbose;
}

const char*
NEM_rootd_routerd_path()
{
	return routerd_path;
}

const char*
NEM_rootd_jail_root()
{
	return jail_root;
}
