#ifndef ENGINE_UTIL
#define ENGINE_UTIL
#include "vk_types.h"
namespace BlackKey{
	glm::vec4 Vec3Tovec4(glm::vec3 v, float fill = FLT_MAX);
	glm::vec4 NormalizePlane(glm::vec4 p);
}

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

#endif