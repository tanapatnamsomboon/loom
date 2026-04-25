#pragma once

#include "loom/renderer/texture.h"
#include <glad/glad.h>

namespace Loom {

    class OpenGLTexture2D : public Texture2D {
    public:
        OpenGLTexture2D(const std::string& path);
        OpenGLTexture2D(uint32_t width, uint32_t height);
        ~OpenGLTexture2D() override;

        uint32_t GetWidth() const override { return mWidth; }
        uint32_t GetHeight() const override { return mHeight; }
        uint32_t GetRendererID() const override { return mRendererID; }

        const std::string& GetPath() const override { return mPath; }

        void SetData(void* data, uint32_t size) override;

        void Bind(uint32_t slot = 0) const override;

    private:
        std::string mPath;
        uint32_t    mWidth, mHeight;
        uint32_t    mRendererID;
        GLenum      mInternalFormat, mDataFormat;
    };

} // namespace Loom
