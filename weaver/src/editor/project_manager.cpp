#include "project_manager.h"
#include <imgui.h>
#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/project/project.h>
#include <loom/project/project_serializer.h>
#include <nfd.hpp>
#include <filesystem>

namespace Weaver {

    ProjectManager::ProjectManager(EditorContext& ctx, ContentBrowserPanel& contentBrowser, SceneManager& sceneManager)
        : mContext(ctx)
        , mContentBrowser(contentBrowser)
        , mSceneManager(sceneManager) {}

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

        if (result == NFD_OKAY) {
            OpenProject(out_path.get());
        } else if (result == NFD_ERROR) {
            LOOM_CORE_ERROR("NFD OpenDialog error: {}", NFD::GetError());
        }
    }

    void ProjectManager::OpenProject(const std::string& filepath) {
        auto project = std::make_shared<Loom::Project>();
        Loom::ProjectSerializer serializer(project);

        if (serializer.Deserialize(filepath)) {
            Loom::Project::SetActive(project);
            Loom::Application::Get().GetWindow().SetTitle("Weaver Editor - " + project->GetConfig().Name);
            mContentBrowser.Init();

            std::filesystem::path start_scene = Loom::Project::GetAssetFileSystemPath(project->GetConfig().StartScene);
            if (!project->GetConfig().StartScene.empty() && std::filesystem::exists(start_scene))
                mSceneManager.OpenScene(start_scene.string());
            else
                mSceneManager.NewScene();
        }
    }

    // -------------------------------------------------------------------------
    // Save Project As
    // -------------------------------------------------------------------------

    void ProjectManager::SaveProjectAs() {
        constexpr nfdfilteritem_t filters[] = {
            { "Loom Project", "loomproj" },
            { "All Files", "*" },
        };

        NFD::Guard      nfd_guard;
        NFD::UniquePath out_path;
        nfdresult_t     result = NFD::SaveDialog(out_path, filters, 2, nullptr, "MyProject.loomproj");

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
        if (mShowWizard) {
            ImGui::OpenPopup("New Project Wizard");
            mShowWizard = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (!ImGui::BeginPopupModal("New Project Wizard", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

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

} // namespace Weaver
