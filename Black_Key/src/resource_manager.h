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
	std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, const std::string& rootPath);
	std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath, bool isPBRMaterial = false);
    VkFilter extract_filter(fastgltf::Filter filter);
    VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
private:
	VulkanEngine* engine;
	int last_material_index = 0;
	VkDescriptorSet bindless_set;
};