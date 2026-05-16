#include "loom/scene/scene_serializer.h"
#include "loom/asset/asset_manager.h"
#include "loom/core/log.h"
#include "loom/core/uuid.h"
#include "loom/project/project.h"
#include "loom/renderer/editor_camera.h"
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

        // Rigidbody 3D Component
        if (entity.HasComponent<Rigidbody3DComponent>()) {
            out << YAML::Key << "Rigidbody3DComponent";
            out << YAML::BeginMap;
            auto& rb = entity.GetComponent<Rigidbody3DComponent>();
            out << YAML::Key << "Type"           << YAML::Value << (int)rb.Type;
            out << YAML::Key << "FixedRotation"  << YAML::Value << rb.FixedRotation;
            out << YAML::Key << "LinearDamping"  << YAML::Value << rb.LinearDamping;
            out << YAML::Key << "AngularDamping" << YAML::Value << rb.AngularDamping;
            out << YAML::EndMap;
        }

        // Box Collider 3D Component
        if (entity.HasComponent<BoxCollider3DComponent>()) {
            out << YAML::Key << "BoxCollider3DComponent";
            out << YAML::BeginMap;
            auto& bc = entity.GetComponent<BoxCollider3DComponent>();
            out << YAML::Key << "Offset"      << YAML::Value << bc.Offset;
            out << YAML::Key << "HalfExtents" << YAML::Value << bc.HalfExtents;
            out << YAML::Key << "Density"     << YAML::Value << bc.Density;
            out << YAML::Key << "Friction"    << YAML::Value << bc.Friction;
            out << YAML::Key << "Restitution" << YAML::Value << bc.Restitution;
            out << YAML::Key << "IsSensor"    << YAML::Value << bc.IsSensor;
            out << YAML::EndMap;
        }

        // Sphere Collider 3D Component
        if (entity.HasComponent<SphereCollider3DComponent>()) {
            out << YAML::Key << "SphereCollider3DComponent";
            out << YAML::BeginMap;
            auto& sc = entity.GetComponent<SphereCollider3DComponent>();
            out << YAML::Key << "Offset"      << YAML::Value << sc.Offset;
            out << YAML::Key << "Radius"      << YAML::Value << sc.Radius;
            out << YAML::Key << "Density"     << YAML::Value << sc.Density;
            out << YAML::Key << "Friction"    << YAML::Value << sc.Friction;
            out << YAML::Key << "Restitution" << YAML::Value << sc.Restitution;
            out << YAML::Key << "IsSensor"    << YAML::Value << sc.IsSensor;
            out << YAML::EndMap;
        }

        // Capsule Collider 3D Component
        if (entity.HasComponent<CapsuleCollider3DComponent>()) {
            out << YAML::Key << "CapsuleCollider3DComponent";
            out << YAML::BeginMap;
            auto& cc = entity.GetComponent<CapsuleCollider3DComponent>();
            out << YAML::Key << "Offset"      << YAML::Value << cc.Offset;
            out << YAML::Key << "Radius"      << YAML::Value << cc.Radius;
            out << YAML::Key << "HalfHeight"  << YAML::Value << cc.HalfHeight;
            out << YAML::Key << "Density"     << YAML::Value << cc.Density;
            out << YAML::Key << "Friction"    << YAML::Value << cc.Friction;
            out << YAML::Key << "Restitution" << YAML::Value << cc.Restitution;
            out << YAML::Key << "IsSensor"    << YAML::Value << cc.IsSensor;
            out << YAML::EndMap;
        }

        // Directional Light Component
        if (entity.HasComponent<DirectionalLightComponent>()) {
            out << YAML::Key << "DirectionalLightComponent";
            out << YAML::BeginMap;
            auto& dl = entity.GetComponent<DirectionalLightComponent>();
            out << YAML::Key << "Color"     << YAML::Value << dl.Color;
            out << YAML::Key << "Intensity" << YAML::Value << dl.Intensity;
            out << YAML::EndMap;
        }

        // Point Light Component
        if (entity.HasComponent<PointLightComponent>()) {
            out << YAML::Key << "PointLightComponent";
            out << YAML::BeginMap;
            auto& pl = entity.GetComponent<PointLightComponent>();
            out << YAML::Key << "Color"     << YAML::Value << pl.Color;
            out << YAML::Key << "Intensity" << YAML::Value << pl.Intensity;
            out << YAML::Key << "Range"     << YAML::Value << pl.Range;
            out << YAML::EndMap;
        }

        // Mesh Renderer Component
        if (entity.HasComponent<MeshRendererComponent>()) {
            out << YAML::Key << "MeshRendererComponent";
            out << YAML::BeginMap;
            auto& mrc = entity.GetComponent<MeshRendererComponent>();
            std::string mesh_path    = mrc.Mesh          ? ToRelativeAssetPath(mrc.Mesh->GetPath())          : mrc.MeshPath;
            std::string albedo_path  = mrc.AlbedoTexture ? ToRelativeAssetPath(mrc.AlbedoTexture->GetPath()) : mrc.AlbedoTexturePath;
            out << YAML::Key << "MeshPath"          << YAML::Value << mesh_path;
            out << YAML::Key << "AlbedoColor"       << YAML::Value << mrc.AlbedoColor;
            out << YAML::Key << "AlbedoTexturePath" << YAML::Value << albedo_path;
            out << YAML::Key << "Roughness"         << YAML::Value << mrc.Roughness;
            out << YAML::Key << "Metallic"          << YAML::Value << mrc.Metallic;
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
            out << YAML::Key << "CurrentClip"      << YAML::Value << anim.CurrentClip;
            out << YAML::Key << "IsPlaying"        << YAML::Value << anim.IsPlaying;
            out << YAML::Key << "PickerCellWidth"  << YAML::Value << anim.PickerCellWidth;
            out << YAML::Key << "PickerCellHeight" << YAML::Value << anim.PickerCellHeight;
            out << YAML::Key << "Clips"            << YAML::Value << YAML::BeginSeq;
            for (const auto& clip : anim.Clips) {
                out << YAML::BeginMap;
                out << YAML::Key << "Name"          << YAML::Value << clip.Name;
                out << YAML::Key << "FrameDuration" << YAML::Value << clip.FrameDuration;
                out << YAML::Key << "Loop"          << YAML::Value << clip.Loop;
                out << YAML::Key << "Frames"        << YAML::Value << YAML::BeginSeq;
                for (const auto& frame : clip.Frames) out << frame;
                out << YAML::EndSeq;
                if (!clip.Events.empty()) {
                    out << YAML::Key << "Events" << YAML::Value << YAML::BeginSeq;
                    for (const auto& ev : clip.Events) {
                        out << YAML::BeginMap;
                        out << YAML::Key << "Frame" << YAML::Value << ev.Frame;
                        out << YAML::Key << "Name"  << YAML::Value << ev.Name;
                        out << YAML::EndMap;
                    }
                    out << YAML::EndSeq;
                }
                out << YAML::EndMap;
            }
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

        // Text Component
        if (entity.HasComponent<TextComponent>()) {
            out << YAML::Key << "TextComponent";
            out << YAML::BeginMap;
            auto& tc = entity.GetComponent<TextComponent>();
            out << YAML::Key << "FontPath"    << YAML::Value << ToRelativeAssetPath(tc.FontPath);
            out << YAML::Key << "Text"        << YAML::Value << tc.Text;
            out << YAML::Key << "Color"       << YAML::Value << tc.Color;
            out << YAML::Key << "FontSize"    << YAML::Value << tc.FontSize;
            out << YAML::Key << "Kerning"     << YAML::Value << tc.Kerning;
            out << YAML::Key << "LineSpacing" << YAML::Value << tc.LineSpacing;
            out << YAML::EndMap;
        }

        // Particle Component
        if (entity.HasComponent<ParticleComponent>()) {
            out << YAML::Key << "ParticleComponent";
            out << YAML::BeginMap;
            auto& pc = entity.GetComponent<ParticleComponent>();
            out << YAML::Key << "Shape"         << YAML::Value << (int)pc.Shape;
            out << YAML::Key << "ShapeSize"     << YAML::Value << pc.ShapeSize;
            out << YAML::Key << "Space"         << YAML::Value << (int)pc.Space;
            out << YAML::Key << "Emitting"      << YAML::Value << pc.Emitting;
            out << YAML::Key << "SpawnRate"     << YAML::Value << pc.SpawnRate;
            out << YAML::Key << "LifetimeMin"   << YAML::Value << pc.LifetimeMin;
            out << YAML::Key << "LifetimeMax"   << YAML::Value << pc.LifetimeMax;
            out << YAML::Key << "VelocityMin"   << YAML::Value << pc.VelocityMin;
            out << YAML::Key << "VelocityMax"   << YAML::Value << pc.VelocityMax;
            out << YAML::Key << "Gravity"       << YAML::Value << pc.Gravity;
            out << YAML::Key << "GravityScale"  << YAML::Value << pc.GravityScale;
            out << YAML::Key << "RotationSpeed" << YAML::Value << pc.RotationSpeed;
            out << YAML::Key << "ColorBegin"    << YAML::Value << pc.ColorBegin;
            out << YAML::Key << "ColorEnd"      << YAML::Value << pc.ColorEnd;
            out << YAML::Key << "SizeBegin"     << YAML::Value << pc.SizeBegin;
            out << YAML::Key << "SizeEnd"       << YAML::Value << pc.SizeEnd;
            out << YAML::Key << "MaxParticles"  << YAML::Value << pc.MaxParticles;
            out << YAML::Key << "TexturePath"   << YAML::Value << ToRelativeAssetPath(pc.TexturePath);
            out << YAML::EndMap;
        }

        // Tilemap Component
        if (entity.HasComponent<TilemapComponent>()) {
            out << YAML::Key << "TilemapComponent";
            out << YAML::BeginMap;
            auto& tm = entity.GetComponent<TilemapComponent>();
            out << YAML::Key << "SpritesheetPath" << YAML::Value << ToRelativeAssetPath(tm.SpritesheetPath);
            out << YAML::Key << "Columns"         << YAML::Value << tm.Columns;
            out << YAML::Key << "Rows"            << YAML::Value << tm.Rows;
            out << YAML::Key << "TileWidth"       << YAML::Value << tm.TileWidth;
            out << YAML::Key << "TileHeight"      << YAML::Value << tm.TileHeight;
            out << YAML::Key << "SheetColumns"    << YAML::Value << tm.SheetColumns;
            out << YAML::Key << "SheetRows"       << YAML::Value << tm.SheetRows;
            out << YAML::Key << "Tiles" << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (int t : tm.Tiles) out << t;
            out << YAML::EndSeq;
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

    void SceneSerializer::Serialize(const std::string& filepath, const EditorCamera* camera) {
        YAML::Emitter out;
        out << YAML::BeginMap;
        std::string scene_name = std::filesystem::path(filepath).stem().string();
        out << YAML::Key << "Scene" << YAML::Value << scene_name;

        if (camera) {
            out << YAML::Key << "EditorCamera" << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value << camera->GetPosition();
            out << YAML::Key << "Pitch"    << YAML::Value << camera->GetPitch();
            out << YAML::Key << "Yaw"      << YAML::Value << camera->GetYaw();
            out << YAML::EndMap;
        }

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

    bool SceneSerializer::Deserialize(const std::string& filepath, EditorCamera* out_camera) {
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

        if (out_camera) {
            if (auto cam_node = data["EditorCamera"]) {
                glm::vec3 pos   = YAML_GET(cam_node["Position"], glm::vec3, glm::vec3(0.0f, 0.0f, 5.0f));
                float     pitch = YAML_GET(cam_node["Pitch"],    float,     0.0f);
                float     yaw   = YAML_GET(cam_node["Yaw"],      float,     0.0f);
                out_camera->SetState(pos, pitch, yaw);
            }
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

            // Rigidbody 3D Component
            if (auto rb_node = entity_node["Rigidbody3DComponent"]) {
                auto& rb           = entity.AddComponent<Rigidbody3DComponent>();
                rb.Type            = (Rigidbody3DComponent::BodyType)YAML_GET(rb_node["Type"], int, 0);
                rb.FixedRotation   = YAML_GET(rb_node["FixedRotation"],  bool,  false);
                rb.LinearDamping   = YAML_GET(rb_node["LinearDamping"],  float, 0.05f);
                rb.AngularDamping  = YAML_GET(rb_node["AngularDamping"], float, 0.05f);
            }

            // Box Collider 3D Component
            if (auto bc_node = entity_node["BoxCollider3DComponent"]) {
                auto& bc      = entity.AddComponent<BoxCollider3DComponent>();
                bc.Offset      = YAML_GET(bc_node["Offset"],      glm::vec3, glm::vec3(0.0f));
                bc.HalfExtents = YAML_GET(bc_node["HalfExtents"], glm::vec3, glm::vec3(0.5f));
                bc.Density     = YAML_GET(bc_node["Density"],     float,     1.0f);
                bc.Friction    = YAML_GET(bc_node["Friction"],    float,     0.5f);
                bc.Restitution = YAML_GET(bc_node["Restitution"], float,     0.0f);
                bc.IsSensor    = YAML_GET(bc_node["IsSensor"],    bool,      false);
            }

            // Sphere Collider 3D Component
            if (auto sc_node = entity_node["SphereCollider3DComponent"]) {
                auto& sc      = entity.AddComponent<SphereCollider3DComponent>();
                sc.Offset      = YAML_GET(sc_node["Offset"],      glm::vec3, glm::vec3(0.0f));
                sc.Radius      = YAML_GET(sc_node["Radius"],      float,     0.5f);
                sc.Density     = YAML_GET(sc_node["Density"],     float,     1.0f);
                sc.Friction    = YAML_GET(sc_node["Friction"],    float,     0.5f);
                sc.Restitution = YAML_GET(sc_node["Restitution"], float,     0.0f);
                sc.IsSensor    = YAML_GET(sc_node["IsSensor"],    bool,      false);
            }

            // Capsule Collider 3D Component
            if (auto cc_node = entity_node["CapsuleCollider3DComponent"]) {
                auto& cc       = entity.AddComponent<CapsuleCollider3DComponent>();
                cc.Offset      = YAML_GET(cc_node["Offset"],      glm::vec3, glm::vec3(0.0f));
                cc.Radius      = YAML_GET(cc_node["Radius"],      float,     0.5f);
                cc.HalfHeight  = YAML_GET(cc_node["HalfHeight"],  float,     0.5f);
                cc.Density     = YAML_GET(cc_node["Density"],     float,     1.0f);
                cc.Friction    = YAML_GET(cc_node["Friction"],    float,     0.5f);
                cc.Restitution = YAML_GET(cc_node["Restitution"], float,     0.0f);
                cc.IsSensor    = YAML_GET(cc_node["IsSensor"],    bool,      false);
            }

            // Directional Light Component
            if (auto dl_node = entity_node["DirectionalLightComponent"]) {
                auto& dl     = entity.AddComponent<DirectionalLightComponent>();
                dl.Color     = YAML_GET(dl_node["Color"],     glm::vec3, glm::vec3(1.0f));
                dl.Intensity = YAML_GET(dl_node["Intensity"], float,     1.0f);
            }

            // Point Light Component
            if (auto pl_node = entity_node["PointLightComponent"]) {
                auto& pl     = entity.AddComponent<PointLightComponent>();
                pl.Color     = YAML_GET(pl_node["Color"],     glm::vec3, glm::vec3(1.0f));
                pl.Intensity = YAML_GET(pl_node["Intensity"], float,     1.0f);
                pl.Range     = YAML_GET(pl_node["Range"],     float,     10.0f);
            }

            // Mesh Renderer Component
            if (auto mrc_node = entity_node["MeshRendererComponent"]) {
                auto& mrc            = entity.AddComponent<MeshRendererComponent>();
                mrc.MeshPath          = YAML_GET(mrc_node["MeshPath"],          std::string, "");
                mrc.AlbedoColor       = YAML_GET(mrc_node["AlbedoColor"],       glm::vec4,   glm::vec4(1.0f));
                mrc.AlbedoTexturePath = YAML_GET(mrc_node["AlbedoTexturePath"], std::string, "");
                mrc.Roughness         = YAML_GET(mrc_node["Roughness"],         float,       0.5f);
                mrc.Metallic          = YAML_GET(mrc_node["Metallic"],          float,       0.0f);

                if (!mrc.MeshPath.empty()) {
                    std::filesystem::path mesh_phys = Project::GetAssetFileSystemPath(mrc.MeshPath);
                    mrc.Mesh = AssetManager::GetMesh(mesh_phys.string());
                }
                if (!mrc.AlbedoTexturePath.empty()) {
                    std::filesystem::path tex_phys = Project::GetAssetFileSystemPath(mrc.AlbedoTexturePath);
                    mrc.AlbedoTexture = AssetManager::GetTexture(tex_phys.string());
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
                auto& anim             = entity.AddComponent<AnimationComponent>();
                anim.CurrentClip       = YAML_GET(anim_node["CurrentClip"],      std::string, std::string());
                anim.IsPlaying         = YAML_GET(anim_node["IsPlaying"],        bool,        true);
                anim.PickerCellWidth   = YAML_GET(anim_node["PickerCellWidth"],  int,         64);
                anim.PickerCellHeight  = YAML_GET(anim_node["PickerCellHeight"], int,         64);

                if (auto clips_node = anim_node["Clips"]) {
                    for (auto clip_node : clips_node) {
                        AnimationClip clip;
                        clip.Name          = YAML_GET(clip_node["Name"],          std::string, std::string());
                        clip.FrameDuration = YAML_GET(clip_node["FrameDuration"], float,       0.1f);
                        clip.Loop          = YAML_GET(clip_node["Loop"],          bool,        true);
                        if (auto frames_node = clip_node["Frames"]) {
                            for (auto frame_node : frames_node)
                                clip.Frames.push_back(frame_node.as<glm::vec4>());
                        }
                        if (auto events_node = clip_node["Events"]) {
                            for (auto ev_node : events_node) {
                                AnimationEvent ev;
                                ev.Frame = YAML_GET(ev_node["Frame"], int,         0);
                                ev.Name  = YAML_GET(ev_node["Name"],  std::string, std::string());
                                clip.Events.push_back(std::move(ev));
                            }
                        }
                        anim.Clips.push_back(std::move(clip));
                    }
                } else if (auto frames_node = anim_node["Frames"]) {
                    // Backward compat: pre-multiclip schema -> single "Default" clip.
                    AnimationClip clip("Default");
                    clip.FrameDuration = YAML_GET(anim_node["FrameDuration"], float, 0.1f);
                    clip.Loop          = YAML_GET(anim_node["Loop"],          bool,  true);
                    for (auto frame_node : frames_node)
                        clip.Frames.push_back(frame_node.as<glm::vec4>());
                    anim.Clips.push_back(std::move(clip));
                    if (anim.CurrentClip.empty()) anim.CurrentClip = "Default";
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

            // Text Component
            if (auto tc_node = entity_node["TextComponent"]) {
                auto& tc      = entity.AddComponent<TextComponent>();
                tc.FontPath    = YAML_GET(tc_node["FontPath"],    std::string, "");
                tc.Text        = YAML_GET(tc_node["Text"],        std::string, "Text");
                tc.Color       = YAML_GET(tc_node["Color"],       glm::vec4,   glm::vec4(1.0f));
                tc.FontSize    = YAML_GET(tc_node["FontSize"],    float,       1.0f);
                tc.Kerning     = YAML_GET(tc_node["Kerning"],     float,       0.0f);
                tc.LineSpacing = YAML_GET(tc_node["LineSpacing"], float,       0.0f);
            }

            // Tilemap Component
            if (auto tm_node = entity_node["TilemapComponent"]) {
                auto& tm        = entity.AddComponent<TilemapComponent>();
                tm.SpritesheetPath = YAML_GET(tm_node["SpritesheetPath"], std::string, "");
                tm.Columns      = YAML_GET(tm_node["Columns"],      int,   10);
                tm.Rows         = YAML_GET(tm_node["Rows"],         int,   10);
                tm.TileWidth    = YAML_GET(tm_node["TileWidth"],    float, 1.0f);
                tm.TileHeight   = YAML_GET(tm_node["TileHeight"],   float, 1.0f);
                tm.SheetColumns = YAML_GET(tm_node["SheetColumns"], int,   4);
                tm.SheetRows    = YAML_GET(tm_node["SheetRows"],    int,   4);
                if (auto tiles_node = tm_node["Tiles"]) {
                    tm.Tiles.reserve(tiles_node.size());
                    for (auto t : tiles_node)
                        tm.Tiles.push_back(t.as<int>());
                }
                tm.Tiles.resize(tm.Columns * tm.Rows, -1);
            }

            // Particle Component
            if (auto pc_node = entity_node["ParticleComponent"]) {
                auto& pc = entity.AddComponent<ParticleComponent>();
                pc.Shape         = (ParticleComponent::EmitterShape)   YAML_GET(pc_node["Shape"], int, 0);
                pc.ShapeSize     = YAML_GET(pc_node["ShapeSize"],     glm::vec2, glm::vec2(1.0f));
                pc.Space         = (ParticleComponent::SimulationSpace)YAML_GET(pc_node["Space"], int, 0);
                pc.Emitting      = YAML_GET(pc_node["Emitting"],      bool,  true);
                pc.SpawnRate     = YAML_GET(pc_node["SpawnRate"],     float, 20.0f);
                pc.LifetimeMin   = YAML_GET(pc_node["LifetimeMin"],   float, 0.5f);
                pc.LifetimeMax   = YAML_GET(pc_node["LifetimeMax"],   float, 1.5f);
                pc.VelocityMin   = YAML_GET(pc_node["VelocityMin"],   glm::vec2, glm::vec2(-1.0f));
                pc.VelocityMax   = YAML_GET(pc_node["VelocityMax"],   glm::vec2, glm::vec2( 1.0f));
                pc.Gravity       = YAML_GET(pc_node["Gravity"],       glm::vec2, glm::vec2(0.0f, -9.8f));
                pc.GravityScale  = YAML_GET(pc_node["GravityScale"],  float, 0.0f);
                pc.RotationSpeed = YAML_GET(pc_node["RotationSpeed"], float, 0.0f);
                pc.ColorBegin    = YAML_GET(pc_node["ColorBegin"],    glm::vec4, glm::vec4(1.0f));
                pc.ColorEnd      = YAML_GET(pc_node["ColorEnd"],      glm::vec4, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
                pc.SizeBegin     = YAML_GET(pc_node["SizeBegin"],     float, 0.2f);
                pc.SizeEnd       = YAML_GET(pc_node["SizeEnd"],       float, 0.0f);
                pc.MaxParticles  = YAML_GET(pc_node["MaxParticles"],  int,   256);
                pc.TexturePath   = YAML_GET(pc_node["TexturePath"],   std::string, "");
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

    // Shared entity-from-node loader. preserve_uuid=true restores the original UUID
    // (for undo/redo); false mints a fresh UUID (for prefab instantiation).
    static Entity DeserializeEntityFromNode(const YAML::Node& data, Scene* scene, bool preserve_uuid) {
        std::string entity_name = "Entity";
        if (auto tag_node = data["TagComponent"])
            entity_name = YAML_GET(tag_node["Tag"], std::string, "Entity");

        UUID uuid = preserve_uuid
            ? UUID(YAML_GET(data["Entity"], uint64_t, (uint64_t)0))
            : UUID();
        Entity entity = scene->CreateEntityWithUUID(uuid, entity_name);

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

        if (auto rb_node = data["Rigidbody3DComponent"]) {
            auto& rb           = entity.AddComponent<Rigidbody3DComponent>();
            rb.Type            = (Rigidbody3DComponent::BodyType)YAML_GET(rb_node["Type"], int, 0);
            rb.FixedRotation   = YAML_GET(rb_node["FixedRotation"],  bool,  false);
            rb.LinearDamping   = YAML_GET(rb_node["LinearDamping"],  float, 0.05f);
            rb.AngularDamping  = YAML_GET(rb_node["AngularDamping"], float, 0.05f);
        }

        if (auto bc_node = data["BoxCollider3DComponent"]) {
            auto& bc      = entity.AddComponent<BoxCollider3DComponent>();
            bc.Offset      = YAML_GET(bc_node["Offset"],      glm::vec3, glm::vec3(0.0f));
            bc.HalfExtents = YAML_GET(bc_node["HalfExtents"], glm::vec3, glm::vec3(0.5f));
            bc.Density     = YAML_GET(bc_node["Density"],     float,     1.0f);
            bc.Friction    = YAML_GET(bc_node["Friction"],    float,     0.5f);
            bc.Restitution = YAML_GET(bc_node["Restitution"], float,     0.0f);
            bc.IsSensor    = YAML_GET(bc_node["IsSensor"],    bool,      false);
        }

        if (auto sc_node = data["SphereCollider3DComponent"]) {
            auto& sc      = entity.AddComponent<SphereCollider3DComponent>();
            sc.Offset      = YAML_GET(sc_node["Offset"],      glm::vec3, glm::vec3(0.0f));
            sc.Radius      = YAML_GET(sc_node["Radius"],      float,     0.5f);
            sc.Density     = YAML_GET(sc_node["Density"],     float,     1.0f);
            sc.Friction    = YAML_GET(sc_node["Friction"],    float,     0.5f);
            sc.Restitution = YAML_GET(sc_node["Restitution"], float,     0.0f);
            sc.IsSensor    = YAML_GET(sc_node["IsSensor"],    bool,      false);
        }

        if (auto cc_node = data["CapsuleCollider3DComponent"]) {
            auto& cc       = entity.AddComponent<CapsuleCollider3DComponent>();
            cc.Offset      = YAML_GET(cc_node["Offset"],      glm::vec3, glm::vec3(0.0f));
            cc.Radius      = YAML_GET(cc_node["Radius"],      float,     0.5f);
            cc.HalfHeight  = YAML_GET(cc_node["HalfHeight"],  float,     0.5f);
            cc.Density     = YAML_GET(cc_node["Density"],     float,     1.0f);
            cc.Friction    = YAML_GET(cc_node["Friction"],    float,     0.5f);
            cc.Restitution = YAML_GET(cc_node["Restitution"], float,     0.0f);
            cc.IsSensor    = YAML_GET(cc_node["IsSensor"],    bool,      false);
        }

        if (auto dl_node = data["DirectionalLightComponent"]) {
            auto& dl     = entity.AddComponent<DirectionalLightComponent>();
            dl.Color     = YAML_GET(dl_node["Color"],     glm::vec3, glm::vec3(1.0f));
            dl.Intensity = YAML_GET(dl_node["Intensity"], float,     1.0f);
        }

        if (auto pl_node = data["PointLightComponent"]) {
            auto& pl     = entity.AddComponent<PointLightComponent>();
            pl.Color     = YAML_GET(pl_node["Color"],     glm::vec3, glm::vec3(1.0f));
            pl.Intensity = YAML_GET(pl_node["Intensity"], float,     1.0f);
            pl.Range     = YAML_GET(pl_node["Range"],     float,     10.0f);
        }

        if (auto mrc_node = data["MeshRendererComponent"]) {
            auto& mrc            = entity.AddComponent<MeshRendererComponent>();
            mrc.MeshPath          = YAML_GET(mrc_node["MeshPath"],          std::string, "");
            mrc.AlbedoColor       = YAML_GET(mrc_node["AlbedoColor"],       glm::vec4,   glm::vec4(1.0f));
            mrc.AlbedoTexturePath = YAML_GET(mrc_node["AlbedoTexturePath"], std::string, "");
            mrc.Roughness         = YAML_GET(mrc_node["Roughness"],         float,       0.5f);
            mrc.Metallic          = YAML_GET(mrc_node["Metallic"],          float,       0.0f);
            if (!mrc.MeshPath.empty()) {
                std::filesystem::path mesh_phys = Project::GetAssetFileSystemPath(mrc.MeshPath);
                mrc.Mesh = AssetManager::GetMesh(mesh_phys.string());
            }
            if (!mrc.AlbedoTexturePath.empty()) {
                std::filesystem::path tex_phys = Project::GetAssetFileSystemPath(mrc.AlbedoTexturePath);
                mrc.AlbedoTexture = AssetManager::GetTexture(tex_phys.string());
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
                    std::string name = it->first.as<std::string>();
                    auto        fn   = it->second;
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
            auto& anim            = entity.AddComponent<AnimationComponent>();
            anim.CurrentClip      = YAML_GET(anim_node["CurrentClip"],      std::string, std::string());
            anim.IsPlaying        = YAML_GET(anim_node["IsPlaying"],        bool,        true);
            anim.PickerCellWidth  = YAML_GET(anim_node["PickerCellWidth"],  int,         64);
            anim.PickerCellHeight = YAML_GET(anim_node["PickerCellHeight"], int,         64);

            if (auto clips_node = anim_node["Clips"]) {
                for (auto clip_node : clips_node) {
                    AnimationClip clip;
                    clip.Name          = YAML_GET(clip_node["Name"],          std::string, std::string());
                    clip.FrameDuration = YAML_GET(clip_node["FrameDuration"], float,       0.1f);
                    clip.Loop          = YAML_GET(clip_node["Loop"],          bool,        true);
                    if (auto frames_node = clip_node["Frames"]) {
                        for (auto frame_node : frames_node)
                            clip.Frames.push_back(frame_node.as<glm::vec4>());
                    }
                    if (auto events_node = clip_node["Events"]) {
                        for (auto ev_node : events_node) {
                            AnimationEvent ev;
                            ev.Frame = YAML_GET(ev_node["Frame"], int,         0);
                            ev.Name  = YAML_GET(ev_node["Name"],  std::string, std::string());
                            clip.Events.push_back(std::move(ev));
                        }
                    }
                    anim.Clips.push_back(std::move(clip));
                }
            } else if (auto frames_node = anim_node["Frames"]) {
                // Backward compat: pre-multiclip schema -> single "Default" clip.
                AnimationClip clip("Default");
                clip.FrameDuration = YAML_GET(anim_node["FrameDuration"], float, 0.1f);
                clip.Loop          = YAML_GET(anim_node["Loop"],          bool,  true);
                for (auto frame_node : frames_node)
                    clip.Frames.push_back(frame_node.as<glm::vec4>());
                anim.Clips.push_back(std::move(clip));
                if (anim.CurrentClip.empty()) anim.CurrentClip = "Default";
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

        if (auto tm_node = data["TilemapComponent"]) {
            auto& tm        = entity.AddComponent<TilemapComponent>();
            tm.SpritesheetPath = YAML_GET(tm_node["SpritesheetPath"], std::string, "");
            tm.Columns      = YAML_GET(tm_node["Columns"],      int,   10);
            tm.Rows         = YAML_GET(tm_node["Rows"],         int,   10);
            tm.TileWidth    = YAML_GET(tm_node["TileWidth"],    float, 1.0f);
            tm.TileHeight   = YAML_GET(tm_node["TileHeight"],   float, 1.0f);
            tm.SheetColumns = YAML_GET(tm_node["SheetColumns"], int,   4);
            tm.SheetRows    = YAML_GET(tm_node["SheetRows"],    int,   4);
            if (auto tiles_node = tm_node["Tiles"]) {
                tm.Tiles.reserve(tiles_node.size());
                for (auto t : tiles_node)
                    tm.Tiles.push_back(t.as<int>());
            }
            tm.Tiles.resize(tm.Columns * tm.Rows, -1);
        }

        if (auto pc_node = data["ParticleComponent"]) {
            auto& pc = entity.AddComponent<ParticleComponent>();
            pc.Shape         = (ParticleComponent::EmitterShape)   YAML_GET(pc_node["Shape"], int, 0);
            pc.ShapeSize     = YAML_GET(pc_node["ShapeSize"],     glm::vec2, glm::vec2(1.0f));
            pc.Space         = (ParticleComponent::SimulationSpace)YAML_GET(pc_node["Space"], int, 0);
            pc.Emitting      = YAML_GET(pc_node["Emitting"],      bool,  true);
            pc.SpawnRate     = YAML_GET(pc_node["SpawnRate"],     float, 20.0f);
            pc.LifetimeMin   = YAML_GET(pc_node["LifetimeMin"],   float, 0.5f);
            pc.LifetimeMax   = YAML_GET(pc_node["LifetimeMax"],   float, 1.5f);
            pc.VelocityMin   = YAML_GET(pc_node["VelocityMin"],   glm::vec2, glm::vec2(-1.0f));
            pc.VelocityMax   = YAML_GET(pc_node["VelocityMax"],   glm::vec2, glm::vec2( 1.0f));
            pc.Gravity       = YAML_GET(pc_node["Gravity"],       glm::vec2, glm::vec2(0.0f, -9.8f));
            pc.GravityScale  = YAML_GET(pc_node["GravityScale"],  float, 0.0f);
            pc.RotationSpeed = YAML_GET(pc_node["RotationSpeed"], float, 0.0f);
            pc.ColorBegin    = YAML_GET(pc_node["ColorBegin"],    glm::vec4, glm::vec4(1.0f));
            pc.ColorEnd      = YAML_GET(pc_node["ColorEnd"],      glm::vec4, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
            pc.SizeBegin     = YAML_GET(pc_node["SizeBegin"],     float, 0.2f);
            pc.SizeEnd       = YAML_GET(pc_node["SizeEnd"],       float, 0.0f);
            pc.MaxParticles  = YAML_GET(pc_node["MaxParticles"],  int,   256);
            pc.TexturePath   = YAML_GET(pc_node["TexturePath"],   std::string, "");
        }

        return entity;
    }

    // ---------------------------------------------------------------------------

    std::string SceneSerializer::SerializePrefabToString(Entity entity) {
        YAML::Emitter out;
        SerializeEntity(out, entity);
        return std::string(out.c_str());
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
        Entity entity = DeserializeEntityFromNode(data, scene, false);
        std::string name = entity ? entity.GetComponent<TagComponent>().Tag : "?";
        LOOM_CORE_INFO("SceneSerializer: instantiated prefab '{}' as '{}'", filepath, name);
        return entity;
    }

    Entity SceneSerializer::DeserializePrefabIntoFromString(const std::string& yaml_str, Scene* scene) {
        YAML::Node data;
        try {
            data = YAML::Load(yaml_str);
        } catch (const YAML::Exception& e) {
            LOOM_CORE_ERROR("SceneSerializer: failed to parse prefab YAML: '{}'", e.what());
            return {};
        }
        if (!data["Entity"]) return {};
        return DeserializeEntityFromNode(data, scene, true);
    }

} // namespace Loom
