#pragma once

#include "loom/core/uuid.h"
#include "loom/renderer/texture.h"
#include "loom/scene/scene_camera.h"
#include "loom/scene/scriptable_entity.h"
#include <box2d/id.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
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

} // namespace Loom
