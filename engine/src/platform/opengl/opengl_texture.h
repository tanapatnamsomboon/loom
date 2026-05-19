#pragma once

#include "loom/renderer/texture.h"
#include <glad/glad.h>

namespace Loom {

    class OpenGLTexture2D : public Texture2D {
    public:
        OpenGLTexture2D(const std::string& path, const TextureSpecification& spec = {});
        OpenGLTexture2D(uint32_t width, uint32_t height);
        OpenGLTexture2D(uint32_t width, uint32_t height, const TextureSpecification& spec);
        ~OpenGLTexture2D() override;

        uint32_t GetWidth() const override { return mWidth; }
        uint32_t GetHeight() const override { return mHeight; }
        uint32_t GetRendererID() const override { return mRendererID; }

        const std::string& GetPath() const override { return mPath; }

        void SetData(void* data, uint32_t size) override;

        void Bind(uint32_t slot = 0) const override;

        void Reload() override;

    private:
        void Load(const std::string& path, const TextureSpecification& spec);

    private:
        std::string          mPath;
        TextureSpecification mSpec;
        uint32_t             mWidth = 0, mHeight = 0;
        uint32_t             mRendererID = 0;
        GLenum               mInternalFormat = 0, mDataFormat = 0;
        bool                 mIsHDR = false;
    };

} // namespace Loom
