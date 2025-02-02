#include "vk_types.h"
#include<stack>
namespace BlackKey{
	
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

//makes more sense yh
struct DeletionStack
{
	std::stack<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push(function);
	}

	void flush() {
		
		//Loop through stack executing all deletion functions
		while(!deletors.empty()) {
			auto func = deletors.top();
			func();
		}
	}
};