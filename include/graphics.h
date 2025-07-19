#pragma once
#include "vk_types.h"
#include "camera.h"
#include "engine_util.h"
#include "vk_renderer.h"
class VulkanEngine;

namespace black_key {
	bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
	void generate_brdf_lut(VulkanEngine* engine, IBLData& ibl);
	void generate_irradiance_cube(VulkanEngine* engine, IBLData& ibl);
	void generate_prefiltered_cubemap(VulkanEngine* engine, IBLData& ibl);
	void build_clusters(VulkanEngine* engine, PipelineCreationInfo& info, DescriptorAllocator& descriptorAllocator);

	struct PushBlock {
		glm::mat4 mvp;
		VkDeviceAddress vertexBuffer;
		float roughness;
		uint32_t numSamples = 32u;
	};

}