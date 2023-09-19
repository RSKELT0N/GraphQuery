#include <gui.hpp>

#include <cstdio>
#include <imnodes.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

static int Initialise_GLFW(const char *);
static int Initialise_IMGUI();
static int Initialise_Nodes_Editor();
static int Initialise_Frames();
static void Clean_Up();
static void On_Update();

int graphquery::gui::Initialise(const char* window_name)
{
    return Initialise_GLFW(window_name) |
           Initialise_IMGUI() |
           Initialise_Nodes_Editor() |
           Initialise_Frames();
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
    graphquery::gui::_window = glfwCreateWindow(1280, 720, window_name, nullptr, nullptr);

    if(graphquery::gui::_window == nullptr)
    {
        return 1;
    }

    glfwMakeContextCurrent(graphquery::gui::_window);
    glfwSwapInterval(1); // Enable vsync

    return 0;
}

int Initialise_IMGUI()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(graphquery::gui::_window, true);

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
    return 0;
}

void graphquery::gui::Render()
{
    while(glfwWindowShouldClose(graphquery::gui::_window) == 0)
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

    for(int i = 0; i < 10; i++)
    {
        ImNodes::BeginNode(i);

        ImNodes::BeginOutputAttribute(i);
        ImGui::Text("Opin %d", i);
        ImNodes::EndOutputAttribute();

        ImNodes::BeginInputAttribute(i+9);
        ImGui::Text("Ipin %d", i*2);
        ImNodes::EndInputAttribute();

        ImNodes::EndNode();
    }

    ImNodes::EndNodeEditor();
    ImGui::End();

    static int count = 0;
    if(ImGui::Begin("Test")) {}
    ImGui::Text("%s: %d", "Count", count++);
    ImGui::End();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(graphquery::gui::_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    glClearColor(graphquery::gui::_background.x * graphquery::gui::_background.w,
                 graphquery::gui::_background.y * graphquery::gui::_background.w,
                 graphquery::gui::_background.z * graphquery::gui::_background.w,
                 graphquery::gui::_background.w);

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

    glfwSwapBuffers(graphquery::gui::_window);
}

static void Clean_Up()
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(graphquery::gui::_window);
    glfwTerminate();
}