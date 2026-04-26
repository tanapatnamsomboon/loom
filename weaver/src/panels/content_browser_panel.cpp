#include "content_browser_panel.h"
#include <imgui.h>

namespace Weaver {

    ContentBrowserPanel::ContentBrowserPanel()
        : mBaseDirectory(std::filesystem::path("assets"))
        , mCurrentDirectory(mBaseDirectory) {
        mDirectoryIcon = Loom::Texture2D::Create("assets/icons/directory_icon.png");
        mFileIcon      = Loom::Texture2D::Create("assets/icons/file_icon.png");
    }

    void ContentBrowserPanel::OnImGuiRender() {
        ImGui::Begin("Content Browser");

        if (!std::filesystem::equivalent(mCurrentDirectory, mBaseDirectory)) {
            if (ImGui::Button("<-")) {
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
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (directory_entry.is_directory()) {
                    mCurrentDirectory /= path.filename();
                }
            }
            ImGui::TextWrapped("%s", filename_string.c_str());
            ImGui::NextColumn();
        }

        ImGui::End();
    }

} // namespace Weaver
