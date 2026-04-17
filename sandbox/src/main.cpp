#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/core/log.h>
#include <loom/renderer/orthographic_camera.h>
#include <loom/renderer/render_command.h>
#include <loom/renderer/renderer_2d.h>
#include <imgui.h>

class ExampleLayer : public Loom::Layer {
public:
    ExampleLayer()
        : Layer("Example"), mCamera(-1.6f, 1.6f, -0.9f, 0.9f), mCameraPosition(0.0f) {
        mTexture.reset(Loom::Texture2D::Create("assets/textures/box.png"));
    }

    void OnAttach() override {
        ImGuiContext* context;
        ImGuiMemAllocFunc alloc_func;
        ImGuiMemFreeFunc free_func;
        void* user_data;

        Loom::Application::Get().GetImGuiLayer()->GetContextAndAllocators(&context, &alloc_func, &free_func, &user_data);

        ImGui::SetCurrentContext(context);
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);
    }

    void OnUpdate(Loom::Timestep ts) override {
        if (Loom::Input::IsKeyPressed('A'))
            mCameraPosition.x -= mCameraMoveSpeed * ts;
        else if (Loom::Input::IsKeyPressed('D'))
            mCameraPosition.x += mCameraMoveSpeed * ts;

        if (Loom::Input::IsKeyPressed('W'))
            mCameraPosition.y += mCameraMoveSpeed * ts;
        else if (Loom::Input::IsKeyPressed('S'))
            mCameraPosition.y -= mCameraMoveSpeed * ts;

        mCamera.SetPosition(mCameraPosition);

        Loom::RenderCommand::SetClearColor(0.2f, 0.2f, 0.8f, 1.0f);
        Loom::RenderCommand::Clear();
        Loom::Renderer2D::BeginScene(mCamera);
        Loom::Renderer2D::DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, mTexture);
        Loom::Renderer2D::EndScene();
    }

    void OnImGuiRender() override {
        ImGui::Begin("Settings");
        ImGui::Text("Hello Loom Engine!");
        ImGui::End();
    }

private:
    std::shared_ptr<Loom::Texture2D> mTexture;
    Loom::OrthographicCamera mCamera;
    glm::vec3 mCameraPosition;
    float mCameraMoveSpeed = 1.0f;
};

int main() {
    Loom::Log::Init();
    LOOM_INFO("Sandbox initialized!");

    Loom::Application app;
    app.PushLayer(new ExampleLayer());
    app.Run();
    return 0;
}