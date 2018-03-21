#pragma once

// NEM_rootd_comp_t defines the lifecycle for an application component. An
// application component is a discrete thing that can be loaded via main.
// Components are setup in the order they're added, and torndown in 
// the reverse order, so any cross-component dependencies can be explicitly
// annotated by the caller.
typedef struct {
	// setup should initialize the component. Any errors returned prevents
	// the app from initializing. This is done before the app starts
	// running, so any network requests that need to be made won't happen
	// until this returns.
	NEM_err_t(*setup)(NEM_app_t *);

	// try_shutdown is called repeatedly until it returns true or a timer
	// expires. When the timer expires, teardown is called.
	bool(*try_shutdown)();

	// teardown signals that application termination is immenient and that
	// the component should release any resources it's using.
	void(*teardown)();
}
NEM_rootd_comp_t;

// NEM_rootd_add_comp registers a component with the application. It can
// only be called before NEM_rootd_main.
void NEM_rootd_add_comp(const NEM_rootd_comp_t *comp);

// NEM_rootd_main runs the application components as configured.
NEM_err_t NEM_rootd_main(int argc, char *argv[]);

// NEM_rootd_shutdown stops the running application.
void NEM_rootd_shutdown();

