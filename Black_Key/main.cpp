#include "vk_engine.h"
#include "Renderers/clustered_forward_renderer.h"
#include "Renderers/voxel_cone_tracing_renderer.h"
#include <memory>

int main(int argc, char* argv[])
{
	auto engine = std::make_shared<VulkanEngine>();
	
	std::unique_ptr<BaseRenderer> VXGIDemo = std::make_unique<VoxelConeTracingRenderer>();
	
	VXGIDemo->Init(engine.get());
	VXGIDemo->Run();
	VXGIDemo->Cleanup();
	engine->cleanup();	

	return 0;
}
