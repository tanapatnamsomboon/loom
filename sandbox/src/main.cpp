#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/core/log.h>
#include <loom/renderer/render_command.h>
#include <loom/scene/scene.h>
#include <loom/scene/entity.h>
#include <loom/scene/components.h>

class ExampleLayer : public Loom::Layer {
public:
    ExampleLayer() : Layer("Example") {
        mScene = std::make_shared<Loom::Scene>();

        mSquareEntity = mScene->CreateEntity("Green Square");
        mSquareEntity.AddComponent<Loom::SpriteRendererComponent>(glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f });

        mCameraEntity = mScene->CreateEntity("Camera");
        mCameraEntity.AddComponent<Loom::CameraComponent>();

        auto& cc = mCameraEntity.GetComponent<Loom::CameraComponent>();
        cc.Camera.SetViewportSize(1280, 720);
    }

    void OnUpdate(Loom::Timestep ts) override {
        auto& camera_transform = mCameraEntity.GetComponent<Loom::TransformComponent>();
        float speed = 5.0f * ts;

        if (Loom::Input::IsKeyPressed('A')) camera_transform.Translation.x -= speed;
        if (Loom::Input::IsKeyPressed('D')) camera_transform.Translation.x += speed;
        if (Loom::Input::IsKeyPressed('W')) camera_transform.Translation.y += speed;
        if (Loom::Input::IsKeyPressed('S')) camera_transform.Translation.y -= speed;

        Loom::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Loom::RenderCommand::Clear();

        mScene->OnUpdate(ts);
    }

private:
    std::shared_ptr<Loom::Scene> mScene;
    Loom::Entity mSquareEntity;
    Loom::Entity mCameraEntity;
};

int main() {
    Loom::Log::Init();
    LOOM_INFO("Sandbox initialized!");

    Loom::Application app;
    app.PushLayer(new ExampleLayer());
    app.Run();
    return 0;
}