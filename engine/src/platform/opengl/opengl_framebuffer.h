#pragma once

#include "loom/renderer/framebuffer.h"

namespace Loom {

    class OpenGLFramebuffer : public Framebuffer {
    public:
        OpenGLFramebuffer(const FramebufferSpecification& spec);
        ~OpenGLFramebuffer() override;

        void Invalidate();

        void Bind() override;
        void Unbind() override;
        void Resize(uint32_t width, uint32_t height) override;

        uint32_t GetColorAttachmentRendererID() const override { return mColorAttachment; }
        const FramebufferSpecification& GetSpecification() const override { return mSpecification; }

    private:
        uint32_t mRendererID = 0;
        uint32_t mColorAttachment = 0;
        uint32_t mDepthAttachment = 0;
        FramebufferSpecification mSpecification;
    };

} // namespace Loom