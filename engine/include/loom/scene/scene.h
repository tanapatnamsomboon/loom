#pragma once

#include "loom/core/core.h"
#include "loom/core/timestep.h"
#include <entt/entt.hpp>

namespace Loom {

    class Entity;

    class LOOM_API Scene {
    public:
        Scene();
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());

        void OnUpdate(Timestep ts);

    private:
        entt::registry mRegistry;

        friend class Entity;
    };

} // namespace Loom