#include "nem.h"
#include "lifecycle.h"

extern const NEM_rootd_comp_t
	NEM_rootd_c_svc_daemon,
	NEM_rootd_c_svc_host,
	NEM_rootd_c_signal,
	NEM_rootd_c_routerd;

int
main(int argc, char *argv[])
{
	NEM_rootd_add_comp(&NEM_rootd_c_signal);
	NEM_rootd_add_comp(&NEM_rootd_c_svc_daemon);
	NEM_rootd_add_comp(&NEM_rootd_c_svc_host);
	NEM_rootd_add_comp(&NEM_rootd_c_routerd);

	NEM_err_t err = NEM_rootd_main(argc, argv);
	if (!NEM_err_ok(err)) {
		printf("ERROR: %s\n", NEM_err_string(err));
		return 1;
	}

	return 0;
}
