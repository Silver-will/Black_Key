#ifndef VOXELIZER_H
#define VOXELIZER_H
#include "vk_types.h"
#include <memory>
#include "resource_manager.h"
#include <print>

struct Voxelizer {
	int voxel_res = 128;
	int debug_vertex_count;

	AllocatedImage voxel_radiance_image;
	AllocatedImage voxel_opacity_image;

	void InitializeVoxelizer(std::shared_ptr<ResourceManager> resource_manager);
	void InitializeResources(ResourceManager* resource_manager);
	GPUMeshBuffers voxel_buffer;
	AllocatedBuffer vertex_buffer;
};

struct VoxelVertex {
	glm::vec4 pos;
	VoxelVertex(float x, float y, float z) : pos{glm::vec4(x,y,z,1.0)} {}
};

#endif