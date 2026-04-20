#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/core/log.h>
#include <loom/renderer/render_command.h>
#include <loom/scene/scene.h>
#include <loom/scene/entity.h>
#include <loom/scene/components.h>

class CameraController : public Loom::ScriptableEntity {
public:
    void OnCreate() override {
        auto& name = GetComponent<Loom::TagComponent>().Tag;
        LOOM_INFO("Camera '{0}' created!", name);
    }

    void OnDestroy() override {
    }

    void OnUpdate(Loom::Timestep ts) override {
        auto& transform = GetComponent<Loom::TransformComponent>();
        float speed = 5.0f * ts;

        if (Loom::Input::IsKeyPressed('A')) transform.Translation.x -= speed;
        if (Loom::Input::IsKeyPressed('D')) transform.Translation.x += speed;
        if (Loom::Input::IsKeyPressed('W')) transform.Translation.y += speed;
        if (Loom::Input::IsKeyPressed('S')) transform.Translation.y -= speed;
    }
};

class ExampleLayer : public Loom::Layer {
public:
    ExampleLayer() : Layer("Example") {
        mScene = std::make_shared<Loom::Scene>();

        mSquareEntity = mScene->CreateEntity("Green Square");
        mSquareEntity.AddComponent<Loom::SpriteRendererComponent>(glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f });

        mCameraEntity = mScene->CreateEntity("Main Camera");
        mCameraEntity.AddComponent<Loom::CameraComponent>();
        mCameraEntity.AddComponent<Loom::NativeScriptComponent>().Bind<CameraController>();

        auto& cc = mCameraEntity.GetComponent<Loom::CameraComponent>();
        cc.Camera.SetViewportSize(1280, 720);
    }

    void OnUpdate(Loom::Timestep ts) override {
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