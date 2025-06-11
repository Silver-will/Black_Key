#include "vk_engine.h"
#include "Renderers/clustered_forward_renderer.h"
#include <memory>

int main(int argc, char* argv[])
{
	/*VulkanEngine engine;
	engine.init();

	std::unique_ptr<BaseRenderer> clusteredLightingDemo = std::make_unique<ClusteredForwardRenderer>();
	clusteredLightingDemo->Init(&engine);
	clusteredLightingDemo->Run();
	clusteredLightingDemo->Cleanup();

	//engine.run();	
	engine.cleanup();	
	*/

	VulkanEngine engine;

	engine.init();

	engine.run();

	engine.cleanup();

	return 0;
}
