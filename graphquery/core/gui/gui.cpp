#include "gui.hpp"
#include "log/loggers/log_stdo.hpp"

#include <cstdio>
#include <algorithm>
#include <imnodes.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <log/logger.hpp>
#include <log/loggers/log_gui.hpp>

#include <gui/frames/frame_output.hpp>
#include <gui/frames/frame_dock.hpp>
#include <gui/frames/frame_menubar.hpp>

static int Initialise_GLFW(const char *);
static int Initialise_IMGUI();
static int Initialise_Nodes_Editor();
static int Initialise_Frames();
static void Render_Frames();
static void Clean_Up();
static void On_Update();

bool frame_dock_open = false;
bool frame_output_open = true;

graphquery::logger::CLogSystem log__;

int graphquery::gui::Initialise(const char* window_name)
{
    int valid = Initialise_GLFW(window_name);
    valid |= Initialise_IMGUI();
    valid |= Initialise_Nodes_Editor();
    valid |= Initialise_Frames();

    log__.Add_Logger(new graphquery::logger::CLogGUI());
    log__.Add_Logger(new graphquery::logger::CLogSTDO());

    return valid;
}

void graphquery::gui::Render()
{
    while(glfwWindowShouldClose(*graphquery::gui::_window) == 0)
    {
        On_Update();
    }
    Clean_Up();
}

int Initialise_GLFW(const char * window_name)
{
    glfwSetErrorCallback([](int error, const char* desc)
    {
        fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
    });

    if(glfwInit() == GLFW_FALSE) return 1;

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
#else
    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
#endif

    // Create window with graphics context
    graphquery::gui::_window = std::make_unique<GLFWwindow *>(glfwCreateWindow(1280, 720, window_name, nullptr, nullptr));

    if(graphquery::gui::_window == nullptr)
    {
        return 1;
    }

    glfwMakeContextCurrent(*graphquery::gui::_window);
    glfwSwapInterval(1); // Enable vsync

    return 0;
}

int Initialise_IMGUI() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(*graphquery::gui::_window, true);

    if(!ImGui_ImplOpenGL3_Init(IMGUI_GL_VERSION))
        return 1;

    return 0;
}

int Initialise_Nodes_Editor()
{
    ImNodes::CreateContext();
    ImNodes::StyleColorsDark();
    ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

    ImNodesIO& io = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle& style = ImNodes::GetStyle();
    style.Flags |= ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;

    return 0;
}

int Initialise_Frames()
{
    // Background dock frame
    graphquery::gui::_frames.emplace_back(std::make_unique<graphquery::gui::IFrame *>(new graphquery::gui::CFrameDock(* graphquery::gui::_window, frame_dock_open)));

    // Menu bar
    graphquery::gui::_frames.emplace_back(std::make_unique<graphquery::gui::IFrame *>(new graphquery::gui::CFrameMenuBar()));

    // Log output frame
    graphquery::gui::_frames.emplace_back(std::make_unique<graphquery::gui::IFrame *>(new graphquery::gui::CFrameLog()));
    return 0;
}

static void Render_Frames()
{
    std::for_each(graphquery::gui::_frames.begin(),
                  graphquery::gui::_frames.end(),
                  [] (std::unique_ptr<graphquery::gui::IFrame *> & frame) {
                        (*frame)->Render_Frame();
                  });
}

static void On_Update()
{
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    Render_Frames();

    // Rendering
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(*graphquery::gui::_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(*graphquery::gui::_window);
}

static void Clean_Up()
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(*graphquery::gui::_window);
    glfwTerminate();
}