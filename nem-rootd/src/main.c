#include "nem.h"
#include "state.h"

static void
do_stuff(NEM_thunk1_t *thunk, void *varg)
{
	NEM_app_t *app = NEM_thunk1_ptr(thunk);
	printf("did stuff\n");
	NEM_app_stop(app);
}

int
main(int argc, char *argv[])
{
	if (NEM_rootd_state_init(argc, argv)) {
		NEM_app_t app;
		NEM_panic_if_err(NEM_app_init_root(&app));
		NEM_app_defer(&app, NEM_thunk1_new_ptr(
			&do_stuff,
			&app
		));
		NEM_panic_if_err(NEM_app_run(&app));
		NEM_app_free(&app);

		NEM_rootd_state_close();
	}
}
