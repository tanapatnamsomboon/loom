#pragma once

#include "loom/core/core.h"
#include <cstdint>
#include <memory>
#include <string>

namespace Loom {

    enum class FilterMode { Nearest, Linear };
    enum class WrapMode   { Repeat,  Clamp  };

    struct TextureSpecification {
        FilterMode Filter       = FilterMode::Nearest;
        WrapMode   Wrap         = WrapMode::Repeat;
        bool       GenerateMips = true;
    };

    class LOOM_API Texture {
    public:
        virtual ~Texture() = default;

        virtual uint32_t GetWidth() const      = 0;
        virtual uint32_t GetHeight() const     = 0;
        virtual uint32_t GetRendererID() const = 0;

        virtual const std::string& GetPath() const = 0;

        virtual void SetData(void* data, uint32_t size) = 0;

        virtual void Bind(uint32_t slot = 0) const = 0;

        virtual void Reload() {}
    };

    class LOOM_API Texture2D : public Texture {
    public:
        static std::shared_ptr<Texture2D> Create(const std::string& path,
                                                  const TextureSpecification& spec = {});
        static std::shared_ptr<Texture2D> Create(uint32_t width, uint32_t height);
        static std::shared_ptr<Texture2D> Create(uint32_t width, uint32_t height,
                                                  const TextureSpecification& spec);
    };

} // namespace Loom
