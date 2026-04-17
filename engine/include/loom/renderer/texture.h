#pragma once

#include "loom/core/core.h"
#include <cstdint>
#include <string>

namespace Loom {

    class LOOM_API Texture {
    public:
        virtual ~Texture() = default;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual void Bind(uint32_t slot = 0) const = 0;
    };

    class LOOM_API Texture2D : public Texture {
    public:
        static Texture2D* Create(const std::string& path);
    };

} // namespace Loom