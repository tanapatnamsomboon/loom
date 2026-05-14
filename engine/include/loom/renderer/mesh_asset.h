#pragma once

#include "loom/core/core.h"
#include "loom/renderer/vertex_array.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace Loom {

    struct MeshVertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    class LOOM_API MeshAsset {
    public:
        // Loads a glTF (.gltf) or binary glTF (.glb) file from disk.
        // All meshes / primitives in the file are concatenated into a single VAO
        // (positions are required; missing normals default to (0,0,1) and missing
        // UVs to (0,0)). Returns nullptr on parse / validation failure.
        static std::shared_ptr<MeshAsset> Create(const std::string& path);

        const std::shared_ptr<VertexArray>& GetVertexArray() const { return mVertexArray; }
        uint32_t GetVertexCount() const { return mVertexCount; }
        uint32_t GetIndexCount()  const { return mIndexCount; }
        const std::string& GetPath() const { return mPath; }

    private:
        std::shared_ptr<VertexArray> mVertexArray;
        std::string mPath;
        uint32_t mVertexCount = 0;
        uint32_t mIndexCount  = 0;
    };

} // namespace Loom
