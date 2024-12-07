#pragma once
#include "vk_types.h"
#include "camera.h"
#include "Lights.h"

class VulkanEngine;

struct Cascade{
    std::vector<glm::mat4> lightSpaceMatrix;
    std::vector<float> cascadeDistances;
};

struct ShadowCascades
{

    Cascade getCascades(VulkanEngine* engine);
    int getCascadeLevels() {
        return cascadeCount;
    };
private:
    float cascadeSplitLambda = 0.95f;
    int cascadeCount = 4;
};