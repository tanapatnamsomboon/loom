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

    void Open(const std::string& key, const std::string& title, const char* filters, Callback on_pick);
    void Save(const std::string& key, const std::string& title, const char* filters,
              const std::string& default_filename, Callback on_pick);
    void PickFolder(const std::string& key, const std::string& title, Callback on_pick);

    // Call once per frame after panels render, while inside the ImGui frame.
    void Render();

    // Returns the path relative to the active project's asset directory if `absolute` is inside it
    // (forward-slash form), otherwise returns the absolute path in generic form.
    // Falls back to the absolute generic path when no project is active.
    std::string MakeAssetRelative(const std::filesystem::path& absolute);

} // namespace Weaver::FileDialog
