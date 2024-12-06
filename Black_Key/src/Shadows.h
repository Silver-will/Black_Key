#pragma once
#include "vk_types.h"
#include "camera.h"
#include "Lights.h"

class VulkanEngine;

struct ShadowCascades
{
    ShadowCascades(const float camNearPlane,const float camFarPlane,Camera& cam,const DirectionalLight& dirLight);
    ShadowCascades() {}
    std::vector<glm::vec4> getFrustumCornersWorldSpace(glm::mat4& projView);
    glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane);
    std::vector<glm::mat4> getLightSpaceMatrices(VkExtent2D& windowSize);
    //Second implementation for calculating light ProjView matrices
    std::vector<glm::mat4> getLightSpaceMatrices2(VulkanEngine* engine);
    void setCascadeLevels(std::vector<float> cascades);
    void update(const DirectionalLight& light,Camera& cam);
    std::vector<float> getCascadeLevels()const;

private:
    int numOfCascades;
    float nearPlane, farPlane;
    float cascadeSplitLambda = 0.95f;
    Camera* camera;
    DirectionalLight light;
    std::vector<float> shadowCascadeLevels;
    VkExtent2D windowSize;
};