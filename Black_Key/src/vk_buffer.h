#pragma once
#include "vk_types.h"

class VulkanEngine;

namespace vkutil {
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,VulkanEngine* engine);
	void destroy_buffer(const AllocatedBuffer& buffer, VulkanEngine* engine);
}

