// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include "vk_loader.h"
#include "vk_types.h"
#include "engine_util.h"
#include "vk_descriptors.h"
#include "vk_renderer.h"
//#include <vma/vk_mem_alloc.h>
#include "camera.h"
#include "engine_psos.h"
#include "shadows.h"
#include "Lights.h"
#include <chrono>
#include "scene_manager.h"
#include "resource_manager.h"
#include <ktxvulkan.h>

struct FrameData {

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors;
	
	DescriptorAllocator bindless_material_descriptor;
};
class VulkanEngine {
public:
	//initializes everything in the engine
	void init(VkPhysicalDeviceFeatures baseFeatures, VkPhysicalDeviceVulkan11Features features11, VkPhysicalDeviceVulkan12Features features12, VkPhysicalDeviceVulkan13Features features13);

	//shuts down the engine
	void cleanup();

	//draw loop

	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;// GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;


	bool _isInitialized{ false };

	VkExtent2D _windowExtent{ 1920,1080 };
	float _aspect_width = 1920;
	float _aspect_height = 1080;


	GLFWwindow* window{ nullptr };

	static VulkanEngine& Get();

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;
	VmaAllocator _allocator;
	
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	DeletionQueue _mainDeletionQueue;
	VkSampleCountFlagBits msaa_samples;

	
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
	//r
	void init_vulkan(VkPhysicalDeviceFeatures baseFeatures, VkPhysicalDeviceVulkan11Features features11, VkPhysicalDeviceVulkan12Features features12, VkPhysicalDeviceVulkan13Features features13);
	VkSampleCountFlagBits GetMSAASampleCount();
private:
	void init_imgui();
	void init_commands();
	void init_sync_structures();
	void destroy_swapchain();
};
