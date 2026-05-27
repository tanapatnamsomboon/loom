#include "file_dialog.h"
#include "icons_fork_awesome.h"
#include <ImGuiFileDialog.h>
#include <imgui.h>
#include <loom/project/project.h>
#include <cstdlib>
#include <system_error>
#include <unordered_map>

namespace Weaver::FileDialog {

    namespace {
        struct ActiveDialog {
            Callback Cb;
            bool     ModalScoped = false; // rendered by RenderModalScope() only
        };
        std::unordered_map<std::string, ActiveDialog> sActive;

        constexpr ImVec2 kMinSize = { 920, 540 };

        std::string ResolveStartDir(const std::string& start_dir) {
            if (!start_dir.empty())
                return start_dir;
            if (Loom::Project::GetActive())
                return Loom::Project::GetAssetDirectory().generic_string();
            return ".";
        }

        IGFD::FileStyle PlaceStyle(const char* icon) {
            return IGFD::FileStyle(ImVec4(0.82f, 0.84f, 0.88f, 1.0f), icon);
        }

        void RefreshProjectPlaces() {
            auto* dlg = ImGuiFileDialog::Instance();
            dlg->RemovePlacesGroup("Project");

            if (!Loom::Project::GetActive())
                return;

            dlg->AddPlacesGroup("Project", 1, false, true);
            auto* group = dlg->GetPlacesGroupPtr("Project");
            if (!group)
                return;

            std::filesystem::path assets = Loom::Project::GetAssetDirectory();
            group->AddPlace("Assets", assets.generic_string(), false, PlaceStyle(ICON_FK_FOLDER_OPEN));

            for (const char* sub : { "scenes", "textures", "scripts", "models",
                                     "meshes", "audio", "fonts", "prefabs", "materials" }) {
                std::filesystem::path p = assets / sub;
                std::error_code ec;
                if (std::filesystem::is_directory(p, ec))
                    group->AddPlace(sub, p.generic_string(), false, PlaceStyle(ICON_FK_FOLDER));
            }
        }

        void SetupSystemPlaces() {
            const char* home = std::getenv("USERPROFILE");
            if (!home)
                return;

            auto* dlg = ImGuiFileDialog::Instance();
            dlg->RemovePlacesGroup("System");
            dlg->AddPlacesGroup("System", 2, false, true);
            auto* group = dlg->GetPlacesGroupPtr("System");
            if (!group)
                return;

            std::filesystem::path user = home;
            auto add = [&](const char* name, const std::filesystem::path& p, const char* icon) {
                std::error_code ec;
                if (std::filesystem::is_directory(p, ec))
                    group->AddPlace(name, p.generic_string(), false, PlaceStyle(icon));
            };
            add("Home",      user,               ICON_FK_HOME);
            add("Desktop",   user / "Desktop",   ICON_FK_DESKTOP);
            add("Documents", user / "Documents", ICON_FK_FILE_TEXT_O);
            add("Downloads", user / "Downloads", ICON_FK_DOWNLOAD);
            add("Pictures",  user / "Pictures",  ICON_FK_PICTURE_O);
            add("Music",     user / "Music",     ICON_FK_MUSIC);
            add("Videos",    user / "Videos",    ICON_FK_FILM);
        }
    }

    void Init() {
        auto* dlg = ImGuiFileDialog::Instance();

        dlg->SetFileStyle(IGFD_FileStyleByTypeDir,  "", ImVec4(0.90f, 0.78f, 0.40f, 1.0f), ICON_FK_FOLDER);
        dlg->SetFileStyle(IGFD_FileStyleByTypeFile, "", ImVec4(0.78f, 0.80f, 0.82f, 1.0f), ICON_FK_FILE);

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

        SetupSystemPlaces();
    }

    namespace {
        void OpenImpl(const std::string& key, const std::string& title, const char* filters,
                      Callback on_pick, const std::string& start_dir, bool modal_scoped) {
            RefreshProjectPlaces();
            IGFD::FileDialogConfig config;
            config.path  = ResolveStartDir(start_dir);
            config.flags = ImGuiFileDialogFlags_Modal;
            ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
            sActive[key] = { std::move(on_pick), modal_scoped };
        }

