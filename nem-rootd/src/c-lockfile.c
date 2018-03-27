#include "nem.h"
#include "lifecycle.h"
#include "state.h"

static int lock_fd;
static const char *lock_name = "lockfile";

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-lockfile: setup\n");
	}

	char *lock_path;
	if (0 > asprintf(&lock_path, NEM_rootd_jail_root(), lock_name)) {
		return NEM_err_errno();
	}

	lock_fd = open(lock_path, O_CREAT|O_NOFOLLOW|O_CLOEXEC, 0600);
	free(lock_path);
	if (0 > lock_fd) {
		return NEM_err_errno();
	}

	if (0 != flock(lock_fd, LOCK_EX|LOCK_NB)) {
		NEM_err_t err = NEM_err_errno();
		if (NEM_rootd_verbose()) {
			printf("c-lockfile: unable to lock file\n");
		}
		close(lock_fd);
		return err;
	}

	return NEM_err_none;
}

static void
teardown()
{
	if (NEM_rootd_verbose()) {
		printf("c-lockfile: teardown\n");
	}

	flock(lock_fd, LOCK_UN);
	close(lock_fd);
}

const NEM_rootd_comp_t NEM_rootd_c_lockfile = {
	.name     = "c-lockfile",
	.setup    = &setup,
	.teardown = &teardown,
};
