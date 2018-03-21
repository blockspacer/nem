#pragma once

typedef struct {
	NEM_err_t(*setup)();
	bool(*try_shutdown)();
	void(*teardown)();
}
NEM_rootd_comp_t;

void NEM_rootd_add_comp(const NEM_rootd_comp_t *comp);

NEM_err_t NEM_rootd_main(int argc, char *argv[]);
void NEM_rootd_shutdown();

