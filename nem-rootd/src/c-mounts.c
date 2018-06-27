#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>

#include "nem.h"
#include "c-mounts.h"
#include "c-log.h"
#include "c-disk.h"
#include "c-images.h"
#include "c-config.h"
#include "utils.h"

#define NEM_IOVEC_PAIR(key, val) \
	{ .iov_base = (void*)key, .iov_len = strlen(key) }, \
	{ .iov_base = (void*)val, .iov_len = strlen(val) }

typedef enum {
	NEM_MOUNT_UFS,
	NEM_MOUNT_NULLFS,
	NEM_MOUNT_DEVFS,
	NEM_MOUNT_UNKNOWN,
}
NEM_mount_type_t;

static const struct {
	NEM_mount_type_t type;
	const char      *name;
}
fstype_names[] = {
	{ NEM_MOUNT_UFS,     "ufs"     },
	{ NEM_MOUNT_NULLFS,  "nullfs"  },
	{ NEM_MOUNT_DEVFS,   "devfs"   },
	{ NEM_MOUNT_UNKNOWN, "unknown" },
};

static NEM_mount_type_t
NEM_mount_type_from_str(const char *str)
{
	for (size_t i = 0; i < NEM_ARRSIZE(fstype_names); i += 1) {
		if (!strcmp(fstype_names[i].name, str)) {
			return fstype_names[i].type;
		}
	}
	return NEM_MOUNT_UNKNOWN;
}

static const char*
NEM_mount_type_str(NEM_mount_type_t ty)
{
	for (size_t i = 0; i < NEM_ARRSIZE(fstype_names); i += 1) {
		if (ty == fstype_names[i].type) {
			return fstype_names[i].name;
		}
	}
	return "unknown";
}

struct NEM_mount_t {
	// NB: These have to be strictly ordered since they need to be
	// unmounted in the reverse order.
	LIST_ENTRY(NEM_mount_t) link;

	NEM_mount_type_t type;
	NEM_disk_t      *disk; // NB: empty for NULLFS mounts.
	char            *source;
	char            *dest;
	int              refcount;
	bool             owned;
	bool             ephemeral;
	bool             seen;
};

static int
NEM_mount_cmp(const NEM_mount_t *lhs, const NEM_mount_t *rhs)
{
	if (lhs->type != rhs->type) {
		return (lhs->type > rhs->type) ? 1 : -1;
	}

	int diff = strcmp(lhs->source, rhs->source);
	if (0 != diff) {
		return diff;
	}

	return strcmp(lhs->dest, rhs->dest);
}

static NEM_err_t
NEM_mount_new_ufs(
	NEM_mount_t **out,
	NEM_disk_t   *disk,
	const char   *dest,
	int           flags
) {
	char *device = NULL;
	asprintf(&device, "/dev/%s", NEM_disk_device(disk));

	struct iovec params[] = {
		NEM_IOVEC_PAIR("fstype", "ufs"),
		NEM_IOVEC_PAIR("fspath", device),
		NEM_IOVEC_PAIR("target", dest),
	};

	if (0 != nmount(params, NEM_ARRSIZE(params), flags)) {
		free(device);
		return NEM_err_errno();
	}

	NEM_mount_t *this = NEM_malloc(sizeof(*this));
	this->type = NEM_MOUNT_UFS;
	this->disk = disk;
	this->source = device;
	this->dest = strdup(dest);
	this->owned = true;
	*out = this;
	return NEM_err_none;
}

static NEM_err_t
NEM_mount_new_nullfs(
	NEM_mount_t **out,
	const char   *src,
	const char   *dest,
	int           flags
) {
	struct iovec params[] = {
		NEM_IOVEC_PAIR("fstype", "nullfs"),
		NEM_IOVEC_PAIR("fspath", src),
		NEM_IOVEC_PAIR("target", dest),
	};

	if (0 != nmount(params, NEM_ARRSIZE(params), flags)) {
		return NEM_err_errno();
	}

	NEM_mount_t *this = NEM_malloc(sizeof(*this));
	this->type = NEM_MOUNT_NULLFS;
	this->source = strdup(src);
	this->dest = strdup(dest);
	this->owned = true;
	*out = this;
	return NEM_err_none;
}

static void
NEM_mount_free(NEM_mount_t *this)
{
	if (this->owned) {
		if (0 != unmount(this->dest, 0)) {
			NEM_logf(
				COMP_MOUNTS,
				"umount %s failed: ",
				this->dest,
				NEM_err_string(NEM_err_errno())
			);
		}
	}

	if (NULL != this->disk) {
		NEM_disk_free(this->disk);
		this->disk = NULL;
	}

	if (this->ephemeral) {
		unlink(this->source);
	}

	free(this->source);
	free(this->dest);
}

