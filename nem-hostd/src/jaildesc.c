#include "nem.h"
#include "nem-marshal-macros.h"
#include "jaildesc.h"

static const int default_isolate =
	NEM_HOSTD_ISOLATE_JAIL
	| NEM_HOSTD_ISOLATE_NETWORK;

static const struct {
	NEM_hostd_jailimg_isolate_t iso;
	const char *name;
}
jailimg_isolate_names[] = {
	{ NEM_HOSTD_ISOLATE_JAIL,     "jail"     },
	{ NEM_HOSTD_ISOLATE_CAPSICUM, "capsicum" },
	{ NEM_HOSTD_ISOLATE_NETWORK,  "network"  },
};

static const NEM_hostd_jailimg_isolate_t
jailimg_isolate_from_name(const char *name)
{
	for (size_t i = 0; i < NEM_ARRSIZE(jailimg_isolate_names); i += 1) {
		if (!strcmp(jailimg_isolate_names[i].name, name)) {
			return jailimg_isolate_names[i].iso;
		}
	}

	return 0;
}

static const struct {
	NEM_hostd_jailimg_isolate_t iso;
	const char *noname;
}
jailimg_isolate_nonames[] = {
	{ NEM_HOSTD_ISOLATE_JAIL,     "no_jail"        },
	{ NEM_HOSTD_ISOLATE_CAPSICUM, "no_capsicum"    },
	{ NEM_HOSTD_ISOLATE_NETWORK,  "expose_network" },
};

static const NEM_hostd_jailimg_isolate_t
jailimg_isolate_from_noname(const char *noname)
{
	for (size_t i = 0; i < NEM_ARRSIZE(jailimg_isolate_nonames); i += 1) {
		if (!strcmp(jailimg_isolate_nonames[i].noname, noname)) {
			return jailimg_isolate_nonames[i].iso;
		}
	}

	return 0;
}

bool
jailimg_isolate_valid(const char *name)
{
	return
		jailimg_isolate_from_name(name) != 0
		|| jailimg_isolate_from_noname(name) != 0;
}

static const struct {
	NEM_hostd_jailimg_type_t type;
	const char *name;
}
jailimg_types[] = {
	{ NEM_HOSTD_IMG_INVALID,    "invalid" },
	{ NEM_HOSTD_IMG_SINGLEFILE, "file"    },
	{ NEM_HOSTD_IMG_IMAGE,      "image"   },
	{ NEM_HOSTD_IMG_VNODE,      "vnode"   },
	{ NEM_HOSTD_IMG_SHARED,     "shared"  },
};

static NEM_hostd_jailimg_type_t
jailimg_type_from_name(const char *name)
{
	for (size_t i = 0; i < NEM_ARRSIZE(jailimg_types); i += 1) {
		if (!strcmp(jailimg_types[i].name, name)) {
			return jailimg_types[i].type;
		}
	}

	return NEM_HOSTD_IMG_INVALID;
}

#define TYPE NEM_hostd_jailimg_t
static const NEM_marshal_field_t NEM_hostd_jailimg_fs[] = {
	{ "type",    NEM_MARSHAL_STRING, O(type),     -1, NULL },
	{ "dest",    NEM_MARSHAL_STRING, O(dest),     -1, NULL },
	{ "name",    NEM_MARSHAL_STRING, O(name),     -1, NULL },
	{ "semver",  NEM_MARSHAL_STRING, O(semver),   -1, NULL },
	{ "sha256",  NEM_MARSHAL_STRING, O(sha256),   -1, NULL },
	{ "len",     NEM_MARSHAL_UINT64, O(len),      -1, NULL },
	{ "persist", NEM_MARSHAL_BOOL,   O(persist),  -1, NULL },	
};
MAP(NEM_hostd_jailimg_m, NEM_hostd_jailimg_fs);
#undef TYPE

NEM_err_t
NEM_hostd_jailimg_valid(const NEM_hostd_jailimg_t *this)
{
	NEM_hostd_jailimg_type_t type = jailimg_type_from_name(this->type);

	if (NEM_HOSTD_IMG_INVALID == type) {
		return NEM_err_static("NEM_hostd_jailimg_valid: invalid type");
	}
	if (NULL == this->dest || 0 == this->dest[0]) {
		return NEM_err_static("NEM_hostd_jailimg_valid: missing or empty dest");
	}
	if ('/' != this->dest[0]) {
		return NEM_err_static("NEM_hostd_jailimg_valid: doesn't start with a /");
	}

	if (NULL == this->name || 0 == this->name[0]) {
		if (NEM_HOSTD_IMG_VNODE == type) {
			if (this->persist) {
				return NEM_err_static(
					"NEM_hostd_jailimg_valid: persisted vnode must be named"
				);
			}
			// okay.
		}
		else {
			return NEM_err_static(
				"NEM_hostd_jailimg_valid: non-vnode image must be named"
			);
		}
	}

	if (NEM_HOSTD_IMG_VNODE == type) {
		if (0 == this->len) {
			return NEM_err_static(
				"NEM_hostd_jailimg_valid: non-vnode image must have non-zero "
			    "size"
			);
		}
	}

	if (NEM_HOSTD_IMG_SHARED == type || NEM_HOSTD_IMG_VNODE == type) {
		if (NULL != this->semver || NULL != this->sha256) {
			return NEM_err_static(
				"NEM_hostd_jailimg_valid: vnode/shared images cannot be "
				"versioned"
			);
		}
	}

	return NEM_err_none;
}

NEM_hostd_jailimg_type_t 
NEM_hostd_jailimg_type(const NEM_hostd_jailimg_t *this)
{
	return jailimg_type_from_name(this->type);
}

#define TYPE NEM_hostd_jaildesc_t
static const NEM_marshal_field_t  NEM_hostd_jaildesc_fs[] = {
	{ "jail_id",      NEM_MARSHAL_STRING, O(jail_id),      -1, NULL },
	{ "want_running", NEM_MARSHAL_BOOL,   O(want_running), -1, NULL },
	{ "exe_path",     NEM_MARSHAL_STRING, O(exe_path),     -1, NULL },
	{
		"isolation_flags",
		NEM_MARSHAL_STRING|NEM_MARSHAL_ARRAY,
		O(isolation_flags), O(isolation_flags_len),
	},
	{
		"images",
		NEM_MARSHAL_STRUCT|NEM_MARSHAL_ARRAY,
		O(images), O(images_len),
		&NEM_hostd_jailimg_m,
	},
};
MAP(NEM_hostd_jaildesc_m, NEM_hostd_jaildesc_fs);
#undef TYPE

bool
NEM_hostd_jaildesc_use_isolate(
	const NEM_hostd_jaildesc_t *this,
	NEM_hostd_jailimg_isolate_t iso
) {
	int this_iso = NEM_hostd_jaildesc_isolate(this);
	return this_iso & iso;
}

int
NEM_hostd_jaildesc_isolate(const NEM_hostd_jaildesc_t *this)
{
	int isolate = default_isolate;

	for (size_t i = 0; i < this->isolation_flags_len; i += 1) {
		const char *flag = this->isolation_flags[i];

		int tmp = jailimg_isolate_from_name(flag);
		if (tmp) {
			isolate |= tmp;
			continue;
		}

		tmp = jailimg_isolate_from_noname(flag);
		if (tmp) {
			isolate &= ~tmp;
			continue;
		}

		NEM_panicf("NEM_hostd_jaildesc_isolate: invalid flag %s", flag);
	}

	return isolate;
}
