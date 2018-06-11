#include <sys/types.h>
#include <stdarg.h>
#include "nem.h"
#include "c-log.h"
#include "c-args.h"

static const char*
NEM_comp_name(NEM_rootd_comp_t comp)
{
	static const struct {
		NEM_rootd_comp_t comp;
		const char      *name;
	}
	names[] = {
		{ COMP_ARGS,     "c-args"     },
		{ COMP_LOG,      "c-log"      },
		{ COMP_LOCKFILE, "c-lockfile" },
		{ COMP_SIGNAL,   "c-signal"   },
		{ COMP_DATABASE, "c-database" },
		{ COMP_ROUTERD,  "c-routerd"  },
		{ COMP_IMAGES,   "c-images"   },
		{ COMP_JAILS,    "c-jails"    },
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

	if (NEM_rootd_verbose()) {
		printf("%s: %s\n", NEM_comp_name(comp), entry);
	}

	free(entry);
}
