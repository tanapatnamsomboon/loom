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
        const std::filesystem::path& GetProjectDirectory() const { return mProjectDirectory; }
        void SetProjectDirectory(const std::filesystem::path& path) { mProjectDirectory = path; }

        static std::filesystem::path GetAssetDirectory() {
            if (sActiveProject)
                return sActiveProject->GetProjectDirectory() / sActiveProject->mConfig.AssetDirectory;
            return "";
        }

        static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path) {
            if (sActiveProject)
                return GetAssetDirectory() / path;
            return path;
        }

        static std::filesystem::path GetEngineAssetDirectory() {
            return "resources";
        }

        static std::filesystem::path GetEngineAssetFileSystemPath(const std::filesystem::path& path) {
            return GetEngineAssetDirectory() / path;
        }

        static std::shared_ptr<Project> GetActive() { return sActiveProject; }

        static void SetActive(std::shared_ptr<Project> project) { sActiveProject = project; }

        ProjectConfig& GetConfig() { return mConfig; }

    private:
        ProjectConfig mConfig;
        std::filesystem::path mProjectDirectory;

        inline static std::shared_ptr<Project> sActiveProject;
    };

} // namespace Loom