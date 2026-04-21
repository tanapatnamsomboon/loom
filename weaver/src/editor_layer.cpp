#include "editor_layer.h"
#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/core/log.h>
#include <loom/renderer/render_command.h>
#include <loom/scene/components.h>
#include <imgui.h>

namespace Weaver {

    EditorLayer::EditorLayer() : Layer("EditorLayer") {
        Loom::FramebufferSpecification fb_spec;
        fb_spec.Width = 1280;
        fb_spec.Height = 720;
        mFramebuffer = Loom::Framebuffer::Create(fb_spec);

        mScene = std::make_shared<Loom::Scene>();

        auto red_square = mScene->CreateEntity("Red Square");
        red_square.AddComponent<Loom::SpriteRendererComponent>(glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f });
        red_square.GetComponent<Loom::TransformComponent>().Translation.x = 1.0f;

        mEditorCamera = Loom::EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);

        mHierarchyPanel.SetContext(mScene);
    }

    void EditorLayer::OnAttach() {
        ImGuiContext* context;
        ImGuiMemAllocFunc alloc_func;
        ImGuiMemFreeFunc free_func;
        void* user_data;

        Loom::Application::Get().GetImGuiLayer()->GetContextAndAllocators(&context, &alloc_func, &free_func, &user_data);

        ImGui::SetCurrentContext(context);
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);
    }

    void EditorLayer::OnUpdate(Loom::Timestep ts) {
        if (Loom::FramebufferSpecification spec = mFramebuffer->GetSpecification();
            mViewportSize.x > 0.0f && mViewportSize.y > 0.0f &&
            (spec.Width != mViewportSize.x || spec.Height != mViewportSize.y)) {

            mFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
            mEditorCamera.SetViewportSize(mViewportSize.x, mViewportSize.y);
        }

        if (mViewportHovered) {
            mEditorCamera.OnUpdate(ts);
        } else {
            mEditorCamera.ResetMousePosition();
        }

        mFramebuffer->Bind();

        Loom::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Loom::RenderCommand::Clear();

        mScene->OnUpdateEditor(ts, mEditorCamera);

        mFramebuffer->Unbind();
    }

    void EditorLayer::OnEvent(Loom::Event& event) {
        mEditorCamera.OnEvent(event);
    }

    void EditorLayer::OnImGuiRender() {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        mHierarchyPanel.OnImGuiRender();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport");

        mViewportFocused = ImGui::IsWindowFocused();
        mViewportHovered = ImGui::IsWindowHovered();

        Loom::Application::Get().GetImGuiLayer()->BlockEvents(!mViewportFocused && !mViewportHovered);

        ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
        mViewportSize = { viewport_panel_size.x, viewport_panel_size.y };

        uint32_t texture_id = mFramebuffer->GetColorAttachmentRendererID();
        ImGui::Image((void*)(intptr_t)texture_id, ImVec2{ mViewportSize.x, mViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        ImGui::End();
        ImGui::PopStyleVar();
    }

}