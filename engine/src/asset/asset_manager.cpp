#include "loom/asset/asset_manager.h"
#include "loom/core/log.h"

namespace Loom {

    std::mutex AssetManager::sMutex;
    std::unordered_map<std::string, std::weak_ptr<Texture2D>> AssetManager::sTextureCache;
    std::unordered_map<std::string, std::weak_ptr<Shader>>    AssetManager::sShaderCache;

    std::shared_ptr<Texture2D> AssetManager::GetTexture(const std::string& path) {
        std::lock_guard<std::mutex> lock(sMutex);

        auto it = sTextureCache.find(path);
        if (it != sTextureCache.end()) {
            if (auto asset = it->second.lock())
                return asset;
        }

        LOOM_CORE_TRACE("AssetManager: loading texture '{}'", path);
        auto asset = Texture2D::Create(path);
        sTextureCache[path] = asset;
        return asset;
    }

    std::shared_ptr<Shader> AssetManager::GetShader(const std::string& path) {
        std::lock_guard<std::mutex> lock(sMutex);

        auto it = sShaderCache.find(path);
        if (it != sShaderCache.end()) {
            if (auto asset = it->second.lock())
                return asset;
        }

        LOOM_CORE_TRACE("AssetManager: loading shader '{}'", path);
        auto asset = Shader::Create(path);
        sShaderCache[path] = asset;
        return asset;
    }

    void AssetManager::Trim() {
        std::lock_guard<std::mutex> lock(sMutex);

        for (auto it = sTextureCache.begin(); it != sTextureCache.end(); ) {
            it = it->second.expired() ? sTextureCache.erase(it) : ++it;
        }

        for (auto it = sShaderCache.begin(); it != sShaderCache.end(); ) {
            it = it->second.expired() ? sShaderCache.erase(it) : ++it;
        }
    }

    void AssetManager::Clear() {
        std::lock_guard<std::mutex> lock(sMutex);
        sTextureCache.clear();
        sShaderCache.clear();
    }

} // namespace Loom