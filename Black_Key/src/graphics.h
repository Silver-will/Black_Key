#pragma once
#include "vk_types.h"
#include "camera.h"
namespace blackKey {
	std::vector<glm::vec4> getFrustumCornerWorldsSpace(const glm::mat4& projview);
	glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane, const Camera& camera, const glm::vec3& lightDir, const VkExtent2D& windowExtent);
	std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
	std::vector<glm::mat4> getLightSpaceMatrices(const std::vector<float>& shadowCascadeLevels,const float cameraNearPlane,const float cameraFarPlane, Camera& camera, glm::vec3& lightDir, VkExtent2D window);
}