#pragma once

#include <string>
#include <filesystem>
#include <memory>

namespace Loom {

    struct ProjectConfig {
        std::string Name = "Untitled";
        std::filesystem::path StartScene;
        std::filesystem::path AssetDirectory;
    };

    class Project {
    public:
        static const std::filesystem::path& GetAssetDirectory() {
            static std::filesystem::path empty_path = "";
            if (sActiveProject)
                return sActiveProject->mConfig.AssetDirectory;
            return empty_path;
        }

        static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path) {
            if (sActiveProject)
                return GetAssetDirectory() / path;
            return path;
        }

        static std::shared_ptr<Project> GetActive() { return sActiveProject; }

        static void SetActive(std::shared_ptr<Project> project) { sActiveProject = project; }

        ProjectConfig& GetConfig() { return mConfig; }

    private:
        ProjectConfig mConfig;

        inline static std::shared_ptr<Project> sActiveProject;
    };

} // namespace Loom