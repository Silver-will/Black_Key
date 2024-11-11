#pragma once
#include "vk_types.h"
#include "camera.h"
#include "Lights.h"

struct ShadowCascades
{
    ShadowCascades(const float camNearPlane,const float camFarPlane,const Camera& cam,const DirectionalLight& dirLight);
    ShadowCascades() {}
    std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projView);
    glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane);
    std::vector<glm::mat4> getLightSpaceMatrices(VkExtent2D& windowSize);
    void setCascadeLevels(std::vector<float> cascades);
    void update(const DirectionalLight& light, const Camera& cam);
    std::vector<float> getCascadeLevels()const;

private:
    int numOfCascades;
    float nearPlane, farPlane;
    Camera camera;
    DirectionalLight light;
    std::vector<float> shadowCascadeLevels;
    VkExtent2D windowSize;
};