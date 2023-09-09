#include <fmt/color.h>
#include <imgui.h>

int main([[maybe_unused]]int argc, [[maybe_unused]]char** argv) {

    ImGui::Begin("Window A");
    ImGui::Text("This is window A");
    ImGui::End();
    
    ImGui::Begin("Window B");
    ImGui::Text("This is window B");
    ImGui::End();
    
    ImGui::Begin("Window A");
    ImGui::Button("Button on window A");
    ImGui::End();
    
    ImGui::Begin("Window B");
    ImGui::Button("Button on window B");
    ImGui::End();

    return 0;
}
