#pragma once

#if defined(_MSC_VER)
#include <io.h>
#endif
#include <stdio.h>
#include "ogt_vox.h"

const ogt_vox_scene* load_vox_scene(const char* filename, uint32_t scene_read_flags = 0);
const ogt_vox_scene* load_vox_scene_with_groups(const char* filename);
uint32_t count_solid_voxels_in_model(const ogt_vox_model* model);
void ogt_demo();

