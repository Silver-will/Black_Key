#pragma once
#include "vk_types.h"
#include "camera.h"
#include "vk_renderer.h"
class VulkanEngine;

namespace black_key {
	bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
	void generate_brdf_lut(VulkanEngine* engine);
	void generate_irradiance_cube(VulkanEngine* engine);
	void generate_prefiltered_cubemap(VulkanEngine* engine);
	void build_clusters(VulkanEngine* engine);
}