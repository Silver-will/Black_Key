#pragma once
#include "vk_types.h"
#include "camera.h"
#include "engine_util.h"
#include "vk_renderer.h"
class VulkanEngine;

namespace black_key {
	bool IsVisible(const RenderObject& obj, const glm::mat4& viewproj);
	void GenerateBRDFLUT(VulkanEngine* engine, IBLData& ibl);
	void GenerateIrradianceCube(VulkanEngine* engine, IBLData& ibl);
	void GeneratedPrefilteredCubemap(VulkanEngine* engine, IBLData& ibl);
	void BuildClusters(VulkanEngine* engine, PipelineCreationInfo& info, DescriptorAllocator& descriptorAllocator);

	struct PushBlock {
		glm::mat4 mvp;
		VkDeviceAddress vertexBuffer;
		float roughness;
		uint32_t numSamples = 32u;
	};

}