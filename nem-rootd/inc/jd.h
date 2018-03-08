#pragma once
#include <sys/types.h>
#include <stdbool.h>
#include "nem.h"

// Jail-related structs are broken down into two sections:
// 
//  * jail descriptions (NEM_jd_*) contain the TOML-specified data that
//    defines how a jail is set up.
//  * jail instances (NEM_ji_*) are the spawned jail instances. A jail
//    description can have multiple instances. Each instance retains a
//    reference to the jail description and updates with the description.
//
// Both jail descriptions and jail instances are persisted locally.
//
// This file describes only the jd half.

// NEM_jd_mounttype_t defines the possible mount types. These don't map just
// to nmount calls -- they also deal with device creation and filesystem 
// initialization (in some cases).
typedef enum {
	// NEM_JD_MOUNTTYPE_IMAGE is a ro mount from a base file image. This is
	// useful for creating deployable application artifacts and for base
	// system images.
	NEM_JD_MOUNTTYPE_IMAGE,

	// NEM_JD_MOUNTTYPE_VNODE is a rw mount from a fixed-size file-backed
	// image. This can be persisted via means that I don't currently know.
	NEM_JD_MOUNTTYPE_VNODE,

	// NEM_JD_MOUNTTYPE_SHARED is a rw nullfs mount from a shared pool. The
	// idea is that there is a disk somewhere and instead of dealing with
	// partitions we just nullfs-mount directories into the jail r/w. 
	// This is easy, but (1) doesn't let us track per-jail disk usage and
	// (2) is a massive cluster to clean up. Because of this, all shares are
	// automatically persisted.
	NEM_JD_MOUNTTYPE_SHARED,

	//NEM_JD_MOUNTTYPE_PART,
}
NEM_jd_mounttype_t;

// NEM_jd_mount_t is the description of a single mount needed by a jail. It's
// a tagged union.
typedef struct {
	NEM_jd_mounttype_t type;

	// dest is the root within the jail where this image will be mounted.
	// This can be /-prefixed or not it doesn't matter.
	char *dest;

	union {
		struct {
			// image is the name of the image to use. It must already be
			// somewhere in the image repository (e.g. a directory). If
			// the sha256 is non-zero, the image must match the sha256.
			char *name;
			char  sha256[32];
		}
		image;

		struct {
			// len is the length of the filesystem to make. 
			size_t len;
			// persist, if set, keeps the filesystem around past closing time.
			// It will be re-used when the jail instance is restarted.
			bool   persist;
		}
		vnode;

		struct {
			// len is the expected maximum filesystem size. It isn't enforced.
			size_t len;
		}
		shared;
	};
}
NEM_jd_mount_t;

// NEM_jd_t is a jail description.
typedef struct {
	// name is the unique name for this jail. This jail description can be
	// instanciated each time; each instance will have a generated name.
	// Updates to the jail description will be applied to all instances when
	// they restart, so the name links the instance with the description.
	char *name;

	// num_mounts/mounts is an array that describes the mounts needed for
	// this jail.
	size_t          num_mounts;
	NEM_jd_mount_t *mounts;

	// exe_path is the path to the root executable for the jail. This should
	// speak NEM (e.g. using NEM_app_init) and will receive a NEM_chan_t to
	// the NEM-rootd process for signalling. When this process exits, the
	// jail is torn down.
	char *exe_path;
}
NEM_jd_t;

NEM_err_t NEM_jd_init_toml(NEM_jd_t *this, const char *buf, size_t len);
void NEM_jd_free(NEM_jd_t *this);
