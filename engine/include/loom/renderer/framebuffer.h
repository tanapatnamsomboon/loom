#pragma once

#include "loom/core/core.h"
#include <cstdint>
#include <memory>

namespace Loom {

    struct FramebufferSpecification {
        uint32_t Width, Height;
        uint32_t Samples = 1;
        bool SwapChainTarget = false;
    };

    class LOOM_API Framebuffer {
    public:
        virtual ~Framebuffer() = default;

        virtual void Bind() = 0;
        virtual void Unbind() = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual uint32_t GetColorAttachmentRendererID() const = 0;

        virtual const FramebufferSpecification& GetSpecification() const = 0;

        static std::shared_ptr<Framebuffer> Create(const FramebufferSpecification& spec);
    };

} // namespace Loom