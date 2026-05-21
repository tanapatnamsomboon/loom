#include "editor_layer.h"
#include "editor/commands.h"
#include "editor/file_dialog.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <loom/asset/font_manager.h>
#include <loom/asset/asset_manager.h>
#include <loom/core/application.h>
#include <loom/core/input.h>
#include <loom/core/log.h>
#include <loom/project/project.h>
#include <loom/renderer/cubemap.h>
#include <loom/scene/components.h>
#include <loom/scene/scene_loader.h>
#include <loom/scene/scene_serializer.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <filesystem>

namespace Weaver {

#pragma region Construction & Initialization

    EditorLayer::EditorLayer()
        : Layer("EditorLayer")
        , mViewportPanel(mContext)
        , mToolbarPanel(mContext)
        , mScenePropertiesPanel(mContext)
        , mSceneManager(mContext)
        , mProjectManager(mContext, mContentBrowserPanel, mSceneManager) {

        mContext.EditorScene    = std::make_shared<Loom::Scene>();
        mContext.ActiveScene    = mContext.EditorScene;
        mContext.EditorCamera   = Loom::EditorCamera(60.0f, 1.778f, 0.1f, 1000.0f);
        mContext.HierarchyPanel = &mSceneHierarchyPanel;

        mSceneHierarchyPanel.SetContext(mContext.ActiveScene);
        mSceneHierarchyPanel.SetEditorContext(&mContext);
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
        ImGui::SetAllocatorFunctions(alloc_func, free_func, user_data);

        // ImGuizmo is statically linked into weaver and has its own ImGui*
        // pointer. Without this, IsOver/IsUsing read the wrong context and the
        // gizmo draws correctly but never reacts to hover/click.
        ImGuizmo::SetImGuiContext(context);

        mViewportPanel.Init();
        mToolbarPanel.Init();

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
        mSceneHierarchyPanel.SetCommandCallback([this](std::unique_ptr<IEditorCommand> cmd) {
            mContext.History.Push(std::move(cmd));
        });

        mToolbarPanel.SetOnPlayPressed([this] { mSceneManager.OnScenePlay(); });
        mToolbarPanel.SetOnStopPressed([this] { mSceneManager.OnSceneStop(); });

        LoadFallbackEnvironment();

        mProjectManager.ShowWizard();
    }

    void EditorLayer::LoadFallbackEnvironment() {
        // Editor-only fallback: when a scene has no environment of its own,
        // the editor renders this on top so the user always sees a lit world
        // while building. Play mode never uses it — that path is governed by
        // the scene's explicit (possibly empty) environment.
        std::filesystem::path engine_default =
            Loom::Project::GetEngineAssetFileSystemPath("environments/default.hdr");
        if (!std::filesystem::exists(engine_default)) {
            LOOM_CORE_WARN("Editor fallback HDR not found at {} — scenes with no environment will render with zero IBL.",
                           engine_default.generic_string());
            return;
        }

        auto equirect = Loom::AssetManager::GetTexture(engine_default.generic_string());
        if (!equirect) {
            LOOM_CORE_WARN("Editor fallback HDR failed to load.");
            return;
        }

        // Same face_size as a scene env (matches viewport density at typical
        // editor FOVs — see scene.cpp comment).
        auto skybox = Loom::TextureCubemap::CreateFromEquirect(equirect, 2048);
        if (!skybox) return;
        auto irradiance = Loom::TextureCubemap::CreateIrradiance(skybox, 128);
        auto prefilter  = Loom::TextureCubemap::CreatePrefiltered(skybox, 256);

        mContext.FallbackEnvironment.Equirect   = std::move(equirect);
        mContext.FallbackEnvironment.Skybox     = std::move(skybox);
        mContext.FallbackEnvironment.Irradiance = std::move(irradiance);
        mContext.FallbackEnvironment.Prefilter  = std::move(prefilter);
        LOOM_CORE_INFO("Editor fallback environment loaded from {}", engine_default.generic_string());
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

        // Tile paint claims the click — viewport's per-frame mouse-down poll handles paint.
        if (mViewportPanel.IsTilePaintActive())
            return true;

        // Gizmo handles take priority over entity selection. ImGuizmo's hover
        // state is based on the previous frame's Manipulate; the drag itself
        // starts automatically inside RenderGizmo this frame.
        if (mViewportPanel.IsGizmoBusy())
            return true;

        mSceneHierarchyPanel.SetSelectedEntity(mContext.HoveredEntity);
        return false;
    }

