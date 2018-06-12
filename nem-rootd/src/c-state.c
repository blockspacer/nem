#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>

#include "nem.h"
#include "c-state.h"
#include "utils.h"

/*
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
*/

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	/*
	err = check_options();
	if (!NEM_err_ok(err)) {
		return err;
	}

	NEM_logf(
		COMP_STATE,
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
	*/

	return NEM_err_none;
}

static void
teardown()
{
	/*
	free(rootd_path);
	free(jail_root);
	free(routerd_path);
	*/
}

const NEM_app_comp_t NEM_rootd_c_state = {
	.name     = "state",
	.setup    = &setup,
	.teardown = &teardown,
};
