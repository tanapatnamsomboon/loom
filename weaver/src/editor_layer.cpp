#include "editor_layer.h"
#include "loom/renderer/framebuffer.h"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
// clang-format off
#include <ImGuizmo.h>
#include <imgui_internal.h>
// clang-format on
#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/math/math.h>
#include <loom/renderer/render_command.h>
#include <loom/renderer/renderer_2d.h>
#include <loom/scene/components.h>
#include <loom/scene/scene_serializer.h>
#include <nfd.hpp>
#include <filesystem>

namespace Weaver {

#pragma region Construction & Initialization

    EditorLayer::EditorLayer()
        : Layer("EditorLayer") {
        // Initialize Framebuffer
        Loom::FramebufferSpecification fb_spec;
        fb_spec.Attachments = {
            Loom::FramebufferTextureFormat::RGBA8,
            Loom::FramebufferTextureFormat::RED_INTEGER,
            Loom::FramebufferTextureFormat::DEPTH24STENCIL8
        };
        fb_spec.Width  = 1280;
        fb_spec.Height = 720;
        mFramebuffer   = Loom::Framebuffer::Create(fb_spec);

        // Scene setup
        mEditorScene = std::make_shared<Loom::Scene>();
        mActiveScene = mEditorScene;

        // Camera
        mEditorCamera = Loom::EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);

        // Hierarchy Panel
        mSceneHierarchyPanel.SetContext(mActiveScene);
    }

    void EditorLayer::OnAttach() {
        ImGuiContext*     context;
        ImGuiMemAllocFunc alloc_func;
        ImGuiMemFreeFunc  free_func;
        void*             user_data;

        Loom::Application::Get().GetImGuiLayer()->GetContextAndAllocators(&context, &alloc_func, &free_func, &user_data);

        ImGui::SetCurrentContext(context);
        ImGuizmo::SetImGuiContext(context);
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);

        mSceneHierarchyPanel.Init();
    }

#pragma endregion

#pragma region Update Loop

    void EditorLayer::OnUpdate(Loom::Timestep ts) {
        HandleViewportResize();
        UpdateScene(ts);
        HandleMousePicking();
        mFramebuffer->Unbind();
    }

    void EditorLayer::HandleViewportResize() {
        mActiveScene->OnViewportResize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);

        Loom::FramebufferSpecification spec = mFramebuffer->GetSpecification();
        if (mViewportSize.x > 0.0f && mViewportSize.y > 0.0f && (spec.Width != mViewportSize.x || spec.Height != mViewportSize.y)) {
            mFramebuffer->Resize((uint32_t)mViewportSize.x, (uint32_t)mViewportSize.y);
            mEditorCamera.SetViewportSize(mViewportSize.x, mViewportSize.y);
        }
    }

    void EditorLayer::UpdateScene(Loom::Timestep ts) {
        if (mSceneState == SceneState::Edit) {
            if (mViewportHovered) {
                mEditorCamera.OnUpdate(ts);
            } else {
                mEditorCamera.ResetMousePosition();
            }
        }

        mFramebuffer->Bind();
        Loom::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Loom::RenderCommand::Clear();
        mFramebuffer->ClearAttachment(1, -1);

        switch (mSceneState) {
            case SceneState::Edit:
                mActiveScene->OnUpdateEditor(ts, mEditorCamera, mSceneHierarchyPanel.GetSelectedEntity());
                break;
            case SceneState::Play:
                mActiveScene->OnUpdateRuntime(ts);
                break;
        }
    }

    void EditorLayer::HandleMousePicking() {
        auto [mx, my] = ImGui::GetMousePos();
        mx -= mViewportBounds[0].x;
        my -= mViewportBounds[0].y;

        glm::vec2 viewport_size = mViewportBounds[1] - mViewportBounds[0];
        my                      = viewport_size.y - my;

        int mouse_x = (int)mx;
        int mouse_y = (int)my;

        if (mouse_x >= 0 && mouse_y >= 0 && mouse_x < (int)viewport_size.x && mouse_y < (int)viewport_size.y) {
            int pixel_data = mFramebuffer->ReadPixel(1, mouse_x, mouse_y);
            if (pixel_data == -1) {
                mHoveredEntity = Loom::Entity();
            } else {
                mHoveredEntity = Loom::Entity((entt::entity)pixel_data, mActiveScene.get());
            }
        } else {
            mHoveredEntity = Loom::Entity();
        }
    }

#pragma endregion

