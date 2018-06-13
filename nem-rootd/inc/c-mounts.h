#pragma once
#include "jaildesc.h"

struct NEM_mount_t;
typedef struct NEM_mount_t NEM_mount_t;

typedef struct {
	NEM_mount_t **mounts;
	size_t        mounts_len;
}
NEM_mountset_t;

typedef struct {
	NEM_err_t       err;
	NEM_mountset_t *set;
}
NEM_mount_ca;

void NEM_mount_images(
	const NEM_jailimg_t *imgs,
	size_t               imgs_len,
	const char          *base,
	NEM_thunk1_t        *thunk
);

void NEM_unmount_set(NEM_mountset_t *set);

extern const NEM_app_comp_t NEM_rootd_c_mounts;
