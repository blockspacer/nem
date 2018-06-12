#include "nem.h"
#include "c-log.h"
#include "c-state.h"

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	NEM_logf(COMP_JAILS, "setup");
	return NEM_err_none;
}

static bool
try_shutdown(NEM_app_t *app)
{
	NEM_logf(COMP_JAILS, "try-shutdown");
	return true;
}

static void
teardown(NEM_app_t *app)
{
	NEM_logf(COMP_JAILS, "teardown");
}

const NEM_app_comp_t NEM_rootd_c_jails = {
	.name         = "jails",
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};