// XXX: Might wrap this in a struct or something?
typedef LIST_HEAD(NEM_mountlist_t, NEM_mount_t) NEM_mountlist_t;

static NEM_mount_t*
NEM_mountlist_find(NEM_mountlist_t *this, const NEM_mount_t *dummy)
{
	NEM_mount_t *entry;
	LIST_FOREACH(entry, this, link) {
		if (0 == NEM_mount_cmp(entry, dummy)) {
			return entry;
		}
	}
	return NULL;
}

static void
NEM_mountlist_free(NEM_mountlist_t *this)
{
	NEM_mount_t *entry, *tmp;

	// NB: New entries are prependede to the list, so forward iteration
	// is newest -> oldest mounts.
	LIST_FOREACH_SAFE(entry, this, link, tmp) {
		NEM_mount_free(entry);
		LIST_REMOVE(entry, link);
		free(entry);
	}
	
	bzero(this, sizeof(*this));
}

static NEM_mountlist_t static_mounts = {0};
static NEM_kq_t *static_kq = NULL;

static NEM_err_t
NEM_mountlist_rescan(NEM_mountlist_t *this)
{
	NEM_mount_t *entry = NULL, *tmp = NULL;
	LIST_FOREACH(entry, this, link) {
		entry->seen = false;
	}

	int num_mounts = getfsstat(NULL, 0, MNT_WAIT);
	if (-1 == num_mounts) {
		return NEM_err_errno();
	}

	size_t len = (size_t) num_mounts * sizeof(struct statfs);
	struct statfs *mnts = NEM_malloc(len);
	num_mounts = getfsstat(mnts, len, MNT_WAIT);
	if (-1 == num_mounts) {
		free(mnts);
		return NEM_err_errno();
	}

	for (size_t i = 0; i < num_mounts; i += 1) {
		NEM_mount_type_t ty = NEM_mount_type_from_str(mnts[i].f_fstypename);

		NEM_mount_t dummy = {
			.type   = ty,
			.source = mnts[i].f_mntfromname,
			.dest   = mnts[i].f_mntonname,
		};
		NEM_mount_t *entry = NEM_mountlist_find(this, &dummy);
		if (NULL == entry) {
			entry = NEM_malloc(sizeof(NEM_mount_t));
			entry->type = ty;
			entry->source = strdup(mnts[i].f_mntfromname);
			entry->dest = strdup(mnts[i].f_mntonname);
			entry->owned = false;
			NEM_disk_init_device(&entry->disk, entry->source);
			LIST_INSERT_HEAD(this, entry, link);
		}

		entry->seen = true;
	}
	free(mnts);

	LIST_FOREACH_SAFE(entry, this, link, tmp) {
		// XXX: This is kind of a weird edge-case where we've still got
		// references to a filesystem but it's dead (e.g. removable media).
		// We need to keep the references around on our side but should
		// probably note that it's dead because weird stuff might happen?
		// How often are we bothering to rescan? I dunno.
		if (!entry->seen && 0 == entry->refcount) {
			NEM_mount_free(entry);
			LIST_REMOVE(entry, link);
			free(entry);
		}
	}

	return NEM_err_none;
}

typedef struct {
	NEM_child_t   child;
	NEM_thunk1_t *thunk;
}
NEM_mount_newfs_baton_t;

static void
NEM_mount_newfs_done(NEM_thunk1_t *thunk, void *varg)
{
	NEM_child_ca *ca = varg;
	NEM_mount_newfs_baton_t *baton = NEM_thunk1_inlineptr(thunk);
	NEM_err_t err = NEM_err_none;
	if (0 == ca->exitcode) {
		err = NEM_err_static("NEM_mount_newfs: newfs returned none-zero");
	}
	NEM_child_free(&baton->child);
	NEM_thunk1_invoke(&baton->thunk, &NEM_err_none);
	free(baton);
}

static void
NEM_mount_newfs_args(NEM_thunk1_t *thunk, void *varg)
{
	NEM_child_ca *ca = varg;
	NEM_disk_t *disk = NEM_thunk1_ptr(thunk);

	char **args = NEM_malloc(sizeof(char*) * 4);
	args[0] = strdup("/sbin/newfs");
	args[1] = strdup("-U");
	args[2] = strdup("-t");
	args[3] = strdup(NEM_disk_device(disk));
	ca->args = args;
}

