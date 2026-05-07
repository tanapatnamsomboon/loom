#include "project_manager.h"
#include <imgui.h>
#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/project/project.h>
#include <loom/project/project_serializer.h>
#include <nfd.hpp>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>

namespace Weaver {

    static void NfdRestoreFocus() {
        auto* w = (GLFWwindow*)Loom::Application::Get().GetWindow().GetNativeWindow();
        glfwFocusWindow(w);
        NfdRestoreFocus();
    }

    ProjectManager::ProjectManager(EditorContext& ctx, ContentBrowserPanel& contentBrowser, SceneManager& sceneManager)
        : mContext(ctx)
        , mContentBrowser(contentBrowser)
        , mSceneManager(sceneManager) {
        mPrefs = EditorPrefsSerializer::Load();
    }

    // -------------------------------------------------------------------------
    // New Project
    // -------------------------------------------------------------------------

    void ProjectManager::NewProject() {
        mShowWizard = true;
    }

    // -------------------------------------------------------------------------
    // Open Project
    // -------------------------------------------------------------------------

    void ProjectManager::OpenProject() {
        constexpr nfdfilteritem_t filters[] = {
            { "Loom Project", "loomproj" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::OpenDialog(out_path, filters, 2);
        NfdRestoreFocus();

        if (result == NFD_OKAY) {
            OpenProject(out_path.get());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD OpenDialog error: {}", NFD::GetError());
        }
    }

    void ProjectManager::OpenProject(const std::string& filepath) {
        auto project = std::make_shared<Loom::Project>();
        Loom::ProjectSerializer serializer(project);

        if (!serializer.Deserialize(filepath)) {
            mErrorMessage  = "Failed to open project.\n\nThe file may be corrupt or reference a missing AssetDirectory.\nCheck the console log for details.";
            mShowErrorModal = true;
            return;
        }

        Loom::Project::SetActive(project);
        AddToRecent(filepath);
        Loom::Application::Get().GetWindow().SetTitle("Weaver Editor - " + project->GetConfig().Name);
        mContentBrowser.Init();

        std::filesystem::path start_scene = Loom::Project::GetAssetFileSystemPath(project->GetConfig().StartScene);
        if (!project->GetConfig().StartScene.empty() && std::filesystem::exists(start_scene))
            mSceneManager.OpenScene(start_scene.string());
        else
            mSceneManager.NewScene();
    }

    // -------------------------------------------------------------------------
    // Save Project As
    // -------------------------------------------------------------------------

    void ProjectManager::OpenSettings() {
        auto project = Loom::Project::GetActive();
        if (!project) return;
        const auto& cfg = project->GetConfig();
        strncpy(mSettingsName,        cfg.Name.c_str(),               sizeof(mSettingsName) - 1);
        strncpy(mSettingsStartScene,  cfg.StartScene.string().c_str(), sizeof(mSettingsStartScene) - 1);
        strncpy(mSettingsWindowTitle, cfg.WindowTitle.c_str(),         sizeof(mSettingsWindowTitle) - 1);
        mSettingsWindowWidth  = cfg.WindowWidth;
        mSettingsWindowHeight = cfg.WindowHeight;
        mShowSettingsModal = true;
    }

    void ProjectManager::SaveProjectAs() {
        if (!Loom::Project::GetActive()) return;

        constexpr nfdfilteritem_t filters[] = {
            { "Loom Project", "loomproj" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::SaveDialog(out_path, filters, 2, nullptr, "MyProject.loomproj");
        NfdRestoreFocus();

        if (result == NFD_OKAY) {
            std::filesystem::path path = out_path.get();
            if (path.extension() != ".loomproj")
                path += ".loomproj";

            std::filesystem::create_directories(path.parent_path());

            Loom::ProjectSerializer serializer(Loom::Project::GetActive());
            serializer.Serialize(path.string());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD SaveDialog error: {}", NFD::GetError());
        }
    }

    // -------------------------------------------------------------------------
    // UI — New Project Wizard Modal
    // -------------------------------------------------------------------------

    void ProjectManager::OnImGuiRender() {
        if (!mDeferredOpenPath.empty()) {
            std::string path = std::move(mDeferredOpenPath);
            mDeferredOpenPath.clear();
            OpenProject(path);
        }

        if (mShowWizard) {
            ImGui::OpenPopup("New Project Wizard");
            mShowWizard = false;
        }

        if (mShowErrorModal) {
            ImGui::OpenPopup("Project Load Error");
            mShowErrorModal = false;
        }

        if (mShowSettingsModal) {
            ImGui::OpenPopup("Project Settings");
            mShowSettingsModal = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Project Load Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(mErrorMessage.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Project Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            auto project = Loom::Project::GetActive();
            if (project) {
                ImGui::Text("Project Settings");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::InputText("Project Name", mSettingsName, sizeof(mSettingsName));

                // --- Start Scene ---
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Start Scene");
                ImGui::SameLine();
                constexpr float browse_w = 28.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browse_w - ImGui::GetStyle().ItemSpacing.x);
                ImGui::InputText("##StartScene", mSettingsStartScene, sizeof(mSettingsStartScene));
                ImGui::SameLine();
                if (ImGui::Button("...##BrowseStartScene", { browse_w, 0.0f })) {
                    constexpr nfdfilteritem_t filters[] = { { "Loom Scene", "loom" } };
                    NFD::Guard      guard;
                    NFD::UniquePath out_path;
                    if (NFD::OpenDialog(out_path, filters, 1) == NFD_OKAY) {
                        std::filesystem::path picked(out_path.get());
                        std::filesystem::path asset_dir = Loom::Project::GetAssetDirectory();
                        std::error_code ec;
                        auto rel = std::filesystem::relative(picked, asset_dir, ec);
                        std::string rel_str = (!ec && !rel.empty() && rel.string().find("..") == std::string::npos)
                            ? rel.generic_string() : picked.generic_string();
                        strncpy(mSettingsStartScene, rel_str.c_str(), sizeof(mSettingsStartScene) - 1);
                    }
                    NfdRestoreFocus();
                }
                ImGui::TextDisabled("  Relative to asset directory");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextUnformatted("Runtime Window");
                ImGui::Spacing();

                ImGui::InputText("Window Title",  mSettingsWindowTitle, sizeof(mSettingsWindowTitle));
                ImGui::TextDisabled("  Leave blank to use the project name");
                ImGui::InputInt("Width",  &mSettingsWindowWidth);
                ImGui::InputInt("Height", &mSettingsWindowHeight);
                mSettingsWindowWidth  = std::max(1, mSettingsWindowWidth);
                mSettingsWindowHeight = std::max(1, mSettingsWindowHeight);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Apply", ImVec2(120, 0))) {
                    auto& cfg          = project->GetConfig();
                    cfg.Name           = mSettingsName;
                    cfg.StartScene     = mSettingsStartScene;
                    cfg.WindowTitle    = mSettingsWindowTitle;
                    cfg.WindowWidth    = mSettingsWindowWidth;
                    cfg.WindowHeight   = mSettingsWindowHeight;
                    Loom::Application::Get().GetWindow().SetTitle("Weaver Editor - " + cfg.Name);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (!ImGui::BeginPopupModal("New Project Wizard", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        if (!mPrefs.RecentProjects.empty()) {
            ImGui::TextUnformatted("Recent Projects");
            ImGui::Separator();
            for (const auto& proj_path : mPrefs.RecentProjects) {
                auto stem = std::filesystem::path(proj_path).stem().string();
                if (ImGui::Selectable(stem.c_str())) {
                    mDeferredOpenPath = proj_path;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", proj_path.c_str());
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        ImGui::Text("Create a New Loom Engine Project");
        ImGui::Separator();

        ImGui::InputText("Project Name", mProjectName, sizeof(mProjectName));

        ImGui::Text("Location: %s", mProjectPath.empty() ? "Not Selected" : mProjectPath.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            NFD::Guard      nfd_guard;
            NFD::UniquePath out_path;
            if (NFD::PickFolder(out_path) == NFD_OKAY)
                mProjectPath = out_path.get();
            NfdRestoreFocus();
        }

        ImGui::Spacing();
        ImGui::Separator();

        bool can_create = !mProjectPath.empty() && strlen(mProjectName) > 0;
        if (!can_create) ImGui::BeginDisabled();

        if (ImGui::Button("Create Project", ImVec2(120, 0))) {
            std::filesystem::path root_dir  = std::filesystem::path((const char8_t*)mProjectPath.c_str())
                                            / std::filesystem::path((const char8_t*)mProjectName);
            std::filesystem::path asset_dir = root_dir / "assets";

            std::filesystem::create_directories(asset_dir / "scenes");
            std::filesystem::create_directories(asset_dir / "textures");
            std::filesystem::create_directories(asset_dir / "scripts");

            auto new_project = std::make_shared<Loom::Project>();
            new_project->GetConfig().Name           = mProjectName;
            new_project->GetConfig().AssetDirectory = "assets";
            new_project->SetProjectDirectory(root_dir);

            std::filesystem::path proj_file = root_dir / (std::string(mProjectName) + ".loomproj");
            Loom::ProjectSerializer serializer(new_project);
            serializer.Serialize(proj_file.string());

            Loom::Project::SetActive(new_project);
            Loom::Application::Get().GetWindow().SetTitle("Weaver Editor - " + std::string(mProjectName));
            mContentBrowser.Init();
            mSceneManager.NewScene();

            AddToRecent(proj_file.string());
            LOOM_CORE_INFO("Created new project at: {0}", root_dir.string());
            ImGui::CloseCurrentPopup();
        }

        if (!can_create) ImGui::EndDisabled();

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    void ProjectManager::AddToRecent(const std::string& filepath) {
        std::string path = filepath;  // copy before mutating the vector; filepath may alias an element in it
        auto& list = mPrefs.RecentProjects;
        list.erase(std::remove(list.begin(), list.end(), path), list.end());
        list.insert(list.begin(), std::move(path));
        if (list.size() > EditorPrefs::kMaxRecent)
            list.resize(EditorPrefs::kMaxRecent);
        EditorPrefsSerializer::Save(mPrefs);
    }

} // namespace Weaver