#pragma region Input & Events

    void EditorLayer::OnEvent(Loom::Event& event) {
        mEditorCamera.OnEvent(event);

        Loom::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Loom::MouseButtonPressedEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
        dispatcher.Dispatch<Loom::KeyPressedEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
    }

    bool EditorLayer::OnMouseButtonPressed(Loom::MouseButtonPressedEvent& event) {
        if (event.GetMouseButton() == 0 && mViewportHovered && !ImGuizmo::IsOver()) {
            mSceneHierarchyPanel.SetSelectedEntity(mHoveredEntity);
        }
        return false;
    }

    bool EditorLayer::OnKeyPressed(Loom::KeyPressedEvent& event) {
        HandleShortcuts(event);
        HandleGizmoTypeChange(event);
        return false;
    }

    void EditorLayer::HandleShortcuts(Loom::KeyPressedEvent& event) {
        bool ctrl  = Loom::Input::IsKeyPressed(Loom::Key::LeftControl) || Loom::Input::IsKeyPressed(Loom::Key::RightControl);
        bool shift = Loom::Input::IsKeyPressed(Loom::Key::LeftShift) || Loom::Input::IsKeyPressed(Loom::Key::RightShift);

        switch ((Loom::Key)event.GetKeyCode()) {
            case Loom::Key::N:
                if (ctrl) {
                    NewScene();
                    return;
                }
                break;
            case Loom::Key::O:
                if (ctrl) {
                    OpenScene();
                    return;
                }
                break;
            case Loom::Key::S:
                if (ctrl && shift) {
                    SaveSceneAs();
                    return;
                }
                if (ctrl) {
                    SaveScene();
                    return;
                }
                break;
            default:
                break;
        }
    }

    void EditorLayer::HandleGizmoTypeChange(Loom::KeyPressedEvent& event) {
        if (Loom::Input::IsMouseButtonPressed(Loom::Mouse::ButtonRight))
            return;

        switch ((Loom::Key)event.GetKeyCode()) {
            case Loom::Key::Q:
                mGizmoType = -1;
                break;
            case Loom::Key::W:
                mGizmoType = ImGuizmo::OPERATION::TRANSLATE;
                break;
            case Loom::Key::E:
                mGizmoType = ImGuizmo::OPERATION::ROTATE;
                break;
            case Loom::Key::R:
                mGizmoType = ImGuizmo::OPERATION::SCALE;
                break;
            default:
                break;
        }
    }

#pragma endregion

