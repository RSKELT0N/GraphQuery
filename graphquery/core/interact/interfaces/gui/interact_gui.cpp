#include "interact_gui.h"

#include "db/system.h"
#include "interact/interfaces/gui/frames/frame_dock.h"
#include "interact/interfaces/gui/frames/frame_graph_db.h"
#include "interact/interfaces/gui/frames/frame_graph_visual.h"
#include "interact/interfaces/gui/frames/frame_menubar.h"
#include "interact/interfaces/gui/frames/frame_output.h"
#include "interact/interfaces/gui/frames/frame_db_query.h"
#include "interact/interfaces/gui/frames/frame_db_analytic.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "GLFW/glfw3.h"

#include <algorithm>
#include <cstdio>

graphquery::interact::CInteractGUI::
CInteractGUI()
{
    initialise_glfw();
    initialise_imgui();
    initialise_nodes_editor();
}

void
graphquery::interact::CInteractGUI::render() noexcept
{
    initialise_frames();
    [[likely]] while (glfwWindowShouldClose(m_window) == 0)
    {
        on_update();
    }
    clean_up();
}

void
graphquery::interact::CInteractGUI::initialise_glfw() noexcept
{
    glfwSetErrorCallback([](int error, const char * desc) { fprintf(stderr, "GLFW Error %d: %s\n", error, desc); });

    if (glfwInit() == GLFW_FALSE)
        return;

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
#endif

    // Create window with graphics context
    m_window = glfwCreateWindow(1280, 720, PROJECT_NAME, nullptr, nullptr);

    [[unlikely]] if (m_window == nullptr)
    {
        return;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync
}

void
graphquery::interact::CInteractGUI::initialise_imgui() noexcept
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    m_imgui_context = ImGui::CreateContext();

    ImGuiIO & io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle & style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);

    if (!ImGui_ImplOpenGL3_Init(IMGUI_GL_VERSION))
        [[unlikely]]
            database::_log_system->error("Error initialising OpenGl for ImGUI.");
}

void
graphquery::interact::CInteractGUI::initialise_nodes_editor() noexcept
{
    m_imnodes_context = ImNodes::CreateContext();
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
                                                          database::_db_storage->get_is_graph_loaded(),
                                                          database::_db_storage->get_graph_table()));

    // Log output frame
    const auto frame_log = std::make_shared<CFrameLog>();
    auto frame_logger    = m_frames.emplace_back(frame_log);
    database::_log_system->add_logger(frame_log);

    // Graph DB
    m_frames.emplace_back(std::make_unique<CFrameGraphDB>(database::_db_storage->get_is_db_loaded(),
                                                          database::_db_storage->get_is_graph_loaded(),
                                                          database::_db_storage->get_graph(),
                                                          database::_db_storage->get_graph_table()));

    // Graph visual
    m_frames.emplace_back(std::make_unique<CFrameGraphVisual>(database::_db_storage->get_is_db_loaded(),
                                                          database::_db_storage->get_is_graph_loaded(),
                                                          database::_db_storage->get_graph()));

    // DB Query
    m_frames.emplace_back(std::make_unique<CFrameDBQuery>(database::_db_storage->get_is_db_loaded(),
                                                          database::_db_storage->get_is_graph_loaded(),
                                                          database::_db_storage->get_graph(),
                                                          database::_db_query->get_result_table(),
                                                          database::_db_analytic->get_algorithm_table()));

    // DB Analytic
    m_frames.emplace_back(std::make_unique<CFrameDBAnalytic>(database::_db_storage->get_is_db_loaded(),
                                                          database::_db_storage->get_is_graph_loaded(),
                                                          database::_db_storage->get_graph(),
                                                          database::_db_analytic->get_result_table(),
                                                          database::_db_analytic->get_algorithm_table()));
}

void
graphquery::interact::CInteractGUI::render_frames() noexcept
{
    std::for_each(m_frames.begin(), m_frames.end(), [](auto & frame) { frame->render_frame(); });
}

void
graphquery::interact::CInteractGUI::on_update() noexcept
{
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetCurrentContext(m_imgui_context);
    ImNodes::SetCurrentContext(m_imnodes_context);

    render_frames();

    // Rendering
    ImGui::Render();

    int display_w;
    int display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    [[likely]] if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow * backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(m_window);
}

void
graphquery::interact::CInteractGUI::clean_up() noexcept
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImNodes::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}
