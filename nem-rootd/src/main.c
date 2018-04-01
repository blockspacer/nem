#include "nem.h"
#include "lifecycle.h"
#include "c-routerd.h"

extern const NEM_rootd_comp_t
	NEM_rootd_c_lockfile,
	NEM_rootd_c_signal,
	NEM_rootd_c_database,
	NEM_rootd_c_routerd,
	NEM_rootd_c_images,
	NEM_rootd_c_svc_daemon,
	NEM_rootd_c_svc_host,
	NEM_rootd_c_svc_imghost,
	NEM_rootd_c_jails;

int
main(int argc, char *argv[])
{
	NEM_rootd_add_comp(&NEM_rootd_c_signal);
	NEM_rootd_add_comp(&NEM_rootd_c_lockfile);
	NEM_rootd_add_comp(&NEM_rootd_c_database);
	NEM_rootd_add_comp(&NEM_rootd_c_routerd);
	NEM_rootd_add_comp(&NEM_rootd_c_images);
	NEM_rootd_add_comp(&NEM_rootd_c_svc_daemon);
	NEM_rootd_add_comp(&NEM_rootd_c_svc_host);
	NEM_rootd_add_comp(&NEM_rootd_c_svc_imghost);
	NEM_rootd_add_comp(&NEM_rootd_c_jails);

	NEM_panic_if_err(NEM_rootd_routerd_bind_http(3000));
	NEM_panic_if_err(NEM_rootd_routerd_bind_http(3001));
	NEM_panic_if_err(NEM_rootd_routerd_bind_http(3002));

	NEM_err_t err = NEM_rootd_main(argc, argv);
	if (!NEM_err_ok(err)) {
		printf("ERROR: %s\n", NEM_err_string(err));
		return 1;
	}

	return 0;
}
