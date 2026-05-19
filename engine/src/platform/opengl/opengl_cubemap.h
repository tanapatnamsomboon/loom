#pragma once

#include "loom/renderer/cubemap.h"
#include <glad/glad.h>

namespace Loom {

    class OpenGLTextureCubemap : public TextureCubemap {
    public:
        OpenGLTextureCubemap(uint32_t face_size, CubemapFormat format, uint32_t mip_levels);
        ~OpenGLTextureCubemap() override;

        uint32_t      GetRendererID() const override { return mRendererID; }
        uint32_t      GetFaceSize()   const override { return mFaceSize; }
        uint32_t      GetMipLevels()  const override { return mMipLevels; }
        CubemapFormat GetFormat()     const override { return mFormat; }

        void Bind(uint32_t slot = 0) const override;

    private:
        uint32_t      mRendererID = 0;
        uint32_t      mFaceSize   = 0;
        uint32_t      mMipLevels  = 1;
        CubemapFormat mFormat     = CubemapFormat::RGB16F;
    };

} // namespace Loom
