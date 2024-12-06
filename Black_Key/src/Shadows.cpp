#include "Shadows.h"
#include "vk_engine.h"

ShadowCascades::ShadowCascades(const float camNearPlane, const float camFarPlane, Camera& cam, const DirectionalLight& dirLight)
{
    camera = &cam;
    this->nearPlane = camNearPlane;
    this->farPlane = camFarPlane;
    this->light = dirLight;
}

std::vector<glm::vec4> ShadowCascades::getFrustumCornersWorldSpace(glm::mat4& projView)
{
    const auto inv = glm::inverse(projView);

    std::vector<glm::vec4> frustumCorners;
    for (unsigned int x = 0; x < 2; ++x)
    {
        for (unsigned int y = 0; y < 2; ++y)
        {
            for (unsigned int z = 0; z < 2; ++z)
            {
                const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    return frustumCorners;
}

glm::mat4 ShadowCascades::getLightSpaceMatrix(const float nearPlane, const float farPlane)
{
    auto proj = glm::perspective(
        glm::radians(camera->zoom), (float)windowSize.width / (float)windowSize.height, nearPlane,
        farPlane);
    proj[1][1] *= -1;
    auto view = camera->getViewMatrix();
    //auto front = camera->getFront();
    //auto view = glm::lookAt(camera->position, camera->position + front, camera->up);
    auto projView = proj * view;
    const auto corners = getFrustumCornersWorldSpace(projView);

    glm::vec3 center = glm::vec3(0, 0, 0);
    for (const auto& v : corners)
    {
        center += glm::vec3(v);
    }
    center /= corners.size();
    
    auto direction = glm::vec3(light.direction.x, light.direction.y, light.direction.z);

    direction = glm::normalize(-direction);
    auto lightView = glm::lookAt(center + direction, center, glm::vec3(0.0f, 1.0f, 0.0f));

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
    
    glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    lightProjection[1][1] *= -1;
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

std::vector<glm::mat4> ShadowCascades::getLightSpaceMatrices2(VulkanEngine* engine)
{
    shadowCascadeLevels.clear();
    const int SHADOW_MAP_CASCADE_COUNT = 4;
    float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

    float nearClip = nearPlane;
    float farClip = farPlane;
    float clipRange = farClip - nearClip;

    float minZ = nearClip;
    float maxZ = nearClip + clipRange;

    float range = maxZ - minZ;
    float ratio = maxZ / minZ;
    shadowCascadeLevels.resize(SHADOW_MAP_CASCADE_COUNT);
    std::vector<glm::mat4> lightMatrices;
    lightMatrices.resize(SHADOW_MAP_CASCADE_COUNT);

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
        auto projview = engine->sceneData.proj * engine->mainCamera.getPreCalcViewMatrix();
        projview[1][1] *= -1;
        glm::mat4 invCam = glm::inverse(projview);
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

        glm::vec3 lightDir = glm::normalize(glm::vec3(-light.direction));
        glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

        // Store split distance and matrix in cascade
        shadowCascadeLevels[i] = (nearPlane + splitDist * clipRange) * -1.0f;
        //cascades[i].splitDepth = (nearPlane + splitDist * clipRange) * -1.0f;
        lightMatrices[i] = lightOrthoMatrix * lightViewMatrix;
        lightMatrices[i][1][1] *= -1;

        lastSplitDist = cascadeSplits[i];
    }
    return lightMatrices;
}

void ShadowCascades::update(const DirectionalLight& light,Camera& cam)
{
    this->light = light;
    this->camera = &cam;
}

std::vector<float> ShadowCascades::getCascadeLevels()const
{
    return shadowCascadeLevels;
}