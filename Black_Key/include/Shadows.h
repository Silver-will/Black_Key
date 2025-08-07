#pragma once
#include "vk_types.h"
#include "camera.h"
#include "Lights.h"

class VulkanEngine;

struct Cascade{
    std::vector<glm::mat4> lightSpaceMatrix;
    std::vector<float> cascadeDistances;
    std::vector<glm::mat4> lightViewMatrices;
    std::vector<glm::mat4> lightProjMatrices;
};

struct ShadowCascades
{

    Cascade getCascades(VulkanEngine* engine, Camera& mainCamera, GPUSceneData& scene_data);
    void SetShadowMapTextureSize(uint32_t size);
    int getCascadeLevels() {
        return cascadeCount;
    };
private:
    float cascadeSplitLambda = 0.95f;
    int cascadeCount = 4;
    uint32_t shadowMapTextureSize;

};