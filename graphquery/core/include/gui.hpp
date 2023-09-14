#pragma once

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace graphquery::gui
{
    void Render();
    int Initialise(const char* window_name);

    inline GLFWwindow* m_window = nullptr;
    inline constexpr ImVec4 m_background = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
}
