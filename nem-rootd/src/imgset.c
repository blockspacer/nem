#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "nem.h"
#include "imgset.h"
#include "utils.h"

const char*
NEM_imgver_status_string(int status)
{
	static const struct {
		int status;
		const char *str;
	}
	table[] = {
		{ NEM_ROOTD_IMGV_OK,       "OKAY"     },
		{ NEM_ROOTD_IMGV_MOUNTED,  "MOUNTED"  },
		{ NEM_ROOTD_IMGV_BAD_HASH, "BAD HASH" },
		{ NEM_ROOTD_IMGV_BAD_SIZE, "BAD SIZE" },
		{ NEM_ROOTD_IMGV_MISSING,  "MISSING"  },
	};

	for (size_t i = 0; i < NEM_ARRSIZE(table); i += 1) {
		if (table[i].status == status) {
			return table[i].str;
		}
	}

	return "UNKNOWN";
}

static void
NEM_imgver_free(NEM_imgver_t *this)
{
	free(this->sha256);
	free(this->version);
	bzero(this, sizeof(*this));
}

static void
NEM_img_free(NEM_img_t *this)
{
	free(this->name);
	free(this->vers);
	bzero(this, sizeof(*this));
}

void
NEM_imgset_init(NEM_imgset_t *this)
{
	bzero(this, sizeof(*this));
}

void
NEM_imgset_free(NEM_imgset_t *this)
{
	for (size_t i = 0; i < this->imgs_len; i += 1) {
		NEM_img_free(&this->imgs[i]);
	}
	for (size_t i = 0; i < this->vers_len; i += 1) {
		NEM_imgver_free(&this->vers[i]);
	}
	free(this->vers);
	free(this->imgs);
}

NEM_img_t*
NEM_imgset_img_by_name(
	NEM_imgset_t *this,
	const char        *name
) {
	for (size_t i = 0; i < this->imgs_len; i += 1) {
		if (!strcmp(name, this->imgs[i].name)) {
			return &this->imgs[i];
		}
	}

	return NULL;
}

NEM_img_t*
NEM_imgset_img_by_id(NEM_imgset_t *this, int id)
{
	for (size_t i = 0; i < this->imgs_len; i += 1) {
		if (this->imgs[i].id == id) {
			return &this->imgs[i];
		}
	}

	return NULL;
}

NEM_imgver_t*
NEM_imgset_imgver_by_hash(
	NEM_imgset_t *this,
	const char        *sha256hex
) {
	for (size_t i = 0; i < this->vers_len; i += 1) {
		if (!strcmp(this->vers[i].sha256, sha256hex)) {
			return &this->vers[i];
		}
	}

	return NULL;
}

NEM_imgver_t*
NEM_imgset_imgver_by_id(NEM_imgset_t *this, int id)
{
	for (size_t i = 0; i < this->vers_len; i += 1) {
		if (this->vers[i].id == id) {
			return &this->vers[i];
		}
	}

	return NULL;
}

NEM_imgver_t*
NEM_img_imgver_latest(NEM_imgset_t *set, NEM_img_t *this)
{
	if (0 == this->vers_len) {
		return NULL;
	}

	NEM_imgver_t *latest = NEM_imgset_imgver_by_id(set, this->vers[0]);
	for (size_t i = 1; i < this->vers_len; i += 1) {
		NEM_imgver_t *test = NEM_imgset_imgver_by_id(set, this->vers[i]);
		if (0 < NEM_tm_cmp(&latest->created, &test->created)) {
			latest = test;
		}
	}

	return latest;
}

NEM_imgver_t*
NEM_img_imgver_by_semver(
	NEM_imgset_t *set,
	NEM_img_t    *this,
	const char   *require
) {
	NEM_semver_t best_sem;
	NEM_imgver_t *best_ver = NULL;
	NEM_semver_match_t match;

	NEM_err_t err = NEM_semver_init_match(&best_sem, &match, require);
	if (!NEM_err_ok(err)) {
		return NULL;
	}

	for (size_t i = 0; i < this->vers_len; i += 1) {
		NEM_imgver_t *test = &set->vers[this->vers[i]];
		NEM_semver_t test_sem;
		if (!NEM_err_ok(NEM_semver_init(&test_sem, test->version))) {
			// XXX: Should probably log this out somewhere.
			continue;
		}
		
		if (0 <= NEM_semver_cmp(&best_sem, &test_sem, match)) {
			best_ver = test;
		}
	}

	return best_ver;
}

NEM_imgver_t*
NEM_img_imgver_by_hash(
	NEM_imgset_t *set,
	NEM_img_t    *this,
	const char   *sha256hex
) {
	for (size_t i = 0; i < this->vers_len; i += 1) {
		NEM_imgver_t *ver = NEM_imgset_imgver_by_id(set, this->vers[i]);
		if (!strcmp(ver->sha256, sha256hex)) {
			return ver;
		}
	}

	return NULL;
}

