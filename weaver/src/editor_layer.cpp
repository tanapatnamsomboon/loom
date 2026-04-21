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
        fb_spec.Attachments = {
            Loom::FramebufferTextureFormat::RGBA8,
            Loom::FramebufferTextureFormat::RED_INTEGER,
            Loom::FramebufferTextureFormat::DEPTH24STENCIL8
        };
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

        mFramebuffer->ClearAttachment(1, -1);

        mScene->OnUpdateEditor(ts, mEditorCamera);

        auto [mx, my] = ImGui::GetMousePos();
        mx -= mViewportBounds[0].x;
        my -= mViewportBounds[0].y;

        glm::vec2 viewport_size = mViewportBounds[1] - mViewportBounds[0];
        my = viewport_size.y - my;

        int mouse_x = (int)mx;
        int mouse_y = (int)my;

        if (mouse_x >= 0                   &&
            mouse_y >= 0                   &&
            mouse_x < (int)viewport_size.x &&
            mouse_y < (int)viewport_size.y) {
            int pixel_data = mFramebuffer->ReadPixel(1, mouse_x, mouse_y);
            if (pixel_data == -1) {
                mHoveredEntity = Loom::Entity();
            } else {
                mHoveredEntity = Loom::Entity((entt::entity)pixel_data, mScene.get());
            }
        } else {
            mHoveredEntity = Loom::Entity();
        }

        mFramebuffer->Unbind();
    }

    void EditorLayer::OnEvent(Loom::Event& event) {
        mEditorCamera.OnEvent(event);

        Loom::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Loom::MouseButtonPressedEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
        dispatcher.Dispatch<Loom::KeyPressedEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
    }

    void EditorLayer::OnImGuiRender() {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        mHierarchyPanel.OnImGuiRender();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport");

        mViewportFocused = ImGui::IsWindowFocused();
        mViewportHovered = ImGui::IsWindowHovered();
        Loom::Application::Get().GetImGuiLayer()->BlockEvents(!mViewportHovered);

        auto viewport_min_region = ImGui::GetWindowContentRegionMin();
        auto viewport_max_region = ImGui::GetWindowContentRegionMax();
        auto viewport_offset = ImGui::GetWindowPos();
        mViewportBounds[0] = { viewport_min_region.x + viewport_offset.x, viewport_min_region.y + viewport_offset.y };
        mViewportBounds[1] = { viewport_max_region.x + viewport_offset.x, viewport_max_region.y + viewport_offset.y };

        ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
        mViewportSize = { viewport_panel_size.x, viewport_panel_size.y };

        uint32_t texture_id = mFramebuffer->GetColorAttachmentRendererID(0);
        ImGui::Image((void*)(intptr_t)texture_id, ImVec2{ mViewportSize.x, mViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        ImGui::End();
        ImGui::PopStyleVar();
    }

    bool EditorLayer::OnMouseButtonPressed(Loom::MouseButtonPressedEvent& event) {
        if (event.GetMouseButton() == 0) {
            if (mViewportHovered) {
                mHierarchyPanel.SetSelectedEntity(mHoveredEntity);
            }
        }
        return false;
    }

    bool EditorLayer::OnKeyPressed(Loom::KeyPressedEvent& event) {
        return false;
    }

}
