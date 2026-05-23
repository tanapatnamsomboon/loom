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
        // ORM-packed texture URI (R = ambient occlusion, G = roughness,
        // B = metallic — the industry-standard channel layout, emitted by
        // Substance, Unreal, and Blender's glTF exporter when properly wired).
        // Sourced from the glTF's metallicRoughnessTexture; if the file also
        // sets occlusionTexture to a different URI, we keep the MR one and log
        // a warning (nudges authors toward ORM packing). Same relative-path /
        // embedded-skip semantics as BaseColorTexture.
        std::string ORMTexture;
        // Emissive texture URI + factor. The factor already folds the
        // KHR_materials_emissive_strength multiplier when the extension is
        // present, so consumers can apply it verbatim. Default (0,0,0) means
        // the material is not emissive.
        std::string EmissiveTexture;
        glm::vec3   EmissiveFactor = { 0.0f, 0.0f, 0.0f };
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

        // Re-parses the source file at mPath and atomically swaps in the new
        // geometry + material. Existing shared_ptr holders see the refreshed
        // data on their next access. On failure (file missing / parse error)
        // the existing data is preserved and an error is logged. Driven by the
        // FileWatcher when an artist re-exports the glTF on disk.
        void Reload();

    private:
        std::shared_ptr<VertexArray> mVertexArray;
        std::string mPath;
        uint32_t mVertexCount = 0;
        uint32_t mIndexCount  = 0;
        MeshMaterial mMaterial;
    };

} // namespace Loom
