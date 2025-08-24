#ifndef VOXELIZER_H
#define VOXELIZER_H
#include "vk_types.h"
#include <print>

struct Voxelizer {
	int voxel_texture_resolution = 128;

	AllocatedImage voxel_radiance_image;
	AllocatedImage voxel_opacity_image;

	void InitializeVoxelDebugger();
	AllocatedBuffer vertices;
};

struct VoxelVertex {
	glm::vec4 pos;
	VoxelVertex(float x, float y, float z) : pos{glm::vec4(x,y,z,1.0)} {}
};
#endif