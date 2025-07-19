#include "vk_device.h"
#include "vk_initializers.h"
#include "vk_engine.h"

VkCommandBuffer vk_device::create_command_buffer(VkCommandBufferLevel level, VkCommandPool pool, VulkanEngine* engine)
{
	auto alloc_info = vkinit::command_buffer_allocate_info(pool, 1);
	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(engine->_device, &alloc_info, &cmd));
	return cmd;
}

#define VK_FLAGS_NONE 0
void vk_device::flush_command_buffer(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool, VulkanEngine* engine)
{
	if (cmd == VK_NULL_HANDLE)
	{
		return;
	}

	VK_CHECK(vkEndCommandBuffer(cmd));

	auto cmdInfo = vkinit::command_buffer_submit_info(cmd);
	auto fenceInfo = vkinit::fence_create_info(VK_FLAGS_NONE);
	VkFence fence;
	VK_CHECK(vkCreateFence(engine->_device, &fenceInfo, nullptr, &fence));
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

	VK_CHECK(vkQueueSubmit2(engine->_graphicsQueue, 1, &submit, fence));
	VK_CHECK(vkWaitForFences(engine->_device, 1, &fence, VK_TRUE, 1000000000000));
}