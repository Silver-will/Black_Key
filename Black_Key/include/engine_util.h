#ifndef ENGINE_UTIL
#define ENGINE_UTIL
#include "vk_types.h"
#include "vk_descriptors.h"

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct IBLData {
	AllocatedImage _lutBRDF;
	AllocatedImage _irradianceCube;
	AllocatedImage _preFilteredCube;
	VkSampler _irradianceCubeSampler;
	VkSampler _lutBRDFSampler;
};

namespace BlackKey{
	glm::vec4 Vec3Tovec4(glm::vec3 v, float fill = FLT_MAX);
	glm::vec4 roundVec4(glm::vec4 v);
	glm::vec4 NormalizePlane(glm::vec4 p);
    uint32_t PreviousPow2(uint32_t v);
    uint32_t GetImageMipLevels(uint32_t width, uint32_t height);

	struct BloomMip {
		glm::vec2 size;
		glm::ivec2 i_size;
		AllocatedImage mip;
	};

	struct FrameData {

		VkCommandPool _commandPool;
		VkCommandBuffer _mainCommandBuffer;

		VkSemaphore _swapchainSemaphore, _renderSemaphore;
		VkFence _renderFence;

		DeletionQueue _deletionQueue;
		DescriptorAllocatorGrowable _frameDescriptors;

		DescriptorAllocator bindless_material_descriptor;
	};

}

std::string GetAssetPath();
#endif