#pragma once

#include "loom/core/log.h"
#include "loom/scene/scene.h"
#include <entt/entt.hpp>
#include <vector>

namespace Loom {

    class LOOM_API Entity {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene)
            : mEntityHandle(handle)
            , mScene(scene) {}

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args) {
            if (HasComponent<T>())
                LOOM_CORE_FATAL("Entity already has component!");
            return mScene->mRegistry.emplace<T>(mEntityHandle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent() const {
            if (!HasComponent<T>())
                LOOM_CORE_FATAL("Entity does not have component!");
            return mScene->mRegistry.get<T>(mEntityHandle);
        }

        template<typename T>
        bool HasComponent() const {
            return mScene->mRegistry.all_of<T>(mEntityHandle);
        }

        template<typename T>
        void RemoveComponent() {
            if (!HasComponent<T>())
                LOOM_CORE_FATAL("Entity does not have component!");
            mScene->mRegistry.remove<T>(mEntityHandle);
        }

        operator bool() const { return mEntityHandle != entt::null; }

        // True only if this handle points at a live entity in its scene's
        // registry. operator bool is a cheap null-check and does NOT catch an
        // entity destroyed since the handle was taken — use this across scene
        // swaps and after a runtime Destroy().
        bool IsValid() const {
            return mScene && mEntityHandle != entt::null
                && mScene->mRegistry.valid(mEntityHandle);
        }

        operator uint32_t() const { return (uint32_t)mEntityHandle; }
        operator entt::entity() const { return mEntityHandle; }

        bool operator==(const Entity& other) const {
            return mEntityHandle == other.mEntityHandle && mScene == other.mScene;
        }

        bool operator!=(const Entity& other) const {
            return !(*this == other);
        }

        void                SetParent(Entity parent);
        void                RemoveParent();
        Entity              GetParent() const;
        std::vector<Entity> GetChildren() const;

    private:
        entt::entity mEntityHandle{ entt::null };
        Scene*       mScene = nullptr;
    };

} // namespace Loom
