#include "platform/opengl/opengl_framebuffer.h"
#include "loom/core/log.h"
#include <glad/glad.h>

namespace Loom {

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec)
        : mSpecification(spec) {
        Invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer() {
        glDeleteFramebuffers(1, &mRendererID);
        glDeleteTextures(1, &mColorAttachment);
        glDeleteTextures(1, &mDepthAttachment);
    }

    void OpenGLFramebuffer::Invalidate() {
        if (mRendererID) {
            glDeleteFramebuffers(1, &mRendererID);
            glDeleteTextures(1, &mColorAttachment);
            glDeleteTextures(1, &mDepthAttachment);
        }

        glCreateFramebuffers(1, &mRendererID);
        glBindFramebuffer(GL_FRAMEBUFFER, mRendererID);

        glCreateTextures(GL_TEXTURE_2D, 1, &mColorAttachment);
        glBindTexture(GL_TEXTURE_2D, mColorAttachment);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mSpecification.Width, mSpecification.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorAttachment, 0);

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

} // namespace Loom