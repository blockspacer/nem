#pragma once
#include <sys/types.h>
#include <time.h>

typedef enum {
	NEM_ROOTD_IMGV_OK,
	NEM_ROOTD_IMGV_MOUNTED,
	NEM_ROOTD_IMGV_BAD_HASH,
	NEM_ROOTD_IMGV_BAD_SIZE,
	NEM_ROOTD_IMGV_MISSING,
}
NEM_rootd_imgvstat_t;

const char* NEM_rootd_imgv_status_string(int status);

typedef struct {
	int       id;
	int       image_id;
	struct tm created;
	int       size;
	char     *sha256;
	char     *version;
	int       status;
	int       refcount;
}
NEM_rootd_imgv_t;

typedef struct {
	int   id;
	char *name;

	int    *vers;
	size_t  vers_len;
	size_t  vers_cap;
}
NEM_rootd_img_t;

typedef struct {
	NEM_rootd_img_t *imgs;
	size_t           imgs_len;
	size_t           imgs_cap;

	NEM_rootd_imgv_t *vers;
	size_t            vers_len;
	size_t            vers_cap;
}
NEM_rootd_imgset_t;

void NEM_rootd_imgset_init(NEM_rootd_imgset_t *this);
NEM_rootd_imgset_t* NEM_rootd_imgset_copy(const NEM_rootd_imgset_t *this);
void NEM_rootd_imgset_free(NEM_rootd_imgset_t *this);

NEM_rootd_img_t* NEM_rootd_imgset_img_by_name(
	NEM_rootd_imgset_t *this,
	const char        *name
);
NEM_rootd_img_t *NEM_rootd_imgset_img_by_id(
	NEM_rootd_imgset_t *this,
	int                 id
);
NEM_rootd_imgv_t* NEM_rootd_imgset_imgv_by_hash(
	NEM_rootd_imgset_t *this,
	const char        *sha256hex
);
NEM_rootd_imgv_t* NEM_rootd_imgset_imgv_by_id(
	NEM_rootd_imgset_t *this,
	int                 id
);

NEM_err_t NEM_rootd_imgset_add_img(
	NEM_rootd_imgset_t *this,
	NEM_rootd_img_t   **img
);
NEM_err_t NEM_rootd_imgset_add_ver(
	NEM_rootd_imgset_t *this,
	NEM_rootd_imgv_t  **ver,
	NEM_rootd_img_t    *img
);
