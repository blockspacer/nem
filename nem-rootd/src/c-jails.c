#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "nem.h"
#include "lifecycle.h"
#include "state.h"

static int lock_fd;

static NEM_err_t
path_join(char **out, const char *base, const char *rest)
{
	int ret = asprintf(out, "%s/%s", base, rest);
	if (0 > ret) {
		return NEM_err_errno();
	}

	return NEM_err_none;
}

static NEM_err_t
make_directory(const char *base, const char *path)
{
	char *joined;
	NEM_err_t err = path_join(&joined, base, path);
	if (!NEM_err_ok(err)) {
		return err;
	}

	if (NEM_rootd_verbose()) {
		printf("c-jails: making %s\n", joined);
	}

	int ret = mkdir(joined, 0755);
	free(joined);

	if (0 > ret) {
		if (EEXIST != errno) {
			return NEM_err_errno();
		}
	}

	return NEM_err_none;
}

static NEM_err_t
make_directories(const char *base)
{
	const char *paths[] = {
		"images",
		"running",
		"persisted",
	};

	for (size_t i = 0; i < NEM_ARRSIZE(paths); i += 1) {
		NEM_err_t err = make_directory(base, paths[i]);
		if (!NEM_err_ok(err)) {
			return err;
		}
	}

	return NEM_err_none;
}

static NEM_err_t
open_lockfile(const char *base)
{
	char *lock_path;
	NEM_err_t err = path_join(&lock_path, base, "lock");
	if (!NEM_err_ok(err)) {
		return err;
	}

	if (NEM_rootd_verbose()) {
		printf("c-jails: locking\n");
	}

	lock_fd = open(lock_path, O_CREAT|O_NOFOLLOW|O_CLOEXEC, 0600);
	free(lock_path);
	if (0 > lock_fd) {
		return NEM_err_errno();
	}

	if (0 != flock(lock_fd, LOCK_EX|LOCK_NB)) {
		err = NEM_err_errno();
		if (NEM_rootd_verbose()) {
			printf("c-jails: unable to lock lockfile\n");
		}
		close(lock_fd);
		return err;
	}

	return NEM_err_none;
}

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-jails: setup\n");
	}

	NEM_err_t err;
	const char *base = NEM_rootd_jail_root();

	err = make_directories(base);
	if (!NEM_err_ok(err)) {
		return err;
	}

	err = open_lockfile(base);
	if (!NEM_err_ok(err)) {
		return err;
	}

	return NEM_err_none;
}

static bool
try_shutdown()
{
	if (NEM_rootd_verbose()) {
		printf("c-jails: try-shutdown\n");
	}

	return true;
}

static void
teardown()
{
	if (NEM_rootd_verbose()) {
		printf("c-jails: teardown\n");
	}
}

const NEM_rootd_comp_t NEM_rootd_c_jails = {
	.name         = "c-jails",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
