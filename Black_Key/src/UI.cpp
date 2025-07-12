#include "UI.h"
#include "vk_engine.h"
#include <string>
#include <glm/glm.hpp>
using namespace std::literals::string_literals;

void setLights(glm::vec4& lightValues)
{
    ImGui::InputFloat("R", &lightValues[0], 0.00f, 1.0f);
    ImGui::InputFloat("G", &lightValues[1], 0.00f, 1.0f);
    ImGui::InputFloat("B", &lightValues[2], 0.00f, 1.0f);

}
void RenderUI(VulkanEngine* engine)
{
   
}

