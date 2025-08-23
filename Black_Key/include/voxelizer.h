#ifndef VOXELIZER_H
#define VOXELIZER_H
#include "vk_types.h"
#include <print>

struct Voxelizer {
	int voxel_texture_resolution = 128;

	AllocatedImage voxel_radiance_image;
	AllocatedImage voxel_opacity_image;
};
#endif