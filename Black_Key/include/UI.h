#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

class VulkanEngine;

void RenderUI(VulkanEngine* engine);

void SetImguiTheme(float alpha);