#pragma once

#include "loom/core/core.h"
#include "loom/renderer/texture.h"
#include <array>
#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace Loom {

struct GlyphData {
    float     s0, s1;    // atlas U range
    float     t0, t1;    // atlas V range (stb convention: t0 = top row, t1 = bottom row)
    glm::vec2 QuadMin;   // local-space BL (x0, -y1) normalized by baked size
    glm::vec2 QuadMax;   // local-space TR (x1, -y0) normalized by baked size
    float     Advance = 0.0f;  // cursor advance normalized by baked size
};

class LOOM_API FontAsset {
public:
    static constexpr int AtlasWidth  = 1024;
    static constexpr int AtlasHeight = 1024;
    // Baked larger than typical render size so Linear sampling downsamples (sharp) instead of
    // upsampling (blurry). 72 px fits all 96 ASCII glyphs in a 1024² atlas with padding.
    static constexpr int BakedSize   = 72;

    static std::shared_ptr<FontAsset> Create(const std::string& path);

    bool GetGlyphData(char c, GlyphData& out) const;

    std::shared_ptr<Texture2D> GetAtlasTexture() const { return mAtlasTexture; }
    float GetLineHeight() const { return mLineHeight; }
    const std::string& GetPath() const { return mPath; }

private:
    FontAsset() = default;

    std::string                mPath;
    std::shared_ptr<Texture2D> mAtlasTexture;
    std::array<GlyphData, 96>  mGlyphs = {};
    float                      mLineHeight = 1.0f;
};

} // namespace Loom
