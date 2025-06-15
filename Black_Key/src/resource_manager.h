#pragma once
#include "vk_engine.h"

#include <iostream>

#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "vk_images.h"
#include <glm/gtx/quaternion.hpp>


#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>
#include <fastgltf/base64.hpp>
#include <fastgltf/types.hpp>
#include <iostream>
#include <string>

class RESOURCEMANAGER
{
	RESOURCEMANAGER(VulkanEngine* engine_ptr) : engine{engine_ptr}{}
	VulkanEngine* engine;
	std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, const std::string& rootPath)

};

