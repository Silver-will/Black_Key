#include "input_handler.h"

void framebuffer_resize_callback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
	app->resize_requested = true;
	if(width == 0 || height == 0)
		app->stop_rendering = true;
	else
		app->stop_rendering = false;
}