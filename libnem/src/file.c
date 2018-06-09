#include "nem.h"

NEM_err_t
NEM_file_init(NEM_file_t *this, const char *path)
{
	bzero(this, sizeof(*this));

	int fd = open(path, O_RDONLY|O_CLOEXEC);
	if (0 > fd) {
		return NEM_err_errno();
	}

	if (0 > fstat(fd, &this->stat)) {
		close(fd);
		return NEM_err_errno();
	}

	void *data = NULL;

	if (0 < this->stat.st_size) {
		// NB: mmap returns EINVAL for size=0.
		data = mmap(NULL, this->stat.st_size, PROT_READ, MAP_NOCORE, fd, 0);
		if (MAP_FAILED == data) {
			close(fd);
			return NEM_err_errno();
		}
	}

	close(fd);
	this->path = strdup(path);
	this->data = data;
	return NEM_err_none;
}

void
NEM_file_free(NEM_file_t *this)
{
	free((void*)this->path);
	if (NULL != this->data) {
		if (0 != munmap((void*)this->data, this->stat.st_size)) {
			NEM_panicf_errno("NEM_file_free: munmap failed");
		}
	}
}

size_t
NEM_file_len(NEM_file_t *this)
{
	return this->stat.st_size;
}

const void*
NEM_file_data(NEM_file_t *this)
{
	return this->data;
}
