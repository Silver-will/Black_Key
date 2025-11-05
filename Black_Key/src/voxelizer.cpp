#include "voxelizer.h"
#include "vk_engine.h"
#include <memory>


void Voxelizer::InitializeVoxelizer(std::shared_ptr<ResourceManager> resource_manager) {
    auto res = voxel_res;
    debug_vertex_count = std::size_t(res * res * res);

    std::vector<VoxelVertex> vertices;
    vertices.reserve(debug_vertex_count);

    for (uint16_t z = 0; z < res; ++z)
    {
        for (uint16_t y = 0; y < res; ++y)
        {
            for (uint16_t x = 0; x < res; ++x)
            {
                vertices.push_back(VoxelVertex(x, y, z));
            }
        }
    }

    //Voxel texture visualization buffer
    size_t buffer_size = vertices.size() * sizeof(VoxelVertex);
    voxel_buffer.vertexBuffer = resource_manager->CreateBuffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = voxel_buffer.vertexBuffer.buffer };
    voxel_buffer.vertexBufferAddress = vkGetBufferDeviceAddress(resource_manager->engine->_device, &deviceAdressInfo);


    AllocatedBuffer staging = resource_manager->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = nullptr;
    vmaMapMemory(resource_manager->engine->_allocator, staging.allocation, &data);
    // copy vertex buffer
    memcpy(data, vertices.data(), buffer_size);

    resource_manager->engine->immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = buffer_size;

        vkCmdCopyBuffer(cmd, staging.buffer, voxel_buffer.vertexBuffer.buffer, 1, &vertexCopy);
        });

    vmaUnmapMemory(resource_manager->engine->_allocator, staging.allocation);
    resource_manager->DestroyBuffer(staging);



    //Create 3D texture for GPU scene Voxelization
    voxel_radiance_packed = resource_manager->CreateImage(
        VkExtent3D(voxel_res, voxel_res, voxel_res)
        , VK_FORMAT_R32_UINT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_VIEW_TYPE_3D, false, 1, VK_SAMPLE_COUNT_1_BIT, -1, VK_IMAGE_TYPE_3D);

}

void Voxelizer::InitializeResources(ResourceManager* resource_manager)
{
    auto res = voxel_res;
    debug_vertex_count = std::size_t(res * res * res);

    std::vector<VoxelVertex> vertices;
    vertices.reserve(debug_vertex_count);

    for (uint16_t z = 0; z < res; ++z)
    {
        for (uint16_t y = 0; y < res; ++y)
        {
            for (uint16_t x = 0; x < res; ++x)
            {
                vertices.push_back(VoxelVertex(x, y, z));
            }
        }
    }

    //Voxel texture visualization buffer
    size_t buffer_size = vertices.size() * sizeof(VoxelVertex);
    voxel_buffer.vertexBuffer = resource_manager->CreateBuffer(buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAdressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = voxel_buffer.vertexBuffer.buffer };
    voxel_buffer.vertexBufferAddress = vkGetBufferDeviceAddress(resource_manager->engine->_device, &deviceAdressInfo);


    AllocatedBuffer staging = resource_manager->CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = nullptr;
    vmaMapMemory(resource_manager->engine->_allocator, staging.allocation, &data);
    // copy vertex buffer
    memcpy(data, vertices.data(), buffer_size);

    resource_manager->engine->immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = buffer_size;

        vkCmdCopyBuffer(cmd, staging.buffer, voxel_buffer.vertexBuffer.buffer, 1, &vertexCopy);
        });

    vmaUnmapMemory(resource_manager->engine->_allocator, staging.allocation);
    resource_manager->DestroyBuffer(staging);

    //Create 3D texture for GPU scene Voxelization
    voxel_radiance_packed = resource_manager->CreateImage(
        VkExtent3D(voxel_res, voxel_res, voxel_res)
        , VK_FORMAT_R32_UINT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_IMAGE_VIEW_TYPE_3D, true, 1, VK_SAMPLE_COUNT_1_BIT, -1, VK_IMAGE_TYPE_3D);

    voxel_radiance_image = resource_manager->CreateImage(
        VkExtent3D(voxel_res, voxel_res, voxel_res)
        , VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_IMAGE_VIEW_TYPE_3D, true, 1, VK_SAMPLE_COUNT_1_BIT, -1, VK_IMAGE_TYPE_3D);
}


std::vector<glm::mat4> Voxelizer::GetVoxelizationMatrices(VoxelRegion voxel_region)
{
    glm::mat4 proj[3];

    glm::vec3 size = voxel_region.max_pos - voxel_region.min_pos;

    proj[0] = glm::orthoLH(0.0f, size.z, 0.0f, size.y, 0.0f, size.x);
    proj[1] = glm::orthoLH(0.0f, size.x, 0.0f, size.z, 0.0f, size.y);
    proj[2] = glm::orthoLH(0.0f, size.x, 0.0f, size.y, 0.0f, size.z);

    std::vector<glm::mat4> viewProj(6);
    //std::vector<glm::mat4> viewProjInv[3];

    glm::vec3 xyStart = voxel_region.min_pos + glm::vec3(0.0f, 0.0f, size.z);
    viewProj[0] = glm::lookAtLH(xyStart, xyStart + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    viewProj[1] = glm::lookAtLH(xyStart, xyStart + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    viewProj[2] = glm::lookAtLH(voxel_region.min_pos, voxel_region.min_pos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));


    for (int i = 0; i < 3; ++i)
    {
        viewProj[i] = proj[i] * viewProj[i];
        viewProj[i + 3] = glm::inverse(viewProj[i]);
    }
    return viewProj;
}