#include "content_browser_panel.h"
#include <imgui.h>
#include <loom/project/project.h>
#include <loom/asset/asset_manager.h>

namespace Weaver {

    void ContentBrowserPanel::Init() {
        if (!Loom::Project::GetActive()) {
            mBaseDirectory = "";
            mCurrentDirectory = "";
            return;
        }

        mBaseDirectory = Loom::Project::GetAssetDirectory();
        mCurrentDirectory = mBaseDirectory;

        std::string dir_icon_path = Loom::Project::GetEngineAssetFileSystemPath("icons/directory_icon.png").generic_string();
        std::string file_icon_path = Loom::Project::GetEngineAssetFileSystemPath("icons/file_icon.png").generic_string();

        Loom::TextureSpecification icon_spec;
        icon_spec.Filter       = Loom::FilterMode::Linear;
        icon_spec.GenerateMips = false;
        mDirectoryIcon = Loom::AssetManager::GetTexture(dir_icon_path, icon_spec);
        mFileIcon      = Loom::AssetManager::GetTexture(file_icon_path, icon_spec);
    }

    void ContentBrowserPanel::OnImGuiRender() {
        ImGui::Begin("Content Browser");

        if (!Loom::Project::GetActive() || mCurrentDirectory.empty()) {
            ImGui::Text("No Project Loaded. Please create or open a project.");
            ImGui::End();
            return;
        }

        std::error_code ec;
        if (!std::filesystem::exists(mCurrentDirectory, ec) || !std::filesystem::is_directory(mCurrentDirectory, ec)) {
            ImGui::Text("Warning: The asset directory could not be found on disk.");
            ImGui::End();
            return;
        }

        if (!std::filesystem::equivalent(mCurrentDirectory, mBaseDirectory, ec)) {
            if (ImGui::Button("<- Back")) {
                mCurrentDirectory = mCurrentDirectory.parent_path();
            }
        }

        static float padding        = 16.0f;
        static float thumbnail_size = 128.0f;
        float        cell_size      = thumbnail_size + padding;

        float panel_width  = ImGui::GetContentRegionAvail().x;
        int   column_count = (int)(panel_width / cell_size);
        if (column_count < 1)
            column_count = 1;

        ImGui::Columns(column_count, 0, false);

        for (auto& directory_entry : std::filesystem::directory_iterator(mCurrentDirectory)) {
            const auto& path            = directory_entry.path();
            std::string filename_string = path.filename().string();

            std::shared_ptr<Loom::Texture2D> icon = directory_entry.is_directory() ? mDirectoryIcon : mFileIcon;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::ImageButton(filename_string.c_str(), (ImTextureID)icon->GetRendererID(), { thumbnail_size, thumbnail_size }, { 0, 1 }, { 1, 0 });
            ImGui::PopStyleColor();

            if (ImGui::BeginDragDropSource()) {
                std::string relative_str = std::filesystem::relative(path, mBaseDirectory).generic_string();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", relative_str.c_str(), relative_str.size() + 1);
                ImGui::Text("%s", filename_string.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (directory_entry.is_directory()) {
                    mCurrentDirectory /= path.filename();
                } else if (path.extension() == ".loom") {
                    if (mSceneOpenCallback)
                        mSceneOpenCallback(path);
                } else if (path.extension() == ".lprefab") {
                    if (mPrefabInstantiateCallback) {
                        auto rel = std::filesystem::relative(path, mBaseDirectory);
                        mPrefabInstantiateCallback(rel);
                    }
                }
            }

            ImGui::TextWrapped("%s", filename_string.c_str());
            ImGui::NextColumn();
        }

        ImGui::End();
    }

} // namespace Weaver
