#include "loom/scene/scene_serializer.h"
#include "loom/asset/asset_manager.h"
#include "loom/core/log.h"
#include "loom/core/uuid.h"
#include "loom/scene/components.h"
#include "loom/scene/entity.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace YAML {

    template<>
    struct convert<glm::vec2> {
        static Node encode(const glm::vec2& v) {
            Node node;
            node.push_back(v.x);
            node.push_back(v.y);
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::vec2& v) {
            if (!node.IsSequence() || node.size() != 2)
                return false;
            v.x = node[0].as<float>();
            v.y = node[1].as<float>();
            return true;
        }
    };

    template<>
    struct convert<glm::vec3> {
        static Node encode(const glm::vec3& v) {
            Node node;
            node.push_back(v.x);
            node.push_back(v.y);
            node.push_back(v.z);
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::vec3& v) {
            if (!node.IsSequence() || node.size() != 3)
                return false;
            v.x = node[0].as<float>();
            v.y = node[1].as<float>();
            v.z = node[2].as<float>();
            return true;
        }
    };

    template<>
    struct convert<glm::vec4> {
        static Node encode(const glm::vec4& v) {
            Node node;
            node.push_back(v.x);
            node.push_back(v.y);
            node.push_back(v.z);
            node.push_back(v.w);
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::vec4& v) {
            if (!node.IsSequence() || node.size() != 4)
                return false;
            v.x = node[0].as<float>();
            v.y = node[1].as<float>();
            v.z = node[2].as<float>();
            v.w = node[3].as<float>();
            return true;
        }
    };

} // namespace YAML

