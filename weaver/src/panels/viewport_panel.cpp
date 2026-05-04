#include "viewport_panel.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
// clang-format off
#include <ImGuizmo.h>
// clang-format on
#include <loom/asset/asset_manager.h>
#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/math/math.h>
#include <loom/project/project.h>
#include <loom/renderer/render_command.h>
#include <loom/scene/components.h>
#include <filesystem>

namespace Weaver {

    ViewportPanel::ViewportPanel(EditorContext& ctx)
        : mContext(ctx) {
        Loom::FramebufferSpecification spec;
        spec.Attachments = {
            Loom::FramebufferTextureFormat::RGBA8,
            Loom::FramebufferTextureFormat::RED_INTEGER,
            Loom::FramebufferTextureFormat::DEPTH24STENCIL8
        };
        spec.Width   = 1280;
        spec.Height  = 720;
        mFramebuffer = Loom::Framebuffer::Create(spec);
    }

    void ViewportPanel::Init() {
        std::string skybox_path = Loom::Project::GetEngineAssetFileSystemPath("shaders/skybox").generic_string();
        std::string grid_path   = Loom::Project::GetEngineAssetFileSystemPath("shaders/grid").generic_string();

        float skybox_vertices[] = {
            -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f
        };
        uint32_t skybox_indices[] = {
            1, 2, 6, 6, 5, 1, // Right
            0, 4, 7, 7, 3, 0, // Left
            3, 7, 6, 6, 2, 3, // Top
            0, 1, 5, 5, 4, 0, // Bottom
            5, 6, 7, 7, 4, 5, // Back
            1, 0, 3, 3, 2, 1  // Front
        };

        mSkyboxVAO = Loom::VertexArray::Create();
        mSkyboxVBO = Loom::VertexBuffer::Create(sizeof(skybox_vertices));
        mSkyboxVBO->SetData(skybox_vertices, sizeof(skybox_vertices));
        mSkyboxVBO->SetLayout({ { Loom::ShaderDataType::Float3, "aPosition" } });
        mSkyboxVAO->AddVertexBuffer(mSkyboxVBO);
        auto skybox_ibo = Loom::IndexBuffer::Create(skybox_indices, sizeof(skybox_indices) / sizeof(uint32_t));
        mSkyboxVAO->SetIndexBuffer(skybox_ibo);
        mSkyboxShader = Loom::AssetManager::GetShader(skybox_path);

        float grid_vertices[] = {
            -1.0f, 0.0f, -1.0f,
             1.0f, 0.0f, -1.0f,
             1.0f, 0.0f,  1.0f,
            -1.0f, 0.0f,  1.0f
        };
        uint32_t grid_indices[] = { 0, 1, 2, 2, 3, 0 };

        mGridVAO = Loom::VertexArray::Create();
        mGridVBO = Loom::VertexBuffer::Create(sizeof(grid_vertices));
        mGridVBO->SetData(grid_vertices, sizeof(grid_vertices));
        mGridVBO->SetLayout({ { Loom::ShaderDataType::Float3, "aPosition" } });
        mGridVAO->AddVertexBuffer(mGridVBO);
        auto grid_ibo = Loom::IndexBuffer::Create(grid_indices, sizeof(grid_indices) / sizeof(uint32_t));
        mGridVAO->SetIndexBuffer(grid_ibo);
        mGridShader = Loom::AssetManager::GetShader(grid_path);
    }

    void ViewportPanel::BeginFrame() {
        HandleViewportResize();

        mFramebuffer->Bind();
        Loom::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Loom::RenderCommand::Clear();
        mFramebuffer->ClearAttachment(1, -1);
    }

    void ViewportPanel::HandleViewportResize() {
        mContext.ActiveScene->OnViewportResize((uint32_t)mContext.ViewportSize.x, (uint32_t)mContext.ViewportSize.y);

        Loom::FramebufferSpecification spec = mFramebuffer->GetSpecification();
        if (mContext.ViewportSize.x > 0.0f && mContext.ViewportSize.y > 0.0f &&
            (spec.Width != mContext.ViewportSize.x || spec.Height != mContext.ViewportSize.y)) {
            mFramebuffer->Resize((uint32_t)mContext.ViewportSize.x, (uint32_t)mContext.ViewportSize.y);
            mContext.EditorCamera.SetViewportSize(mContext.ViewportSize.x, mContext.ViewportSize.y);
        }
    }

