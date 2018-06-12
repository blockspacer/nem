#include "nem.h"
#include "utils.h"
#include "c-log.h"
#include "c-config.h"

static int lock_fd;
static const char *lock_name = "lockfile";
static char *lock_path = NULL;

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	NEM_logf(COMP_LOCKFILE, "setup");

	NEM_err_t err = NEM_path_join(
		&lock_path,
		NEM_rootd_jail_root(),
		lock_name
	);
	if (!NEM_err_ok(err)) {
		return err;
	}

	lock_fd = open(lock_path, O_CREAT|O_NOFOLLOW|O_CLOEXEC, 0600);
	if (0 > lock_fd) {
		return NEM_err_errno();
	}

	if (0 != flock(lock_fd, LOCK_EX|LOCK_NB)) {
		NEM_err_t err = NEM_err_errno();
		NEM_logf(COMP_LOCKFILE, "unable to lock file");
		close(lock_fd);
		return err;
	}

	NEM_logf(COMP_LOCKFILE, "locked '%s'", lock_path);

	return NEM_err_none;
}

static void
teardown(NEM_app_t *app)
{
	NEM_logf(COMP_LOCKFILE, "teardown");

	flock(lock_fd, LOCK_UN);
	close(lock_fd);
	if (0 != unlink(lock_path)) {
		NEM_logf(
			COMP_LOCKFILE,
			"unable to unlink '%s': '%s'",
			lock_path,
			NEM_err_string(NEM_err_errno())
		);
	}
	free(lock_path);
}

const NEM_app_comp_t NEM_rootd_c_lockfile = {
	.name     = "lockfile",
	.setup    = &setup,
	.teardown = &teardown,
};
