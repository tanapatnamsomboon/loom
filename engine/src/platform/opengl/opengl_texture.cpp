#include "platform/opengl/opengl_texture.h"
#include "loom/core/log.h"
#include <stb_image.h>

namespace Loom {

    void OpenGLTexture2D::Load(const std::string& path, const TextureSpecification& spec) {
        // HDR (Radiance .hdr / RGBE) takes the float path; everything else is 8-bit.
        // Detection by extension keeps callers oblivious — same `AssetManager::GetTexture`
        // entry point for sRGB textures and HDR environment maps.
        bool is_hdr = path.size() >= 4 && path.compare(path.size() - 4, 4, ".hdr") == 0;

        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);

        void*  data_void   = nullptr;
        size_t bytes_per_channel = 1;
        if (is_hdr) {
            data_void          = stbi_loadf(path.c_str(), &width, &height, &channels, 0);
            bytes_per_channel  = sizeof(float);
        } else {
            data_void          = stbi_load(path.c_str(), &width, &height, &channels, 0);
        }

        if (!data_void) {
            LOOM_CORE_ERROR("Failed to load image: {0}", path);
            return;
        }

        mWidth  = width;
        mHeight = height;
        mIsHDR  = is_hdr;

        mInternalFormat = 0;
        mDataFormat     = 0;
        if (is_hdr) {
            // Always store HDR as float internally. stb returns 3 channels (RGB) for .hdr.
            mInternalFormat = (channels == 4) ? GL_RGBA16F : GL_RGB16F;
            mDataFormat     = (channels == 4) ? GL_RGBA    : GL_RGB;
        } else if (channels == 4) {
            mInternalFormat = GL_RGBA8;
            mDataFormat     = GL_RGBA;
        } else if (channels == 3) {
            mInternalFormat = GL_RGB8;
            mDataFormat     = GL_RGB;
        }

        uint32_t levels = spec.GenerateMips
            ? static_cast<uint32_t>(std::floor(std::log2(std::max(mWidth, mHeight)))) + 1
            : 1;

        glCreateTextures(GL_TEXTURE_2D, 1, &mRendererID);
        glTextureStorage2D(mRendererID, levels, mInternalFormat, mWidth, mHeight);

        GLenum min_filter;
        if (spec.GenerateMips) {
            min_filter = (spec.Filter == FilterMode::Linear)
                ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
        } else {
            min_filter = (spec.Filter == FilterMode::Linear) ? GL_LINEAR : GL_NEAREST;
        }
        GLenum mag_filter = (spec.Filter == FilterMode::Linear) ? GL_LINEAR : GL_NEAREST;
        // HDR equirect maps wrap horizontally (sphere) and clamp vertically (poles).
        // Forcing Clamp for HDR avoids visible seams at the bottom of the sphere.
        GLenum wrap       = is_hdr ? GL_CLAMP_TO_EDGE
                                   : ((spec.Wrap == WrapMode::Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT);

        glTextureParameteri(mRendererID, GL_TEXTURE_MIN_FILTER, min_filter);
        glTextureParameteri(mRendererID, GL_TEXTURE_MAG_FILTER, mag_filter);
        glTextureParameteri(mRendererID, GL_TEXTURE_WRAP_S, wrap);
        glTextureParameteri(mRendererID, GL_TEXTURE_WRAP_T, wrap);

        if (spec.GenerateMips && spec.Filter == FilterMode::Linear) {
            float max_anisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_anisotropy);
            glTextureParameterf(mRendererID, GL_TEXTURE_MAX_ANISOTROPY, max_anisotropy);
        }

        GLenum data_type = is_hdr ? GL_FLOAT : GL_UNSIGNED_BYTE;
        glTextureSubImage2D(mRendererID, 0, 0, 0, mWidth, mHeight, mDataFormat, data_type, data_void);
        if (spec.GenerateMips)
            glGenerateTextureMipmap(mRendererID);

        stbi_image_free(data_void);
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path, const TextureSpecification& spec)
        : mPath(path), mSpec(spec) {
        Load(path, spec);
    }

    OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
        : mWidth(width)
        , mHeight(height) {
        mInternalFormat = GL_RGBA8;
        mDataFormat     = GL_RGBA;

        glCreateTextures(GL_TEXTURE_2D, 1, &mRendererID);
        glTextureStorage2D(mRendererID, 1, mInternalFormat, mWidth, mHeight);

        glTextureParameteri(mRendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mRendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, const TextureSpecification& spec)
        : mWidth(width), mHeight(height), mSpec(spec) {
        mInternalFormat = GL_RGBA8;
        mDataFormat     = GL_RGBA;

        glCreateTextures(GL_TEXTURE_2D, 1, &mRendererID);
        glTextureStorage2D(mRendererID, 1, mInternalFormat, mWidth, mHeight);

        GLenum filter = (spec.Filter == FilterMode::Linear) ? GL_LINEAR : GL_NEAREST;
        GLenum wrap   = (spec.Wrap == WrapMode::Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
        glTextureParameteri(mRendererID, GL_TEXTURE_MIN_FILTER, filter);
        glTextureParameteri(mRendererID, GL_TEXTURE_MAG_FILTER, filter);
        glTextureParameteri(mRendererID, GL_TEXTURE_WRAP_S, wrap);
        glTextureParameteri(mRendererID, GL_TEXTURE_WRAP_T, wrap);
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

    void OpenGLTexture2D::Reload() {
        if (mPath.empty()) return;
        glDeleteTextures(1, &mRendererID);
        mRendererID = 0;
        Load(mPath, mSpec);
    }

} // namespace Loom
