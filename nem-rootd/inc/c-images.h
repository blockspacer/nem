#pragma once
#include "jaildesc.h"

NEM_err_t NEM_rootd_find_image(const NEM_jailimg_t *ji, char **path);

extern const NEM_app_comp_t NEM_rootd_c_images;
