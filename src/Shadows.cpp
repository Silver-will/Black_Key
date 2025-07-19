#include "Shadows.h"
#include "vk_engine.h"

Cascade ShadowCascades::getCascades(VulkanEngine* engine, Camera& mainCamera, GPUSceneData& scene_data)
{
	Cascade cascades;
	const int SHADOW_MAP_CASCADE_COUNT = 4;
	cascades.lightSpaceMatrix.resize(SHADOW_MAP_CASCADE_COUNT);
	cascades.cascadeDistances.resize(SHADOW_MAP_CASCADE_COUNT);

	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = mainCamera.getNearClip();
	float farClip = mainCamera.getFarClip();
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(mainCamera.matrices.perspective * mainCamera.matrices.view);
		for (uint32_t j = 0; j < 8; j++) {
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
			frustumCorners[j] = invCorner / invCorner.w;
		}

		for (uint32_t j = 0; j < 4; j++) {
			glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
			frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
			frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t j = 0; j < 8; j++) {
			frustumCenter += frustumCorners[j];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t j = 0; j < 8; j++) {
			float distance = glm::length(frustumCorners[j] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::vec3 lightDir = glm::normalize(scene_data.sunlightDirection);
		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);
		lightOrthoMatrix[1][1] *= -1;

		//Quick rounding code 
		auto lightSpaceMatrix = lightOrthoMatrix * lightViewMatrix;
		auto shadowOrigin = lightSpaceMatrix * glm::vec4(0, 0, 0, 1.0f);
		shadowOrigin *= (float)shadowMapTextureSize/2.0f;
		auto roundedOrigin = BlackKey::roundVec4(shadowOrigin);
		auto roundedOffset = roundedOrigin - shadowOrigin;
		roundedOffset *= 2.0f/(float)shadowMapTextureSize;
		roundedOffset = glm::vec4(roundedOffset.r, roundedOffset.g, 0.0f, 0.0f);
		lightOrthoMatrix[3][0] += roundedOffset.r;
		lightOrthoMatrix[3][1] += roundedOffset.g;

		// Store split distance and matrix in cascade
		cascades.cascadeDistances[i] = (mainCamera.getNearClip() + splitDist * clipRange) * -1.0f;
		cascades.lightSpaceMatrix[i] = lightOrthoMatrix * lightViewMatrix;
		cascades.lightViewMatrices.push_back(lightViewMatrix);
		cascades.lightProjMatrices.push_back(lightOrthoMatrix);

		lastSplitDist = cascadeSplits[i];
	}
	return cascades;
}

void ShadowCascades::SetShadowMapTextureSize(uint32_t size)
{
	this->shadowMapTextureSize = size;
}