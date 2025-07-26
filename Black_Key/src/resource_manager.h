#pragma once


#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_loader.h"
#include <glm/gtx/quaternion.hpp>

#include <string>

#include "engine_util.h"

class VulkanEngine;

struct ResourceManager
{
	ResourceManager() {}
	ResourceManager(VulkanEngine* engine_ptr);
	void init(VulkanEngine* engine_ptr);

	//Gltf loading functions
	std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath, bool isPBRMaterial = false);
	std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, const std::string& rootPath);
	
	//Bindless helper functions
	void write_material_array();
	VkDescriptorSet* GetBindlessSet();
	//Displays the contents of a GPU only buffer
	void ReadBackBufferData(VkCommandBuffer cmd, AllocatedBuffer* buffer);
	AllocatedBuffer* GetReadBackBuffer();
	void cleanup();

	//Resource management
	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	AllocatedBuffer CreateAndUpload(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data);
	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	AllocatedImage CreateImageEmpty(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VkImageViewType viewType, bool mipmapped, int layers, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT, int mipLevels = -1);
	void DestroyImage(const AllocatedImage& img);
	VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
	VkFilter extract_filter(fastgltf::Filter filter);
	MaterialInstance SetMaterialProperties(const vkutil::MaterialPass pass, int mat_index);

	DeletionQueue deletionQueue;
	VkDescriptorSetLayout bindless_descriptor_layout;
	VulkanEngine* engine = nullptr;
	AllocatedImage errorCheckerboardImage;
	GLTFMetallic_Roughness* PBRpipeline;
private:
	bool readBackBufferInitialized = false;
	VkSampler defaultSamplerNearest;
	VkSampler defaultSamplerLinear;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
	std::vector< GLTFMetallic_Roughness::MaterialResources> bindless_resources{};
	DescriptorAllocator bindless_material_descriptor;
	DescriptorWriter writer;

	int last_material_index{ 0 };
	AllocatedImage _whiteImage;
	AllocatedImage _greyImage;
	AllocatedImage _blackImage;
	AllocatedImage storageImage;
	VkDescriptorSet bindless_set;
	AllocatedBuffer readableBuffer;
};