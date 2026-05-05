#include "editor_layer.h"
#include <imgui.h>
// clang-format off
#include <ImGuizmo.h>
#include <imgui_internal.h>
// clang-format on
#include <loom/asset/asset_manager.h>
#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/project/project.h>
#include <loom/scene/scene_loader.h>
#include <loom/scene/scene_serializer.h>
#include <filesystem>

namespace Weaver {

#pragma region Construction & Initialization

    EditorLayer::EditorLayer()
        : Layer("EditorLayer")
        , mViewportPanel(mContext)
        , mToolbarPanel(mContext)
        , mSceneManager(mContext)
        , mProjectManager(mContext, mContentBrowserPanel, mSceneManager) {

        mContext.EditorScene    = std::make_shared<Loom::Scene>();
        mContext.ActiveScene    = mContext.EditorScene;
        mContext.EditorCamera   = Loom::EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);
        mContext.HierarchyPanel = &mSceneHierarchyPanel;

        mSceneHierarchyPanel.SetContext(mContext.ActiveScene);
    }

    void EditorLayer::OnAttach() {
        std::string icon_path = Loom::Project::GetEngineAssetFileSystemPath("icons/weaver.png").generic_string();
        Loom::Application::Get().GetWindow().SetIcon(icon_path);

        ImGuiContext*     context;
        ImGuiMemAllocFunc alloc_func;
        ImGuiMemFreeFunc  free_func;
        void*             user_data;
        Loom::Application::Get().GetImGuiLayer()->GetContextAndAllocators(&context, &alloc_func, &free_func, &user_data);
        ImGui::SetCurrentContext(context);
        ImGuizmo::SetImGuiContext(context);
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);

        mViewportPanel.Init();

        mContentBrowserPanel.Init();
        mContentBrowserPanel.SetSceneOpenCallback([this](const std::filesystem::path& path) {
            mSceneManager.OpenScene(path.string());
        });

        mViewportPanel.SetSceneOpenCallback([this](const std::filesystem::path& path) {
            auto full = Loom::Project::GetAssetFileSystemPath(path);
            mSceneManager.OpenScene(full.string());
        });

        auto prefab_callback = [this](const std::filesystem::path& rel_path) {
            auto full = Loom::Project::GetAssetFileSystemPath(rel_path);
            Loom::SceneSerializer::DeserializePrefabInto(full.string(), mContext.ActiveScene.get());
            mContext.SceneDirty = true;
        };
        mViewportPanel.SetPrefabInstantiateCallback(prefab_callback);
        mContentBrowserPanel.SetPrefabInstantiateCallback(prefab_callback);

        mSceneHierarchyPanel.Init();
        mSceneHierarchyPanel.SetSceneModifiedCallback([this] {
            mContext.SceneDirty = true;
        });

        mToolbarPanel.SetOnPlayPressed([this] { mSceneManager.OnScenePlay(); });
        mToolbarPanel.SetOnStopPressed([this] { mSceneManager.OnSceneStop(); });

        mProjectManager.ShowWizard();
    }

#pragma endregion

#pragma region Update Loop

    void EditorLayer::OnUpdate(Loom::Timestep ts) {
        Loom::AssetManager::ReloadChanged();

        mViewportPanel.BeginFrame();
        mViewportPanel.RenderScene(ts);
        mViewportPanel.UpdateHoveredEntity();
        mViewportPanel.EndFrame();

        if (mContext.SceneState == SceneState::Play) {
            auto& loader = Loom::SceneLoader::Get();
            if (loader.HasPendingTransition()) {
                mSceneManager.OnRuntimeSceneTransition(loader.GetPendingPath(), loader.IsReload());
                loader.Consume();
            }
        }
    }

#pragma endregion

