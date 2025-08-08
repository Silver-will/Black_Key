#include "stb_image.h"
#include <iostream>
#include "vk_loader.h"

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "vk_images.h"
#include "resource_manager.h"
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <string>

void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes) {
        n->Draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->engine->_device;

    for (auto& [k, v] : meshes) {

        creator->DestroyBuffer(v->meshBuffers.indexBuffer);
        creator->DestroyBuffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images) {

        if (v.image == creator->errorCheckerboardImage.image) {
            // dont destroy the default images
            continue;
        }
        creator->DestroyImage(v);
    }

    for (auto& sampler : samplers) {
        vkDestroySampler(dv, sampler, nullptr);
    }

    auto materialBuffer = materialDataBuffer;
    auto samplersToDestroy = samplers;

    descriptorPool.destroy_pools(dv);

    creator->DestroyBuffer(materialBuffer);
}


void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& s : mesh->surfaces) {
        RenderObject def;
        def.indexCount = s.count;
        def.firstIndex = s.startIndex;
        def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        def.material = &s.material->data;
        def.bounds = s.bounds;
        def.vertexCount = s.vertex_count;
        def.transform = nodeMatrix;
        def.vertexBuffer = mesh->meshBuffers.vertexBuffer.buffer;
        def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;
        def.meshBuffer = &mesh->meshBuffers;

        if (s.material->data.passType == vkutil::MaterialPass::transparency) {
            ctx.TransparentSurfaces.push_back(def);
        }
        else {
            ctx.OpaqueSurfaces.push_back(def);
        }
    }

    // recurse down
    Node::Draw(topMatrix, ctx);
}