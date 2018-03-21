#include "nem.h"
#include "lifecycle.h"

int
main(int argc, char *argv[])
{
	// XXX: Add components here.

	NEM_err_t err = NEM_rootd_main(argc, argv);
	if (!NEM_err_ok(err)) {
		printf("ERROR: %s\n", NEM_err_string(err));
		return 1;
	}

	return 0;
}
