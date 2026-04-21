#include "platform/opengl/opengl_framebuffer.h"
#include "loom/core/log.h"
#include <glad/glad.h>

namespace Loom {

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec)
        : mSpecification(spec) {

        for (auto format : mSpecification.Attachments.Attachments) {
            if (format.TextureFormat == FramebufferTextureFormat::DEPTH24STENCIL8)
                mDepthAttachmentSpecification = format;
            else
                mColorAttachmentSpecifications.emplace_back(format);
        }
        Invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer() {
        glDeleteFramebuffers(1, &mRendererID);
        glDeleteTextures(mColorAttachments.size(), mColorAttachments.data());
        glDeleteTextures(1, &mDepthAttachment);
    }

    void OpenGLFramebuffer::Invalidate() {
        if (mRendererID) {
            glDeleteFramebuffers(1, &mRendererID);
            glDeleteTextures(mColorAttachments.size(), mColorAttachments.data());
            glDeleteTextures(1, &mDepthAttachment);
            mColorAttachments.clear();
        }

        glCreateFramebuffers(1, &mRendererID);
        glBindFramebuffer(GL_FRAMEBUFFER, mRendererID);

        if (mColorAttachmentSpecifications.size()) {
            mColorAttachments.resize(mColorAttachmentSpecifications.size());
            glCreateTextures(GL_TEXTURE_2D, mColorAttachments.size(), mColorAttachments.data());

            for (size_t i = 0; i < mColorAttachments.size(); i++) {
                glBindTexture(GL_TEXTURE_2D, mColorAttachments[i]);
                switch (mColorAttachmentSpecifications[i].TextureFormat) {
                    case FramebufferTextureFormat::RGBA8:
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mSpecification.Width, mSpecification.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        break;
                    case FramebufferTextureFormat::RED_INTEGER:
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I, mSpecification.Width, mSpecification.Height, 0, GL_RED_INTEGER, GL_INT, nullptr);
                        break;
                    case FramebufferTextureFormat::DEPTH24STENCIL8:
                        break;
                    case FramebufferTextureFormat::None:
                        break;
                }

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, mColorAttachments[i], 0);
            }
        }

        if (mColorAttachments.size() > 1) {
            GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers(mColorAttachments.size(), buffers);
        } else if (mColorAttachments.empty()) {
            glDrawBuffer(GL_NONE);
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            LOOM_CORE_FATAL("Framebuffer is incomplete!");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, mRendererID);
        glViewport(0, 0, mSpecification.Width, mSpecification.Height);
    }

    void OpenGLFramebuffer::Unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0 || width > 8192 || height > 8192) {
            LOOM_CORE_WARN("Attempted to resize framebuffer to {0}, {1}", width, height);
            return;
        }

        mSpecification.Width = width;
        mSpecification.Height = height;

        Invalidate();
    }

    void OpenGLFramebuffer::ClearAttachment(uint32_t attachment_index, int value) {
        glClearTexImage(mColorAttachments[attachment_index], 0, GL_RED_INTEGER, GL_INT, &value);
    }

    int OpenGLFramebuffer::ReadPixel(uint32_t attachment_index, int x, int y) {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_index);
        int pixel_data;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixel_data);
        return pixel_data;
    }

} // namespace Loom