#pragma once

typedef struct NEM_disk_md_t NEM_disk_md_t;

NEM_err_t NEM_disk_md_new_mem(NEM_disk_md_t **out, size_t len);
NEM_err_t NEM_disk_md_new_file(NEM_disk_md_t **out, const char *path, bool ro);
void NEM_disk_md_free(NEM_disk_md_t *this);

int NEM_disk_md_unit(NEM_disk_md_t *this);
const char *NEM_disk_md_device(NEM_disk_md_t *this);

extern const NEM_app_comp_t NEM_rootd_c_disk_md;
