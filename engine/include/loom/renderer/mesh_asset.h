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
        glm::vec4 Tangent; // xyz = tangent direction (world), w = bitangent handedness sign (+1/-1)
    };

    // glTF material data extracted at import time. Surfaced for the inspector's
    // "Import Material from glTF" button; the mesh itself never applies these.
    // URIs are relative to the glTF file's directory; embedded textures yield "".
    struct MeshMaterial {
        bool        HasMaterial     = false;
        glm::vec4   BaseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float       MetallicFactor  = 1.0f;
        float       RoughnessFactor = 1.0f;
        std::string BaseColorTexture;
        // R=AO, G=roughness, B=metallic. Sourced from metallicRoughnessTexture;
        // a separate occlusionTexture logs a warning and is dropped.
        std::string ORMTexture;
        std::string EmissiveTexture;
        glm::vec3   EmissiveFactor  = { 0.0f, 0.0f, 0.0f }; // folds KHR_materials_emissive_strength
        std::string NormalTexture;
    };

    class LOOM_API MeshAsset {
    public:
        // Loads a .gltf or .glb. All primitives concatenate into one VAO.
        // Returns nullptr on parse/validation failure.
        static std::shared_ptr<MeshAsset> Create(const std::string& path);

        const std::shared_ptr<VertexArray>& GetVertexArray() const { return mVertexArray; }
        uint32_t GetVertexCount() const { return mVertexCount; }
        uint32_t GetIndexCount()  const { return mIndexCount; }
        const std::string& GetPath() const { return mPath; }
        const MeshMaterial& GetMaterial() const { return mMaterial; }

        // Re-parses mPath and atomically swaps in new geometry+material. On
        // failure existing data is preserved. Driven by the FileWatcher.
        void Reload();

    private:
        std::shared_ptr<VertexArray> mVertexArray;
        std::string mPath;
        uint32_t mVertexCount = 0;
        uint32_t mIndexCount  = 0;
        MeshMaterial mMaterial;
    };

} // namespace Loom
