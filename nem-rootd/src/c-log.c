#include <sys/types.h>
#include <stdarg.h>
#include "nem.h"
#include "c-log.h"
#include "c-args.h"
#include "utils.h"

static const char*
NEM_comp_name(NEM_rootd_comp_t comp)
{
	static const struct {
		NEM_rootd_comp_t comp;
		const char      *name;
	}
	names[] = {
		{ COMP_ARGS,     "args"     },
		{ COMP_LOG,      "log"      },
		{ COMP_LOCKFILE, "lockfile" },
		{ COMP_SIGNAL,   "signal"   },
		{ COMP_DATABASE, "database" },
		{ COMP_ROUTERD,  "routerd"  },
		{ COMP_IMAGES,   "images"   },
		{ COMP_MOUNTS,   "mounts"   },
		{ COMP_MD,       "md"       },
		{ COMP_JAILS,    "jails"    },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(names); i += 1) {
		if (comp == names[i].comp) {
			return names[i].name;
		}
	}

	return "(unknown)";
}

void
NEM_logf(NEM_rootd_comp_t comp, const char *fmt, ...)
{
	va_list ap;
	char *entry = NULL;

	va_start(ap, fmt);
	int err = vasprintf(&entry, fmt, ap);
	va_end(ap);

	if (0 > err) {
		NEM_panicf_errno("NEM_logf:");
	}

	if (NEM_rootd_verbose() && !NEM_rootd_testing) {
		printf("%s: %s\n", NEM_comp_name(comp), entry);
	}

	free(entry);
}
