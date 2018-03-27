#include "nem.h"
#include "lifecycle.h"
#include "state.h"

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-jails: setup\n");
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
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
