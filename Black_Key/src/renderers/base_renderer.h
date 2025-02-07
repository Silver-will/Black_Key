#pragma once

struct BaseRenderer {
	BaseRenderer() {}
	virtual void RenderFrame();
	virtual void RecreateSwapChainResource();
	virtual void ChangeView();
	virtual void RecreateSceneResources();
	virtual ~BaseRenderer();
};