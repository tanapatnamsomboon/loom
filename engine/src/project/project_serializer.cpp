#include "loom/project/project_serializer.h"
#include "loom/core/log.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Loom {

    static constexpr int k_ProjectVersion = 1;

    ProjectSerializer::ProjectSerializer(std::shared_ptr<Project> project)
        : mProject(project) {}

    bool ProjectSerializer::Serialize(const std::string& filepath) {
        const auto& config = mProject->GetConfig();

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Project" << YAML::Value;
        out << YAML::BeginMap;
        out << YAML::Key << "Version"        << YAML::Value << k_ProjectVersion;
        out << YAML::Key << "Name"           << YAML::Value << config.Name;
        out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory.string();
        out << YAML::Key << "StartScene"     << YAML::Value << config.StartScene.string();
        out << YAML::Key << "WindowTitle"    << YAML::Value << config.WindowTitle;
        out << YAML::Key << "WindowWidth"    << YAML::Value << config.WindowWidth;
        out << YAML::Key << "WindowHeight"   << YAML::Value << config.WindowHeight;
        out << YAML::EndMap;
        out << YAML::EndMap;

        std::filesystem::path path = std::filesystem::path((const char8_t*)filepath.c_str());
        std::ofstream fout(path);
        if (!fout.is_open()) {
            LOOM_CORE_ERROR("ProjectSerializer: failed to open '{}' for writing.", filepath);
            return false;
        }

        fout << out.c_str();
        return true;
    }

    bool ProjectSerializer::Deserialize(const std::string& filepath) {
        std::filesystem::path path = std::filesystem::path((const char8_t*)filepath.c_str());

        std::ifstream stream(path);
        if (!stream.is_open()) {
            LOOM_CORE_ERROR("ProjectSerializer: failed to open '{}'.", filepath);
            return false;
        }

        YAML::Node data;
        try {
            data = YAML::Load(stream);
        } catch (YAML::ParserException& e) {
            LOOM_CORE_ERROR("ProjectSerializer: failed to parse '{}': {}", filepath, e.what());
            return false;
        }

        auto project_node = data["Project"];
        if (!project_node) {
            LOOM_CORE_ERROR("ProjectSerializer: '{}' is missing the 'Project' root node.", filepath);
            return false;
        }

        // Version check
        if (!project_node["Version"]) {
            LOOM_CORE_WARN("ProjectSerializer: '{}' has no Version field - treating as version 1.", filepath);
        } else {
            int version = project_node["Version"].as<int>();
            if (version > k_ProjectVersion)
                LOOM_CORE_WARN("ProjectSerializer: '{}' was saved with a newer engine (version {}), "
                               "current schema is version {}. Some fields may be ignored.",
                               filepath, version, k_ProjectVersion);
        }

        auto& config = mProject->GetConfig();

        // Required: Name
        if (!project_node["Name"]) {
            LOOM_CORE_ERROR("ProjectSerializer: '{}' is missing the 'Name' field.", filepath);
            return false;
        }
        config.Name = project_node["Name"].as<std::string>();

        // Required: AssetDirectory — must exist on disk
        if (!project_node["AssetDirectory"] || project_node["AssetDirectory"].as<std::string>().empty()) {
            LOOM_CORE_ERROR("ProjectSerializer: '{}' has no AssetDirectory.", filepath);
            return false;
        }
        config.AssetDirectory = project_node["AssetDirectory"].as<std::string>();

        auto asset_abs = path.parent_path() / config.AssetDirectory;
        if (!std::filesystem::exists(asset_abs)) {
            LOOM_CORE_ERROR("ProjectSerializer: AssetDirectory '{}' does not exist on disk.",
                            asset_abs.string());
            return false;
        }

        // Optional: StartScene — warn but don't fail (new projects start without one)
        config.StartScene = project_node["StartScene"] ? project_node["StartScene"].as<std::string>() : "";
        if (config.StartScene.empty())
            LOOM_CORE_WARN("ProjectSerializer: '{}' has no StartScene set.", filepath);

        // Optional: window config — fall back to safe defaults
        config.WindowTitle  = project_node["WindowTitle"]  ? project_node["WindowTitle"].as<std::string>() : "";
        config.WindowWidth  = project_node["WindowWidth"]  ? project_node["WindowWidth"].as<int>()  : 1280;
        config.WindowHeight = project_node["WindowHeight"] ? project_node["WindowHeight"].as<int>() : 720;

        mProject->SetProjectDirectory(path.parent_path());
        return true;
    }

} // namespace Loom