    bool EditorLayer::OnKeyPressed(Loom::KeyPressedEvent& event) {
        HandleShortcuts(event);
        return false;
    }

    void EditorLayer::HandleShortcuts(Loom::KeyPressedEvent& event) {
        bool ctrl  = Loom::Input::IsKeyPressed(Loom::Key::LeftControl) || Loom::Input::IsKeyPressed(Loom::Key::RightControl);
        bool shift = Loom::Input::IsKeyPressed(Loom::Key::LeftShift)   || Loom::Input::IsKeyPressed(Loom::Key::RightShift);

        // Gizmo shortcuts only fire when:
        //   - no text widget is focused (otherwise typing 'W' would flip the gizmo),
        //   - the viewport has focus, and
        //   - the user is NOT driving the editor camera (RMB free-fly or MMB orbit).
        // The mouse-button gates are the fix for "moving the camera randomly flips
        // my gizmo op mid-drag" — WASD strafe shares letter keys with gizmo ops.
        bool rmb_held       = Loom::Input::IsMouseButtonPressed(Loom::Mouse::ButtonRight);
        bool mmb_held       = Loom::Input::IsMouseButtonPressed(Loom::Mouse::ButtonMiddle);
        bool gizmo_input_ok = mContext.ViewportFocused && !ImGui::GetIO().WantTextInput
                              && !rmb_held && !mmb_held;

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
            case Loom::Key::Z:
                if (ctrl && shift) { mContext.History.Redo(); return; }
                if (ctrl)          { mContext.History.Undo(); return; }
                break;
            case Loom::Key::Y:
                if (ctrl) { mContext.History.Redo(); return; }
                break;
            case Loom::Key::D:
                // Ctrl+D — duplicate the selected entity (edit mode only).
                if (ctrl && !ImGui::GetIO().WantTextInput
                    && mContext.SceneState == SceneState::Edit) {
                    Loom::Entity selected = mSceneHierarchyPanel.GetSelectedEntity();
                    if (selected && mContext.ActiveScene) {
                        auto  cmd = std::make_unique<EntityDuplicateCommand>(mContext.ActiveScene, selected);
                        auto* raw = cmd.get();
                        mContext.History.Push(std::move(cmd));
                        if (raw->GetNewUUID())
                            mSceneHierarchyPanel.SetSelectedEntity(
                                mContext.ActiveScene->GetEntityByUUID(Loom::UUID(raw->GetNewUUID())));
                    }
                    return;
                }
                break;
            case Loom::Key::Q:
                if (ctrl) { mSceneManager.RequestQuit(); return; }
                if (gizmo_input_ok) {
                    mContext.GizmoOp = GizmoOperation::None;
                    mContext.Tool    = ToolMode::Transform;
                    return;
                }
                break;
            case Loom::Key::W:
                if (gizmo_input_ok) {
                    mContext.GizmoOp = GizmoOperation::Translate;
                    mContext.Tool    = ToolMode::Transform;
                    return;
                }
                break;
            case Loom::Key::E:
                if (gizmo_input_ok) {
                    mContext.GizmoOp = GizmoOperation::Rotate;
                    mContext.Tool    = ToolMode::Transform;
                    return;
                }
                break;
            case Loom::Key::R:
                if (gizmo_input_ok) {
                    mContext.GizmoOp = GizmoOperation::Scale;
                    mContext.Tool    = ToolMode::Transform;
                    return;
                }
                break;
            case Loom::Key::B:
                if (gizmo_input_ok && !ctrl && !shift) {
                    mContext.Tool = (mContext.Tool == ToolMode::TilePaint)
                                  ? ToolMode::Transform
                                  : ToolMode::TilePaint;
                    return;
                }
                break;
            case Loom::Key::X:
                if (gizmo_input_ok && !ctrl && !shift) {
                    mContext.GizmoMode = (mContext.GizmoMode == GizmoSpace::Local) ? GizmoSpace::World : GizmoSpace::Local;
                    return;
                }
                break;
            case Loom::Key::F:
                // Frame-selected — same key as Maya / Unity (Blender's Numpad-.).
                // INTENTIONALLY global within the editor: doesn't require viewport
                // focus or hover. The common flow is "click an entity in the
                // hierarchy → press F" — at that point the mouse is over the
                // hierarchy and the hierarchy panel has focus, so any ViewportXxx
                // gate would silently swallow the keystroke. Only blocks for
                // text-input widgets and active camera control.
                {
                    bool frame_ok = !ImGui::GetIO().WantTextInput
                                    && !rmb_held && !mmb_held
                                    && !ctrl && !shift;
                    if (frame_ok) {
                        Loom::Entity selected = mSceneHierarchyPanel.GetSelectedEntity();
                        if (selected && selected.HasComponent<Loom::TransformComponent>() && mContext.ActiveScene) {
                            glm::mat4 world = mContext.ActiveScene->GetWorldTransform(selected);
                            glm::vec3 pos   = glm::vec3(world[3]);
                            glm::vec3 scale = {
                                glm::length(glm::vec3(world[0])),
                                glm::length(glm::vec3(world[1])),
                                glm::length(glm::vec3(world[2])),
                            };
                            float radius = std::max({ scale.x, scale.y, scale.z }) * 1.5f;
                            mContext.EditorCamera.FocusOn(pos, std::max(radius, 1.0f));
                        }
                        return;
                    }
                }
                break;
            default:
                break;
        }
    }