NEM_err_t
NEM_imgset_add_img(
	NEM_imgset_t *this,
	NEM_img_t   **img
) {
	if ((*img)->id == 0) {
		NEM_img_free(*img);
		return NEM_err_static("NEM_imgset_add_img: invalid id");
	}
	if ((*img)->name == NULL || (*img)->name[0] == 0) {
		NEM_img_free(*img);
		return NEM_err_static("NEM_imgset_add_img: invalid name");
	}

	for (size_t i = 0; i < this->imgs_len; i += 1) {
		NEM_img_t *tmp = &this->imgs[i];
		if (*img == tmp) {
			return NEM_err_none;
		}
		if (!strcmp((*img)->name, tmp->name)) {
			bool valid = (*img)->id == tmp->id;
			NEM_img_free(*img);
			if (!valid) {
				return NEM_err_static(
					"NEM_imgset_add_img: dupe name, diff id"
				);
			}
			*img = tmp;
			return NEM_err_none;
		}
		if ((*img)->id == tmp->id) {
			bool valid = 0 == strcmp((*img)->name, tmp->name);
			NEM_img_free(*img);
			if (!valid) {
				return NEM_err_static(
					"NEM_imgset_add_img: dupe id, diff name"
				);
			}
			*img = tmp;
			return NEM_err_none;
		}
	}

	if (this->imgs_len >= this->imgs_cap) {
		this->imgs_cap = this->imgs_cap ? this->imgs_cap * 2 : 8;
		this->imgs = NEM_panic_if_null(realloc(
			this->imgs,
			this->imgs_cap * sizeof(this->imgs[0])
		));
	}

	memcpy(&this->imgs[this->imgs_len], *img, sizeof(**img));
	bzero(*img, sizeof(**img));
	*img = &this->imgs[this->imgs_len];
	this->imgs_len += 1;
	return NEM_err_none;
}

NEM_err_t
NEM_imgset_add_ver(
	NEM_imgset_t *this,
	NEM_imgver_t **ver,
	NEM_img_t    *image
) {
	if ((*ver)->id == 0) {
		NEM_imgver_free(*ver);
		return NEM_err_static("NEM_imgset_add_ver: invalid id");
	}
	if ((*ver)->sha256 == NULL || strlen((*ver)->sha256) != 64) {
		NEM_imgver_free(*ver);
		return NEM_err_static("NEM_imgset_add_ver: invalid hash");
	}
	if ((*ver)->version == NULL || (*ver)->version[0] == 0) {
		NEM_imgver_free(*ver);
		return NEM_err_static("NEM_imgset_add_ver: invalid version");
	}
	for (size_t i = 0; i < strlen((*ver)->sha256); i += 1) {
		char ch = (*ver)->sha256[i];
		if (ch >= '0' && ch <= '9') {
			continue;
		}
		if (ch >= 'a' && ch <= 'f') {
			continue;
		}
		NEM_imgver_free(*ver);
		return NEM_err_static("NEM_imgset_add_ver: invalid hex hash");
	}

	for (size_t i = 0; i < this->vers_len; i += 1) {
		NEM_imgver_t *tmp = &this->vers[i];
		if (*ver == tmp) {
			goto link;
		}

		if (!strcmp((*ver)->sha256, tmp->sha256)) {
			goto dupe;
		}
		if ((*ver)->id == tmp->id) {
			goto dupe;
		}
		// NB: Don't use the 'version' field as a unique identifier, but 
		// enforce that it can't be changed.
		continue;
	dupe:
		if ((*ver)->id != tmp->id) {
			NEM_imgver_free(*ver);
			return NEM_err_static("NEM_imgset_add_ver: dupe, mismatch id");
		}
		if (strcmp((*ver)->sha256, tmp->sha256)) {
			NEM_imgver_free(*ver);
			return NEM_err_static("NEM_imgset_add_ver: dupe, mismatch hash");
		}
		if (strcmp((*ver)->version, tmp->version)) {
			NEM_imgver_free(*ver);
			return NEM_err_static("NEM_imgset_add_ver: dupe, mismatch version");
		}
		NEM_imgver_free(*ver);
		*ver = tmp;
		goto link;
	}

	if (this->vers_len >= this->vers_cap) {
		this->vers_cap = this->vers_cap ? this->vers_cap * 2 : 8;
		this->vers = NEM_panic_if_null(realloc(
			this->vers,
			this->vers_cap * sizeof(this->vers[0])
		));
	}

	memcpy(&this->vers[this->vers_len], *ver, sizeof(NEM_imgver_t));
	bzero(*ver, sizeof(NEM_imgver_t));
	*ver = &this->vers[this->vers_len];
	this->vers_len += 1;

link:
	if (NULL != image) {
		if (image->vers_len >= image->vers_cap) {
			image->vers_cap = image->vers_cap ? image->vers_cap * 2 : 8;
			image->vers = NEM_panic_if_null(realloc(
				image->vers,
				image->vers_cap * sizeof(image->vers[0])
			));
		}

		image->vers[image->vers_len] = (*ver)->id;
		image->vers_len += 1;
	}

	return NEM_err_none;
}
