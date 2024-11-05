#pragma once
#include "vk_types.h"
struct DirectionalLight
{
	DirectionalLight(const glm::vec4& dir,const glm::vec4& col,const glm::vec4& intens): direction{dir},
								color{col},intensity{intens} {}
	DirectionalLight() {}
	glm::vec4 direction;
	glm::vec4 intensity;
	glm::vec4 color;
};

struct PointLight
{
	glm::vec3 position;
	glm::vec3 intensity;
	glm::vec3 color;

};

