#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/core/layer.h>
#include <loom/renderer/render_command.h>

class ExampleLayer : public Loom::Layer {
public:
    ExampleLayer() : Layer("Example") {}

    void OnUpdate() override {
        Loom::RenderCommand::SetClearColor(0.2f, 0.2f, 0.8f, 1.0f);
        Loom::RenderCommand::Clear();
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