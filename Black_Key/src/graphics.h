#pragma once
#include "vk_types.h"
#include "camera.h"
#include "vk_renderer.h"
namespace black_key {
	bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
}