#pragma region Input & Events

    void EditorLayer::OnEvent(Loom::Event& event) {
        mContext.EditorCamera.OnEvent(event);

        Loom::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Loom::WindowCloseEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnWindowClose));
        dispatcher.Dispatch<Loom::MouseButtonPressedEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
        dispatcher.Dispatch<Loom::KeyPressedEvent>(LOOM_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
    }

    bool EditorLayer::OnWindowClose(Loom::WindowCloseEvent& event) {
        mSceneManager.RequestQuit();
        return true; // always consume — RequestQuit decides whether to actually close
    }

    bool EditorLayer::OnMouseButtonPressed(Loom::MouseButtonPressedEvent& event) {
        if (event.GetMouseButton() != 0 || !mContext.ViewportHovered)
            return false;

        Loom::Entity selected     = mContext.HierarchyPanel->GetSelectedEntity();
        bool         gizmo_active = selected && mContext.GizmoType != -1 && ImGuizmo::IsOver();

        if (!gizmo_active) {
            // Gizmo handles are not rendered into the entity ID attachment, so HoveredEntity
            // is null when the cursor lands on a handle. Guard against accidentally deselecting
            // when IsOver() is briefly false (one-frame lag on first hover) and the pick
            // buffer also returns null because the cursor is over a handle.
            bool gizmo_visible = selected && mContext.GizmoType != -1;
            if (!gizmo_visible || mContext.HoveredEntity)
                mSceneHierarchyPanel.SetSelectedEntity(mContext.HoveredEntity);
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
        bool shift = Loom::Input::IsKeyPressed(Loom::Key::LeftShift)   || Loom::Input::IsKeyPressed(Loom::Key::RightShift);

        switch ((Loom::Key)event.GetKeyCode()) {
            case Loom::Key::N:
                if (ctrl) { mSceneManager.NewScene();   return; }
                break;
            case Loom::Key::O:
                if (ctrl) { mSceneManager.OpenScene();  return; }
                break;
            case Loom::Key::S:
                if (ctrl && shift) { mSceneManager.SaveSceneAs(); return; }
                if (ctrl)          { mSceneManager.SaveScene();   return; }
                break;
            case Loom::Key::Q:
                if (ctrl) { mSceneManager.RequestQuit(); return; }
                break;
            default:
                break;
        }
    }

    void EditorLayer::HandleGizmoTypeChange(Loom::KeyPressedEvent& event) {
        if (Loom::Input::IsMouseButtonPressed(Loom::Mouse::ButtonRight))
            return;

        switch ((Loom::Key)event.GetKeyCode()) {
            case Loom::Key::Q: mContext.GizmoType = -1;                              break;
            case Loom::Key::W: mContext.GizmoType = ImGuizmo::OPERATION::TRANSLATE;  break;
            case Loom::Key::E: mContext.GizmoType = ImGuizmo::OPERATION::ROTATE;     break;
            case Loom::Key::R: mContext.GizmoType = ImGuizmo::OPERATION::SCALE;      break;
            default: break;
        }
    }

#pragma endregion

#pragma region ImGui Rendering

    void EditorLayer::OnImGuiRender() {
        ImGuizmo::BeginFrame();

        static bool dockspace_open = true;
        static bool opt_fullscreen = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                          | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
                          | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Weaver Main Dockspace", &dockspace_open, window_flags);
        ImGui::PopStyleVar();
        if (opt_fullscreen) ImGui::PopStyleVar(2);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiID dockspace_id = ImGui::GetID("WeaverDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        RenderMainMenuBar();
        RenderAboutModal();

        mSceneManager.OnImGuiRender();
        mProjectManager.OnImGuiRender();

        if (mShowSceneHierarchyPanel) mSceneHierarchyPanel.OnImGuiRender();
        if (mShowContentBrowserPanel) mContentBrowserPanel.OnImGuiRender();

        mViewportPanel.OnImGuiRender();
        mToolbarPanel.OnImGuiRender(); // must come after viewport (needs updated ViewportBounds)

        ImGui::End();
    }

    void EditorLayer::RenderMainMenuBar() {
        if (!ImGui::BeginMainMenuBar()) return;

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project..."))     mProjectManager.NewProject();
            if (ImGui::MenuItem("Open Project..."))    mProjectManager.OpenProject();
            if (ImGui::MenuItem("Save Project As...")) mProjectManager.SaveProjectAs();
            ImGui::BeginDisabled(!Loom::Project::GetActive());
            if (ImGui::MenuItem("Project Settings...")) mProjectManager.OpenSettings();
            ImGui::EndDisabled();
            ImGui::Separator();
            if (ImGui::MenuItem("New",        "Ctrl+N"))       mSceneManager.NewScene();
            if (ImGui::MenuItem("Open...",    "Ctrl+O"))       mSceneManager.OpenScene();
            if (ImGui::MenuItem("Save",       "Ctrl+S"))       mSceneManager.SaveScene();
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) mSceneManager.SaveSceneAs();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit",       "Ctrl+Q"))       mSceneManager.RequestQuit();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &mShowSceneHierarchyPanel);
            ImGui::MenuItem("Content Browser", nullptr, &mShowContentBrowserPanel);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("About")) {
            if (ImGui::MenuItem("About Weaver")) mShowAboutModal = true;
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void EditorLayer::RenderAboutModal() {
        if (mShowAboutModal) {
            ImGui::OpenPopup("About Weaver");
            mShowAboutModal = false;
        }

        if (!ImGui::BeginPopupModal("About Weaver", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::Text("Weaver Editor");
        ImGui::Separator();
        ImGui::Text("A custom 2D/3D engine editor.");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

#pragma endregion

} // namespace Weaver
