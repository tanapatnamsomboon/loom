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

    // glTF pbrMetallicRoughness material data, extracted at import time. A
    // mesh never applies this itself — it is surfaced so the Game Developer
    // can copy it onto a MeshRendererComponent via the inspector's
    // "Import Material from glTF" button.
    struct MeshMaterial {
        bool      HasMaterial     = false;                  // false = primitive had no material
        glm::vec4 BaseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float     MetallicFactor  = 1.0f;                   // glTF spec default
        float     RoughnessFactor = 1.0f;                   // glTF spec default
        // Base-color texture URI as authored in the glTF, relative to the
        // model file's own directory. Empty when the primitive has no
        // base-color texture, or when the texture is embedded (.glb buffer
        // view / data-URI) — embedded textures can't resolve to an asset path.
        std::string BaseColorTexture;
        // Metallic-roughness texture URI (glTF packs roughness in G, metallic
        // in B). Same relative-path / embedded-skip semantics as BaseColorTexture.
        std::string MetallicRoughnessTexture;
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
        // pbrMetallicRoughness material of the first primitive that carries one.
        const MeshMaterial& GetMaterial() const { return mMaterial; }

    private:
        std::shared_ptr<VertexArray> mVertexArray;
        std::string mPath;
        uint32_t mVertexCount = 0;
        uint32_t mIndexCount  = 0;
        MeshMaterial mMaterial;
    };

} // namespace Loom