static void
NEM_mount_newfs(NEM_disk_t *disk, NEM_thunk1_t *thunk)
{
	if (!NEM_rootd_is_root()) {
		NEM_err_t err = NEM_err_static("NEM_mount_newfs: need root");
		NEM_thunk1_invoke(&thunk, &err);
		return;
	}

	NEM_thunk1_t *run_cb = NEM_thunk1_new_ptr(&NEM_mount_newfs_args, disk);
	NEM_thunk1_t *done_cb = NEM_thunk1_new(
		&NEM_mount_newfs_done,
		sizeof(NEM_mount_newfs_baton_t)
	);
	NEM_mount_newfs_baton_t *baton = NEM_thunk1_inlineptr(done_cb);
	baton->thunk = thunk;

	NEM_err_t err = NEM_child_init(
		&baton->child,
		static_kq,
		"/sbin/newfs",
		run_cb
	);
	if (!NEM_err_ok(err)) {
		NEM_thunk1_discard(&run_cb);
		NEM_thunk1_discard(&done_cb);
		NEM_thunk1_invoke(&thunk, &err);
		return;
	}

	NEM_child_on_close(&baton->child, done_cb);
}

typedef struct {
	const NEM_jailimg_t *imgs;
	size_t               imgs_len;
	const char          *base;
	NEM_mountset_t      *set;
	NEM_thunk1_t        *cb;
}
NEM_mount_images_baton_t;

static void
NEM_mountset_append(NEM_mountset_t *this, NEM_mount_t *mount)
{
	this->mounts = NEM_panic_if_null(realloc(
		this->mounts,
		sizeof(NEM_mount_t*) * (this->mounts_len + 1)
	));
	this->mounts[this->mounts_len] = mount;
	this->mounts_len += 1;
}

static void
NEM_mount_images_done(NEM_mount_images_baton_t *baton, NEM_err_t err)
{
	if (!NEM_err_ok(err)) {
		// NB: Unmount in the reverse order.
		for (size_t i = 1; i <= baton->set->mounts_len; i += 1) {
			size_t idx = baton->set->mounts_len - i;
			NEM_mount_t *mount = baton->set->mounts[idx];
			NEM_mount_free(baton->set->mounts[idx]);
			if (mount->owned) {
				LIST_REMOVE(mount, link);
				free(mount);
			}
		}
		free(baton->set);

		NEM_mount_ca ca = {
			.err = err,
		};
		NEM_thunk1_invoke(&baton->cb, &ca);
		free(baton);
	}
	else {
		NEM_mount_ca ca = {
			.err = NEM_err_none,
			.set = baton->set,
		};
		NEM_thunk1_invoke(&baton->cb, &ca);
		free(baton);
	}
}

static void
NEM_mount_images_post_newfs(NEM_thunk1_t *thunk, void *varg)
{
	NEM_mount_images_baton_t *baton = NEM_thunk1_ptr(thunk);
	NEM_err_t *err = varg;
	if (!NEM_err_ok(*err)) {
		NEM_mount_images_done(baton, *err);
		return;
	}
}

static NEM_err_t
NEM_mount_images_singlefile(
	NEM_mount_images_baton_t *baton,
	const NEM_jailimg_t      *img
) {
	struct stat sb;
	if (0 != stat(img->name, &sb)) {
		return NEM_err_errno();
	}
	if (!S_ISREG(sb.st_mode)) {
		return NEM_err_static(
			"NEM_mount_images: singlefile only works with files"
		);
	}

	char *dst_path = NULL;
	NEM_path_join(&dst_path, baton->base, img->name);
	int fd_out = open(dst_path, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC);
	if (0 > fd_out) {
		free(dst_path);
		return NEM_err_errno();
	}

	int fd_in = open(img->name, O_RDONLY|O_CLOEXEC);
	if (0 > fd_in) {
		free(dst_path);
		close(fd_out);
		return NEM_err_errno();
	}

	size_t buf_len = 4096;
	char *buf = NEM_malloc(buf_len);
	ssize_t read_bs = 0;
	NEM_err_t err = NEM_err_none;

	for (;;) {
		read_bs = read(fd_in, buf, buf_len);
		if (0 > read_bs) {
			err = NEM_err_errno();
			goto done;
		}
		else if (0 == read_bs) {
			goto done;
		}

		for (size_t wrote_bs = 0; wrote_bs < read_bs;) {
			ssize_t wrote = write(fd_out, buf + wrote_bs, read_bs - wrote_bs);
			if (0 > wrote) {
				err = NEM_err_errno();
				goto done;
			}
			wrote_bs += wrote;
		}
	}

done:
	if (!NEM_err_ok(err)) {
		unlink(dst_path);
	}

	free(buf);
	free(dst_path);
	close(fd_in);
	close(fd_out);
	return err;
}

