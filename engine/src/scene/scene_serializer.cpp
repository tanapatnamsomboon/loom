#include "loom/scene/scene_serializer.h"
#include "loom/asset/asset_manager.h"
#include "loom/core/log.h"
#include "loom/core/uuid.h"
#include "loom/project/project.h"
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

    // Converts an absolute asset path to a path relative to the project asset directory.
    // Falls back to a generic absolute path if the file is outside the asset directory.
    static std::string ToRelativeAssetPath(const std::string& abs_path) {
        if (abs_path.empty()) return {};
        std::filesystem::path asset_dir = Project::GetAssetDirectory();
        if (asset_dir.empty()) return std::filesystem::path(abs_path).generic_string();
        std::error_code ec;
        auto rel = std::filesystem::relative(std::filesystem::path(abs_path), asset_dir, ec);
        if (!ec && !rel.empty() && rel.string().find("..") == std::string::npos)
            return rel.generic_string();
        return std::filesystem::path(abs_path).generic_string();
    }

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

            std::string texture_path = src.Texture ? ToRelativeAssetPath(src.Texture->GetPath()) : "";

            out << YAML::Key << "Texture"       << YAML::Value << texture_path;
            out << YAML::Key << "TilingFactor"  << YAML::Value << src.TilingFactor;
            out << YAML::Key << "FilterMode"    << YAML::Value << (int)src.TexSpec.Filter;
            out << YAML::Key << "WrapMode"      << YAML::Value << (int)src.TexSpec.Wrap;
            out << YAML::Key << "GenerateMips"  << YAML::Value << src.TexSpec.GenerateMips;
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

        // Lua Script Component
        if (entity.HasComponent<LuaScriptComponent>()) {
            out << YAML::Key << "LuaScriptComponent";
            out << YAML::BeginMap;
            auto& lsc = entity.GetComponent<LuaScriptComponent>();
            out << YAML::Key << "ScriptPath" << YAML::Value << ToRelativeAssetPath(lsc.ScriptPath);
            if (!lsc.Fields.empty()) {
                out << YAML::Key << "Fields" << YAML::BeginMap;
                for (const auto& [name, field] : lsc.Fields) {
                    out << YAML::Key << name << YAML::BeginMap;
                    out << YAML::Key << "Type" << YAML::Value << (int)field.Type;
                    out << YAML::Key << "Value" << YAML::Value;
                    std::visit([&](auto&& v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, bool>)        out << v;
                        else if constexpr (std::is_same_v<T, int>)    out << v;
                        else if constexpr (std::is_same_v<T, float>)  out << v;
                        else if constexpr (std::is_same_v<T, std::string>) out << v;
                        else if constexpr (std::is_same_v<T, glm::vec2>)   out << v;
                        else if constexpr (std::is_same_v<T, glm::vec3>)   out << v;
                    }, field.Value);
                    out << YAML::EndMap;
                }
                out << YAML::EndMap;
            }
            out << YAML::EndMap;
        }

        // Rigidbody 2D Component
        if (entity.HasComponent<Rigidbody2DComponent>()) {
            out << YAML::Key << "Rigidbody2DComponent";
            out << YAML::BeginMap;
            auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
            out << YAML::Key << "BodyType" << YAML::Value << (int)rb2d.Type;
            out << YAML::Key << "FixedRotation" << YAML::Value << rb2d.FixedRotation;
            out << YAML::EndMap;
        }

        // Box Collider 2D Component
        if (entity.HasComponent<BoxCollider2DComponent>()) {
            out << YAML::Key << "BoxCollider2DComponent";
            out << YAML::BeginMap;
            auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
            out << YAML::Key << "Offset" << YAML::Value << bc2d.Offset;
            out << YAML::Key << "Size" << YAML::Value << bc2d.Size;
            out << YAML::Key << "Density" << YAML::Value << bc2d.Density;
            out << YAML::Key << "Friction" << YAML::Value << bc2d.Friction;
            out << YAML::Key << "Restitution" << YAML::Value << bc2d.Restitution;
            out << YAML::Key << "RestitutionThreshold" << YAML::Value << bc2d.RestitutionThreshold;
            out << YAML::Key << "IsSensor" << YAML::Value << bc2d.IsSensor;
            out << YAML::EndMap;
        }

        // Circle Collider 2D Component
        if (entity.HasComponent<CircleCollider2DComponent>()) {
            out << YAML::Key << "CircleCollider2DComponent";
            out << YAML::BeginMap;
            auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
            out << YAML::Key << "Offset" << YAML::Value << cc2d.Offset;
            out << YAML::Key << "Radius" << YAML::Value << cc2d.Radius;
            out << YAML::Key << "Density" << YAML::Value << cc2d.Density;
            out << YAML::Key << "Friction" << YAML::Value << cc2d.Friction;
            out << YAML::Key << "Restitution" << YAML::Value << cc2d.Restitution;
            out << YAML::Key << "RestitutionThreshold" << YAML::Value << cc2d.RestitutionThreshold;
            out << YAML::Key << "IsSensor" << YAML::Value << cc2d.IsSensor;
            out << YAML::EndMap;
        }

        // Animation Component
        if (entity.HasComponent<AnimationComponent>()) {
            out << YAML::Key << "AnimationComponent";
            out << YAML::BeginMap;
            auto& anim = entity.GetComponent<AnimationComponent>();
            out << YAML::Key << "FrameDuration" << YAML::Value << anim.FrameDuration;
            out << YAML::Key << "Loop"          << YAML::Value << anim.Loop;
            out << YAML::Key << "IsPlaying"     << YAML::Value << anim.IsPlaying;
            out << YAML::Key << "Frames"        << YAML::Value << YAML::BeginSeq;
            for (const auto& frame : anim.Frames)
                out << frame;
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        // Audio Source Component
        if (entity.HasComponent<AudioSourceComponent>()) {
            out << YAML::Key << "AudioSourceComponent";
            out << YAML::BeginMap;
            auto& asc = entity.GetComponent<AudioSourceComponent>();
            out << YAML::Key << "AssetPath" << YAML::Value << ToRelativeAssetPath(asc.AssetPath);
            out << YAML::Key << "Volume"    << YAML::Value << asc.Volume;
            out << YAML::Key << "Pitch"     << YAML::Value << asc.Pitch;
            out << YAML::Key << "Pan"       << YAML::Value << asc.Pan;
            out << YAML::Key << "Loop"      << YAML::Value << asc.Loop;
            out << YAML::Key << "AutoPlay"  << YAML::Value << asc.AutoPlay;
            out << YAML::EndMap;
        }

        // Relationship Component — only serialize the parent UUID; children are implied
        if (entity.HasComponent<RelationshipComponent>()) {
            Entity parent = entity.GetParent();
            if (parent) {
                out << YAML::Key << "ParentID" << YAML::Value << (uint64_t)parent.GetComponent<IDComponent>().ID;
            }
        }

        out << YAML::EndMap;
    }

    SceneSerializer::SceneSerializer(const std::shared_ptr<Scene>& scene)
        : mScene(scene) {}

    void SceneSerializer::Serialize(const std::string& filepath) {
        YAML::Emitter out;
        out << YAML::BeginMap;
        std::string scene_name = std::filesystem::path(filepath).stem().string();
        out << YAML::Key << "Scene" << YAML::Value << scene_name;
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        // Sort by UUID so the on-disk order is stable across save/load round-trips.
        auto view = mScene->GetAllEntitiesWith<IDComponent>();
        std::vector<entt::entity> sorted(view.begin(), view.end());
        std::sort(sorted.begin(), sorted.end(), [&](entt::entity a, entt::entity b) {
            return (uint64_t)mScene->mRegistry.get<IDComponent>(a).ID
                 < (uint64_t)mScene->mRegistry.get<IDComponent>(b).ID;
        });
        for (auto entity_id : sorted) {
            Entity entity{ entity_id, mScene.get() };
            if (!entity)
                continue;
            SerializeEntity(out, entity);
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::filesystem::path path = std::filesystem::path((const char8_t*)filepath.c_str());
        std::ofstream fout(path);
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
        std::filesystem::path path = std::filesystem::path((const char8_t*)filepath.c_str());
        YAML::Node data;
        try {
            data = YAML::LoadFile(path.generic_string());
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
                src.TilingFactor   = YAML_GET(src_node["TilingFactor"], float, 1.0f);

                src.TexSpec.Filter       = (FilterMode)YAML_GET(src_node["FilterMode"],   int,  0);
                src.TexSpec.Wrap         = (WrapMode)YAML_GET(src_node["WrapMode"],       int,  0);
                src.TexSpec.GenerateMips = YAML_GET(src_node["GenerateMips"],             bool, true);

                if (!texture_path.empty()) {
                    std::filesystem::path physical_path = Project::GetAssetFileSystemPath(texture_path);
                    src.Texture = AssetManager::GetTexture(physical_path.string(), src.TexSpec);
                } else {
                    src.Texture = nullptr;
                }
            }

            // Native Script Component
            if (auto nsc_node = entity_node["NativeScriptComponent"]) {
                auto& nsc         = entity.AddComponent<NativeScriptComponent>();
                auto  script_name = YAML_GET(nsc_node["ScriptName"], std::string, "");
                if (!script_name.empty()) {
                    nsc.BindByName(script_name);
                }
            }

            // Lua Script Component
            if (auto lsc_node = entity_node["LuaScriptComponent"]) {
                auto& lsc      = entity.AddComponent<LuaScriptComponent>();
                lsc.ScriptPath = YAML_GET(lsc_node["ScriptPath"], std::string, "");
                if (auto fields_node = lsc_node["Fields"]) {
                    for (auto it = fields_node.begin(); it != fields_node.end(); ++it) {
                        std::string name     = it->first.as<std::string>();
                        auto        fn       = it->second;
                        ScriptField field;
                        field.Name = name;
                        field.Type = (ScriptFieldType)YAML_GET(fn["Type"], int, 0);
                        if (auto vn = fn["Value"]) {
                            switch (field.Type) {
                                case ScriptFieldType::Float:  field.Value = vn.as<float>();       break;
                                case ScriptFieldType::Int:    field.Value = vn.as<int>();         break;
                                case ScriptFieldType::Bool:   field.Value = vn.as<bool>();        break;
                                case ScriptFieldType::Vec2:   field.Value = vn.as<glm::vec2>();   break;
                                case ScriptFieldType::Vec3:   field.Value = vn.as<glm::vec3>();   break;
                                case ScriptFieldType::String: field.Value = vn.as<std::string>(); break;
                            }
                        }
                        lsc.Fields[name] = std::move(field);
                    }
                }
            }

            // Rigidbody 2D Component
            if (auto rb2d_node = entity_node["Rigidbody2DComponent"]) {
                auto& rb2d = entity.AddComponent<Rigidbody2DComponent>();
                rb2d.Type = (Rigidbody2DComponent::BodyType)YAML_GET(rb2d_node["BodyType"], int, 0);
                rb2d.FixedRotation = YAML_GET(rb2d_node["FixedRotation"], bool, false);
            }

            // Box Collider 2D Component
            if (auto bc2d_node = entity_node["BoxCollider2DComponent"]) {
                auto& bc2d = entity.AddComponent<BoxCollider2DComponent>();
                bc2d.Offset               = YAML_GET(bc2d_node["Offset"],               glm::vec2, glm::vec2(0.0f));
                bc2d.Size                 = YAML_GET(bc2d_node["Size"],                 glm::vec2, glm::vec2(0.5f));
                bc2d.Density              = YAML_GET(bc2d_node["Density"],              float, 1.0f);
                bc2d.Friction             = YAML_GET(bc2d_node["Friction"],             float, 0.5f);
                bc2d.Restitution          = YAML_GET(bc2d_node["Restitution"],          float, 0.0f);
                bc2d.RestitutionThreshold = YAML_GET(bc2d_node["RestitutionThreshold"], float, 0.5f);
                bc2d.IsSensor             = YAML_GET(bc2d_node["IsSensor"],             bool,  false);
            }

            // Animation Component
            if (auto anim_node = entity_node["AnimationComponent"]) {
                auto& anim        = entity.AddComponent<AnimationComponent>();
                anim.FrameDuration = YAML_GET(anim_node["FrameDuration"], float, 0.1f);
                anim.Loop          = YAML_GET(anim_node["Loop"],          bool,  true);
                anim.IsPlaying     = YAML_GET(anim_node["IsPlaying"],     bool,  true);
                if (auto frames_node = anim_node["Frames"]) {
                    for (auto frame_node : frames_node)
                        anim.Frames.push_back(frame_node.as<glm::vec4>());
                }
            }

            // Circle Collider 2D Component
            if (auto cc2d_node = entity_node["CircleCollider2DComponent"]) {
                auto& cc2d = entity.AddComponent<CircleCollider2DComponent>();
                cc2d.Offset               = YAML_GET(cc2d_node["Offset"],               glm::vec2, glm::vec2(0.0f));
                cc2d.Radius               = YAML_GET(cc2d_node["Radius"],               float, 0.5f);
                cc2d.Density              = YAML_GET(cc2d_node["Density"],              float, 1.0f);
                cc2d.Friction             = YAML_GET(cc2d_node["Friction"],             float, 0.5f);
                cc2d.Restitution          = YAML_GET(cc2d_node["Restitution"],          float, 0.0f);
                cc2d.RestitutionThreshold = YAML_GET(cc2d_node["RestitutionThreshold"], float, 0.5f);
                cc2d.IsSensor             = YAML_GET(cc2d_node["IsSensor"],             bool,  false);
            }

            // Audio Source Component
            if (auto asc_node = entity_node["AudioSourceComponent"]) {
                auto& asc    = entity.AddComponent<AudioSourceComponent>();
                asc.AssetPath = YAML_GET(asc_node["AssetPath"], std::string, "");
                asc.Volume    = YAML_GET(asc_node["Volume"],    float,       1.0f);
                asc.Pitch     = YAML_GET(asc_node["Pitch"],     float,       1.0f);
                asc.Pan       = YAML_GET(asc_node["Pan"],       float,       0.0f);
                asc.Loop      = YAML_GET(asc_node["Loop"],      bool,        false);
                asc.AutoPlay  = YAML_GET(asc_node["AutoPlay"],  bool,        false);
            }
        }

        // Second pass: wire up parent-child relationships
        for (auto entity_node : entities_node) {
            auto parent_id_node = entity_node["ParentID"];
            if (!parent_id_node)
                continue;

            uint64_t child_uuid  = entity_node["Entity"].as<uint64_t>();
            uint64_t parent_uuid = parent_id_node.as<uint64_t>();

            Entity child_entity  = mScene->GetEntityByUUID(UUID(child_uuid));
            Entity parent_entity = mScene->GetEntityByUUID(UUID(parent_uuid));

            if (child_entity && parent_entity)
                mScene->SetParent(child_entity, parent_entity);
        }

        LOOM_CORE_INFO("SceneSerializer: loaded scene from '{}'", filepath);
        return true;
    }

    void SceneSerializer::SerializePrefab(const std::string& filepath, Entity entity) {
        YAML::Emitter out;
        SerializeEntity(out, entity);

        std::filesystem::path path = std::filesystem::path((const char8_t*)filepath.c_str());
        std::ofstream fout(path);
        if (!fout.is_open()) {
            LOOM_CORE_ERROR("SceneSerializer: could not open '{}' for writing", filepath);
            return;
        }
        fout << out.c_str();
        LOOM_CORE_INFO("SceneSerializer: saved prefab to '{}'", filepath);
    }

    Entity SceneSerializer::DeserializePrefab(const std::string& filepath) {
        return DeserializePrefabInto(filepath, mScene.get());
    }

    Entity SceneSerializer::DeserializePrefabInto(const std::string& filepath, Scene* scene) {
        std::filesystem::path path = std::filesystem::path((const char8_t*)filepath.c_str());
        YAML::Node data;
        try {
            data = YAML::LoadFile(path.generic_string());
        } catch (const YAML::Exception& e) {
            LOOM_CORE_ERROR("SceneSerializer: failed to load prefab '{}': '{}'", filepath, e.what());
            return {};
        }

        if (!data["Entity"]) {
            LOOM_CORE_ERROR("SceneSerializer: '{}' is not a valid prefab file", filepath);
            return {};
        }

        std::string name = "Entity";
        if (auto tag_node = data["TagComponent"])
            name = YAML_GET(tag_node["Tag"], std::string, "Entity");

        // Always mint a fresh UUID — prefabs are templates, not identity-preserving
        Entity entity = scene->CreateEntityWithUUID(UUID(), name);

        if (auto tc_node = data["TransformComponent"]) {
            auto& tc       = entity.GetComponent<TransformComponent>();
            tc.Translation = YAML_GET(tc_node["Translation"], glm::vec3, glm::vec3(0.0f));
            tc.Rotation    = YAML_GET(tc_node["Rotation"],    glm::vec3, glm::vec3(0.0f));
            tc.Scale       = YAML_GET(tc_node["Scale"],       glm::vec3, glm::vec3(1.0f));
        }

        if (auto cc_node = data["CameraComponent"]) {
            auto& cc            = entity.AddComponent<CameraComponent>();
            cc.Primary          = YAML_GET(cc_node["Primary"], bool, false);
            cc.FixedAspectRatio = YAML_GET(cc_node["FixedAspectRatio"], bool, true);

            cc.Camera.SetOrthographic(
                YAML_GET(cc_node["OrthographicSize"], float, 10.0f),
                YAML_GET(cc_node["OrthographicNear"], float, 0.1f),
                YAML_GET(cc_node["OrthographicFar"],  float, 1000.0f));
            cc.Camera.SetPerspective(
                YAML_GET(cc_node["PerspectiveFOV"],  float, 60.0f),
                YAML_GET(cc_node["PerspectiveNear"], float, 0.1f),
                YAML_GET(cc_node["PerspectiveFar"],  float, 100.0f));
            cc.Camera.SetProjectionType(
                (SceneCamera::ProjectionType)YAML_GET(cc_node["ProjectionType"], int, 0));
        }

        if (auto src_node = data["SpriteRendererComponent"]) {
            auto& src          = entity.AddComponent<SpriteRendererComponent>();
            auto  texture_path = YAML_GET(src_node["Texture"], std::string, "");
            src.Color          = YAML_GET(src_node["Color"], glm::vec4, glm::vec4(1.0f));
            src.TilingFactor   = YAML_GET(src_node["TilingFactor"], float, 1.0f);
            src.TexSpec.Filter       = (FilterMode)YAML_GET(src_node["FilterMode"],   int,  0);
            src.TexSpec.Wrap         = (WrapMode)YAML_GET(src_node["WrapMode"],       int,  0);
            src.TexSpec.GenerateMips = YAML_GET(src_node["GenerateMips"],             bool, true);
            if (!texture_path.empty()) {
                std::filesystem::path physical_path = Project::GetAssetFileSystemPath(texture_path);
                src.Texture = AssetManager::GetTexture(physical_path.string(), src.TexSpec);
            }
        }

        if (auto nsc_node = data["NativeScriptComponent"]) {
            auto& nsc         = entity.AddComponent<NativeScriptComponent>();
            auto  script_name = YAML_GET(nsc_node["ScriptName"], std::string, "");
            if (!script_name.empty())
                nsc.BindByName(script_name);
        }

        if (auto lsc_node = data["LuaScriptComponent"]) {
            auto& lsc      = entity.AddComponent<LuaScriptComponent>();
            lsc.ScriptPath = YAML_GET(lsc_node["ScriptPath"], std::string, "");
            if (auto fields_node = lsc_node["Fields"]) {
                for (auto it = fields_node.begin(); it != fields_node.end(); ++it) {
                    std::string name     = it->first.as<std::string>();
                    auto        fn       = it->second;
                    ScriptField field;
                    field.Name = name;
                    field.Type = (ScriptFieldType)YAML_GET(fn["Type"], int, 0);
                    if (auto vn = fn["Value"]) {
                        switch (field.Type) {
                            case ScriptFieldType::Float:  field.Value = vn.as<float>();       break;
                            case ScriptFieldType::Int:    field.Value = vn.as<int>();         break;
                            case ScriptFieldType::Bool:   field.Value = vn.as<bool>();        break;
                            case ScriptFieldType::Vec2:   field.Value = vn.as<glm::vec2>();   break;
                            case ScriptFieldType::Vec3:   field.Value = vn.as<glm::vec3>();   break;
                            case ScriptFieldType::String: field.Value = vn.as<std::string>(); break;
                        }
                    }
                    lsc.Fields[name] = std::move(field);
                }
            }
        }

        if (auto rb2d_node = data["Rigidbody2DComponent"]) {
            auto& rb2d = entity.AddComponent<Rigidbody2DComponent>();
            rb2d.Type          = (Rigidbody2DComponent::BodyType)YAML_GET(rb2d_node["BodyType"], int, 0);
            rb2d.FixedRotation = YAML_GET(rb2d_node["FixedRotation"], bool, false);
        }

        if (auto bc2d_node = data["BoxCollider2DComponent"]) {
            auto& bc2d = entity.AddComponent<BoxCollider2DComponent>();
            bc2d.Offset               = YAML_GET(bc2d_node["Offset"],               glm::vec2, glm::vec2(0.0f));
            bc2d.Size                 = YAML_GET(bc2d_node["Size"],                 glm::vec2, glm::vec2(0.5f));
            bc2d.Density              = YAML_GET(bc2d_node["Density"],              float, 1.0f);
            bc2d.Friction             = YAML_GET(bc2d_node["Friction"],             float, 0.5f);
            bc2d.Restitution          = YAML_GET(bc2d_node["Restitution"],          float, 0.0f);
            bc2d.RestitutionThreshold = YAML_GET(bc2d_node["RestitutionThreshold"], float, 0.5f);
            bc2d.IsSensor             = YAML_GET(bc2d_node["IsSensor"],             bool,  false);
        }

        if (auto cc2d_node = data["CircleCollider2DComponent"]) {
            auto& cc2d = entity.AddComponent<CircleCollider2DComponent>();
            cc2d.Offset               = YAML_GET(cc2d_node["Offset"],               glm::vec2, glm::vec2(0.0f));
            cc2d.Radius               = YAML_GET(cc2d_node["Radius"],               float, 0.5f);
            cc2d.Density              = YAML_GET(cc2d_node["Density"],              float, 1.0f);
            cc2d.Friction             = YAML_GET(cc2d_node["Friction"],             float, 0.5f);
            cc2d.Restitution          = YAML_GET(cc2d_node["Restitution"],          float, 0.0f);
            cc2d.RestitutionThreshold = YAML_GET(cc2d_node["RestitutionThreshold"], float, 0.5f);
            cc2d.IsSensor             = YAML_GET(cc2d_node["IsSensor"],             bool,  false);
        }

        if (auto anim_node = data["AnimationComponent"]) {
            auto& anim        = entity.AddComponent<AnimationComponent>();
            anim.FrameDuration = YAML_GET(anim_node["FrameDuration"], float, 0.1f);
            anim.Loop          = YAML_GET(anim_node["Loop"],          bool,  true);
            anim.IsPlaying     = YAML_GET(anim_node["IsPlaying"],     bool,  true);
            if (auto frames_node = anim_node["Frames"]) {
                for (auto frame_node : frames_node)
                    anim.Frames.push_back(frame_node.as<glm::vec4>());
            }
        }

        if (auto asc_node = data["AudioSourceComponent"]) {
            auto& asc     = entity.AddComponent<AudioSourceComponent>();
            asc.AssetPath = YAML_GET(asc_node["AssetPath"], std::string, "");
            asc.Volume    = YAML_GET(asc_node["Volume"],    float,       1.0f);
            asc.Pitch     = YAML_GET(asc_node["Pitch"],     float,       1.0f);
            asc.Pan       = YAML_GET(asc_node["Pan"],       float,       0.0f);
            asc.Loop      = YAML_GET(asc_node["Loop"],      bool,        false);
            asc.AutoPlay  = YAML_GET(asc_node["AutoPlay"],  bool,        false);
        }

        LOOM_CORE_INFO("SceneSerializer: instantiated prefab '{}' as '{}'", filepath, name);
        return entity;
    }

} // namespace Loom
