#pragma once

#include "loom/core/core.h"
#include "loom/core/timestep.h"
#include "loom/core/uuid.h"
#include "loom/renderer/editor_camera.h"
#include "loom/renderer/texture.h"
#include <entt/entt.hpp>
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
        void OnUpdateRuntime(Timestep ts);

        Entity CreateEntity(const std::string& name = std::string());
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
        void   DestroyEntity(Entity entity);

        Entity GetEntityByUUID(UUID uuid);

        template<typename... Components>
        auto GetAllEntitiesWith() {
            return mRegistry.view<Components...>();
        }

    private:
        void DrawCameraFrustum(const TransformComponent& transform, const CameraComponent& camera);

    private:
        entt::registry mRegistry;

        std::unordered_map<UUID, entt::entity> mEntityMap;
        std::shared_ptr<Texture2D> mCameraIcon;

        friend class Entity;
        friend class SceneHierarchyPanel;
        friend class SceneSerializer;
    };

} // namespace Loom
