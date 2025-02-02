#pragma once
#include<stack>
#include "vk_types.h"
#include "engine_util.h"
class VulkanEngine;

struct AllocateBufferInfo {

};

struct AllocateImageInfo {

};

struct ResourceManager {
	ResourceManager(VulkanEngine* engine);
	AllocatedBuffer AllocateBuffer(AllocateBufferInfo buffer_info);
	AllocatedBuffer AllocateImage(AllocateImageInfo buffer_info);
private:
	DeletionStack deletion_stack;
	VulkanEngine* engine;
};