#pragma once

#include "loom/core/log.h"
#include <entt/entt.hpp>

namespace Loom {

    class Entity {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene)
            : mEntityHandle(handle), mScene(scene) {}

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args) {
            if (HasComponent<T>()) LOOM_CORE_FATAL("Entity already has component!");
            return mScene->mRegistry.emplace<T>(mEntityHandle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent() {
            if (!HasComponent<T>()) LOOM_CORE_FATAL("Entity does not have component!");
            return mScene->mRegistry.get<T>(mEntityHandle);
        }

        template<typename T>
        bool HasComponent() {
            return mScene->mRegistry.all_of<T>(mEntityHandle);
        }

        template<typename T>
        void RemoveComponent() {
            if (!HasComponent<T>()) LOOM_CORE_FATAL("Entity does not have component!");
            mScene->mRegistry.remove<T>(mEntityHandle);
        }

        operator bool() const { return mEntityHandle != entt::null; }
        operator uint32_t() const { return (uint32_t)mEntityHandle; }

        bool operator==(const Entity& other) const {
            return mEntityHandle == other.mEntityHandle && mScene == other.mScene;
        }

        bool operator!=(const Entity& other) const {
            return !(*this == other);
        }

    private:
        entt::entity mEntityHandle{ entt::null };
        Scene* mScene = nullptr;
    };

} // namespace Loom