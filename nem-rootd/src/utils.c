#include <sys/types.h>
#include <dirent.h>
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

NEM_err_t
NEM_erase_dir(const char *path)
{
	int dir_fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (-1 == dir_fd) {
		return NEM_err_errno();
	}

	size_t buf_len = sizeof(struct dirent) * 100;
	char *buf = NEM_malloc(buf_len);
	char *fullpath = NULL;
	NEM_err_t err = NEM_err_none;

	for (;;) {
		lseek(dir_fd, 0, SEEK_SET);
		int num_bytes = getdents(dir_fd, buf, buf_len);
		if (-1 == num_bytes) {
			err = NEM_err_errno();
			goto done;
		}
		bool did_work = false;

		for (char *ptr = buf; ptr < buf + num_bytes;) {
			struct dirent *ent = (struct dirent*) ptr;
			ptr += ent->d_reclen;

			if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
				continue;
			}

			asprintf(&fullpath, "%s/%s", path, ent->d_name);

			if (DT_DIR == ent->d_type) {
				err = NEM_erase_dir(fullpath);
				if (!NEM_err_ok(err)) {
					goto done;
				}
				if (-1 == rmdir(fullpath)) {
					err = NEM_err_errno();
					goto done;
				}
			}
			else {
				if (-1 == unlink(fullpath)) {
					err = NEM_err_errno();
					goto done;
				}
			}
			did_work = true;
			free(fullpath);
			fullpath = NULL;
		}
		if (!did_work) {
			break;
		}
	}

done:
	free(fullpath);
	free(buf);
	close(dir_fd);
	return err;
}
