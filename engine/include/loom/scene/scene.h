#pragma once

#include "loom/core/core.h"
#include "loom/core/timestep.h"
#include "loom/core/uuid.h"
#include <entt/entt.hpp>

namespace Loom {

    class Entity;

    class LOOM_API Scene {
    public:
        Scene();
        ~Scene();

        void OnUpdate(Timestep ts);

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(Entity entity);

        Entity GetEntityByUUID(UUID uuid);

        template<typename... Components>
        auto GetAllEntitiesWith() {
            return mRegistry.view<Components...>();
        }

    private:
        entt::registry mRegistry;

        friend class Entity;
        friend class SceneHierarchyPanel;
    };

} // namespace Loom