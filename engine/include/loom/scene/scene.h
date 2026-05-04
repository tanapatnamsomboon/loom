#pragma once

#include "loom/core/core.h"
#include "loom/core/timestep.h"
#include "loom/core/uuid.h"
#include "loom/renderer/editor_camera.h"
#include "loom/renderer/texture.h"
#include <box2d/id.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <unordered_map>

namespace Loom {

    class Entity;

    struct TransformComponent;
    struct CameraComponent;

    class LOOM_API Scene {
    public:
        Scene();
        ~Scene();

        static std::shared_ptr<Scene> Copy(std::shared_ptr<Scene> other);
        void                          OnViewportResize(uint32_t width, uint32_t height);

        void OnUpdateEditor(Timestep ts, EditorCamera& camera, Entity selected_entity);
        void OnRuntimeStart();
        void OnUpdateRuntime(Timestep ts);
        void OnRuntimeStop();

        Entity CreateEntity(const std::string& name = std::string());
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
        void   DestroyEntity(Entity entity);

        Entity    GetEntityByUUID(UUID uuid);
        Entity    GetEntityByTag(const std::string& tag);
        glm::mat4 GetWorldTransform(Entity entity);
        void      SetParent(Entity child, Entity parent);
        void      RemoveParent(Entity child);

        struct RaycastHit2D {
            bool         hit          = false;
            glm::vec2    point        = {};
            glm::vec2    normal       = {};
            entt::entity entityHandle = entt::null;
        };
        RaycastHit2D Raycast2D(glm::vec2 origin, glm::vec2 direction, float distance);

        template<typename... Components>
        auto GetAllEntitiesWith() {
            return mRegistry.view<Components...>();
        }

        void SetShowPhysicsColliders(bool show) { mShowPhysicsColliders = show; }
        bool IsShowingPhysicsColliders() const { return mShowPhysicsColliders; }

    private:
        void DrawCameraFrustum(const glm::mat4& world_transform, const CameraComponent& camera);
        void RenderPhysicsColliders();

    private:
        entt::registry mRegistry;

        std::unordered_map<UUID, entt::entity> mEntityMap;
        std::shared_ptr<Texture2D> mCameraIcon;

        b2WorldId mPhysicsWorld = b2_nullWorldId;

        bool mShowPhysicsColliders = false;

        friend class Entity;
        friend class SceneHierarchyPanel;
        friend class SceneSerializer;
    };

} // namespace Loom