#pragma endregion

#pragma region ImGui Rendering

    void EditorLayer::OnImGuiRender() {
        // ImGuizmo per-frame init must run immediately after ImGui::NewFrame()
        // and BEFORE any Begin/End, per the canonical pattern. Nesting it
        // inside the dockspace Begin (the previous placement) left ImGuizmo's
        // internal "current window" state unset, which made IsOver/IsUsing
        // always return false even though Manipulate still drew the gizmo.
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

        if (mShowSceneHierarchyPanel)  mSceneHierarchyPanel.OnImGuiRender();
        if (mShowContentBrowserPanel)  mContentBrowserPanel.OnImGuiRender();
        if (mShowScenePropertiesPanel) mScenePropertiesPanel.OnImGuiRender(&mShowScenePropertiesPanel);

        mViewportPanel.OnImGuiRender();
        mToolbarPanel.OnImGuiRender(); // must come after viewport (needs updated ViewportBounds)

        FileDialog::Render(); // poll active ImGuiFileDialog instances; fires callbacks on OK

        ImGui::End();
    }

    void EditorLayer::RenderMainMenuBar() {
        float font_size = ImGui::GetFontSize();
        float padding_y = (24.0f - font_size) / 2.0f;
        Loom::FontManager::Push(Loom::FontType::MediumBold);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 0.0f));
        if (ImGui::BeginMainMenuBar()) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5.0f);
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Project..."))  mProjectManager.NewProject();
                if (ImGui::MenuItem("Open Project...")) mProjectManager.OpenProject();
                ImGui::BeginDisabled(!Loom::Project::GetActive());
                if (ImGui::MenuItem("Save Project As...")) mProjectManager.SaveProjectAs();
                ImGui::EndDisabled();
                {
                    const auto& recent = mProjectManager.GetRecentProjects();
                    if (recent.empty()) {
                        ImGui::BeginDisabled(true);
                        ImGui::MenuItem("Open Recent");
                        ImGui::EndDisabled();
                    } else {
                        if (ImGui::BeginMenu("Open Recent")) {
                            for (const auto& path : recent) {
                                auto label = std::filesystem::path(path).stem().string();
                                if (ImGui::MenuItem(label.c_str()))
                                    mProjectManager.OpenProject(path);
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("%s", path.c_str());
                            }
                            ImGui::EndMenu();
                        }
                    }
                }
                ImGui::BeginDisabled(!Loom::Project::GetActive());
                if (ImGui::MenuItem("Project Settings...")) mProjectManager.OpenSettings();
                ImGui::EndDisabled();
                ImGui::Separator();
                if (ImGui::MenuItem("New",        "Ctrl+N"))       mSceneManager.NewScene();
                if (ImGui::MenuItem("Open...",    "Ctrl+O"))       mSceneManager.OpenScene();
                ImGui::BeginDisabled(mContext.SceneState == SceneState::Play);
                if (ImGui::MenuItem("Save",       "Ctrl+S"))       mSceneManager.SaveScene();
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) mSceneManager.SaveSceneAs();
                ImGui::EndDisabled();
                ImGui::Separator();
                if (ImGui::MenuItem("Exit",       "Ctrl+Q"))       mSceneManager.RequestQuit();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Scene Hierarchy",  nullptr, &mShowSceneHierarchyPanel);
                ImGui::MenuItem("Content Browser",  nullptr, &mShowContentBrowserPanel);
                ImGui::MenuItem("Scene Properties", nullptr, &mShowScenePropertiesPanel);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("About")) {
                if (ImGui::MenuItem("About Weaver")) mShowAboutModal = true;
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar(1);
        Loom::FontManager::Pop();
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
