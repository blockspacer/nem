#pragma once

typedef enum {
	NEM_DISK_NONE,      // empty disk
	NEM_DISK_PHYSICAL,  // disk
	NEM_DISK_PARTITION, // part
	NEM_DISK_ENCRYPT,   // eli
	NEM_DISK_VIRTUAL,   // md
}
NEM_disk_type_t;

typedef struct {
	NEM_disk_type_t (*type)(void *this);
	const char* (*device)(void *this);
	void (*free)(void *this);
	bool (*mounted)(void *this);
	bool (*readonly)(void *this);
}
NEM_disk_vt;

typedef struct {
	const NEM_disk_vt *vt;
	void              *this;
}
NEM_disk_t;

// NEM_disk_init_device initializes a disk given a device path.
NEM_err_t NEM_disk_init_device(NEM_disk_t *out, const char *device);

// NEM_disk_init_mem initializes a new memory-backed virtual disk of the
// requested size. The contents are uninitialized.
NEM_err_t NEM_disk_init_mem(NEM_disk_t *out, size_t len);

// NEM_disk_init_file initializes a new file-backed virtual disk of the
// requested file. The file must exist ahead of time and is unchanged (e.g.
// it should be created and initialized before calling this method). If the
// ro flag is set, the underlying file is immutable.
NEM_err_t NEM_disk_init_file(NEM_disk_t *out, const char *path, bool ro);

/*
 * Static dispatch methods
 */

static inline NEM_disk_type_t
NEM_disk_type(NEM_disk_t this)
{
	return this.vt->type(this.this);
}

static inline const char*
NEM_disk_device(NEM_disk_t this)
{
	return this.vt->device(this.this);
}

static inline void
NEM_disk_free(NEM_disk_t this)
{
	return this.vt->free(this.this);
}

static inline bool
NEM_disk_mounted(NEM_disk_t this)
{
	return this.vt->mounted(this.this);
}

static inline bool
NEM_disk_readonly(NEM_disk_t this)
{
	return this.vt->readonly(this.this);
}

extern const NEM_app_comp_t NEM_rootd_c_disk;
