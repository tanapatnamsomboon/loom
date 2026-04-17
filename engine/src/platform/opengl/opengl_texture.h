#pragma once

#include "loom/renderer/texture.h"
#include <glad/glad.h>

namespace Loom {

    class OpenGLTexture2D : public Texture2D {
    public:
        OpenGLTexture2D(const std::string& path);
        ~OpenGLTexture2D() override;

        uint32_t GetWidth() const override { return mWidth; }
        uint32_t GetHeight() const override { return mHeight; }

        void Bind(uint32_t slot = 0) const override;

    private:
        std::string mPath;
        uint32_t mWidth, mHeight;
        uint32_t mRendererID;
    };

} // namespace Loom