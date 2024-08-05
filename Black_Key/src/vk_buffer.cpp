#include "vk_buffer.h"
#include "vk_engine.h"

AllocatedBuffer vkutil::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VulkanEngine* engine)
{
	// allocate buffer
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer;


	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(engine->_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));

	return newBuffer;
}

void vkutil::destroy_buffer(const AllocatedBuffer& buffer, VulkanEngine* engine)
{
	vmaDestroyBuffer(engine->_allocator, buffer.buffer, buffer.allocation);
}