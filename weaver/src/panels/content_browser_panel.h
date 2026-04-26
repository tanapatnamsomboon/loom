#pragma once

#include <loom/renderer/texture.h>
#include <filesystem>

namespace Weaver {

    class ContentBrowserPanel {
    public:
        ContentBrowserPanel();
        void OnImGuiRender();

    private:
        std::filesystem::path mBaseDirectory;
        std::filesystem::path mCurrentDirectory;

        std::shared_ptr<Loom::Texture2D> mDirectoryIcon;
        std::shared_ptr<Loom::Texture2D> mFileIcon;
    };

} // namespace Weaver