        void SaveImpl(const std::string& key, const std::string& title, const char* filters,
                      const std::string& default_filename, Callback on_pick,
                      const std::string& start_dir, bool modal_scoped) {
            RefreshProjectPlaces();
            IGFD::FileDialogConfig config;
            config.path     = ResolveStartDir(start_dir);
            config.fileName = default_filename;
            config.flags    = ImGuiFileDialogFlags_ConfirmOverwrite | ImGuiFileDialogFlags_Modal;
            ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
            sActive[key] = { std::move(on_pick), modal_scoped };
        }

        void PickFolderImpl(const std::string& key, const std::string& title, Callback on_pick,
                            const std::string& start_dir, bool modal_scoped) {
            RefreshProjectPlaces();
            IGFD::FileDialogConfig config;
            config.path  = ResolveStartDir(start_dir);
            config.flags = ImGuiFileDialogFlags_Modal;
            // nullptr filters puts ImGuiFileDialog into folder-pick mode.
            ImGuiFileDialog::Instance()->OpenDialog(key, title, nullptr, config);
            sActive[key] = { std::move(on_pick), modal_scoped };
        }

        // Renders every active dialog whose ModalScoped flag matches `want_modal_scoped`.
        // ImGuiFileDialog's Display() calls BeginPopupModal; modal-scoped entries
        // must be displayed inside their parent modal's Begin/End block to nest.
        void RenderFiltered(bool want_modal_scoped) {
            if (sActive.empty())
                return;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,    8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,     8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,     6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,     4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,      3.0f);

            for (auto it = sActive.begin(); it != sActive.end(); ) {
                if (it->second.ModalScoped != want_modal_scoped) { ++it; continue; }
                const std::string& key = it->first;
                if (ImGuiFileDialog::Instance()->Display(key, ImGuiWindowFlags_NoCollapse, kMinSize)) {
                    if (ImGuiFileDialog::Instance()->IsOk()) {
                        // GetFilePathName() is empty in folder-pick mode when
                        // the user clicks OK without single-selecting an item
                        // (the natural flow: navigate into the target folder
                        // and confirm). Fall back to the current navigation
                        // path so folder picks always yield a usable result.
                        std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                        if (path.empty())
                            path = ImGuiFileDialog::Instance()->GetCurrentPath();
                        Callback cb = std::move(it->second.Cb);
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

            ImGui::PopStyleVar(6);
        }
    } // namespace

    void Open(const std::string& key, const std::string& title, const char* filters,
              Callback on_pick, const std::string& start_dir) {
        OpenImpl(key, title, filters, std::move(on_pick), start_dir, /*modal_scoped*/ false);
    }

    void Save(const std::string& key, const std::string& title, const char* filters,
              const std::string& default_filename, Callback on_pick,
              const std::string& start_dir) {
        SaveImpl(key, title, filters, default_filename, std::move(on_pick), start_dir, /*modal_scoped*/ false);
    }

    void PickFolder(const std::string& key, const std::string& title, Callback on_pick,
                    const std::string& start_dir) {
        PickFolderImpl(key, title, std::move(on_pick), start_dir, /*modal_scoped*/ false);
    }

    void OpenInModal(const std::string& key, const std::string& title, const char* filters,
                     Callback on_pick, const std::string& start_dir) {
        OpenImpl(key, title, filters, std::move(on_pick), start_dir, /*modal_scoped*/ true);
    }

    void SaveInModal(const std::string& key, const std::string& title, const char* filters,
                     const std::string& default_filename, Callback on_pick,
                     const std::string& start_dir) {
        SaveImpl(key, title, filters, default_filename, std::move(on_pick), start_dir, /*modal_scoped*/ true);
    }

    void PickFolderInModal(const std::string& key, const std::string& title, Callback on_pick,
                           const std::string& start_dir) {
        PickFolderImpl(key, title, std::move(on_pick), start_dir, /*modal_scoped*/ true);
    }

    void Render()            { RenderFiltered(/*want_modal_scoped*/ false); }
    void RenderModalScope()  { RenderFiltered(/*want_modal_scoped*/ true); }

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
