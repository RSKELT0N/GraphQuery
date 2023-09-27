#pragma once

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#define IMGUI_GL_VERSION "#version 400"
#else
#define IMGUI_GL_VERSION "#version 130"
#endif

#include "frame.hpp"

#include <memory>
#include <vector>
#include "imgui.h"
#include "GLFW/glfw3.h"

namespace graphquery::gui
{
    void Render();
    [[nodiscard]] int Initialise(const char* window_name);

    inline std::vector<std::unique_ptr<IFrame *> > _frames;
    inline std::unique_ptr<GLFWwindow*> _window;
}