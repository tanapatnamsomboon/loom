#pragma once

#include "loom/core/core.h"
#include "loom/renderer/font_asset.h"
#include "loom/renderer/mesh_asset.h"
#include "loom/renderer/shader.h"
#include "loom/renderer/texture.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Loom {

    // Filtering preset for 3D mesh albedo textures. The default
    // TextureSpecification uses Nearest — correct for pixel-art 2D sprites,
    // but it makes mesh albedo blocky/aliased. Mesh albedo wants trilinear.
    // Every mesh-albedo load site must pass this same spec, since the
    // AssetManager texture cache is keyed on the spec.
    inline constexpr TextureSpecification kMeshAlbedoTextureSpec{
        FilterMode::Linear, WrapMode::Repeat, /*GenerateMips=*/true };

    class LOOM_API AssetManager {
    public:
        static std::shared_ptr<Texture2D> GetTexture(const std::string& path,
                                                      const TextureSpecification& spec = {});
        static std::shared_ptr<Shader>    GetShader(const std::string& path);
        static std::shared_ptr<FontAsset> GetFont(const std::string& path);
        static std::shared_ptr<MeshAsset> GetMesh(const std::string& path);

        static void Trim();
        static void Clear();

        // Poll the file watcher and reload any textures or shaders whose source files changed.
        // Call once per frame from the editor update loop.
        static void ReloadChanged();

    private:
        static std::mutex sMutex;

        static std::unordered_map<std::string, std::weak_ptr<Texture2D>> sTextureCache;
        static std::unordered_map<std::string, std::weak_ptr<Shader>>    sShaderCache;
        static std::unordered_map<std::string, std::weak_ptr<FontAsset>> sFontCache;
        static std::unordered_map<std::string, std::weak_ptr<MeshAsset>> sMeshCache;
    };
} // namespace Loom
