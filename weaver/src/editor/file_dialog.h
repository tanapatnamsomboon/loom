#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace Weaver::FileDialog {

    using Callback = std::function<void(const std::string& abs_path)>;

    // ImGuiFileDialog filter examples:
    //   ".loom"                    — single extension
    //   ".png,.jpg,.jpeg,.bmp"     — comma-separated
    //   "Images (*.png *.jpg){.png,.jpg}"   — labeled
    // Pass nullptr filters to PickFolder for directory mode.
    //
    // start_dir: directory the dialog opens in. Leave empty to use the default —
    // the active project's asset directory (or the process CWD if no project).

    // Registers per-file-type colors and Fork Awesome icons. Call once at editor
    // startup, after the ImGui font atlas has been built.
    void Init();

    void Open(const std::string& key, const std::string& title, const char* filters,
              Callback on_pick, const std::string& start_dir = "");
    void Save(const std::string& key, const std::string& title, const char* filters,
              const std::string& default_filename, Callback on_pick,
              const std::string& start_dir = "");
    void PickFolder(const std::string& key, const std::string& title, Callback on_pick,
                    const std::string& start_dir = "");

    // ── Modal-scoped variants ─────────────────────────────────────────────
    // When opening a file dialog from inside an ImGui modal (e.g. the New
    // Project Wizard's Browse... button), use these. They register the dialog
    // as "modal-scoped" — the outer Render() skips it; RenderModalScope()
    // must be called from INSIDE the parent modal's Begin/End block so the
    // file dialog nests correctly in ImGui's popup stack. Skipping that call
    // closes the parent modal because the nested-popup chain breaks.
    void OpenInModal(const std::string& key, const std::string& title, const char* filters,
                     Callback on_pick, const std::string& start_dir = "");
    void SaveInModal(const std::string& key, const std::string& title, const char* filters,
                     const std::string& default_filename, Callback on_pick,
                     const std::string& start_dir = "");
    void PickFolderInModal(const std::string& key, const std::string& title, Callback on_pick,
                           const std::string& start_dir = "");

    // Outer-scope render (renders only non-modal-scoped dialogs).
    // Call once per frame after panels render, while inside the ImGui frame.
    void Render();
    // Modal-scope render (renders only modal-scoped dialogs).
    // Call inside any modal's Begin/End block before EndPopup.
    void RenderModalScope();

    // Returns the path relative to the active project's asset directory if `absolute` is inside it
    // (forward-slash form), otherwise returns the absolute path in generic form.
    // Falls back to the absolute generic path when no project is active.
    std::string MakeAssetRelative(const std::filesystem::path& absolute);

} // namespace Weaver::FileDialog
