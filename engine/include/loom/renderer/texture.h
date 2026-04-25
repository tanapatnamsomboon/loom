#pragma once

#include "loom/core/core.h"
#include <cstdint>
#include <memory>
#include <string>

namespace Loom {

    class LOOM_API Texture {
    public:
        virtual ~Texture() = default;

        virtual uint32_t GetWidth() const      = 0;
        virtual uint32_t GetHeight() const     = 0;
        virtual uint32_t GetRendererID() const = 0;

        virtual const std::string& GetPath() const = 0;

        virtual void SetData(void* data, uint32_t size) = 0;

        virtual void Bind(uint32_t slot = 0) const = 0;
    };

    class LOOM_API Texture2D : public Texture {
    public:
        static std::shared_ptr<Texture2D> Create(const std::string& path);
        static std::shared_ptr<Texture2D> Create(uint32_t width, uint32_t height);
    };

} // namespace Loom
