#include "nem.h"
#include "signals.h"
#include "args.h"
#include "config.h"

static const NEM_app_comp_t *comps[] = {
	&NEM_hostd_c_signals,
	&NEM_hostd_c_args,
	&NEM_hostd_c_config,
};

int
main(int argc, char **argv)
{
	NEM_app_t app;
	NEM_app_init_root(&app);
	NEM_app_add_comps(&app, comps, NEM_ARRSIZE(comps));
	NEM_err_t err = NEM_app_main(&app, argc, argv);
	if (!NEM_err_ok(err)) {
		printf("error: %s\n", NEM_err_string(err));
	}

	return 0;
}
