#include "nem.h"
#include "c-routerd.h"

extern const NEM_app_comp_t
	NEM_rootd_c_args,
	NEM_rootd_c_config,
	NEM_rootd_c_lockfile,
	NEM_rootd_c_signal,
	NEM_rootd_c_database,
	NEM_rootd_c_routerd,
	NEM_rootd_c_images,
	NEM_rootd_c_md,
	NEM_rootd_c_mounts,
	NEM_rootd_c_jails;

int
main(int argc, char *argv[])
{
	static const NEM_app_comp_t *comps[] = {
		&NEM_rootd_c_args,
		&NEM_rootd_c_config,

		&NEM_rootd_c_lockfile,
		&NEM_rootd_c_signal,
		&NEM_rootd_c_database,

		&NEM_rootd_c_images,
		&NEM_rootd_c_md,
		&NEM_rootd_c_mounts,
		&NEM_rootd_c_jails,
	};

	NEM_app_t app;
	NEM_app_init_root(&app);
	NEM_app_add_comps(&app, comps, NEM_ARRSIZE(comps));

	NEM_panic_if_err(NEM_rootd_routerd_bind_http(3000));
	NEM_panic_if_err(NEM_rootd_routerd_bind_http(3001));
	NEM_panic_if_err(NEM_rootd_routerd_bind_http(3002));

	NEM_err_t err = NEM_app_main(&app, argc, argv);
	if (!NEM_err_ok(err)) {
		printf("ERROR: %s\n", NEM_err_string(err));
		return 1;
	}

	return 0;
}
