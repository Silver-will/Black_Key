#pragma once
#include "vk_types.h"

struct RenderObject {
    uint32_t indexCount;
    uint32_t meshAssetIndexCount;
    uint32_t firstIndex;
    uint32_t vertexCount;
    uint32_t meshAssetVertexCount;
    uint32_t firstVertex;
    VkBuffer indexBuffer;
    VkBuffer vertexBuffer;
    GPUMeshBuffers* meshBuffer;

    MaterialInstance* material;

    glm::mat4 transform;
    Bounds bounds;
    VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
    std::vector<RenderObject> OpaqueSurfaces;
    std::vector<RenderObject> TransparentSurfaces;
};

// base class for a renderable dynamic object
class IRenderable {

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};


