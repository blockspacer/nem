#include "nem.h"

void
NEM_app_init(NEM_app_t *this)
{
	bzero(this, sizeof(*this));
	NEM_kq_init(&this->kq);
}

void
NEM_app_init_root(NEM_app_t *this)
{
	bzero(this, sizeof(*this));
	NEM_kq_init_root(&this->kq);
}

void
NEM_app_add_comp(NEM_app_t *this, const NEM_app_comp_t *comp)
{
	NEM_app_add_comps(this, &comp, 1);
}

void NEM_app_add_comps(
	NEM_app_t            *this,
	NEM_app_comp_t const**comps,
	size_t                num_comps
) {
	if (this->kq.running) {
		NEM_panicf("NEM_app_add_comps: already running");
	}

	size_t wanted_comps = this->comps_len + num_comps;
	this->comps = NEM_panic_if_null(realloc(
		this->comps,
		wanted_comps * sizeof(NEM_app_compentry_t)
	));

	for (size_t i = 0; i < num_comps; i += 1) {
		NEM_app_compentry_t *entry = &this->comps[this->comps_len];
		entry->running = false;
		entry->comp = comps[i];
		this->comps_len += 1;
	}
}

NEM_err_t
NEM_app_main(NEM_app_t *this, int argc, char *argv[])
{
	NEM_err_t err = NEM_err_none;
	size_t i = 0;

	for (size_t i = 0; i < this->comps_len; i += 1) {
		err = this->comps[i].comp->setup(this, argc, argv);
		if (!NEM_err_ok(err)) {
			// XXX: Reallllly want some logging bits here.
			break;
		}
		this->comps[i].running = true;
		this->comps_running += 1;
	}
	if (!NEM_err_ok(err)) {
		// Teardown everything that's been constructed.
		for (; i > 0; i -= 1) {
			if (NULL != this->comps[i - 1].comp->teardown) {
				this->comps[i - 1].comp->teardown(this);
			}
			this->comps[i - 1].running = false;
			this->comps_running -= 1;
		}
		return err;
	}

	this->running = true;
	NEM_panic_if_err(NEM_kq_run(&this->kq));

	for (size_t i = this->comps_len; i > 0; i -= 1) {
		if (NULL != this->comps[i - 1].comp->teardown) {
			this->comps[i - 1].comp->teardown(this);
			this->comps[i - 1].running = false;
			this->comps_running -= 1;
		}
	}

	// NB: This is NEM_app_free effectively.
	NEM_kq_free(&this->kq);
	free(this->comps);

	return NEM_err_none;
}

static void
NEM_app_shutdown_step(NEM_thunk1_t *thunk, void *varg)
{
	NEM_app_t *this = NEM_thunk1_ptr(thunk);

	// NB: Do this in reverse order of initialization to match
	// shutdown behavior.
	for (size_t j = this->comps_len; j > 0; j -= 1) {
		size_t i = j - 1;
		if (!this->comps[i].running) {
			continue;
		}

		if (NULL == this->comps[i].comp->try_shutdown) {
			this->comps[i].running = false;
			this->comps_running -= 1;
		}
		else if (this->comps[i].comp->try_shutdown(this)) {
			this->comps[i].running = false;
			this->comps_running -= 1;
		}
	}
	if (0 != this->comps_running) {
		NEM_kq_after(&this->kq, 100, NEM_thunk1_new_ptr(
			&NEM_app_shutdown_step,
			this
		));
	}
	else {
		NEM_kq_stop(&this->kq);
	}
}

static void
NEM_app_shutdown_now(NEM_thunk1_t *thunk, void *varg)
{
	NEM_app_t *this = NEM_thunk1_ptr(thunk);
	NEM_kq_stop(&this->kq);
}

void
NEM_app_shutdown(NEM_app_t *this)
{
	if (!this->running) {
		return;
	}

	this->running = false;

	// XXX: This is kinda gross.
	NEM_thunk1_t *dummy = NEM_thunk1_new_ptr(&NEM_app_shutdown_step, this);
	NEM_thunk1_invoke(&dummy, NULL);

	NEM_kq_after(
		&this->kq,
		5000,
		NEM_thunk1_new_ptr(
			&NEM_app_shutdown_now,
			this
		)
	);
}
