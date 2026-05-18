#pragma once

#include "loom/core/core.h"
#include "loom/renderer/texture.h"
#include <cstdint>
#include <memory>

namespace Loom {

    // GPU-side cubemap. Six square faces, optionally mip-chained, in either an
    // 8-bit or 16-bit-float format. Used for:
    //   - HDR environment maps (converted from 2:1 equirectangular .hdr files)
    //   - Diffuse irradiance maps (low-res cubemap precomputed from environment)
    //   - Specular prefilter maps (mip-chained, each level convolved at a
    //     different roughness for IBL split-sum approximation)
    enum class CubemapFormat {
        RGBA8,   // 8-bit (rarely used; here for completeness)
        RGB16F,  // HDR — env / irradiance / prefilter sources
    };

    class LOOM_API TextureCubemap {
    public:
        virtual ~TextureCubemap() = default;

        virtual uint32_t GetRendererID() const   = 0;
        virtual uint32_t GetFaceSize()   const   = 0;
        virtual uint32_t GetMipLevels()  const   = 0;
        virtual CubemapFormat GetFormat() const  = 0;

        virtual void Bind(uint32_t slot = 0) const = 0;

        // Empty cubemap of the given size + format. `mip_levels > 1` allocates
        // a full mip chain (storage only — caller fills the levels).
        static std::shared_ptr<TextureCubemap> Create(uint32_t face_size,
                                                      CubemapFormat format = CubemapFormat::RGB16F,
                                                      uint32_t mip_levels = 1);

        // Builds an HDR cubemap from an equirectangular 2:1 .hdr texture via a
        // one-time GPU pass (renders the 6 faces using a unit cube + spherical
        // mapping shader). The source `equirect` must be loaded as HDR float.
        // `face_size` is typically 512.
        static std::shared_ptr<TextureCubemap> CreateFromEquirect(
            const std::shared_ptr<Texture2D>& equirect,
            uint32_t face_size = 512);
    };

} // namespace Loom
