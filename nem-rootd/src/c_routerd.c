#include "nem.h"
#include "state.h"
#include "lifecycle.h"
#include "c_routerd.h"

static NEM_child_t child;
static bool        is_running = false;

static void
routerd_restart(NEM_thunk1_t *thunk, void *varg)
{
	// XXX: Really want some retry semantics here.
	NEM_rootd_shutdown();
}

static NEM_err_t
setup(NEM_app_t *app)
{
	if (NEM_rootd_verbose()) {
		printf("c-routerd: setup\n");
	}

	NEM_err_t err = NEM_child_init(
		&child,
		app->kq,
		NEM_rootd_routerd_path(),
		// XXX: Might want to bind stdout/stderr to something
		// so that we can track output. As-is this leaves them bound
		// to our stdout/stderr which is probably fine.
		NULL
	);
	if (NEM_err_ok(err)) {
		is_running = true;

		err = NEM_child_on_close(
			&child,
			NEM_thunk1_new_ptr(
				routerd_restart,
				app
			)
		);
	}

	if (NEM_rootd_verbose()) {
		printf("c-routerd: routerd running, pid=%d\n", child.pid); 
	}

	return err;
}

static bool
try_shutdown()
{
	printf("c-routerd: try-shutdown\n");
	return true;
}

static void
teardown()
{
	printf("c-routerd: teardown\n");
	if (is_running) {
		if (NEM_rootd_verbose()) {
			printf("c-routerd: killing child\n");
		}
		NEM_child_free(&child);
		is_running = false;
	}
}

const NEM_rootd_comp_t NEM_rootd_c_routerd = {
	.setup        = &setup,
	.try_shutdown = &try_shutdown,
	.teardown     = &teardown,
};

