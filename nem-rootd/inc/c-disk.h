#pragma once

typedef struct NEM_disk_t NEM_disk_t;

// NEM_disk_init_device initializes a disk given a device path. The device
// must already exist.
NEM_err_t NEM_disk_init_device(NEM_disk_t **out, const char *device);

// NEM_disk_init_mem initializes a new memory-backed virtual disk of the
// requested size. The contents are uninitialized.
NEM_err_t NEM_disk_init_mem(NEM_disk_t **out, size_t len);

// NEM_disk_init_file initializes a new file-backed virtual disk of the
// requested file. The file must exist ahead of time and is unchanged (e.g.
// it should be created and initialized before calling this method). If the
// ro flag is set, the underlying file is immutable.
NEM_err_t NEM_disk_init_file(NEM_disk_t **out, const char *path, bool ro);

// NEM_disk_free should be called on every disk returned by one of the
// init methods. If the disk didn't exist prior (and is virtual) it is
// removed.
void NEM_disk_free(NEM_disk_t *this);

const char *NEM_disk_device(NEM_disk_t *this);
const char *NEM_disk_dbg_string(NEM_disk_t *this);

extern const NEM_app_comp_t NEM_rootd_c_disk;
