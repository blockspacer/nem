#include "utils.h"

NEM_err_t
NEM_err_sqlite(sqlite3 *db)
{
	// NB: This is kind of a mess, but the error messages need to outlive
	// the database lifetime. So just copy them into a static-duration
	// storage and hope it gets picked up in time.
	static char *msg = NULL;
	if (NULL != msg) {
		free(msg);
	}
	msg = strdup(sqlite3_errmsg(db));
	return NEM_err_static(msg);
}
