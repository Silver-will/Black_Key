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
	MaterialInstance matData;

	DescriptorWriter writer;

	struct ShadowMatrices {
		glm::mat4 lightSpaceMatrices[16];
	};

	struct MaterialResources {
		AllocatedImage shadowImage;
		VkSampler shadowSampler;
	};

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);
	MaterialResources AllocateResources(VulkanEngine* engine);
	void clear_resources(VkDevice device);

	void write_material(VkDevice device, vkutil::MaterialPass pass, VulkanEngine* engine, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct EarlyDepthPipelineObject {
	MaterialPipeline earlyDepthPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
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

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
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

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
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

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);

	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct UpsamplePipelineObject {
	MaterialPipeline renderImagePipeline;
	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	struct MaterialResources {
		VkSampler Sampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);

	void clear_resources(VkDevice device);
};


struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		int has_metalRough;
		int has_occlusion_tex;
		int has_emission;
		int pad;
		glm::vec4 emission_color;
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		AllocatedImage occlusionImage;
		VkSampler occlusionSampler;
		AllocatedImage emissionImage;
		VkSampler emissionSampler;
		//Flag if the occlusion texture is separate from the metallicRoughness one
		bool separate_occ_texture = false;
		bool rough_metallic_texture_found = false;
		bool normal_texture_found = false;
		bool emissive_texture_found = false;
		AllocatedImage normalImage;
		VkSampler normalSampler;
		AllocatedBuffer material_description_Buffer;
		uint32_t dataBufferOffset;
		MaterialConstants material_description;
	};

	DescriptorWriter writer;

	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);
	void clear_resources(VkDevice device);

	MaterialInstance SetMaterialProperties(const vkutil::MaterialPass pass, int mat_index = -1);
	MaterialInstance WriteMaterial(VkDevice device, vkutil::MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};


struct ConservativeVoxelizationPipelineObject {
	MaterialPipeline conservative_opacity_pipeline;
	MaterialPipeline conservative_radiance_pipeline;
	MaterialPipeline msaa_opacity_pipeline;
	MaterialPipeline msaa_radiance_pipeline;
	VkDescriptorSetLayout materialLayout;

	// Fetch and store conservative rasterization state props for display purposes
	VkPhysicalDeviceConservativeRasterizationPropertiesEXT conservativeRasterProps{};
	void build_pipelines(VulkanEngine* engine, PipelineCreationInfo& info);

	void clear_resources(VkDevice device);
};