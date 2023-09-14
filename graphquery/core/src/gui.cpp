#include <gui.hpp>

#include <cstdio>
#include <imnodes.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

static void Clean_Up();
static void On_Update();
static void NodesEditorInitialise();

int graphquery::gui::Initialise(const char* window_name)
{
    glfwSetErrorCallback([](int error, const char* desc)
    {
        fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
    });
    
    if(glfwInit() == GLFW_FALSE) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    // Create window with graphics context
    m_window = glfwCreateWindow(1280, 720, window_name, nullptr, nullptr);

    if(graphquery::gui::m_window == nullptr)
    {
        return 1;
    }

    glfwMakeContextCurrent(graphquery::gui::m_window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    NodesEditorInitialise();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImNodes::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(graphquery::gui::m_window, true);
    if(!ImGui_ImplOpenGL3_Init("#version 400")) return 1;

    return 0;
}

void NodesEditorInitialise()
{
    ImNodes::CreateContext();
    ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);

    ImNodesIO& io = ImNodes::GetIO();
    io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    io.MultipleSelectModifier.Modifier = &ImGui::GetIO().KeyCtrl;

    ImNodesStyle& style = ImNodes::GetStyle();
    style.Flags |= ImNodesStyleFlags_GridLinesPrimary | ImNodesStyleFlags_GridSnapping;
}

void graphquery::gui::Render()
{
    while(glfwWindowShouldClose(graphquery::gui::m_window) == 0)
    {
        On_Update();
    }
    Clean_Up();
}

static void On_Update()
{
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Show demo window
    ImGui::ShowDemoWindow();

    ImGui::Begin("node editor");
    ImNodes::BeginNodeEditor();

    ImNodes::BeginNode(1);

    const int output_attr_id = 2;
    ImNodes::BeginOutputAttribute(output_attr_id);
    // in between Begin|EndAttribute calls, you can call ImGui
    // UI functions
    ImGui::Text("output pin");
    ImNodes::EndOutputAttribute();

    ImNodes::EndNode();

    ImNodes::EndNodeEditor();
    ImGui::End();

    static int count = 0;
    if(ImGui::Begin("Test")) {}
    ImGui::Text("%s: %d", "Count", count++);
    ImGui::End();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(graphquery::gui::m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    glClearColor(graphquery::gui::m_background.x * graphquery::gui::m_background.w,
                 graphquery::gui::m_background.y * graphquery::gui::m_background.w,
                 graphquery::gui::m_background.z * graphquery::gui::m_background.w,
                 graphquery::gui::m_background.w);

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

    glfwSwapBuffers(graphquery::gui::m_window);
}

static void Clean_Up()
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(graphquery::gui::m_window);
    glfwTerminate();
}