    void ViewportPanel::RenderScene(Loom::Timestep ts) {
        if (!Loom::Project::GetActive())
            return;

        if (mContext.SceneState == SceneState::Edit) {
            if (mContext.ViewportHovered)
                mContext.EditorCamera.OnUpdate(ts);
            else
                mContext.EditorCamera.ResetMousePosition();

            // Skybox
            glm::mat4 view    = mContext.EditorCamera.GetViewMatrix();
            view[3]           = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            glm::mat4 skyboxVP = mContext.EditorCamera.GetProjectionMatrix() * view;
            mSkyboxShader->Bind();
            mSkyboxShader->UploadUniformMat4("uViewProjection", skyboxVP);
            Loom::RenderCommand::DrawIndexed(mSkyboxVAO.get(), 36);

            // Grid
            const auto& gs       = mContext.Grid;
            glm::vec3   cam_pos  = mContext.EditorCamera.GetPosition();
            glm::mat4   grid_transform = glm::translate(glm::mat4(1.0f), { cam_pos.x, 0.0f, cam_pos.z })
                                       * glm::scale(glm::mat4(1.0f), { 150.0f, 1.0f, 150.0f });
            mGridShader->Bind();
            mGridShader->UploadUniformMat4("uViewProjection",  mContext.EditorCamera.GetViewProjectionMatrix());
            mGridShader->UploadUniformMat4("uTransform",       grid_transform);
            mGridShader->UploadUniformFloat3("uCameraPosition", cam_pos);
            mGridShader->UploadUniformFloat("uMinorScale",     gs.MinorScale);
            mGridShader->UploadUniformFloat("uMajorScale",     gs.MajorScale);
            mGridShader->UploadUniformFloat("uLineThickness",  gs.LineThickness);
            mGridShader->UploadUniformFloat("uFadeStart",      gs.FadeStart);
            mGridShader->UploadUniformFloat("uFadeEnd",        gs.FadeEnd);
            mGridShader->UploadUniformFloat4("uMinorColor",    gs.MinorColor);
            mGridShader->UploadUniformFloat4("uMajorColor",    gs.MajorColor);
            Loom::RenderCommand::DrawIndexed(mGridVAO.get(), 6);
        }

        switch (mContext.SceneState) {
            case SceneState::Edit:
                mContext.ActiveScene->OnUpdateEditor(ts, mContext.EditorCamera, mContext.HierarchyPanel->GetSelectedEntity());
                break;
            case SceneState::Play:
                mContext.ActiveScene->OnUpdateRuntime(ts);
                break;
        }
    }

    void ViewportPanel::UpdateHoveredEntity() {
        auto [mx, my] = ImGui::GetMousePos();
        mx -= mContext.ViewportBounds[0].x;
        my -= mContext.ViewportBounds[0].y;

        glm::vec2 size = mContext.ViewportBounds[1] - mContext.ViewportBounds[0];
        my = size.y - my;

        int mouse_x = (int)mx;
        int mouse_y = (int)my;

        if (mouse_x >= 0 && mouse_y >= 0 && mouse_x < (int)size.x && mouse_y < (int)size.y) {
            int pixel = mFramebuffer->ReadPixel(1, mouse_x, mouse_y);
            mContext.HoveredEntity = (pixel == -1)
                ? Loom::Entity()
                : Loom::Entity((entt::entity)pixel, mContext.ActiveScene.get());
        } else {
            mContext.HoveredEntity = Loom::Entity();
        }
    }

    void ViewportPanel::EndFrame() {
        mFramebuffer->Unbind();
    }

