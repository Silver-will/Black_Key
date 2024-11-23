#include "Shadows.h"


ShadowCascades::ShadowCascades(const float camNearPlane, const float camFarPlane, const Camera& cam, const DirectionalLight& dirLight)
{
    this->camera = cam;
    this->nearPlane = camNearPlane;
    this->farPlane = camFarPlane;
    this->light = dirLight;
}

std::vector<glm::vec4> ShadowCascades::getFrustumCornersWorldSpace(const glm::mat4& projView)
{
    const glm::mat4 inverse = glm::inverse(projView);

    std::vector<glm::vec4> frustumCorners;
    for (unsigned int x = 0; x < 2; ++x)
    {
        for (unsigned int y = 0; y < 2; ++y)
        {
            for (unsigned int z = 0; z < 2; ++z)
            {
                const glm::vec4 pt = inverse * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    return frustumCorners;
}

glm::mat4 ShadowCascades::getLightSpaceMatrix(const float nearPlane, const float farPlane)
{
    auto proj = glm::perspective(
        glm::radians(camera.zoom), (float)windowSize.width / (float)windowSize.height, nearPlane,
        farPlane);

    proj[1][1] *= -1;
    const auto corners = getFrustumCornersWorldSpace(proj * camera.getViewMatrix());

    glm::vec3 center = glm::vec3(0, 0, 0);
    for (const auto& v : corners)
    {
        center += glm::vec3(v);
    }
    center /= corners.size();

    const auto lightView = glm::lookAt(center + glm::vec3(light.direction.x, light.direction.y,
                                                                    light.direction.z),center, glm::vec3(0.0f, 1.0f, 0.0f));

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto& v : corners)
    {
        const auto trf = lightView * v;
        
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    // Tune this parameter according to the scene
    constexpr float zMult = 10.0f;
    if (minZ < 0)
    {
        minZ *= zMult;
    }
    else
    {
        minZ /= zMult;
    }
    if (maxZ < 0)
    {
        maxZ /= zMult;
    }
    else
    {
        maxZ *= zMult;
    }

    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}

void ShadowCascades::setCascadeLevels(std::vector<float> shadowCascadeLevels)
{
    this->shadowCascadeLevels = shadowCascadeLevels;
    numOfCascades = shadowCascadeLevels.size();
}

std::vector<glm::mat4> ShadowCascades::getLightSpaceMatrices(VkExtent2D& windowSize)
{
    this->windowSize = windowSize;
    std::vector<glm::mat4> ret;
    for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i)
    {
        if (i == 0)
        {
            ret.push_back(getLightSpaceMatrix(nearPlane, shadowCascadeLevels[i]));
        }
        else if (i < shadowCascadeLevels.size())
        {
            ret.push_back(getLightSpaceMatrix(shadowCascadeLevels[i - 1], shadowCascadeLevels[i]));
        }
        else
        {
            ret.push_back(getLightSpaceMatrix(shadowCascadeLevels[i - 1], farPlane));
        }
    }
    return ret;
}

void ShadowCascades::update(const DirectionalLight& light, const Camera& cam)
{
    this->light = light;
    this->camera = cam;
}

std::vector<float> ShadowCascades::getCascadeLevels()const
{
    return shadowCascadeLevels;
}