static NEM_err_t
NEM_mount_images_image(
	NEM_mount_images_baton_t *baton,
	const NEM_jailimg_t      *img
) {
	char *path = NULL;
	NEM_err_t err = NEM_rootd_find_image(img, &path);
	if (!NEM_err_ok(err)) {
		return err;
	}

	NEM_disk_t *disk = NULL;
	err = NEM_disk_init_file(&disk, path, true);
	if (!NEM_err_ok(err)) {
		return err;
	}

	char *dest = NULL;
	NEM_path_join(&dest, baton->base, img->dest);

	NEM_mount_t *mount = NULL;
	err = NEM_mount_new_ufs(&mount, disk, dest, MNT_RDONLY);
	if (!NEM_err_ok(err)) {
		NEM_disk_free(disk);
		free(dest);
		return err;
	}

	LIST_INSERT_HEAD(&static_mounts, mount, link);
	NEM_mountset_append(baton->set, mount);

	return NEM_err_none;
}

static void
NEM_mount_images_step(NEM_mount_images_baton_t *baton)
{
	if (0 == baton->imgs_len) {
		NEM_mount_images_done(baton, NEM_err_none);
		return;
	}

	const NEM_jailimg_t *img = &baton->imgs[0];
	NEM_err_t err;

	switch (NEM_jailimg_type(img)) {
		case NEM_IMG_SINGLEFILE:
			err = NEM_mount_images_singlefile(baton, img);
			if (!NEM_err_ok(err)) {
				return NEM_mount_images_done(baton, err);
			}
			baton->imgs += 1;
			baton->imgs_len -= 1;
			return NEM_mount_images_step(baton);

		case NEM_IMG_IMAGE:
			err = NEM_mount_images_image(baton, img);
			if (!NEM_err_ok(err)) {
				return NEM_mount_images_done(baton, err);
			}
			baton->imgs += 1;
			baton->imgs_len -= 1;
			return NEM_mount_images_step(baton);

		case NEM_IMG_VNODE:
			// NB: This is an async thing which is annoying; it potentially
			// needs to initialize a new vnode image.
			NEM_panic("TODO");
			break;

		case NEM_IMG_SHARED:
			NEM_panic("TODO");
			break;

		default:
			err = NEM_err_static("NEM_mount_images: invalid image type");
			return NEM_mount_images_done(baton, err);
	}
}

void
NEM_mount_images(
	const NEM_jailimg_t *imgs,
	size_t               imgs_len,
	const char          *base,
	NEM_thunk1_t        *thunk
) {
	// TODO
}

void
NEM_unmount_set(NEM_mountset_t *set)
{
	// TODO
}

static NEM_err_t
setup(NEM_app_t *app, int argc, char *argv[])
{
	if (!LIST_EMPTY(&static_mounts)) {
		NEM_panicf("mounts: corrupt static_mounts during setup");
	}

	NEM_logf(COMP_MOUNTS, "setup");
	static_kq = &app->kq;

	static const struct {
		const char *path;
		bool        empty;
	}
	dirs[] = {
		{ "mounts",           false },
		{ "mounts/tmp",       true  },
		{ "mounts/empty",     true  },
		{ "mounts/null",      false },
		{ "mounts/persisted", false },
	};
	char tmp[PATH_MAX] = {0};

	for (size_t i = 0; i < NEM_ARRSIZE(dirs); i += 1) {
		snprintf(tmp, sizeof(tmp), "%s/%s", NEM_rootd_run_root(), dirs[i].path);
		NEM_err_t err = NEM_ensure_dir(tmp);
		if (!NEM_err_ok(err)) {
			return err;
		}
		if (dirs[i].empty) {
			err = NEM_erase_dir(tmp);
			if (!NEM_err_ok(err)) {
				return err;
			}
		}
	}

	NEM_err_t err = NEM_mountlist_rescan(&static_mounts);
	if (!NEM_err_ok(err)) {
		NEM_mountlist_free(&static_mounts);
		return err;
	}

	NEM_mount_t *mount;
	LIST_FOREACH(mount, &static_mounts, link) {
		NEM_logf(
			COMP_MOUNTS,
			"  - %8.8s:%s %s via %s",
			NEM_mount_type_str(mount->type),
			mount->owned ? "" : " (foreign)",
			mount->dest,
			NEM_disk_device(mount->disk)
		);
	}

	return NEM_err_none;
}

static void
teardown(NEM_app_t *app)
{
	NEM_logf(COMP_MOUNTS, "teardown");
	NEM_mountlist_free(&static_mounts);
	static_kq = NULL;
}

const NEM_app_comp_t NEM_rootd_c_mounts = {
	.name     = "mounts",
	.setup    = &setup,
	.teardown = &teardown,
};
