#include "loom/project/project_serializer.h"
#include "loom/core/log.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Loom {

    ProjectSerializer::ProjectSerializer(std::shared_ptr<Project> project)
        : mProject(project) {}

    bool ProjectSerializer::Serialize(const std::string& filepath) {
        const auto& config = mProject->GetConfig();

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Project" << YAML::Value;
        out << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << config.Name;
        out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory.string();
        out << YAML::Key << "StartScene" << YAML::Value << config.StartScene.string();
        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream fout(filepath);
        if (!fout.is_open()) {
            LOOM_CORE_ERROR("Failed to open file for writing: {0}", filepath);
            return false;
        }

        fout << out.c_str();
        return true;
    }

    bool ProjectSerializer::Deserialize(const std::string& filepath) {
        YAML::Node data;
        try {
            data = YAML::LoadFile(filepath);
        } catch (YAML::ParserException& e) {
            LOOM_CORE_ERROR("Failed to load .loomproj file '{0}': {1}", filepath, e.what());
            return false;
        }

        auto project_node = data["Project"];
        if (!project_node) {
            LOOM_CORE_ERROR("Invalid .loomproj format: Missing 'Project' node.");
            return false;
        }

        auto& config = mProject->GetConfig();
        config.Name = project_node["Name"].as<std::string>();
        config.AssetDirectory = project_node["AssetDirectory"].as<std::string>();
        config.StartScene = project_node["StartScene"].as<std::string>();

        return true;
    }

} // namespace Loom