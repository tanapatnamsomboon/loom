#pragma once

#include "loom/core/core.h"
#include <cstdint>
#include <memory>

namespace Loom {

    enum class FramebufferTextureFormat {
        None = 0,
        RGBA8,
        RED_INTEGER,
        DEPTH24STENCIL8,
        // 32-bit float depth, no stencil. Used by shadow maps — higher precision
        // for the projection, and sampleable in shaders as a shadow texture.
        DEPTH32F
    };

    struct FramebufferTextureSpecification {
        FramebufferTextureSpecification() = default;
        FramebufferTextureSpecification(FramebufferTextureFormat format)
            : TextureFormat(format) {}

        FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
    };

    struct FramebufferAttachmentSpecification {
        FramebufferAttachmentSpecification() = default;
        FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
            : Attachments(attachments) {}

        std::vector<FramebufferTextureSpecification> Attachments;
    };

    struct FramebufferSpecification {
        uint32_t Width, Height;
        FramebufferAttachmentSpecification Attachments;
        uint32_t Samples = 1;
        bool SwapChainTarget = false;
    };

    class LOOM_API Framebuffer {
    public:
        virtual ~Framebuffer() = default;

        virtual void Bind() = 0;
        virtual void Unbind() = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const = 0;
        // Returns the depth attachment's GPU texture handle (0 if no depth attachment exists).
        // Callers bind it as a regular 2D texture for sampling (e.g. shadow map lookups).
        virtual uint32_t GetDepthAttachmentRendererID() const = 0;
        virtual const FramebufferSpecification& GetSpecification() const = 0;

        virtual void ClearAttachment(uint32_t attachment_index, int value) = 0;
        virtual int ReadPixel(uint32_t attachment_index, int x, int y) = 0;

        static std::shared_ptr<Framebuffer> Create(const FramebufferSpecification& spec);
    };

} // namespace Loom