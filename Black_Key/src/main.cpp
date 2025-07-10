#include "vk_engine.h"
#include "Renderers/clustered_forward_renderer.h"
#include <memory>

int main(int argc, char* argv[])
{
	auto engine = std::make_shared<VulkanEngine>();
	
	std::unique_ptr<BaseRenderer> clusteredLightingDemo = std::make_unique<ClusteredForwardRenderer>();
	clusteredLightingDemo->Init(engine.get());
	clusteredLightingDemo->Run();
	clusteredLightingDemo->Cleanup();
	engine->cleanup();	
	

	/*
	VulkanEngine engine;

	engine.init();

	engine.run();

	engine.cleanup();
	*/
	return 0;
}
