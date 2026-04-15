#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/core/layer.h>
#include <loom/core/input.h>

class ExampleLayer : public Loom::Layer {
public:
    ExampleLayer() : Layer("Example") {}

    void OnUpdate() override {
        if (Loom::Input::IsKeyPressed('A')) {
            LOOM_INFO("[ExampleLayer] Button A is being HELD!");
        }
    }

    void OnEvent(Loom::Event& event) override {
        LOOM_TRACE("[ExampleLayer] Event Captured: {0}", event.ToString());
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