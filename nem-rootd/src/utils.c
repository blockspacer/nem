#include <sys/types.h>
#include <stdlib.h>

#include "utils.h"

bool NEM_rootd_testing = false;

NEM_err_t
NEM_path_join(char **out, const char *base, const char *rest)
{
	int ret = asprintf(out, "%s/%s", base, rest);
	if (0 > ret) {
		return NEM_err_errno();
	}

	return NEM_err_none;
}

NEM_err_t
NEM_path_abs(char **path)
{
	char *tmp = realpath(*path, NULL);
	if (NULL == tmp) {
		return NEM_err_errno();
	}

	free(*path);
	*path = tmp;
	return NEM_err_none;
}

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

NEM_err_t
NEM_ensure_dir(const char *path)
{
	struct stat st = {0};

	if (0 != stat(path, &st)) {
		if (ENOENT == errno) {
			if (0 != mkdir(path, 0770)) {
				return NEM_err_errno();
			}

			return NEM_err_none;
		}

		return NEM_err_errno();
	}

	if (!S_ISDIR(st.st_mode)) {
		return NEM_err_static("NEM_ensure_dir: path is something else");
	}

	return NEM_err_none;
}
