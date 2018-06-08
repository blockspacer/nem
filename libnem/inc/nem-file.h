#pragma once

typedef struct {
	const char *path;
	const void *data;
	struct stat stat;
}
NEM_file_t;

NEM_err_t NEM_file_init(NEM_file_t *this, const char *path);
void NEM_file_free(NEM_file_t *this);

size_t NEM_file_len(NEM_file_t *this);
const void* NEM_file_data(NEM_file_t *this);
