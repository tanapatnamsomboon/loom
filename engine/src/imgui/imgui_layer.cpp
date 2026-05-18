#include "loom/imgui/imgui_layer.h"
#include "loom/asset/font_manager.h"
#include "loom/core/application.h"
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace Loom {

    ImGuiLayer::ImGuiLayer()
        : Layer("ImGuiLayer") {}

    void ImGuiLayer::OnAttach() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        Application& app    = Application::Get();
        GLFWwindow*  window = (GLFWwindow*)app.GetWindow().GetNativeWindow();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460");

        FontManager::Init();
    }

    void ImGuiLayer::OnDetach() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::OnEvent(Event& event) {
        if (mBlockEvents) {
            ImGuiIO& io = ImGui::GetIO();
            event.mHandled |= event.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
            // Keyboard: only block when an actual text widget needs the keys
            // (WantTextInput), NOT when ImGui's keyboard nav has merely focused
            // a tree node or menu item (WantCaptureKeyboard). Otherwise editor
            // shortcuts like F-to-focus get swallowed after clicking an entity
            // in the hierarchy. Matches Unity/Unreal: shortcuts always reach
            // the editor unless a textbox is being typed into.
            event.mHandled |= event.IsInCategory(EventCategoryKeyboard) & io.WantTextInput;
        }
    }

    void ImGuiLayer::Begin() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End() {
        ImGuiIO&     io  = ImGui::GetIO();
        Application& app = Application::Get();
        io.DisplaySize   = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void ImGuiLayer::GetContextAndAllocators(ImGuiContext** context, ImGuiMemAllocFunc* alloc_func, ImGuiMemFreeFunc* free_func, void** user_data) {
        *context = ImGui::GetCurrentContext();

        ImGui::GetAllocatorFunctions(alloc_func, free_func, user_data);
    }

} // namespace Loom
