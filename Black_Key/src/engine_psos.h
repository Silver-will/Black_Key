#pragma once
#include "vk_types.h"
#include "vk_renderer.h"
#include "vk_descriptors.h"
#include <unordered_map>
#include <filesystem>
#include "Renderers/base_renderer.h"
#include "Renderers/clustered_forward_renderer.h"

class VulkanEngine;

struct ShadowPipelineResources {
	MaterialPipeline shadowPipeline;

	VkDescriptorSetLayout materialLayout;
	MaterialInstance matData;

	DescriptorWriter writer;

	struct ShadowMatrices {
		glm::mat4 lightSpaceMatrices[16];
	};

	struct MaterialResources {
		AllocatedImage shadowImage;
		VkSampler shadowSampler;
	};

	void build_pipelines(VulkanEngine* engine);
	MaterialResources AllocateResources(VulkanEngine* engine);
	void clear_resources(VkDevice device);

	void write_material(VkDevice device, MaterialPass pass,VulkanEngine* engine, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct EarlyDepthPipelineObject {
	MaterialPipeline earlyDepthPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
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

struct BloomBlurPipelineObject {
	MaterialPipeline postProcesssingPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		VkSampler Sampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct RenderImagePipelineObject {
	MaterialPipeline renderImagePipeline;
	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		VkSampler Sampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		AllocatedImage occlusionImage;
		VkSampler occlusionSampler;
		//Flag if the occlusion texture is separate from the metallicRoughness one
		bool separate_occ_texture = false;
		AllocatedImage normalImage;
		VkSampler normalSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void build_pipelines(VulkanEngine* application);
	void clear_resources(VkDevice device);

	MaterialInstance set_material_properties(const MaterialPass pass);
	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
	void write_material_array(VkDevice device, LoadedGLTF& file, std::vector< GLTFMetallic_Roughness::MaterialResources>& bindless_resources, DescriptorAllocatorGrowable& descriptorAllocator);
};