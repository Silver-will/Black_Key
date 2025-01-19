#pragma once
#include "vk_types.h"
#include <vma/vk_mem_alloc.h>

class VulkanEngine;

namespace vkutil {
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,VulkanEngine* engine);
	void destroy_buffer(const AllocatedBuffer& buffer, VulkanEngine* engine);
	AllocatedBuffer create_and_upload(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data, VulkanEngine* engine);
}

