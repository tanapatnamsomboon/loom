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

        uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override { return mColorAttachments[index]; }
        const FramebufferSpecification& GetSpecification() const override { return mSpecification; }

        void ClearAttachment(uint32_t attachment_index, int value) override;
        int ReadPixel(uint32_t attachment_index, int x, int y) override;

    private:
        uint32_t mRendererID = 0;
        FramebufferSpecification mSpecification;

        std::vector<FramebufferTextureSpecification> mColorAttachmentSpecifications;
        FramebufferTextureSpecification mDepthAttachmentSpecification = FramebufferTextureFormat::None;

        std::vector<uint32_t> mColorAttachments;
        uint32_t mDepthAttachment = 0;
    };

} // namespace Loom