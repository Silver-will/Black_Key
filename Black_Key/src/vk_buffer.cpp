#include "vk_buffer.h"
#include "vk_engine.h"

//#define VMA_IMPLEMENTATION
//#define TRACY_ENABLE
//#include <vma/vk_mem_alloc.h>


//#define VMA_IMPLEMENTATION
//#define TRACY_ENABLE

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

/*
AllocatedBuffer vkutil::create_and_upload(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data, VulkanEngine* engine)
{
	AllocatedBuffer stagingBuffer = create_buffer(allocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, engine);

	void* bufferData = stagingBuffer.allocation->GetMappedData();

	memcpy(bufferData, data, allocSize);

	AllocatedBuffer DataBuffer = create_buffer(allocSize, usage, memoryUsage, engine);

	engine->immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy dataCopy{ 0 };
		dataCopy.dstOffset = 0;
		dataCopy.srcOffset = 0;
		dataCopy.size = allocSize;

		vkCmdCopyBuffer(cmd, stagingBuffer.buffer, DataBuffer.buffer, 1, &dataCopy);
		});

	destroy_buffer(stagingBuffer,engine);
	return DataBuffer;
}
*/

void vkutil::destroy_buffer(const AllocatedBuffer& buffer, VulkanEngine* engine)
{
	vmaDestroyBuffer(engine->_allocator, buffer.buffer, buffer.allocation);
}