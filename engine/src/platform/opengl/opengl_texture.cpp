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

        GLenum internal_format = 0, data_format = 0;
        if (channels == 4) {
            internal_format = GL_RGBA8;
            data_format = GL_RGBA;
        } else if (channels == 3) {
            internal_format = GL_RGB8;
            data_format = GL_RGB;
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &mRendererID);
        glTextureStorage2D(mRendererID, 1, internal_format, mWidth, mHeight);

        glTextureParameteri(mRendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mRendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTextureSubImage2D(mRendererID, 0, 0, 0, mWidth, mHeight, data_format, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);
    }

    OpenGLTexture2D::~OpenGLTexture2D() {
        glDeleteTextures(1, &mRendererID);
    }

    void OpenGLTexture2D::Bind(uint32_t slot) const {
        glBindTextureUnit(slot, mRendererID);
    }

} // namespace Loom