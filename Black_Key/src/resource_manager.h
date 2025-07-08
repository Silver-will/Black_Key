#pragma once

#include <iostream>

#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_loader.h"
#include <glm/gtx/quaternion.hpp>


#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>
#include <fastgltf/base64.hpp>
#include <fastgltf/types.hpp>
#include <iostream>
#include <string>

class VulkanEngine;

struct ResourceManager
{
	ResourceManager() {}
	ResourceManager(VulkanEngine* engine_ptr) : engine{engine_ptr}{}
	void init(VulkanEngine* engine_ptr);

	//Gltf loading functions
	std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath, bool isPBRMaterial = false);
	std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, const std::string& rootPath);
	
	void write_material_array();
	VkDescriptorSet* GetBindlessSet();
	//Displays the contents of a GPU only buffer
	void ReadBackBufferData(VkCommandBuffer cmd, AllocatedBuffer* buffer);
	AllocatedBuffer* GetReadBackBuffer();
	void cleanup();

	//Resource management
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);
	AllocatedBuffer create_and_upload(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data);
	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& img);

	VkFilter extract_filter(fastgltf::Filter filter);
	VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
private:
	bool readBackBufferInitialized = false;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
	std::vector< GLTFMetallic_Roughness::MaterialResources> bindless_resources{};
	DescriptorAllocator bindless_material_descriptor;
	DescriptorWriter writer;
	VulkanEngine* engine = nullptr;
	int last_material_index{ 0 };
	VkDescriptorSet bindless_set;
	AllocatedBuffer readableBuffer;
};