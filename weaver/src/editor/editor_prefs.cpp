#include "editor_prefs.h"
#include <loom/core/log.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <cstdlib>
#include <filesystem>

namespace Weaver {

    static std::filesystem::path GetPrefsPath() {
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            auto dir = std::filesystem::path(appdata) / "Weaver";
            std::filesystem::create_directories(dir);
            return dir / "editor_prefs.yaml";
        }
        return "editor_prefs.yaml";
    }

    EditorPrefs EditorPrefsSerializer::Load() {
        EditorPrefs prefs;
        auto path = GetPrefsPath();

        std::ifstream stream(path);
        if (!stream.is_open())
            return prefs;

        try {
            YAML::Node data = YAML::Load(stream);
            if (auto node = data["RecentProjects"]) {
                for (const auto& entry : node) {
                    std::string p = entry.as<std::string>();
                    if (std::filesystem::exists(p))
                        prefs.RecentProjects.push_back(p);
                }
            }
        } catch (YAML::Exception& e) {
            LOOM_CORE_WARN("EditorPrefs: failed to parse '{}': {}", path.string(), e.what());
        }

        return prefs;
    }

    void EditorPrefsSerializer::Save(const EditorPrefs& prefs) {
        auto path = GetPrefsPath();

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
        for (const auto& p : prefs.RecentProjects)
            out << p;
        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(path);
        if (!fout.is_open()) {
            LOOM_CORE_WARN("EditorPrefs: failed to write to '{}'.", path.string());
            return;
        }
        fout << out.c_str();
    }

} // namespace Weaver
