#include "nem.h"
#include "nemsvc.h"
#include "lifecycle.h"
#include "state.h"
#include "svcdef.h"

NEM_rootd_svcdef_t NEM_rootd_svc_imghost;

static void
svc_imghost_list_images(NEM_thunk_t *thunk, void *varg)
{
}

static void
svc_imghost_list_versions(NEM_thunk_t *thunk, void *varg)
{
}

static void
svc_imghost_add_image(NEM_thunk_t *thunk, void *varg)
{
}

static void
svc_imghost_get_image(NEM_thunk_t *thunk, void *varg)
{
}

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("svc-imghost: setup\n");
	}

	static const struct {
		uint16_t cmd_id;
		void(*fn)(NEM_thunk_t*, void*);
	}
	handlers[] = {
		{ NEM_cmdid_imghost_list_images,   &svc_imghost_list_images   },
		{ NEM_cmdid_imghost_list_versions, &svc_imghost_list_versions },
		{ NEM_cmdid_imghost_add_image,     &svc_imghost_add_image     },
		{ NEM_cmdid_imghost_get_image,     &svc_imghost_get_image     },
	};

	NEM_rootd_svcdef_init(&NEM_rootd_svc_imghost);

	for (size_t i = 0; i < NEM_ARRSIZE(handlers); i += 1) {
		NEM_err_t err = NEM_rootd_svcdef_add(
			&NEM_rootd_svc_imghost,
			NEM_svcid_imghost,
			handlers[i].cmd_id,
			NEM_thunk_new(handlers[i].fn, 0)
		);
		if (!NEM_err_ok(err)) {
			return err;
		}
	}

	return NEM_err_none;
}

static void
teardown()
{
	if (NEM_rootd_verbose()) {
		printf("svc-imghost: teardown\n");
	}

	NEM_rootd_svcdef_free(&NEM_rootd_svc_imghost);
}

const NEM_rootd_comp_t NEM_rootd_c_svc_imghost = {
	.name     = "svc-imghost",
	.setup    = &setup,
	.teardown = &teardown,
};
