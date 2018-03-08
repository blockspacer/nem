#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ucred.h>

#include "nem.h"

int
main(int argc, char *argv[])
{
	int num_mounts = getfsstat(NULL, 0, MNT_WAIT);
	if (-1 == num_mounts) {
		NEM_panicf_errno("getfsstat");
	}

	size_t len = (size_t) num_mounts * sizeof(struct statfs);
	struct statfs *mnts = NEM_malloc(len);
	num_mounts = getfsstat(mnts, len, MNT_WAIT);
	if (-1 == num_mounts) {
		NEM_panicf_errno("getfsstat");
	}

	for (size_t i = 0; i < num_mounts; i += 1) {
		printf(
			"fsid=%d/%d fstype=%s mntfrom=%s mnton=%s\n", 
			mnts[i].f_fsid.val[0],
			mnts[i].f_fsid.val[1],
			mnts[i].f_fstypename,
			mnts[i].f_mntfromname,
			mnts[i].f_mntonname
		);
	}

	free(mnts);
}
