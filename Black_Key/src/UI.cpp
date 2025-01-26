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
    // Demonstrate the various window flags. Typically you would just use the default!
    static bool no_titlebar = false;
    static bool no_scrollbar = false;
    static bool no_menu = false;
    static bool no_move = false;
    static bool no_resize = false;
    static bool no_collapse = false;
    static bool no_close = false;
    static bool no_nav = false;
    static bool no_background = false;
    static bool no_bring_to_front = false;
    static bool no_docking = false;
    static bool unsaved_document = false;

    ImGuiWindowFlags window_flags = 0;
    if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
    if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
    if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
    if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
    if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
    if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
    if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
    if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
    if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    //if (no_docking)         window_flags |= ImGuiWindowFlags_NoDocking;
    if (unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;

    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

    bool p_open = true;
    if (!ImGui::Begin("Black key", &p_open, window_flags))
    {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Lighting"))
    {
        std::string index{};
        if (ImGui::TreeNode("Point Lights"))
        {
            for (size_t i = 0; i < engine->pointData.pointLights.size(); i++)
            {
                index = std::to_string(i);
                if (ImGui::TreeNode(("Point "s + index).c_str()))
                {
                    if (ImGui::TreeNode("Position"))
                    {
                        float pos[3] = { engine->pointData.pointLights[i].position.x, engine->pointData.pointLights[i].position.y, engine->pointData.pointLights[i].position.z };
                        ImGui::SliderFloat3("x,y,z", pos,-15.0f,15.0f);
                        engine->pointData.pointLights[i].position = glm::vec4(pos[0], pos[1], pos[2],0.0f);
                        ImGui::TreePop();
                        ImGui::Spacing();
                    }
                    if (ImGui::TreeNode("Color"))
                    {
                        float col[4] = { engine->pointData.pointLights[i].color.x, engine->pointData.pointLights[i].color.y, engine->pointData.pointLights[i].color.z, engine->pointData.pointLights[i].color.w };
                        ImGui::ColorEdit4("Light color", col);
                        engine->pointData.pointLights[i].color = glm::vec4(col[0], col[1], col[2], col[3]);
                        ImGui::TreePop();
                        ImGui::Spacing();
                    }
                    
                    if (ImGui::TreeNode("Attenuation"))
                    {
                        //move this declaration to a higher scope later
                        ImGui::SliderFloat("Range", &engine->pointData.pointLights[i].range, 0.0f, 1.0f);
                        ImGui::SliderFloat("Intensity", &engine->pointData.pointLights[i].intensity, 0.0f, 1.0f);
                        //ImGui::SliderFloat("quadratic", &points[i].quadratic, 0.0f, 2.0f);
                        //ImGui::SliderFloat("radius", &points[i].quadratic, 0.0f, 100.0f);
                        ImGui::TreePop();
                        ImGui::Spacing();

                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Direct Light"))
        {
            ImGui::SeparatorText("direction");
            float pos[3] = { engine->directLight.direction.x, engine->directLight.direction.y, engine->directLight.direction.z };
            ImGui::SliderFloat3("x,y,z", pos,-7,7);
            engine->directLight.direction = glm::vec4(pos[0], pos[1], pos[2],0.0f);

            ImGui::SeparatorText("color");
            float col[4] = { engine->directLight.color.x, engine->directLight.color.y, engine->directLight.color.z, engine->directLight.color.w};
            ImGui::ColorEdit4("Light color", col);
            engine->directLight.color = glm::vec4(col[0], col[1], col[2], col[3]);
            ImGui::TreePop();
        }
    }
    if (ImGui::CollapsingHeader("Debugging"))
    {
        ImGui::Checkbox("Visualize shadow cascades", &engine->debugShadowMap);
    }

    if (ImGui::CollapsingHeader("Engine Stats"))
    {
        ImGui::SeparatorText("Render timings");
        ImGui::Text("FPS %f ", 1000.0f / engine->stats.frametime);
        ImGui::Text("frametime %f ms", engine->stats.frametime);
        ImGui::Text("drawtime %f ms", engine->stats.mesh_draw_time);
        ImGui::Text("triangles %i", engine->stats.triangle_count);
        ImGui::Text("draws %i", engine->stats.drawcall_count);
        ImGui::Text("UI render time %f ms", engine->stats.ui_draw_time);
        ImGui::Text("Update time %f ms", engine->stats.update_time);
        ImGui::Text("Shadow Pass time %f ms", engine->stats.shadow_pass_time);
    }
    ImGui::End();
    /*
    if (ImGui::CollapsingHeader("Advanced Options"))
    {
        ImGui::SeparatorText("Gamma correction");
        ImGui::SliderFloat("Gamma", &Light_values::gamma, 2.0, 3.0f);
        ImGui::Spacing();
        ImGui::SeparatorText("HDR");
        ImGui::Checkbox("HDR", &Light_values::hdr);
        ImGui::Spacing();
        ImGui::SeparatorText("Bloom");
        ImGui::Checkbox("Bloom", &Light_values::bloom);
        ImGui::Spacing();
        ImGui::SliderFloat("Exposure", &Light_values::exposure, 0.0, 15.0f);
        ImGui::Spacing();
    }
    */
   
}


void SetImguiTheme(float alpha)
{
    ImGuiStyle& style = ImGui::GetStyle();

    // light style from Pacôme Danhiez (user itamago) https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
    style.Alpha = 1.0f;
    style.FrameRounding = 3.0f;
    style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.2f, 0.2f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    //ImGuiCol_::hove
    
    for (int i = 0; i <= ImGuiCol_COUNT; i++)
    {
        ImVec4& col = style.Colors[i];
        if (col.w < 1.00f)
        {
            col.x *= alpha;
            col.y *= alpha;
            col.z *= alpha;
            col.w *= alpha;
        }
    }
}

