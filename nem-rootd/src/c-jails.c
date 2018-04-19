#include "nem.h"
#include "c-state.h"

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	if (NEM_rootd_verbose()) {
		printf("c-jails: setup\n");
	}

	return NEM_err_none;
}

static bool
try_shutdown(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-jails: try-shutdown\n");
	}

	return true;
}

static void
teardown(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-jails: teardown\n");
	}
}

const NEM_app_comp_t NEM_rootd_c_jails = {
	.name         = "c-jails",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
