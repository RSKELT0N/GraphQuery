#pragma once

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#define IMGUI_GL_VERSION "#version 400"
#else
#define IMGUI_GL_VERSION "#version 130"
#endif

#include <frame.hpp>

#include <vector>
#include <imgui.h>
#include <GLFW/glfw3.h>

namespace graphquery::gui
{
    void Render();
    int Initialise(const char* window_name);

    inline std::vector<IFrame *> * _frames;
    inline GLFWwindow* _window = nullptr;
    inline constexpr ImVec4 _background = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
}
