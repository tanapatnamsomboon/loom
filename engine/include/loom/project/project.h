#pragma once

#include "loom/core/core.h"
#include <string>
#include <filesystem>
#include <memory>

namespace Loom {

    // Per-project render-pipeline quality knobs. Defaults match the engine's
    // historical hard-coded values so a project that omits the Graphics block
    // loads with unchanged behaviour.
    struct GraphicsConfig {
        // 4096 x 4 cascades x DEPTH32F = ~256 MB shadow VRAM. Lower for tight
        // VRAM budgets; higher only when 4096 still shows visible texel blockiness.
        uint32_t ShadowMapSize     = 4096;
        // World-space distance from the camera covered by the cascaded shadow
        // maps. Beyond this, the fallback "fully lit" path runs.
        float    ShadowMaxDistance = 200.0f;
    };

    struct ProjectConfig {
        int Version = 1;
        std::string Name = "Untitled";
        std::filesystem::path StartScene;
        std::filesystem::path AssetDirectory;
        // Runtime window settings used by WeaverRuntime; default to project name / 1280×720.
        std::string WindowTitle;
        int WindowWidth  = 1280;
        int WindowHeight = 720;
        GraphicsConfig Graphics;
    };

    class LOOM_API Project {
    public:
        const std::filesystem::path& GetProjectDirectory() const { return mProjectDirectory; }
        void SetProjectDirectory(const std::filesystem::path& path) { mProjectDirectory = path; }

        // Absolute path to the .loomproj file this project was loaded from
        // (or last saved to). Empty until OpenProject / NewProject / SaveAs
        // sets it. Used by Project Settings → Apply to persist back to disk.
        const std::filesystem::path& GetProjectFilePath() const { return mProjectFilePath; }
        void SetProjectFilePath(const std::filesystem::path& path) { mProjectFilePath = path; }

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
        std::filesystem::path mProjectFilePath;

        inline static std::shared_ptr<Project> sActiveProject;
    };

} // namespace Loom