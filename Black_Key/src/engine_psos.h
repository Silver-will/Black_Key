#pragma once
#include "vk_types.h"
#include "vk_renderer.h"
#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>

class VulkanEngine;

struct ShadowPipelineResources {
	MaterialPipeline shadowPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct ShadowMatrices {
		glm::mat4 lightSpaceMatrices[16];
	};

	struct MaterialResources {
		AllocatedImage shadowImage;
		VkSampler shadowSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct EarlyDepthPipelineResources {
	MaterialPipeline earlyDepthPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		AllocatedImage depthImage;
		VkSampler shadowSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct SkyBoxPipelineResources {
	MaterialPipeline skyPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		AllocatedImage cubeMapImage;
		VkSampler cubeMapSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct PostProcessingPipelineResources {
	MaterialPipeline skyPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		AllocatedImage cubeMapImage;
		VkSampler cubeMapSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};