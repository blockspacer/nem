#include <sys/types.h>

#include "nem.h"
#include "lifecycle.h"
#include "state.h"

static NEM_app_t app;
static bool      running;

typedef struct {
	const NEM_rootd_comp_t *comp;
	bool                    running;
}
compstate_t;

static compstate_t comps[10];
static size_t      num_comps = 0;
static size_t      num_comps_running = 0;

void
NEM_rootd_add_comp(const NEM_rootd_comp_t *comp)
{
	if (num_comps == NEM_ARRSIZE(comps)) {
		NEM_panicf("NEM_rootd_add_comp: too many components");
	}
	if (app.running) {
		NEM_panicf("NEM_rootd_add_comp: already running");
	}

	comps[num_comps].comp = comp;
	comps[num_comps].running = false;
	num_comps += 1;
}

NEM_err_t
NEM_rootd_main(int argc, char *argv[])
{
	NEM_err_t err = NEM_rootd_state_init(argc, argv);
	if (!NEM_err_ok(err)) {
		return err;
	}

	NEM_panic_if_err(NEM_app_init_root(&app));

	for (size_t i = 0; i < num_comps; i += 1) {
		err = comps[i].comp->setup(&app);
		// XXX: We really should cleanup here.
		if (!NEM_err_ok(err)) {
			return err;
		}
		comps[i].running = true;
		num_comps_running += 1;
	}

	running = true;

	NEM_panic_if_err(NEM_app_run(&app));
	NEM_app_free(&app);

	for (size_t i = 1; i <= num_comps; i += 1) {
		size_t j = num_comps - i;
		if (NULL != comps[j].comp->teardown) {
			comps[j].comp->teardown();
		}
	}

	NEM_rootd_state_close();
	return NEM_err_none;
}

static void
NEM_rootd_shutdown_step(NEM_thunk1_t *thunk, void *varg)
{
	for (size_t i = 0; i < num_comps; i += 1) {
		if (!comps[i].running) {
			continue;
		}

		if (NULL == comps[i].comp->try_shutdown) {
			comps[i].running = false;
			num_comps_running -= 1;
		}
		else if (comps[i].comp->try_shutdown()) {
			comps[i].running = false;
			num_comps_running -= 1;
		}
	}
	if (0 != num_comps_running) {
		NEM_app_after(&app, 100, NEM_thunk1_new(&NEM_rootd_shutdown_step, 0));
	}
	else {
		NEM_app_stop(&app);
	}
}

static void
NEM_rootd_shutdown_now(NEM_thunk1_t *thunk, void *varg)
{
	NEM_app_stop(&app);
}

void
NEM_rootd_shutdown()
{
	if (!running) {
		return;
	}

	running = false;
	NEM_rootd_shutdown_step(NULL, NULL);

	NEM_app_after(
		&app,
		5000,
		NEM_thunk1_new_ptr(
			&NEM_rootd_shutdown_now,
			NULL
		)
	);
}
