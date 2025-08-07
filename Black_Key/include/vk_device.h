#pragma once
#include "vk_types.h"
class VulkanEngine;

namespace vk_device {
	void flush_command_buffer(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool, VulkanEngine* engine);
	VkCommandBuffer create_command_buffer(VkCommandBufferLevel level, VkCommandPool pool, VulkanEngine* engine);
}