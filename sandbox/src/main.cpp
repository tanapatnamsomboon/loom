#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/core/layer.h>
#include <loom/imgui/imgui_layer.h>
#include <loom/renderer/render_command.h>
#include <imgui.h>

class ExampleLayer : public Loom::Layer {
public:
    ExampleLayer() : Layer("Example") {}

    void OnAttach() override {
        ImGuiContext* context;
        ImGuiMemAllocFunc alloc_func;
        ImGuiMemFreeFunc free_func;
        void* user_data;

        Loom::Application::Get().GetImGuiLayer()->GetContextAndAllocators(&context, &alloc_func, &free_func, &user_data);

        ImGui::SetCurrentContext(context);
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);
    }

    void OnUpdate() override {
        Loom::RenderCommand::SetClearColor(0.2f, 0.2f, 0.8f, 1.0f);
        Loom::RenderCommand::Clear();
    }

    void OnImGuiRender() override {
        static bool show = true;
        ImGui::ShowDemoWindow(&show);
    }
};

int main() {
    Loom::Log::Init();
    LOOM_INFO("Sandbox initialized!");

    Loom::Application app;
    app.PushLayer(new ExampleLayer());
    app.Run();
    return 0;
}