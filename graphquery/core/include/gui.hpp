#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace graphquery::gui
{
    void Initialise(const char* window_name);
    void Run();

    inline GLFWwindow* m_window = nullptr;
    inline constexpr ImVec4 m_background = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
}