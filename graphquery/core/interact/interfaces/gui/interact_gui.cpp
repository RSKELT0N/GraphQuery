#include "interact_gui.h"

#include <algorithm>
#include <cstdio>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "db/system.h"
#include "imnodes.h"
#include "interact/interfaces/gui/frames/frame_dock.h"
#include "interact/interfaces/gui/frames/frame_graph_db.h"
#include "interact/interfaces/gui/frames/frame_graph_visual.h"
#include "interact/interfaces/gui/frames/frame_menubar.h"
#include "interact/interfaces/gui/frames/frame_output.h"

graphquery::interact::CInteractGUI::CInteractGUI()
{
    initialise_glfw();
    initialise_imgui();
    initialise_nodes_editor();
}

void
graphquery::interact::CInteractGUI::render() noexcept
{
    initialise_frames();
    [[likely]] while (glfwWindowShouldClose(*m_window) == 0)
    {
        on_update();
    }
    clean_up();
}

void
graphquery::interact::CInteractGUI::initialise_glfw() noexcept
{
    glfwSetErrorCallback([](int error, const char * desc)
                         { fprintf(stderr, "GLFW Error %d: %s\n", error, desc); });

    if (glfwInit() == GLFW_FALSE)
        return;

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
    m_window = std::make_unique<GLFWwindow *>(glfwCreateWindow(1280, 720, PROJECT_NAME, nullptr, nullptr));

    [[unlikely]] if (m_window == nullptr)
    {
        return;
    }

    glfwMakeContextCurrent(*m_window);
    glfwSwapInterval(1); // Enable vsync
}

void
graphquery::interact::CInteractGUI::initialise_imgui() noexcept
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO & io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle & style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(*m_window, true);

    if (!ImGui_ImplOpenGL3_Init(IMGUI_GL_VERSION)) [[unlikely]]
        database::_log_system->error("Error initialising OpenGl for ImGUI.");
}

void
graphquery::interact::CInteractGUI::initialise_nodes_editor() noexcept
{
    ImNodes::CreateContext();
    ImNodes::StyleColorsDark();
    ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkCreationOnSnap);

    ImNodesIO & io                          = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier      = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle & style = ImNodes::GetStyle();
    style.Flags |= ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;
}

void
graphquery::interact::CInteractGUI::initialise_frames() noexcept
{
    // Background dock frame
    m_frames.emplace_back(std::make_unique<CFrameDock>(m_frame_dock_open));

    // Menu bar
    m_frames.emplace_back(std::make_unique<CFrameMenuBar>(database::_db_storage->get_is_db_loaded(),
                                                          database::_db_storage->get_graph_table()));

    // Log output frame
    auto frame_log = std::make_shared<CFrameLog>();
    m_frames.emplace_back(frame_log);
    database::_log_system->add_logger(frame_log);

    // Graph DB
    m_frames.emplace_back(std::make_unique<CFrameGraphDB>(database::_db_storage->get_is_db_loaded(),
                                                          database::_db_storage->get_graph_table()));

    // Graph visual
    m_frames.emplace_back(std::make_unique<CFrameGraphVisual>());
}

void
graphquery::interact::CInteractGUI::render_frames() noexcept
{
    std::for_each(m_frames.begin(), m_frames.end(), [](auto & frame)
                  { frame->render_frame(); });
}

void
graphquery::interact::CInteractGUI::on_update() noexcept
{
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    render_frames();

    // Rendering
    ImGui::Render();

    int display_w;
    int display_h;
    glfwGetFramebufferSize(*m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we
    // save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call
    //  glfwMakeContextCurrent(window) directly)
    [[likely]] if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow * backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(*m_window);
}

void
graphquery::interact::CInteractGUI::clean_up() noexcept
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImNodes::DestroyContext();

    glfwDestroyWindow(*m_window);
    glfwTerminate();
}
