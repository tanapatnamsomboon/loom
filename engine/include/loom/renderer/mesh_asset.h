#pragma once

#include "loom/core/core.h"
#include "loom/renderer/skeleton.h"
#include "loom/renderer/vertex_array.h"
#include <filesystem>
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
    // URIs are relative to the glTF file's directory; embedded textures yield ""
    // with the matching *Embedded flag set — call ExtractEmbeddedTextures to
    // write them to disk and populate the URIs.
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

        // True when the corresponding texture slot has image data embedded in
        // the glTF (buffer-view inside a .glb, or a base64 data-URI in a .gltf).
        // ExtractEmbeddedTextures consumes these.
        bool BaseColorEmbedded = false;
        bool ORMEmbedded       = false;
        bool EmissiveEmbedded  = false;
        bool NormalEmbedded    = false;
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

        // Skinning surface — empty skeleton means a static mesh that goes
        // through the standard mesh.vert shader path. Renderer3D::Submit
        // branches to the skinned shader iff IsSkinned() is true.
        bool IsSkinned() const { return !mSkeleton.Empty(); }
        const Skeleton& GetSkeleton() const { return mSkeleton; }

        // Re-parses mPath and atomically swaps in new geometry+material. On
        // failure existing data is preserved. Driven by the FileWatcher.
        void Reload();

        // One-shot: re-parses mesh_path, writes each embedded image
        // (buffer-view or data-URI) into dest_dir as <prefix>_<slot>.<ext>
        // with the extension derived from the image MIME type, and fills
        // io_material's URI fields with the written paths (relative to
        // dest_dir). Embedded flags are cleared on success. Returns the
        // number of textures written (0 if nothing was embedded or on parse
        // failure).
        static int ExtractEmbeddedTextures(const std::string&            mesh_path,
                                           const std::filesystem::path&  dest_dir,
                                           const std::string&            filename_prefix,
                                           MeshMaterial&                 io_material);

    private:
        std::shared_ptr<VertexArray> mVertexArray;
        std::string mPath;
        uint32_t mVertexCount = 0;
        uint32_t mIndexCount  = 0;
        MeshMaterial mMaterial;
        // Empty for static meshes.
        Skeleton mSkeleton;
    };

} // namespace Loom
