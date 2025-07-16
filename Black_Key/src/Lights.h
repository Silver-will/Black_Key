#pragma once
#include "vk_types.h"
struct DirectionalLight
{
	DirectionalLight(const glm::vec4& dir,const glm::vec4& col,const glm::vec4& intens): direction{dir},
								color{col},intensity{intens},lastDirection(glm::vec4(0)) {}
	DirectionalLight() {}
	glm::vec4 direction;
	glm::vec4 lastDirection;
	glm::vec4 intensity;
	glm::vec4 color;
};

struct PointLight
{
	PointLight(const glm::vec4& pos, const glm::vec4& col, const float& rad, const float& i)
		: position{ pos }, color{ col }, range{ rad }, intensity{ i } {}
	PointLight() {}
	glm::vec4 position;
	glm::vec4 color;
	float enabled = 1;
	float range;
	float intensity;
	float padding = 20.0f;
};
