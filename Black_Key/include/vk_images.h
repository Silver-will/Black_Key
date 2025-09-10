#pragma once 
#include<vulkan/vulkan.h>
#include "vk_types.h"

class VulkanEngine;

namespace vkutil {

	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
	void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize, VkImageBlit2* region = VK_NULL_HANDLE);
	void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent3D imageSize, int faces = 1);
	AllocatedImage create_image_empty(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VulkanEngine* engine, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, bool mipmapped = false, int layers = 1, VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT, int mipLevels=-1 );
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, VulkanEngine* engine, bool mipmapped = false);
	void destroy_image(const AllocatedImage& img, VulkanEngine* engine);
	AllocatedImage load_cubemap_image(std::string_view path, VkExtent3D size, VulkanEngine* engine, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_cubemap_image(VkExtent3D size, VulkanEngine* engine, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_array_image(VkExtent3D size, VulkanEngine* engine, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	
};