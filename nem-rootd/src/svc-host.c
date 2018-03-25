#include "nem.h"
#include "svcmgr.h"
#include "lifecycle.h"
#include "state.h"

NEM_rootd_svcmgr_t NEM_rootd_svc_host = {};

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-svc-host: setup\n");
	}

	static const struct {
		uint16_t cmd_id;
		void(*fn)(NEM_thunk_t*, void*);
	}
	handlers[] = {
	};

	NEM_rootd_svcmgr_init(&NEM_rootd_svc_host);

	for (size_t i = 0; i < NEM_ARRSIZE(handlers); i += 1) {
		NEM_err_t err = NEM_rootd_svcmgr_add(
			&NEM_rootd_svc_host,
			NEM_svcid_host,
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
		printf("c-svc-host: teardown\n");
	}

	NEM_rootd_svcmgr_free(&NEM_rootd_svc_host);
}

const NEM_rootd_comp_t NEM_rootd_c_svc_host = {
	.setup    = &setup,
	.teardown = &teardown,
};

