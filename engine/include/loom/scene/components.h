#pragma once

#include "loom/core/uuid.h"
#include "loom/renderer/texture.h"
#include "loom/scene/scene_camera.h"
#include "loom/scene/scriptable_entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

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

} // namespace Loom
