#pragma once

#include "loom/core/uuid.h"
#include "loom/renderer/font_asset.h"
#include "loom/renderer/mesh_asset.h"
#include "loom/renderer/texture.h"
#include "loom/scene/scene_camera.h"
#include "loom/scene/scriptable_entity.h"
#include "loom/scripting/script_field.h"
#include <box2d/id.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Loom {

    struct IDComponent {
        UUID ID;

        IDComponent()                   = default;
        IDComponent(const IDComponent&) = default;
    };

    struct TagComponent {
        std::string Tag;
        TagComponent()                    = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& tag)
            : Tag(tag) {}
    };

    struct TransformComponent {
        glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation    = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale       = { 1.0f, 1.0f, 1.0f };

        TransformComponent()                          = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& translation)
            : Translation(translation) {}

        glm::mat4 GetTransform() const {
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), Rotation.z, { 0, 0, 1 }) * glm::rotate(glm::mat4(1.0f), Rotation.y, { 0, 1, 0 }) * glm::rotate(glm::mat4(1.0f), Rotation.x, { 1, 0, 0 });

            return glm::translate(glm::mat4(1.0f), Translation) * rotation * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    struct SpriteRendererComponent {
        glm::vec4                  Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        std::shared_ptr<Texture2D> Texture      = nullptr;
        float                      TilingFactor = 1.0f;
        TextureSpecification       TexSpec;

        SpriteRendererComponent()                               = default;
        SpriteRendererComponent(const SpriteRendererComponent&) = default;
        SpriteRendererComponent(const glm::vec4& color)
            : Color(color) {}
    };

    struct CameraComponent {
        SceneCamera Camera;
        bool        Primary          = true;
        bool        FixedAspectRatio = false;

        CameraComponent()                       = default;
        CameraComponent(const CameraComponent&) = default;
    };

    struct LOOM_API NativeScriptComponent {
        ScriptableEntity* Instance = nullptr;
        std::string ScriptName;

        std::function<ScriptableEntity*()>          InstantiateScript;
        std::function<void(NativeScriptComponent*)> DestroyScript;

        template<typename T>
        void Bind() {
            ScriptName        = typeid(T).name(); // fallback
            InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
            DestroyScript     = [](NativeScriptComponent* nsc) {
                delete nsc->Instance;
                nsc->Instance = nullptr;
            };
        }

        void BindByName(const std::string& name);
        bool IsValid() const { return InstantiateScript != nullptr; }
    };

    struct Rigidbody2DComponent {
        enum class BodyType { Static = 0, Dynamic = 1, Kinematic = 2 };
        BodyType Type = BodyType::Static;
        bool FixedRotation = false;

        // Storage for Box2D runtime body
        b2BodyId RuntimeBody = b2_nullBodyId;

        Rigidbody2DComponent() = default;
        Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
    };

    struct LuaScriptComponent {
        // Absolute path at runtime; relative to asset directory when serialized (like textures).
        std::string ScriptPath;
        // Editor-set overrides for Properties declared in the script.
        std::unordered_map<std::string, ScriptField> Fields;

        LuaScriptComponent()                          = default;
        LuaScriptComponent(const LuaScriptComponent&) = default;
        LuaScriptComponent(const std::string& path)
            : ScriptPath(path) {}
    };

    struct BoxCollider2DComponent {
        glm::vec2 Offset = { 0.0f, 0.0f };
        glm::vec2 Size   = { 0.5f, 0.5f }; // Box2D uses half-extents

        float Density              = 1.0f;
        float Friction             = 0.5f;
        float Restitution          = 0.0f;
        float RestitutionThreshold = 0.5f;
        bool  IsSensor             = false;

        b2ShapeId RuntimeFixture = b2_nullShapeId;

        BoxCollider2DComponent() = default;
        BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
    };

    struct CircleCollider2DComponent {
        glm::vec2 Offset = { 0.0f, 0.0f };
        float     Radius = 0.5f;

        float Density              = 1.0f;
        float Friction             = 0.5f;
        float Restitution          = 0.0f;
        float RestitutionThreshold = 0.5f;
        bool  IsSensor             = false;

        b2ShapeId RuntimeFixture = b2_nullShapeId;

        CircleCollider2DComponent() = default;
        CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
    };

    struct RelationshipComponent {
        entt::entity              Parent   = entt::null;
        std::vector<entt::entity> Children;

        RelationshipComponent()                             = default;
        RelationshipComponent(const RelationshipComponent&) = default;
    };

    struct AnimationComponent {
        // Each frame: (u_min, v_min, u_max, v_max) in normalized [0,1] UV space
        std::vector<glm::vec4> Frames;
        float FrameDuration = 0.1f;
        int   CurrentFrame  = 0;
        float ElapsedTime   = 0.0f;
        bool  IsPlaying     = true;
        bool  Loop          = true;

        AnimationComponent()                          = default;
        AnimationComponent(const AnimationComponent&) = default;
    };

    struct TextComponent {
        // Path relative to asset directory (serialized); Font is the live runtime handle.
        std::string                 FontPath;
        std::shared_ptr<FontAsset>  Font;        // runtime cache — not serialized
        std::string                 Text        = "Text";
        glm::vec4                   Color       = { 1.0f, 1.0f, 1.0f, 1.0f };
        float                       FontSize    = 1.0f;
        float                       Kerning     = 0.0f;
        float                       LineSpacing = 0.0f;

        TextComponent()                     = default;
        TextComponent(const TextComponent&) = default;
    };

    struct TilemapComponent {
        // Spritesheet path relative to asset directory
        std::string               SpritesheetPath;
        std::shared_ptr<Texture2D> Spritesheet;   // runtime handle — not serialized

        int   Columns      = 10;   // map grid width in tiles
        int   Rows         = 10;   // map grid height in tiles
        float TileWidth    = 1.0f; // world-space tile width
        float TileHeight   = 1.0f; // world-space tile height
        int   SheetColumns = 4;    // spritesheet tile columns
        int   SheetRows    = 4;    // spritesheet tile rows

        // Flat row-major array; -1 = empty, >= 0 = 0-based sheet tile index
        std::vector<int> Tiles; // size = Columns * Rows

        TilemapComponent()                        = default;
        TilemapComponent(const TilemapComponent&) = default;
    };

    struct ParticleComponent {
        enum class EmitterShape    { Point = 0, Box = 1, Circle = 2 };
        enum class SimulationSpace { World = 0, Local = 1 };

        // Emitter
        EmitterShape    Shape       = EmitterShape::Point;
        glm::vec2       ShapeSize   = { 1.0f, 1.0f }; // Box: half-extents; Circle: radius (x); ignored for Point
        SimulationSpace Space       = SimulationSpace::World;
        bool            Emitting    = true;
        float           SpawnRate   = 20.0f;          // particles per second

        // Particle initial state
        float     LifetimeMin   = 0.5f;
        float     LifetimeMax   = 1.5f;
        glm::vec2 VelocityMin   = { -1.0f, -1.0f };
        glm::vec2 VelocityMax   = {  1.0f,  1.0f };
        glm::vec2 Gravity       = {  0.0f, -9.8f };
        float     GravityScale  = 0.0f;               // 0 disables gravity by default
        float     RotationSpeed = 0.0f;               // radians/sec, applied to particle local rotation

        // Animated over normalized lifetime [0..1]
        glm::vec4 ColorBegin = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 ColorEnd   = { 1.0f, 1.0f, 1.0f, 0.0f };
        float     SizeBegin  = 0.2f;
        float     SizeEnd    = 0.0f;

        int MaxParticles = 256;

        // Optional texture (1x1 white when empty). Path relative to asset directory.
        std::string                TexturePath;
        std::shared_ptr<Texture2D> Texture; // runtime cache — not serialized

        // Runtime particle pool — not serialized.
        // World-space mode: Position is stored in world coords.
        // Local-space mode: Position is stored in emitter-local coords; transformed at draw time.
        struct ParticleInstance {
            glm::vec2 Position;
            glm::vec2 Velocity;
            float     Rotation;
            float     Age;
            float     Lifetime;
        };
        std::vector<ParticleInstance> Live;
        float SpawnAccumulator = 0.0f;

        ParticleComponent() = default;
        ParticleComponent(const ParticleComponent& other)
            : Shape(other.Shape), ShapeSize(other.ShapeSize), Space(other.Space)
            , Emitting(other.Emitting), SpawnRate(other.SpawnRate)
            , LifetimeMin(other.LifetimeMin), LifetimeMax(other.LifetimeMax)
            , VelocityMin(other.VelocityMin), VelocityMax(other.VelocityMax)
            , Gravity(other.Gravity), GravityScale(other.GravityScale)
            , RotationSpeed(other.RotationSpeed)
            , ColorBegin(other.ColorBegin), ColorEnd(other.ColorEnd)
            , SizeBegin(other.SizeBegin), SizeEnd(other.SizeEnd)
            , MaxParticles(other.MaxParticles)
            , TexturePath(other.TexturePath)
            , Texture(nullptr), Live{}, SpawnAccumulator(0.0f) {}
    };

    struct MeshRendererComponent {
        // Mesh
        std::string                MeshPath;          // relative to asset directory
        std::shared_ptr<MeshAsset> Mesh;              // runtime handle — not serialized

        // Material — albedo
        glm::vec4                  AlbedoColor       = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::string                AlbedoTexturePath; // relative to asset directory
        std::shared_ptr<Texture2D> AlbedoTexture;     // runtime handle — not serialized

        // Material — surface
        float Roughness = 0.5f;
        float Metallic  = 0.0f;

        MeshRendererComponent()                             = default;
        MeshRendererComponent(const MeshRendererComponent&) = default;
    };

    struct Rigidbody3DComponent {
        enum class BodyType { Static = 0, Dynamic = 1, Kinematic = 2 };
        BodyType Type           = BodyType::Static;
        bool     FixedRotation  = false;
        float    LinearDamping  = 0.05f;
        float    AngularDamping = 0.05f;

        // Storage for the runtime Jolt body. UINT32_MAX (== JPH::BodyID::cInvalidBodyID)
        // means no body has been created yet. Stored as a raw integer so this header
        // does not need to include any Jolt types.
        uint32_t RuntimeBodyID = 0xffffffffu;

        Rigidbody3DComponent()                                  = default;
        Rigidbody3DComponent(const Rigidbody3DComponent& o)
            : Type(o.Type), FixedRotation(o.FixedRotation)
            , LinearDamping(o.LinearDamping), AngularDamping(o.AngularDamping)
            , RuntimeBodyID(0xffffffffu) {} // never alias runtime handles on copy
    };

    struct BoxCollider3DComponent {
        glm::vec3 Offset      = { 0.0f, 0.0f, 0.0f };
        glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f }; // Jolt uses half-extents

        // Density is in "game units" (mirrors Box2D's mental model), not kg/m^3.
        // 1.0 keeps masses small enough that scripted impulses feel responsive.
        float Density     = 1.0f;
        float Friction    = 0.5f;
        float Restitution = 0.0f;
        bool  IsSensor    = false;

        BoxCollider3DComponent()                              = default;
        BoxCollider3DComponent(const BoxCollider3DComponent&) = default;
    };

    struct SphereCollider3DComponent {
        glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
        // Default 1.0 matches common sphere assets (Blender UV Sphere etc.).
        float     Radius = 1.0f;

        float Density     = 1.0f;
        float Friction    = 0.5f;
        float Restitution = 0.0f;
        bool  IsSensor    = false;

        SphereCollider3DComponent()                                 = default;
        SphereCollider3DComponent(const SphereCollider3DComponent&) = default;
    };

    struct CapsuleCollider3DComponent {
        // Capsule is oriented along the local Y axis: a cylinder of length
        // 2*HalfHeight capped by a hemisphere of Radius on each end. Total
        // height is 2*(HalfHeight + Radius). Common shape for character bodies.
        glm::vec3 Offset     = { 0.0f, 0.0f, 0.0f };
        float     Radius     = 0.5f;
        float     HalfHeight = 0.5f;

        float Density     = 1.0f;
        float Friction    = 0.5f;
        float Restitution = 0.0f;
        bool  IsSensor    = false;

        CapsuleCollider3DComponent()                                  = default;
        CapsuleCollider3DComponent(const CapsuleCollider3DComponent&) = default;
    };

    struct DirectionalLightComponent {
        // Direction is taken from the entity's TransformComponent rotation:
        // the light shines along the entity's local -Z axis after rotation.
        glm::vec3 Color     = { 1.0f, 0.97f, 0.92f };
        float     Intensity = 1.0f;

        DirectionalLightComponent()                                 = default;
        DirectionalLightComponent(const DirectionalLightComponent&) = default;
    };

    struct PointLightComponent {
        // Position is taken from the entity's world TransformComponent translation.
        glm::vec3 Color     = { 1.0f, 1.0f, 1.0f };
        float     Intensity = 1.0f;
        float     Range     = 10.0f; // distance at which contribution falls to ~0

        PointLightComponent()                           = default;
        PointLightComponent(const PointLightComponent&) = default;
    };

    struct AudioSourceComponent {
        std::string AssetPath;
        float Volume   = 1.0f;
        float Pitch    = 1.0f;
        float Pan      = 0.0f;
        bool  Loop     = false;
        bool  AutoPlay = false;

        void* RuntimeSound = nullptr; // ma_sound* — owned and managed by AudioEngine

        AudioSourceComponent()                                = default;
        AudioSourceComponent(const AudioSourceComponent& o)
            : AssetPath(o.AssetPath), Volume(o.Volume), Pitch(o.Pitch), Pan(o.Pan)
            , Loop(o.Loop), AutoPlay(o.AutoPlay)
            , RuntimeSound(nullptr) {} // never alias runtime handles on copy
    };

} // namespace Loom
