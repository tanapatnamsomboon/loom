#include "scene_manager.h"
#include "file_dialog.h"
#include <imgui.h>
#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/project/project.h>
#include <loom/scene/components.h>
#include <loom/scene/scene_loader.h>
#include <loom/scene/scene_serializer.h>
#include <filesystem>

namespace Weaver {

    SceneManager::SceneManager(EditorContext& ctx)
        : mContext(ctx) {}

    // -------------------------------------------------------------------------
    // New Scene
    // -------------------------------------------------------------------------

    void SceneManager::NewScene() {
        if (mContext.IsDirty()) {
            mPendingAction = PendingAction::New;
            mShowSavePrompt = true;
        } else {
            NewSceneImpl();
        }
    }

    void SceneManager::NewSceneImpl() {
        mContext.ActiveScene = std::make_shared<Loom::Scene>();
        mContext.EditorScene = mContext.ActiveScene;
        mContext.HierarchyPanel->SetContext(mContext.ActiveScene);
        mContext.CurrentScenePath.clear();
        mContext.SceneDirty = false;
        mContext.History.Clear();
    }

    // -------------------------------------------------------------------------
    // Open Scene
    // -------------------------------------------------------------------------

    void SceneManager::OpenScene() {
        FileDialog::Open("OpenScene", "Open Scene", ".loom",
            [this](const std::string& path) { OpenScene(path); });
    }

    void SceneManager::OpenScene(const std::string& filepath) {
        if (mContext.IsDirty()) {
            mPendingAction = PendingAction::Open;
            mPendingPath   = filepath;
            mShowSavePrompt = true;
        } else {
            OpenSceneImpl(filepath);
        }
    }

    void SceneManager::OpenSceneImpl(const std::string& filepath) {
        std::filesystem::path path = std::filesystem::path((const char8_t*)filepath.c_str());

        if (!std::filesystem::exists(path)) {
            LOOM_CORE_WARN("SceneManager: scene file '{}' does not exist", filepath);
            return;
        }

        auto new_scene = std::make_shared<Loom::Scene>();
        Loom::SceneSerializer serializer(new_scene);

        if (serializer.Deserialize(filepath, &mContext.EditorCamera)) {
            mContext.EditorScene = new_scene;
            mContext.ActiveScene = mContext.EditorScene;
            mContext.HierarchyPanel->SetContext(mContext.EditorScene);
            mContext.CurrentScenePath = filepath;
            mContext.SceneDirty       = false;
            mContext.History.Clear();
        }
    }

    // -------------------------------------------------------------------------
    // Save Scene
    // -------------------------------------------------------------------------

    void SceneManager::SaveScene(std::function<void()> on_complete) {
        if (mContext.SceneState == SceneState::Play) {
            LOOM_CORE_WARN("SceneManager: Save is disabled during Play mode");
            return;
        }
        if (mContext.CurrentScenePath.empty()) {
            SaveSceneAs(std::move(on_complete));
            return;
        }
        Loom::SceneSerializer serializer(mContext.ActiveScene);
        serializer.Serialize(mContext.CurrentScenePath, &mContext.EditorCamera);
        mContext.SceneDirty = false;
        mContext.History.MarkSavePoint();
        if (on_complete) on_complete();
    }

    void SceneManager::SaveSceneAs(std::function<void()> on_complete) {
        if (mContext.SceneState == SceneState::Play) {
            LOOM_CORE_WARN("SceneManager: Save As is disabled during Play mode");
            return;
        }
        FileDialog::Save("SaveScene", "Save Scene", ".loom", "scene.loom",
            [this, on_complete = std::move(on_complete)](const std::string& picked) {
                std::filesystem::path path = picked;
                if (path.extension() != ".loom")
                    path += ".loom";

                std::filesystem::create_directories(path.parent_path());

                Loom::SceneSerializer serializer(mContext.ActiveScene);
                serializer.Serialize(path.string(), &mContext.EditorCamera);
                mContext.CurrentScenePath = path.string();
                mContext.SceneDirty       = false;
                mContext.History.MarkSavePoint();

                // Set the project's start scene if it hasn't been assigned yet
                auto active_project = Loom::Project::GetActive();
                if (active_project && active_project->GetConfig().StartScene.empty()) {
                    std::filesystem::path asset_dir            = Loom::Project::GetAssetDirectory();
                    std::filesystem::path relative_scene_path  = std::filesystem::relative(path, asset_dir);
                    active_project->GetConfig().StartScene     = relative_scene_path;
                    LOOM_CORE_INFO("Set project StartScene to {}", relative_scene_path.string());
                }

                if (on_complete) on_complete();
            });
    }

    // -------------------------------------------------------------------------
    // Quit
    // -------------------------------------------------------------------------

    void SceneManager::RequestQuit() {
        if (mContext.IsDirty()) {
            mPendingAction  = PendingAction::Quit;
            mShowSavePrompt = true;
        } else {
            mShowQuitPrompt = true;
        }
    }

    // -------------------------------------------------------------------------
    // Play / Stop
    // -------------------------------------------------------------------------

