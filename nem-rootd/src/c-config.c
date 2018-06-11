#include "nem.h"
#include "c-config.h"

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	return NEM_err_none;
}

static void
teardown(NEM_app_t *app)
{
}

const NEM_app_comp_t NEM_rootd_c_config = {
	.setup    = &setup,
	.teardown = &teardown,
};
