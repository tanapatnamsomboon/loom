#pragma once

#include "loom/core/timestep.h"
#include "loom/scene/entity.h"

namespace Loom {

    class LOOM_API ScriptableEntity {
    public:
        virtual ~ScriptableEntity() {}

        template<typename T>
        T& GetComponent() {
            return mEntity.GetComponent<T>();
        }

    protected:
        virtual void OnCreate() {}
        virtual void OnDestroy() {}
        virtual void OnUpdate(Timestep ts) {}

    private:
        Entity mEntity;

        friend class Scene;
    };

} // namespace Loom
