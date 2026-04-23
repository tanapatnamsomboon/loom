#pragma once

#include "loom/core/core.h"
#include "loom/scene/scene.h"
#include <memory>
#include <string>

namespace Loom {

    class LOOM_API SceneSerializer {
    public:
        SceneSerializer(const std::shared_ptr<Scene>& scene);

        void Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        std::shared_ptr<Scene> mScene;
    };

} // namespace Loom