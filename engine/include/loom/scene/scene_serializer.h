#pragma once

#include "loom/core/core.h"
#include "loom/scene/entity.h"
#include "loom/scene/scene.h"
#include <memory>
#include <string>

namespace Loom {

    class EditorCamera;

    class LOOM_API SceneSerializer {
    public:
        SceneSerializer(const std::shared_ptr<Scene>& scene);

        // Pass a non-null camera pointer to persist / restore editor viewport state.
        void Serialize(const std::string& filepath, const EditorCamera* camera = nullptr);
        bool Deserialize(const std::string& filepath, EditorCamera* out_camera = nullptr);

        void        SerializePrefab(const std::string& filepath, Entity entity);
        std::string SerializePrefabToString(Entity entity);
        Entity      DeserializePrefab(const std::string& filepath);

        static Entity DeserializePrefabInto(const std::string& filepath, Scene* scene);
        // Deserializes from a YAML string and PRESERVES the stored UUID — use for undo/redo.
        static Entity DeserializePrefabIntoFromString(const std::string& yaml_str, Scene* scene);

    private:
        std::shared_ptr<Scene> mScene;
    };

} // namespace Loom