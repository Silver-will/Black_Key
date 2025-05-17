#pragma once
#include "../vk_engine.h"

#include "../vk_initializers.h"
#include "../vk_types.h"
#include "../input_handler.h"
#include "../vk_images.h"
#include "../vk_pipelines.h"
#include "../vk_buffer.h"
#include "../vk_loader.h"

struct BaseRenderer
{
	virtual void Init(VulkanEngine* engine)=0;

	virtual void Cleanup() = 0;

	virtual void Draw() = 0;
	virtual void DrawUI() = 0;

	virtual void Run() = 0;
	virtual void UpdateScene() = 0;
	virtual void LoadAssets() = 0;
	virtual void init_imgui()=0;
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	VulkanEngine* loaded_engine{ nullptr };
};

