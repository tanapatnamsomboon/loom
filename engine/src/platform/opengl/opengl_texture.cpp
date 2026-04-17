#include "platform/opengl/opengl_texture.h"
#include "loom/core/log.h"
#include <stb_image.h>

namespace Loom {

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
        : mPath(path) {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);
        stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (!data) {
            LOOM_CORE_ERROR("Failed to load image: {0}", path);
            return;
        }

        mWidth = width;
        mHeight = height;

        mInternalFormat = 0;
        mDataFormat = 0;
        if (channels == 4) {
            mInternalFormat = GL_RGBA8;
            mDataFormat = GL_RGBA;
        } else if (channels == 3) {
            mInternalFormat = GL_RGB8;
            mDataFormat = GL_RGB;
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &mRendererID);
        glTextureStorage2D(mRendererID, 1, mInternalFormat, mWidth, mHeight);

        glTextureParameteri(mRendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mRendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTextureSubImage2D(mRendererID, 0, 0, 0, mWidth, mHeight, mDataFormat, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);
    }

    OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
        : mWidth(width), mHeight(height) {

        mInternalFormat = GL_RGBA8;
        mDataFormat = GL_RGBA;

        glCreateTextures(GL_TEXTURE_2D, 1, &mRendererID);
        glTextureStorage2D(mRendererID, 1, mInternalFormat, mWidth, mHeight);

        glTextureParameteri(mRendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mRendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    OpenGLTexture2D::~OpenGLTexture2D() {
        glDeleteTextures(1, &mRendererID);
    }

    void OpenGLTexture2D::SetData(void* data, uint32_t size) {
        uint32_t bpp = mDataFormat == GL_RGBA ? 4 : 3;
        if (size != mWidth * mHeight * bpp) {
            LOOM_CORE_ERROR("Data must be entire texture!");
            return;
        }

        glTextureSubImage2D(mRendererID, 0, 0, 0, mWidth, mHeight, mDataFormat, GL_UNSIGNED_BYTE, data);
    }

    void OpenGLTexture2D::Bind(uint32_t slot) const {
        glBindTextureUnit(slot, mRendererID);
    }

} // namespace Loom