#pragma region ImGui Rendering

    void EditorLayer::OnImGuiRender() {
        ImGuizmo::BeginFrame();
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        RenderMainMenuBar();
        RenderModals();
        RenderPanels();
        RenderToolbar();
        RenderViewport();
    }

    void EditorLayer::RenderMainMenuBar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    NewScene();
                }
                if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                    OpenScene();
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    SaveScene();
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    SaveSceneAs();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Scene Hierarchy", nullptr, &mShowSceneHierarchyPanel);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("About")) {
                if (ImGui::MenuItem("About Weaver")) {
                    mShowAboutModal = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void EditorLayer::RenderModals() {
        if (mShowAboutModal) {
            ImGui::OpenPopup("About Weaver");
            mShowAboutModal = false;
        }

        if (ImGui::BeginPopupModal("About Weaver", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Weaver Editor");
            ImGui::Separator();
            ImGui::Text("A custom 2D/3D engine editor.");

            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void EditorLayer::RenderPanels() {
        if (mShowSceneHierarchyPanel) {
            mSceneHierarchyPanel.OnImGuiRender();
        }
        if (mShowContentBrowserPanel) {
            mContentBrowserPanel.OnImGuiRender();
        }
    }

    void EditorLayer::RenderToolbar() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));

        ImGuiWindowClass window_class;
        window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
        ImGui::SetNextWindowClass(&window_class);

        ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        float size = ImGui::GetWindowHeight() - 4.0f;
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x * 0.5f - size * 0.5f);

        if (mSceneState == SceneState::Edit) {
            if (ImGui::Button("Play", ImVec2(50, size)))
                OnScenePlay();
        } else if (mSceneState == SceneState::Play) {
            if (ImGui::Button("Stop", ImVec2(50, size)))
                OnSceneStop();
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void EditorLayer::RenderViewport() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport");

        mViewportFocused = ImGui::IsWindowFocused();
        mViewportHovered = ImGui::IsWindowHovered();
        Loom::Application::Get().GetImGuiLayer()->BlockEvents(!mViewportHovered);

        UpdateViewportBounds();
        UpdateViewportSize();

        uint32_t texture_id = mFramebuffer->GetColorAttachmentRendererID(0);
        ImGui::Image((void*)(intptr_t)texture_id, ImVec2{ mViewportSize.x, mViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        RenderGizmos();

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::UpdateViewportBounds() {
        auto min_region = ImGui::GetWindowContentRegionMin();
        auto max_region = ImGui::GetWindowContentRegionMax();
        auto offset     = ImGui::GetWindowPos();

        mViewportBounds[0] = { min_region.x + offset.x, min_region.y + offset.y };
        mViewportBounds[1] = { max_region.x + offset.x, max_region.y + offset.y };
    }

    void EditorLayer::UpdateViewportSize() {
        ImVec2 content = ImGui::GetContentRegionAvail();
        mViewportSize  = { content.x, content.y };
    }

    void EditorLayer::RenderGizmos() {
        Loom::Entity selected_entity = mSceneHierarchyPanel.GetSelectedEntity();
        if (!selected_entity || mGizmoType == -1)
            return;

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(mViewportBounds[0].x, mViewportBounds[0].y, mViewportBounds[1].x - mViewportBounds[0].x, mViewportBounds[1].y - mViewportBounds[0].y);

        const glm::mat4& proj = mEditorCamera.GetProjectionMatrix();
        glm::mat4        view = mEditorCamera.GetViewMatrix();

        auto&     tc        = selected_entity.GetComponent<Loom::TransformComponent>();
        glm::mat4 transform = tc.GetTransform();

        bool  snap           = Loom::Input::IsKeyPressed(Loom::Key::LeftControl);
        float snap_value     = (mGizmoType == ImGuizmo::OPERATION::ROTATE) ? 45.0f : 0.5f;
        float snap_values[3] = { snap_value, snap_value, snap_value };

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), (ImGuizmo::OPERATION)mGizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, snap ? snap_values : nullptr);

        if (ImGuizmo::IsUsing()) {
            glm::vec3 translation, rotation, scale;
            Loom::Math::DecomposeTransform(transform, translation, rotation, scale);

            glm::vec3 delta_rotation = rotation - tc.Rotation;
            tc.Translation           = translation;
            tc.Rotation += delta_rotation;
            tc.Scale = scale;
        }
    }

#pragma endregion

#pragma region Scene Management

    void EditorLayer::NewScene() {
        mActiveScene = std::make_shared<Loom::Scene>();
        mSceneHierarchyPanel.SetContext(mActiveScene);
        mCurrentScenePath.clear();
    }

    void EditorLayer::OpenScene() {
        constexpr nfdfilteritem_t filters[] = {
            { "Loom Scene", "loom" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::OpenDialog(out_path, filters, 2);

        if (result == NFD_OKAY) {
            OpenScene(out_path.get());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD OpenDialog error: {}", NFD::GetError());
        } // else if NFD_CANCEL: user dismissed, do nothing
    }

    void EditorLayer::OpenScene(const std::string& filepath) {
        if (!std::filesystem::exists(filepath)) {
            LOOM_CORE_WARN("EditorLayer: scene file '{}' does not exist", filepath);
            return;
        }

        auto new_scene = std::make_shared<Loom::Scene>();

        Loom::SceneSerializer serializer(new_scene);

        if (serializer.Deserialize(filepath)) {
            mEditorScene = new_scene;
            mSceneHierarchyPanel.SetContext(mEditorScene);

            mActiveScene      = mEditorScene;
            mCurrentScenePath = filepath;
        }
    }

    void EditorLayer::SaveScene() {
        if (mCurrentScenePath.empty()) {
            SaveSceneAs();
            return;
        }
        Loom::SceneSerializer serializer(mActiveScene);
        serializer.Serialize(mCurrentScenePath);
    }

    void EditorLayer::SaveSceneAs() {
        constexpr nfdfilteritem_t filters[] = {
            { "Loom Scene", "loom" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::SaveDialog(out_path, filters, 2, nullptr, "scene.loom");

        if (result == NFD_OKAY) {
            // nfd-extended does NOT append the extension automatically on all platforms,
            // so we ensure .loom is present.
            std::filesystem::path path = out_path.get();
            if (path.extension() != ".loom")
                path += ".loom";

            std::filesystem::create_directories(path.parent_path());

            Loom::SceneSerializer serializer(mActiveScene);
            serializer.Serialize(path.string());
            mCurrentScenePath = path.string();
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD SaveDialog error: {}", NFD::GetError());
        }
    }

    void EditorLayer::OnScenePlay() {
        mSceneState  = SceneState::Play;
        mActiveScene = Loom::Scene::Copy(mEditorScene);
        mSceneHierarchyPanel.SetContext(mActiveScene);
    }

    void EditorLayer::OnSceneStop() {
        mActiveScene->OnRuntimeStop();
        mSceneState  = SceneState::Edit;
        mActiveScene = mEditorScene;
        mSceneHierarchyPanel.SetContext(mActiveScene);
    }

#pragma endregion

} // namespace Weaver
