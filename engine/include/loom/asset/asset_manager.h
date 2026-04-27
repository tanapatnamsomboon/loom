#pragma once

#include "loom/core/core.h"
#include "loom/renderer/shader.h"
#include "loom/renderer/texture.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Loom {
    class LOOM_API AssetManager {
    public:
        static std::shared_ptr<Texture2D> GetTexture(const std::string& path);
        static std::shared_ptr<Shader> GetShader(const std::string& path);

        static void Trim();
        static void Clear();

    private:
        static std::mutex sMutex;

        static std::unordered_map<std::string, std::weak_ptr<Texture2D>> sTextureCache;
        static std::unordered_map<std::string, std::weak_ptr<Shader>>    sShaderCache;
    };
} // namespace Loom
