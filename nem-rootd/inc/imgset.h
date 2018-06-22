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
NEM_imgverstat_t;

const char* NEM_imgver_status_string(int status);

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
NEM_imgver_t;

typedef struct {
	int   id;
	char *name;

	int    *vers;
	size_t  vers_len;
	size_t  vers_cap;
}
NEM_img_t;

typedef struct {
	NEM_img_t *imgs;
	size_t     imgs_len;
	size_t     imgs_cap;

	NEM_imgver_t *vers;
	size_t        vers_len;
	size_t        vers_cap;
}
NEM_imgset_t;

void NEM_imgset_init(NEM_imgset_t *this);
NEM_imgset_t* NEM_imgset_copy(const NEM_imgset_t *this);
void NEM_imgset_free(NEM_imgset_t *this);

NEM_img_t* NEM_imgset_img_by_name(
	NEM_imgset_t *this,
	const char   *name
);
NEM_img_t *NEM_imgset_img_by_id(
	NEM_imgset_t *this,
	int           id
);
NEM_imgver_t* NEM_imgset_imgver_by_hash(
	NEM_imgset_t *this,
	const char   *sha256hex
);
NEM_imgver_t* NEM_imgset_imgver_by_id(
	NEM_imgset_t *this,
	int           id
);

NEM_imgver_t* NEM_img_imgver_latest(NEM_imgset_t *set, NEM_img_t *this);
NEM_imgver_t* NEM_img_imgver_by_hash(
	NEM_imgset_t *set,
	NEM_img_t    *this,
	const char   *sha256hex
);

NEM_err_t NEM_imgset_add_img(
	NEM_imgset_t *this,
	NEM_img_t   **img
);
NEM_err_t NEM_imgset_add_ver(
	NEM_imgset_t  *this,
	NEM_imgver_t **ver,
	NEM_img_t     *img
);
