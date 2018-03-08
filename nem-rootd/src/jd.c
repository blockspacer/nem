#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <toml2.h>

#include "jd.h"

static char*
get_string(toml2_t *doc, const char *path)
{
	toml2_t *node = toml2_get_path(doc, path);
	if (NULL == node) {
		return NULL;
	}

	return strdup(toml2_string(node));
}

static size_t
get_size(toml2_t *doc, const char *path)
{
	char *str = get_string(doc, path);
	if (NULL == str) {
		return 0;
	}

	size_t len = strlen(str);
	char mag = str[len - 1];
	str[len - 1] = 0;

	long long lval = strtoll(str, NULL, 10);
	free(str);
	if (lval <= 0) {
		return 0;
	}
	if (lval > SIZE_MAX / (1024 * 1024 * 1024)) {
		return 0;
	}

	size_t val = lval;

	switch (mag) {
		case 'g':
		case 'G':
			val *= 1024;
			// fallthrough
		case 'm':
		case 'M':
			val *= 1024;
			// fallthrough
		case 'k':
		case 'K':
			val *= 1024;
			// fallthrough
		case 'b':
		case 'B':
			break;
		default:
			return 0;
	}

	return val;
}

static NEM_err_t
NEM_jd_parse_mount(NEM_jd_mount_t *this, toml2_t *doc)
{
	const char *type = toml2_string(toml2_get(doc, "type"));
	if (NULL == type) {
		return NEM_err_static("NEM_jd_parse_mount: missing type");
	}

	this->dest = get_string(doc, "dest");
	if (NULL == this->dest) {
		return NEM_err_static("NEM_jd_parse_mount: missing dest");
	}

	if (!strcmp(type, "image")) {
		this->type = NEM_JD_MOUNTTYPE_IMAGE;

		const char *sha256 = toml2_string(toml2_get(doc, "sha256"));
		if (NULL != sha256) {
			// XXX: Convert from hex to binary here.
		}

		this->image.name = get_string(doc, "image");
		if (NULL == this->image.name) {
			return NEM_err_static("NEM_jd_parse_mount: image without name");
		}
	}
	else if (!strcmp(type, "vnode")) {
		this->type = NEM_JD_MOUNTTYPE_VNODE;
		this->vnode.len = get_size(doc, "capacity");
		if (0 == this->vnode.len) {
			return NEM_err_static("NEM_jd_parse_mount: missing or 0 capacity");
		}
		this->vnode.persist = toml2_bool(toml2_get(doc, "persist"));
	}
	else if (!strcmp(type, "shared")) {
		this->type = NEM_JD_MOUNTTYPE_SHARED;
		this->shared.len = get_size(doc, "capacity");
		if (0 == this->shared.len) {
			return NEM_err_static("NEM_jd_parse_mount: missing or 0 capacity");
		}
	}
	else {
		return NEM_err_static("NEM_jd_parse_mount: invalid mount type");
	}

	return NEM_err_none;
}

NEM_err_t
NEM_jd_init_toml(NEM_jd_t *this, const char *buf, size_t len)
{
	bzero(this, sizeof(*this));
	NEM_err_t err = NEM_err_none;

	toml2_t doc;
	toml2_init(&doc);

	int terr = toml2_parse(&doc, buf, len);
	if (0 != terr) {
		err = NEM_err_static("NEM_jd_init_toml: failed to parse toml");
		goto done;
	}

	this->name = get_string(&doc, "name");
	if (NULL == this->name) {
		err = NEM_err_static("NEM_jd_init_toml: missing name");
		goto done;
	}

	this->exe_path = get_string(&doc, "exe_path");
	if (NULL == this->exe_path) {
		err = NEM_err_static("NEM_jd_init_toml: missing exe_path");
		goto done;
	}

	toml2_t *mounts = toml2_get(&doc, "mount");
	if (NULL == mounts || TOML2_LIST != toml2_type(mounts)) {
		err = NEM_err_static("NEM_jd_init_toml: mounts missing or not a list");
	}

	this->num_mounts = toml2_len(mounts);
	this->mounts = NEM_malloc(sizeof(NEM_jd_mount_t) * this->num_mounts);
	for (size_t i = 0; i < this->num_mounts; i += 1) {
		toml2_t *mount = toml2_index(mounts, i);
		err = NEM_jd_parse_mount(&this->mounts[i], mount);
		if (!NEM_err_ok(err)) {
			goto done;
		}
	}

done:
	toml2_free(&doc);

	if (!NEM_err_ok(err)) {
		NEM_jd_free(this);
	}

	return err;
}

void
NEM_jd_free(NEM_jd_t *this)
{
	free(this->name);
	free(this->exe_path);

	for (size_t i = 0; i < this->num_mounts; i += 1) {
		NEM_jd_mount_t *mount = &this->mounts[i];
		if (NEM_JD_MOUNTTYPE_IMAGE == mount->type) {
			free(mount->image.name);
		}
		free(mount->dest);
	}

	free(this->mounts);
}
