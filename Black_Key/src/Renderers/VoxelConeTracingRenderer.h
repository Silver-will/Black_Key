#pragma once
#include "base_renderer.h"
#include "../vk_engine.h"

struct VoxelConeTracingRenderer : public BaseRenderer
{
	void Init(VulkanEngine* engine) override;

	void Cleanup() = 0;

	void Draw() = 0;
	void DrawUI() = 0;

	void Run() = 0;
	void UpdateScene() = 0;
	void LoadAssets() = 0;
	void InitImgui() = 0;
};

