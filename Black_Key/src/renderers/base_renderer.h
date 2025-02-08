#pragma once
#include "../vk_engine.h"
#include "../render_graph.h"

struct BaseRenderer {
	BaseRenderer() {}
	virtual void init(VulkanEngine* engine) = 0;

	//shuts down the engine
	virtual void cleanup() = 0;

	//draw loop
	virtual void draw() = 0;
	virtual void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) = 0;

	//run main loop
	virtual void run() = 0;
	virtual void update_scene() = 0;

	black_key::RenderGraph render_graph;
	black_key::RenderGraphBuilder render_graph_builder;
};