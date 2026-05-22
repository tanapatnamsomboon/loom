#include "file_dialog.h"
#include "icons_fork_awesome.h"
#include <ImGuiFileDialog.h>
#include <imgui.h>
#include <loom/project/project.h>
#include <unordered_map>

namespace Weaver::FileDialog {

    namespace {
        std::unordered_map<std::string, Callback> sActive;

        constexpr ImVec2 kMinSize = { 700, 450 };

        // Resolves the directory a dialog should open in. An explicit start_dir
        // always wins; otherwise we default to the project's asset directory so
        // the Game Developer lands inside their own assets, not the build dir.
        std::string ResolveStartDir(const std::string& start_dir) {
            if (!start_dir.empty())
                return start_dir;
            if (Loom::Project::GetActive())
                return Loom::Project::GetAssetDirectory().generic_string();
            return ".";
        }
    }

    void Init() {
        auto* dlg = ImGuiFileDialog::Instance();

        // Folders and the generic-file fallback.
        dlg->SetFileStyle(IGFD_FileStyleByTypeDir,  "", ImVec4(0.90f, 0.78f, 0.40f, 1.0f), ICON_FK_FOLDER);
        dlg->SetFileStyle(IGFD_FileStyleByTypeFile, "", ImVec4(0.78f, 0.80f, 0.82f, 1.0f), ICON_FK_FILE);

        // Per-extension color + icon. Each call styles every file with that extension.
        auto ext = [dlg](const char* e, const ImVec4& c, const char* icon) {
            dlg->SetFileStyle(IGFD_FileStyleByExtention, e, c, icon);
        };

        const ImVec4 kScene  = { 0.55f, 0.82f, 0.55f, 1.0f }; // green
        const ImVec4 kPrefab = { 0.45f, 0.80f, 0.80f, 1.0f }; // teal
        const ImVec4 kScript = { 0.48f, 0.66f, 0.96f, 1.0f }; // blue
        const ImVec4 kImage  = { 0.80f, 0.62f, 0.96f, 1.0f }; // violet
        const ImVec4 kAudio  = { 0.96f, 0.66f, 0.42f, 1.0f }; // orange
        const ImVec4 kMesh   = { 0.52f, 0.86f, 0.86f, 1.0f }; // cyan
        const ImVec4 kHdr    = { 0.96f, 0.82f, 0.46f, 1.0f }; // gold
        const ImVec4 kFont   = { 0.93f, 0.63f, 0.76f, 1.0f }; // pink
        const ImVec4 kProj   = { 0.98f, 0.70f, 0.36f, 1.0f }; // bright orange

        ext(".loom",     kScene,  ICON_FK_FILE_TEXT_O);
        ext(".lprefab",  kPrefab, ICON_FK_CUBE);
        ext(".loomproj", kProj,   ICON_FK_CUBE);
        ext(".lua",      kScript, ICON_FK_FILE_CODE_O);

        for (const char* e : { ".png", ".jpg", ".jpeg", ".bmp", ".tga" })
            ext(e, kImage, ICON_FK_FILE_IMAGE_O);
        for (const char* e : { ".wav", ".mp3", ".ogg", ".flac" })
            ext(e, kAudio, ICON_FK_FILE_AUDIO_O);
        for (const char* e : { ".glb", ".gltf" })
            ext(e, kMesh, ICON_FK_CUBE);
        for (const char* e : { ".ttf", ".otf" })
            ext(e, kFont, ICON_FK_FONT);

        ext(".hdr", kHdr, ICON_FK_PICTURE_O);
    }

    void Open(const std::string& key, const std::string& title, const char* filters,
              Callback on_pick, const std::string& start_dir) {
        IGFD::FileDialogConfig config;
        config.path  = ResolveStartDir(start_dir);
        config.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
        sActive[key] = std::move(on_pick);
    }

    void Save(const std::string& key, const std::string& title, const char* filters,
              const std::string& default_filename, Callback on_pick,
              const std::string& start_dir) {
        IGFD::FileDialogConfig config;
        config.path     = ResolveStartDir(start_dir);
        config.fileName = default_filename;
        config.flags    = ImGuiFileDialogFlags_ConfirmOverwrite | ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
        sActive[key] = std::move(on_pick);
    }

    void PickFolder(const std::string& key, const std::string& title, Callback on_pick,
                    const std::string& start_dir) {
        IGFD::FileDialogConfig config;
        config.path  = ResolveStartDir(start_dir);
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
