#include "file_dialog.h"
#include <ImGuiFileDialog.h>
#include <imgui.h>
#include <loom/project/project.h>
#include <unordered_map>

namespace Weaver::FileDialog {

    namespace {
        std::unordered_map<std::string, Callback> sActive;

        constexpr ImVec2 kMinSize = { 700, 450 };
    }

    void Open(const std::string& key, const std::string& title, const char* filters, Callback on_pick) {
        IGFD::FileDialogConfig config;
        config.path  = ".";
        config.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
        sActive[key] = std::move(on_pick);
    }

    void Save(const std::string& key, const std::string& title, const char* filters,
              const std::string& default_filename, Callback on_pick) {
        IGFD::FileDialogConfig config;
        config.path     = ".";
        config.fileName = default_filename;
        config.flags    = ImGuiFileDialogFlags_ConfirmOverwrite | ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
        sActive[key] = std::move(on_pick);
    }

    void PickFolder(const std::string& key, const std::string& title, Callback on_pick) {
        IGFD::FileDialogConfig config;
        config.path  = ".";
        config.flags = ImGuiFileDialogFlags_Modal;
        // nullptr filters puts ImGuiFileDialog into folder-pick mode.
        ImGuiFileDialog::Instance()->OpenDialog(key, title, nullptr, config);
        sActive[key] = std::move(on_pick);
    }

    void Render() {
        for (auto it = sActive.begin(); it != sActive.end(); ) {
            const std::string& key = it->first;
            if (ImGuiFileDialog::Instance()->Display(key, ImGuiWindowFlags_NoCollapse, kMinSize)) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                    Callback cb = std::move(it->second);
                    ImGuiFileDialog::Instance()->Close();
                    it = sActive.erase(it);
                    cb(path); // invoke after erase so a callback may safely re-open the same key
                    continue;
                }
                ImGuiFileDialog::Instance()->Close();
                it = sActive.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::string MakeAssetRelative(const std::filesystem::path& absolute) {
        if (!Loom::Project::GetActive())
            return absolute.generic_string();
        std::filesystem::path asset_dir = Loom::Project::GetAssetDirectory();
        std::error_code ec;
        auto rel = std::filesystem::relative(absolute, asset_dir, ec);
        if (!ec && !rel.empty() && rel.string().find("..") == std::string::npos)
            return rel.generic_string();
        return absolute.generic_string();
    }

} // namespace Weaver::FileDialog
