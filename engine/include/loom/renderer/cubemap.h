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

        // Diffuse irradiance map from an HDR environment cubemap. One-time GPU
        // pass: each output texel is the Lambertian-weighted integral of the
        // environment over the hemisphere oriented around that texel's
        // direction. 32^2 is sufficient — diffuse irradiance is very low-
        // frequency so a small cubemap is plenty.
        static std::shared_ptr<TextureCubemap> CreateIrradiance(
            const std::shared_ptr<TextureCubemap>& env_cubemap,
            uint32_t face_size = 32);

        // Specular prefilter cubemap (Karis 2013 split-sum). Allocates a full
        // mip chain; each mip is convolved with a GGX importance-sampled
        // kernel where roughness = mip / (maxMip - 1). Mip 0 is the
        // mirror-roughness sample; the deepest mip is fully rough. The PBR
        // shader samples this via `textureLod(prefilter, R, roughness * maxLOD)`
        // and combines with a BRDF LUT to produce the specular IBL term.
        static std::shared_ptr<TextureCubemap> CreatePrefiltered(
            const std::shared_ptr<TextureCubemap>& env_cubemap,
            uint32_t face_size = 256);
    };

} // namespace Loom