    void SceneManager::OnScenePlay() {
        mContext.SceneState = SceneState::Play;
        mContext.HierarchyPanel->SetPlayMode(true);

        Loom::Entity selected     = mContext.HierarchyPanel->GetSelectedEntity();
        bool         has_selected = (bool)selected;
        uint64_t     selected_uuid = 0;
        if (has_selected)
            selected_uuid = (uint64_t)selected.GetComponent<Loom::IDComponent>().ID;

        mContext.ActiveScene = Loom::Scene::Copy(mContext.EditorScene);
        mContext.ActiveScene->OnRuntimeStart();
        mContext.HierarchyPanel->SetContext(mContext.ActiveScene);

        if (has_selected) {
            Loom::Entity runtime_entity = mContext.ActiveScene->GetEntityByUUID(Loom::UUID(selected_uuid));
            if (runtime_entity)
                mContext.HierarchyPanel->SetSelectedEntity(runtime_entity);
        }
    }

    void SceneManager::OnSceneStop() {
        Loom::Entity selected     = mContext.HierarchyPanel->GetSelectedEntity();
        bool         has_selected = (bool)selected;
        uint64_t     selected_uuid = 0;
        if (has_selected)
            selected_uuid = (uint64_t)selected.GetComponent<Loom::IDComponent>().ID;

        mContext.ActiveScene->OnRuntimeStop();
        Loom::SceneLoader::Get().Consume(); // discard any mid-frame transition queued before stop
        mContext.SceneState  = SceneState::Edit;
        mContext.HierarchyPanel->SetPlayMode(false);
        mContext.ActiveScene = mContext.EditorScene;
        mContext.HierarchyPanel->SetContext(mContext.ActiveScene);

        if (has_selected) {
            Loom::Entity editor_entity = mContext.ActiveScene->GetEntityByUUID(Loom::UUID(selected_uuid));
            if (editor_entity)
                mContext.HierarchyPanel->SetSelectedEntity(editor_entity);
        }
    }

    void SceneManager::OnRuntimeSceneTransition(const std::string& relative_path, bool is_reload) {
        mContext.ActiveScene->OnRuntimeStop();

        std::shared_ptr<Loom::Scene> new_scene;

        if (is_reload) {
            LOOM_CORE_INFO("SceneManager: reloading runtime scene");
            new_scene = Loom::Scene::Copy(mContext.EditorScene);
        } else {
            std::filesystem::path full_path = Loom::Project::GetAssetDirectory() / relative_path;

            if (!std::filesystem::exists(full_path)) {
                LOOM_CORE_ERROR("SceneManager: scene '{}' not found — reloading current", relative_path);
                new_scene = Loom::Scene::Copy(mContext.EditorScene);
            } else {
                new_scene = std::make_shared<Loom::Scene>();
                Loom::SceneSerializer serializer(new_scene);
                if (!serializer.Deserialize(full_path.string())) {
                    LOOM_CORE_ERROR("SceneManager: failed to load '{}' — reloading current", relative_path);
                    new_scene = Loom::Scene::Copy(mContext.EditorScene);
                } else {
                    LOOM_CORE_INFO("SceneManager: transitioning to '{}'", relative_path);
                }
            }
        }

        mContext.HierarchyPanel->SetSelectedEntity(Loom::Entity{});
        mContext.ActiveScene = new_scene;
        mContext.HierarchyPanel->SetContext(new_scene);
        mContext.ActiveScene->OnRuntimeStart();
    }

    // -------------------------------------------------------------------------
    // UI — Save Prompt Modal
    // -------------------------------------------------------------------------

    void SceneManager::OnImGuiRender() {
        // Queue popup opens before BeginPopupModal — both must be checked every frame.
        if (mShowSavePrompt) { ImGui::OpenPopup("Save Changes?"); mShowSavePrompt = false; }
        if (mShowQuitPrompt) { ImGui::OpenPopup("Quit?");         mShowQuitPrompt = false; }

        // Center modals over the main viewport so they land on the active monitor, not the leftmost one.
        ImVec2 viewport_center = ImGui::GetMainViewport()->GetCenter();

        // --- Save Changes? ---
        ImGui::SetNextWindowPos(viewport_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Save Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes in the current scene.\nDo you want to save them?");
            ImGui::Separator();

            // Capture the pending action so it can fire after the (possibly async) save completes.
            auto consume_pending = [this]() -> std::function<void()> {
                PendingAction action = mPendingAction;
                std::string   path   = mPendingPath;
                mPendingAction = PendingAction::None;
                mPendingPath.clear();
                return [this, action, path]() {
                    if (action == PendingAction::Open) OpenSceneImpl(path);
                    if (action == PendingAction::New)  NewSceneImpl();
                    if (action == PendingAction::Quit) Loom::Application::Get().Close();
                };
            };

            if (ImGui::Button("Save", ImVec2(100, 0))) {
                SaveScene(consume_pending());
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
                consume_pending()();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                mPendingAction = PendingAction::None;
                mPendingPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // --- Quit? ---
        ImGui::SetNextWindowPos(viewport_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Quit?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to quit?");
            ImGui::Separator();

            if (ImGui::Button("Quit", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
                Loom::Application::Get().Close();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

} // namespace Weaver
