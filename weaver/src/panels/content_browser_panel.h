#pragma once

#include <loom/renderer/texture.h>
#include <filesystem>

namespace Weaver {

    class ContentBrowserPanel {
    public:
        ContentBrowserPanel() = default;

        void Init();
        void OnImGuiRender();

        void SetSceneOpenCallback(const std::function<void(const std::filesystem::path&)>& callback) { mSceneOpenCallback = callback; }

    private:
        std::filesystem::path mBaseDirectory;
        std::filesystem::path mCurrentDirectory;

        std::shared_ptr<Loom::Texture2D> mDirectoryIcon;
        std::shared_ptr<Loom::Texture2D> mFileIcon;

        std::function<void(const std::filesystem::path&)> mSceneOpenCallback;
    };

} // namespace Weaver
