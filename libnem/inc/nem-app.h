#pragma once

typedef struct NEM_app_t NEM_app_t;

// NEM_app_comp_t defines the lifecycle for an application component. An
// application component is a discrete thing that can be loaded via main.
// Components are setup in the order they're added, and torndown in 
// the reverse order, so any cross-component dependencies can be explicitly
// annotated by the caller.
//
// These dispatch tables are assumed to be staticly-allocated and don't have
// a concrete this*.
typedef struct {
	const char *name;

	// setup should initialize the component. Any errors returned prevents
	// the app from initializing. This is done before the app starts
	// running, so any network requests that need to be made won't happen
	// until this returns. If setup returns NEM_err_none, teardown is
	// guarenteed to be called.
	NEM_err_t(*setup)(NEM_app_t *, int argc, char *argv[]);

	// try_shutdown is called repeatedly until it returns true or a timer
	// expires. When the timer expires, teardown is called.
	bool(*try_shutdown)(NEM_app_t *);

	// teardown signals that application termination is immenient and that
	// the component should release any resources it's using.
	void(*teardown)(NEM_app_t *);
}
NEM_app_comp_t;

typedef struct {
	const NEM_app_comp_t *comp;
	bool                  running;
}
NEM_app_compentry_t;

// NEM_app_t is a container for lifecycle components and a NEM_kq_t. The goal
// is to provide a centralized place for initialization and so forth.
struct NEM_app_t {
	NEM_kq_t             kq;
	bool                 running;
	int                  comps_running;
	size_t               comps_len;
	NEM_app_compentry_t *comps;
	void                *data;
};

void NEM_app_init(NEM_app_t *app);
void NEM_app_init_root(NEM_app_t *app);

// NEM_app_add_comp registers a component with the application. It can
// only be called before NEM_app_main.
void NEM_app_add_comp(NEM_app_t *this, const NEM_app_comp_t *comp);
void NEM_app_add_comps(
	NEM_app_t            *this,
	NEM_app_comp_t const**comps,
	size_t                num_comps
);

// NEM_app_main runs the application components as configured.
NEM_err_t NEM_app_main(NEM_app_t *this, int argc, char *argv[]);

// NEM_app_shutdown stops the running application. This automatically frees
// the bits used by the app once everything is stopped.
void NEM_app_shutdown(NEM_app_t *app);