    void ViewportPanel::OnImGuiRender() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });

        std::string title = "Viewport###Viewport";
        if (Loom::Project::GetActive()) {
            std::string filename = mContext.CurrentScenePath.empty()
                ? "Untitled Scene"
                : std::filesystem::path(mContext.CurrentScenePath).filename().string();
            title = filename + (mContext.SceneDirty ? "*" : "") + " (Viewport)###Viewport";
        }

        ImGui::Begin(title.c_str());

        mContext.ViewportFocused = ImGui::IsWindowFocused();
        mContext.ViewportHovered = ImGui::IsWindowHovered();
        Loom::Application::Get().GetImGuiLayer()->BlockEvents(!mContext.ViewportHovered);

        UpdateViewportBounds();
        UpdateViewportSize();

        uint32_t tex_id = mFramebuffer->GetColorAttachmentRendererID(0);
        ImGui::Image((void*)(intptr_t)tex_id, ImVec2{ mContext.ViewportSize.x, mContext.ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                std::filesystem::path dropped((const char*)payload->Data);
                if (dropped.extension() == ".loom" && mSceneOpenCallback) {
                    mSceneOpenCallback(dropped);
                } else if (dropped.extension() == ".lprefab" && mPrefabInstantiateCallback) {
                    mPrefabInstantiateCallback(dropped);
                }
            }
            ImGui::EndDragDropTarget();
        }

        RenderGizmos();

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::UpdateViewportBounds() {
        auto min_region = ImGui::GetWindowContentRegionMin();
        auto max_region = ImGui::GetWindowContentRegionMax();
        auto offset     = ImGui::GetWindowPos();

        mContext.ViewportBounds[0] = { min_region.x + offset.x, min_region.y + offset.y };
        mContext.ViewportBounds[1] = { max_region.x + offset.x, max_region.y + offset.y };
    }

    void ViewportPanel::UpdateViewportSize() {
        ImVec2 content    = ImGui::GetContentRegionAvail();
        mContext.ViewportSize = { content.x, content.y };
    }

    void ViewportPanel::RenderGizmos() {
        if (mContext.SceneState != SceneState::Edit)
            return;

        Loom::Entity selected = mContext.HierarchyPanel->GetSelectedEntity();
        if (!selected || mContext.GizmoType == -1)
            return;

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(
            mContext.ViewportBounds[0].x, mContext.ViewportBounds[0].y,
            mContext.ViewportBounds[1].x - mContext.ViewportBounds[0].x,
            mContext.ViewportBounds[1].y - mContext.ViewportBounds[0].y
        );

        const glm::mat4& proj      = mContext.EditorCamera.GetProjectionMatrix();
        glm::mat4        view      = mContext.EditorCamera.GetViewMatrix();
        auto&            tc        = selected.GetComponent<Loom::TransformComponent>();

        // Gizmo operates in world space
        glm::mat4 world_transform = mContext.ActiveScene->GetWorldTransform(selected);

        bool  snap           = Loom::Input::IsKeyPressed(Loom::Key::LeftControl);
        float snap_value     = (mContext.GizmoType == ImGuizmo::OPERATION::ROTATE) ? 45.0f : 0.5f;
        float snap_values[3] = { snap_value, snap_value, snap_value };

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(proj),
            (ImGuizmo::OPERATION)mContext.GizmoType, (ImGuizmo::MODE)mContext.GizmoMode,
            glm::value_ptr(world_transform), nullptr, snap ? snap_values : nullptr
        );

        if (ImGuizmo::IsUsing()) {
            mContext.SceneDirty = true;

            // Convert world result back to local space if entity has a parent
            Loom::Entity parent = selected.GetParent();
            if (parent) {
                glm::mat4 parent_world = mContext.ActiveScene->GetWorldTransform(parent);
                world_transform        = glm::inverse(parent_world) * world_transform;
            }

            glm::vec3 translation, rotation, scale;
            Loom::Math::DecomposeTransform(world_transform, translation, rotation, scale);

            glm::vec3 delta_rotation = rotation - tc.Rotation;
            tc.Translation           = translation;
            tc.Rotation             += delta_rotation;
            tc.Scale                 = scale;
        }
    }

} // namespace Weaver