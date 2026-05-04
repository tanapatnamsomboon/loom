#include "loom/asset/asset_manager.h"
#include "loom/core/log.h"
#include "scripting/file_watcher.h"
#include <filesystem>

namespace Loom {

    std::mutex AssetManager::sMutex;
    std::unordered_map<std::string, std::weak_ptr<Texture2D>> AssetManager::sTextureCache;
    std::unordered_map<std::string, std::weak_ptr<Shader>>    AssetManager::sShaderCache;

    // Lazily constructed; lives for the lifetime of the process.
    static FileWatcher& GetWatcher() {
        static FileWatcher watcher;
        return watcher;
    }

    std::shared_ptr<Texture2D> AssetManager::GetTexture(const std::string& path,
                                                          const TextureSpecification& spec) {
        std::lock_guard<std::mutex> lock(sMutex);

        std::string key = path + ":"
            + std::to_string((int)spec.Filter) + ":"
            + std::to_string((int)spec.Wrap)   + ":"
            + std::to_string(spec.GenerateMips ? 1 : 0);

        auto it = sTextureCache.find(key);
        if (it != sTextureCache.end()) {
            if (auto asset = it->second.lock())
                return asset;
        }

        LOOM_CORE_TRACE("AssetManager: loading texture '{}'", path);
        auto asset = Texture2D::Create(path, spec);
        sTextureCache[key] = asset;
        GetWatcher().Watch(path);
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
        GetWatcher().Watch(path + ".vert");
        GetWatcher().Watch(path + ".frag");
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

    void AssetManager::ReloadChanged() {
        auto changed = GetWatcher().FlushChanges();
        if (changed.empty()) return;

        // Collect live asset pointers under the lock, reload GL resources outside it.
        std::vector<std::shared_ptr<Texture2D>> textures_to_reload;
        std::vector<std::shared_ptr<Shader>>    shaders_to_reload;
        {
            std::lock_guard<std::mutex> lock(sMutex);
            for (const auto& path : changed) {
                std::string ext = std::filesystem::path(path).extension().string();
                if (ext == ".vert" || ext == ".frag") {
                    // Map watcher path back to base cache key by stripping extension.
                    std::string key = path.substr(0, path.size() - ext.size());
                    auto it = sShaderCache.find(key);
                    if (it != sShaderCache.end()) {
                        if (auto shader = it->second.lock())
                            shaders_to_reload.push_back(std::move(shader));
                    }
                } else {
                    // Texture: match all cache entries whose key starts with "path:".
                    std::string prefix = path + ":";
                    for (auto& [key, weak] : sTextureCache) {
                        if (key.rfind(prefix, 0) == 0) {
                            if (auto texture = weak.lock())
                                textures_to_reload.push_back(std::move(texture));
                        }
                    }
                }
            }
        }

        for (auto& texture : textures_to_reload) {
            LOOM_CORE_INFO("AssetManager: hot-reloading texture '{}'", texture->GetPath());
            texture->Reload();
        }
        for (auto& shader : shaders_to_reload) {
            LOOM_CORE_INFO("AssetManager: hot-reloading shader");
            shader->Reload();
        }
    }

} // namespace Loom