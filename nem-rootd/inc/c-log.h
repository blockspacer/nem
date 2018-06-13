#pragma once

typedef enum {
	COMP_ARGS,
	COMP_LOG,
	COMP_LOCKFILE,
	COMP_SIGNAL,
	COMP_DATABASE,
	COMP_ROUTERD,
	COMP_IMAGES,
	COMP_MD,
	COMP_MOUNTS,
	COMP_JAILS,
}
NEM_rootd_comp_t;

void NEM_logf(NEM_rootd_comp_t comp, const char *fmt, ...);
