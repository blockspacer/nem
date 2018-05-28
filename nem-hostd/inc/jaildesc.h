#pragma once

typedef enum {
	NEM_HOSTD_ISOLATE_JAIL     = 1,
	NEM_HOSTD_ISOLATE_CAPSICUM = 2,
	NEM_HOSTD_ISOLATE_NETWORK  = 4,
}
NEM_hostd_jailimg_isolate_t;

typedef enum {
	NEM_HOSTD_IMG_INVALID    = 0,
	NEM_HOSTD_IMG_SINGLEFILE = 1,
	NEM_HOSTD_IMG_IMAGE      = 2,
	NEM_HOSTD_IMG_VNODE      = 3,
	NEM_HOSTD_IMG_SHARED     = 4,
}
NEM_hostd_jailimg_type_t;

typedef struct {
	// type is the string representation of the NEM_hostd_jailimg_type_t.
	const char *type;

	// dest is the destination mountpoint of the image.
	const char *dest;

	// name is the symbolic name of the image. This references the name in the
	// attached image database. The name may be empty for vnode-type images 
	// with the persist flag unset.
	const char *name;

	// semver restricts the image used to the latest one matching the given
	// semver. This may be null to just use the latest.
	const char *semver;

	// sha256, if provided, limits the image to one whose hash matches what's
	// given. This stacks with the semver restriction.
	const char *sha256;

	// len is the size of the image, in bytes. This is only used for vnode
	// type images which are created (and destroyed) on-demand.
	uint64_t len;

	// persist is for vnode images; if set, the vnode image is kept around and
	// re-used (name must be set in that case).
	bool persist;
}
NEM_hostd_jailimg_t;
const NEM_marshal_map_t NEM_hostd_jailimg_m;

NEM_err_t NEM_hostd_jailimg_valid(const NEM_hostd_jailimg_t *);
NEM_hostd_jailimg_type_t NEM_hostd_jailimg_type(const NEM_hostd_jailimg_t *);

// NEM_hostd_jaildesc_t is a jail description.
typedef struct {
	const char          *jail_id;
	bool                 want_running;
	const char         **isolation_flags;
	size_t               isolation_flags_len;
	const char          *exe_path;
	NEM_hostd_jailimg_t *images;
	size_t               images_len;
}
NEM_hostd_jaildesc_t;
const NEM_marshal_map_t NEM_hostd_jaildesc_m;

bool NEM_hostd_jaildesc_valid(const NEM_hostd_jaildesc_t *);

// NEM_hostd_jaildesc_use_isolate returns true if the given isolation flag
// is set in the jail description. These panic if the jaildesc is not valid.
bool NEM_hostd_jaildesc_use_isolate(
	const NEM_hostd_jaildesc_t*,
	NEM_hostd_jailimg_isolate_t
);
int NEM_hostd_jaildesc_isolate(const NEM_hostd_jaildesc_t *);