namespace Loom {

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v) {
        out << YAML::Flow << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v) {
        out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
        return out;
    }

    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v) {
        out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
        return out;
    }

    void SerializeEntity(YAML::Emitter& out, Entity entity) {
        out << YAML::BeginMap;

        // ID Component
        out << YAML::Key << "Entity" << YAML::Value << (uint64_t)entity.GetComponent<IDComponent>().ID;

        // Tag Component
        if (entity.HasComponent<TagComponent>()) {
            out << YAML::Key << "TagComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Tag" << YAML::Value << entity.GetComponent<TagComponent>().Tag;
            out << YAML::EndMap;
        }

        // Transform Component
        if (entity.HasComponent<TransformComponent>()) {
            out << YAML::Key << "TransformComponent";
            out << YAML::BeginMap;
            auto& tc = entity.GetComponent<TransformComponent>();
            out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
            out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
            out << YAML::Key << "Scale" << YAML::Value << tc.Scale;
            out << YAML::EndMap;
        }

        // Camera Component
        if (entity.HasComponent<CameraComponent>()) {
            out << YAML::Key << "CameraComponent";
            out << YAML::BeginMap;
            auto& cc  = entity.GetComponent<CameraComponent>();
            auto& cam = cc.Camera;
            out << YAML::Key << "Primary" << YAML::Value << cc.Primary;
            out << YAML::Key << "FixedAspectRatio" << YAML::Value << cc.FixedAspectRatio;

            // Orthographic
            out << YAML::Key << "OrthographicSize" << YAML::Value << cam.GetOrthographicSize();
            out << YAML::Key << "OrthographicNear" << YAML::Value << cam.GetOrthographicNearClip();
            out << YAML::Key << "OrthographicFar" << YAML::Value << cam.GetOrthographicFarClip();

            // Perspective
            out << YAML::Key << "PerspectiveFOV" << YAML::Value << cam.GetPerspectiveVerticalFOV();
            out << YAML::Key << "PerspectiveNear" << YAML::Value << cam.GetPerspectiveNearClip();
            out << YAML::Key << "PerspectiveFar" << YAML::Value << cam.GetPerspectiveFarClip();

            // Projection type
            out << YAML::Key << "ProjectionType" << YAML::Value << (int)cam.GetProjectionType();

            out << YAML::EndMap;
        }

        // Sprite Renderer Component
        if (entity.HasComponent<SpriteRendererComponent>()) {
            out << YAML::Key << "SpriteRendererComponent";
            out << YAML::BeginMap;
            auto& src = entity.GetComponent<SpriteRendererComponent>();
            out << YAML::Key << "Color" << YAML::Value << src.Color;
            out << YAML::Key << "Texture" << YAML::Value << (src.Texture ? src.Texture->GetPath() : "");
            out << YAML::Key << "TilingFactor" << YAML::Value << src.TilingFactor;
            out << YAML::EndMap;
        }

        // Native Script Component
        if (entity.HasComponent<NativeScriptComponent>()) {
            out << YAML::Key << "NativeScriptComponent";
            out << YAML::BeginMap;
            auto& nsc = entity.GetComponent<NativeScriptComponent>();
            out << YAML::Key << "ScriptName" << YAML::Value << nsc.ScriptName;
            out << YAML::EndMap;
        }

        out << YAML::EndMap;
    }

    SceneSerializer::SceneSerializer(const std::shared_ptr<Scene>& scene)
        : mScene(scene) {}

    void SceneSerializer::Serialize(const std::string& filepath) {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << "Untitled";
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        auto view = mScene->GetAllEntitiesWith<IDComponent>();
        for (auto entity_id : view) {
            Entity entity{ entity_id, mScene.get() };
            if (!entity)
                continue;
            SerializeEntity(out, entity);
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(filepath);
        if (!fout.is_open()) {
            LOOM_CORE_ERROR("SceneSerializer: could not open '{}' for writing", filepath);
            return;
        }
        fout << out.c_str();
        LOOM_CORE_INFO("SceneSerializer: saved scene to '{}'", filepath);
    }

    #define YAML_GET(node_value, type, fallback) ([&]() -> type { \
        auto&& _yaml_node_ = (node_value);                        \
        return _yaml_node_ ? _yaml_node_.as<type>() : (fallback); \
    }())

    bool SceneSerializer::Deserialize(const std::string& filepath) {
        YAML::Node data;
        try {
            data = YAML::LoadFile(filepath);
        } catch (const YAML::Exception& e) {
            LOOM_CORE_ERROR("SceneSerializer: failed to load '{}': '{}'", filepath, e.what());
            return false;
        }

        if (!data["Scene"]) {
            LOOM_CORE_ERROR("SceneSerializer: '{}' is not a valid scene file", filepath);
            return false;
        }

        auto entities_node = data["Entities"];
        if (!entities_node)
            return true;

        for (auto entity_node : entities_node) {
            // ID Component
            uint64_t uuid = entity_node["Entity"].as<uint64_t>();

            // Tag Component
            std::string name = "Entity";
            if (auto tag_node = entity_node["TagComponent"])
                name = YAML_GET(tag_node["Tag"], std::string, "Untitled Entity");

            Entity entity = mScene->CreateEntityWithUUID(UUID(uuid), name);

            // Transform Component
            if (auto tc_node = entity_node["TransformComponent"]) {
                auto& tc       = entity.GetComponent<TransformComponent>();
                tc.Translation = YAML_GET(tc_node["Translation"], glm::vec3, glm::vec3(0.0f));
                tc.Rotation    = YAML_GET(tc_node["Rotation"], glm::vec3, glm::vec3(0.0f));
                tc.Scale       = YAML_GET(tc_node["Scale"], glm::vec3, glm::vec3(1.0f));
            }

            // Camera Component
            if (auto cc_node = entity_node["CameraComponent"]) {
                auto& cc            = entity.AddComponent<CameraComponent>();
                cc.Primary          = YAML_GET(cc_node["Primary"], bool, false);
                cc.FixedAspectRatio = YAML_GET(cc_node["FixedAspectRatio"], bool, true);

                auto ortho_size = YAML_GET(cc_node["OrthographicSize"], float, 10.0f);
                auto ortho_near = YAML_GET(cc_node["OrthographicNear"], float, 0.1f);
                auto ortho_far  = YAML_GET(cc_node["OrthographicFar"], float, 1000.0f);
                cc.Camera.SetOrthographic(ortho_size, ortho_near, ortho_far);

                auto perspective_fov  = YAML_GET(cc_node["PerspectiveFOV"], float, 60.0f);
                auto perspective_near = YAML_GET(cc_node["PerspectiveNear"], float, 0.1f);
                auto perspective_far  = YAML_GET(cc_node["PerspectiveFar"], float, 100.0f);
                cc.Camera.SetPerspective(perspective_fov, perspective_near, perspective_far);

                auto projection_type = YAML_GET(cc_node["ProjectionType"], int, 0);
                cc.Camera.SetProjectionType((SceneCamera::ProjectionType)projection_type);
            }

            // Sprite Renderer Component
            if (auto src_node = entity_node["SpriteRendererComponent"]) {
                auto& src          = entity.AddComponent<SpriteRendererComponent>();
                auto  texture_path = YAML_GET(src_node["Texture"], std::string, "");
                src.Color          = YAML_GET(src_node["Color"], glm::vec4, glm::vec4(1.0f));
                src.Texture        = texture_path.empty() ? nullptr : AssetManager::GetTexture(texture_path);
                src.TilingFactor   = YAML_GET(src_node["TilingFactor"], float, 1.0f);
            }

            // Native Script Component
            if (auto nsc_node = entity_node["NativeScriptComponent"]) {
                auto& nsc         = entity.AddComponent<NativeScriptComponent>();
                auto  script_name = YAML_GET(nsc_node["ScriptName"], std::string, "");
                nsc.BindByName(script_name);
            }
        }

        LOOM_CORE_INFO("SceneSerializer: loaded scene from '{}'", filepath);
        return true;
    }

} // namespace Loom
