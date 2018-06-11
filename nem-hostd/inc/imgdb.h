#pragma once
#include "jaildesc.h"

typedef NEM_hostd_jailimg_type_t NEM_hostd_img_type_t;

struct NEM_hostd_img_t;

typedef struct {
	struct NEM_hostd_img_t *img;
	const char             *dest;
	bool                    borrowed;
}
NEM_hostd_mount_t;

typedef struct {
	LIST_ENTRY(NEM_hostd_img_t) entry;

	NEM_hostd_img_type_t type;
	const char          *sha256;
	const char          *name;
	const char          *semver;
	bool                 persist;
	size_t               len;
	NEM_hostd_mount_t   *mounts;
	size_t               mounts_len;
}
NEM_hostd_img_t;

extern const NEM_app_comp_t NEM_hostd_c_imgdb;
