#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/core/log.h>
#include <loom/renderer/render_command.h>
#include <loom/renderer/framebuffer.h>
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
        Loom::FramebufferSpecification fb_spec;
        fb_spec.Width = 1280;
        fb_spec.Height = 720;
        mFramebuffer = Loom::Framebuffer::Create(fb_spec);

        mScene = std::make_shared<Loom::Scene>();

        auto red_square = mScene->CreateEntity("Red Square");
        red_square.AddComponent<Loom::SpriteRendererComponent>(glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f });
        red_square.GetComponent<Loom::TransformComponent>().Translation.x = 1.0f;

        mCameraEntity = mScene->CreateEntity("Main Camera");
        mCameraEntity.AddComponent<Loom::CameraComponent>();
        mCameraEntity.AddComponent<Loom::NativeScriptComponent>().Bind<CameraController>();

        auto& cc = mCameraEntity.GetComponent<Loom::CameraComponent>();
        cc.Camera.SetViewportSize(1280, 720);

        mHierarchyPanel.SetContext(mScene);
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
        if (Loom::FramebufferSpecification spec = mFramebuffer->GetSpecification();
            mViewportSize.x > 0.0f && mViewportSize.y > 0.0f &&
            (spec.Width != mViewportSize.x || spec.Height != mViewportSize.y)) {

            mFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

            auto view = mScene->GetAllEntitiesWith<Loom::CameraComponent>();
            for (auto entity : view) {
                auto& camera = view.get<Loom::CameraComponent>(entity);
                if (!camera.FixedAspectRatio)
                    camera.Camera.SetViewportSize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
            }
        }
        mFramebuffer->Bind();

        Loom::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Loom::RenderCommand::Clear();

        mScene->OnUpdate(ts);

        mFramebuffer->Unbind();
    }

    void OnImGuiRender() override {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        mHierarchyPanel.OnImGuiRender();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport");

        ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
        mViewportSize = { viewport_panel_size.x, viewport_panel_size.y };

        uint32_t texture_id = mFramebuffer->GetColorAttachmentRendererID(0);
        ImGui::Image((void*)(intptr_t)texture_id, ImVec2{ mViewportSize.x, mViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    std::shared_ptr<Loom::Scene> mScene;
    Loom::SceneHierarchyPanel mHierarchyPanel;

    Loom::Entity mCameraEntity;

    std::shared_ptr<Loom::Framebuffer> mFramebuffer;
    glm::vec2 mViewportSize = { 0.0f, 0.0f };
};

int main() {
    Loom::Log::Init();
    LOOM_INFO("Sandbox initialized!");

    Loom::Application app;
    app.PushLayer(new ExampleLayer());
    app.Run();
    return 0;
}