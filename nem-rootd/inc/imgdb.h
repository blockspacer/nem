#pragma once
#include <sys/types.h>
#include <time.h>

typedef struct {
	int       id;
	int       image_id;
	struct tm created;
	int       size;
	char     *sha256;
	char     *version;
}
NEM_rootd_imgv_t;

typedef struct {
	int   id;
	char *name;

	NEM_rootd_imgv_t *versions;
	size_t            versions_len;
	size_t            versions_cap;
}
NEM_rootd_img_t;

typedef struct {
	NEM_rootd_img_t *imgs;
	size_t           imgs_len;
	size_t           imgs_cap;
}
NEM_rootd_imgdb_t;

void NEM_rootd_imgdb_init(NEM_rootd_imgdb_t *this);
void NEM_rootd_imgdb_free(NEM_rootd_imgdb_t *this);

NEM_rootd_img_t* NEM_rootd_imgdb_find_img(
	NEM_rootd_imgdb_t *this,
	const char        *name
);

NEM_rootd_img_t* NEM_rootd_imgdb_add_img(NEM_rootd_imgdb_t *this);
NEM_rootd_imgv_t* NEM_rootd_img_add_version(NEM_rootd_img_t *this);
