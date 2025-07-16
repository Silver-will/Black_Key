// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h>


#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <string_view>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vma/vk_mem_alloc.h>

#include <fmt/core.h>

#define GLM_FORCE_RADIANS
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtx/transform.hpp>

namespace vkutil{
    enum class MaterialPass{
        transparency,
        forward,
        early_depth,
        shadow_pass
    };

    struct cullParams {
        glm::mat4 viewmat;
        glm::mat4 projmat;
        bool occlusionCull;
        bool frustrumCull;
        float drawDist;
        bool aabb;
        glm::vec3 aabbmin;
        glm::vec3 aabbmax;
    };

    struct GPUModelInformation {
        glm::mat4 local_transform;
        glm::vec4 sphereBounds;
        uint32_t  texture_index = 0;
        uint32_t  firstIndex = 0;
        uint32_t  indexCount = 0;
        uint32_t  firstVertex = 0;
        uint32_t  vertexCount = 0;
        uint32_t  firstInstance = 0;
        VkDeviceAddress vertexBuffer;
        glm::vec4 pad;
    };

    struct /*alignas(16)*/DrawCullData
    {
        glm::mat4 viewMat;
        float P00, P11, znear, zfar; // symmetric projection parameters
        float frustum[4]; // data for left/right/top/bottom frustum planes
        float lodBase, lodStep; // lod distance i = base * pow(step, i)
        float pyramidWidth, pyramidHeight; // depth pyramid size in texels

        uint32_t drawCount;

        int cullingEnabled;
        int lodEnabled;
        int occlusionEnabled;
        int distanceCheck;
        int AABBcheck;
        float aabbmin_x;
        float aabbmin_y;
        float aabbmin_z;
        float aabbmax_x;
        float aabbmax_y;
        float aabbmax_z;
    };
}

template<typename T>
struct Handle {
    uint32_t handle;
};

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

using PipelineStateObject = MaterialPipeline;

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    vkutil::MaterialPass passType;
    uint32_t material_index;
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect {
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct VolumeTileAABB {
    glm::vec4 minPoint;
    glm::vec4 maxPoint;
};

struct PipelineCreationInfo {
    std::vector<VkDescriptorSetLayout> layouts;
    VkFormat depthFormat = VK_FORMAT_MAX_ENUM;
    VkFormat imageFormat = VK_FORMAT_MAX_ENUM;
};

struct clusterParams {
    float zFar;
    float zNear;
};
struct LightGrid {
    uint32_t offset;
    uint32_t count;
};

struct CullData {
    glm::mat4 view;
};

struct ScreenToView {
    glm::mat4 inverseProjectionMat;
    glm::vec4 tileSizes;
    glm::vec2 screenDimensions;
    float sliceScalingFactor;
    float sliceBiasFactor;
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color; //rgb as color values a stores the indirect buffer index
    glm::vec4 tangents;
};

// Draw indirect version of mesh resources
struct IndirectMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    Bounds bounds;

    bool load_from_meshasset(const char* filename);

    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
};

struct MeshAssetInfo {
    uint32_t mesh_vert_count = 0;
    uint32_t mesh_indice_count = 0;
};
// holds the resources needed for a mesh
struct GPUMeshBuffers {
    MeshAssetInfo mesh_info;//Turns out VMA adds padding to certain allocations so the info struct is incorrect
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct GPUObjectData {
    glm::mat4 model;
    glm::vec4 origin;
    glm::vec4 extents;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::mat4 skyMat;
    glm::vec4 cameraPos;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
    glm::vec3 ConfigData; //x for nearPlane, y for farPlane
    uint32_t lightCount;
    glm::vec4 distances;
    glm::vec4 debugValues; //x for light clusters
  };

struct shadowData {
    glm::mat4 lightSpaceMatrices[4];
   // glm::vec4 distances;
};

struct GPUDrawBindlessPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
    uint32_t material_index;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
    uint32_t material_index;
};

struct HDRDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
    glm::vec4 TexCoordScale;
};

struct MeshObject {
    GPUMeshBuffers* mesh{ nullptr };

    //vkutil::Material* material;
    uint32_t customSortKey;
    glm::mat4 transformMatrix;

    Bounds bounds;

    uint32_t bDrawForwardPass : 1;
    uint32_t bDrawShadowPass : 1;

};

struct EngineStats {
    float frametime;
    int triangle_count;
    int drawcall_count;
    int shadow_drawcall_count;
    float scene_update_time;
    float mesh_draw_time;
    float ui_draw_time;
    float update_time;
    float shadow_pass_time;
};
#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)