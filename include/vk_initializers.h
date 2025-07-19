// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"

namespace vkinit {
//> init_cmd
VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1);
VkBufferMemoryBarrier buffer_barrier(VkBuffer buffer, uint32_t queue);
VkBufferMemoryBarrier2 buffer_barrier_2(VkBuffer buffer, uint32_t queue);
VkImageMemoryBarrier image_barrier(VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask);
//< init_cmd

VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);
VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);

VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo);
VkPresentInfoKHR present_info();

VkRenderingAttachmentInfo attachment_info(VkImageView view, VkImageView* resolve, VkClearValue* clear, VkImageLayout layout, bool clear_on_load = false);

VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, VkImageLayout layout, VkAttachmentLoadOp loadMode = VK_ATTACHMENT_LOAD_OP_CLEAR, VkAttachmentStoreOp storeMode = VK_ATTACHMENT_STORE_OP_STORE);

VkRenderingAttachmentInfo resolve_attachment_info(VkImageView view, VkClearValue* clear, VkImageLayout layout);

VkRenderingInfo rendering_info(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
    VkRenderingAttachmentInfo* depthAttachment, int renderLayers = 1);

VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);

VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags,
    uint32_t binding);
VkDescriptorSetLayoutCreateInfo descriptorset_layout_create_info(VkDescriptorSetLayoutBinding* bindings,
    uint32_t bindingCount);
VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet,
    VkDescriptorImageInfo* imageInfo, uint32_t binding);
VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet,
    VkDescriptorBufferInfo* bufferInfo, uint32_t binding);
VkDescriptorBufferInfo buffer_info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
VkSamplerCreateInfo create_sampler(VkFilter filter, VkSamplerMipmapMode mipMode, VkSamplerAddressMode addressMode, int mipCount);

VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, int layers = 1,VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
VkImageCreateInfo image_cubemap_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, uint32_t mipCount);
VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, VkImageViewType viewType, int layerCount = 1);
VkImageViewCreateInfo imageview_cubemap_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
VkPipelineLayoutCreateInfo pipeline_layout_create_info();
VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage,
    VkShaderModule shaderModule,
    const char * entry = "main");
VkVertexInputBindingDescription vertex_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input);
VkVertexInputAttributeDescription vertex_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);
VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_create_info(std::vector<VkVertexInputBindingDescription>& bindings, std::vector<VkVertexInputAttributeDescription>& attributes);
VkSampleCountFlagBits getMaxAvailableSampleCount(VkPhysicalDeviceProperties& deviceProperties);
} // namespace vkinit
