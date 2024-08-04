#pragma once 


#include "stb_image.h"

#include<vulkan/vulkan.h>
namespace vkutil {

	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
	void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
	void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
	AllocatedImage create_cubemap_image(std::string_view path, VkExtent3D size, VmaAllocator& allocator, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
};