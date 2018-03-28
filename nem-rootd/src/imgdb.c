#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "nem.h"
#include "imgdb.h"

static void
NEM_rootd_imgv_free(NEM_rootd_imgv_t *this)
{
	free(this->sha256);
	free(this->version);
}

static void
NEM_rootd_img_free(NEM_rootd_img_t *this)
{
	for (size_t i = 0; i < this->versions_len; i += 1) {
		NEM_rootd_imgv_free(&this->versions[i]);
	}

	free(this->versions);
	free(this->name);
}

void
NEM_rootd_imgdb_init(NEM_rootd_imgdb_t *this)
{
	bzero(this, sizeof(*this));
}

void
NEM_rootd_imgdb_free(NEM_rootd_imgdb_t *this)
{
	for (size_t i = 0; i < this->imgs_len; i += 1) {
		NEM_rootd_img_free(&this->imgs[i]);
	}

	free(this->imgs);
}

NEM_rootd_img_t*
NEM_rootd_imgdb_find_img(
	NEM_rootd_imgdb_t *this,
	const char        *name
) {
	for (size_t i = 0; i < this->imgs_len; i += 1) {
		if (!strcmp(name, this->imgs[i].name)) {
			return &this->imgs[i];
		}
	}

	return NULL;
}

NEM_rootd_imgv_t*
NEM_rootd_imgdb_find_imgv(
	NEM_rootd_imgdb_t *this,
	const char        *sha256hex
) {
	for (size_t i = 0; i < this->imgs_len; i += 1) {
		NEM_rootd_img_t *img = &this->imgs[i];

		for (size_t j = 0; j < img->versions_len; j += 1) {
			NEM_rootd_imgv_t *imgv = &img->versions[j];
			if (!strcmp(imgv->sha256, sha256hex)) {
				return imgv;
			}
		}
	}

	return NULL;
}

NEM_rootd_img_t*
NEM_rootd_imgdb_add_img(NEM_rootd_imgdb_t *this)
{
	if (this->imgs_cap <= this->imgs_len) {
		this->imgs_cap = this->imgs_cap ? this->imgs_cap * 2 : 8;
		this->imgs = NEM_panic_if_null(realloc(
			this->imgs,
			sizeof(NEM_rootd_img_t) * this->imgs_cap
		));
	}

	NEM_rootd_img_t *img = &this->imgs[this->imgs_len];
	bzero(img, sizeof(*img));
	this->imgs_len += 1;
	return img;
}

NEM_rootd_imgv_t*
NEM_rootd_img_add_version(NEM_rootd_img_t *this)
{
	if (this->versions_cap <= this->versions_len) {
		this->versions_cap = this->versions_cap ? this->versions_cap * 2 : 8;
		this->versions = NEM_panic_if_null(realloc(
			this->versions,
			sizeof(NEM_rootd_imgv_t) * this->versions_cap
		));
	}

	NEM_rootd_imgv_t *ver = &this->versions[this->versions_len];
	bzero(ver, sizeof(*ver));
	this->versions_len += 1;
	return ver;
}
