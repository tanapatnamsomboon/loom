#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include "loom/renderer/font_asset.h"
#include "loom/core/log.h"
#include <fstream>
#include <vector>

namespace Loom {

std::shared_ptr<FontAsset> FontAsset::Create(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOOM_CORE_ERROR("FontAsset: failed to open '{}'", path);
        return nullptr;
    }
    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> ttf_data(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char*>(ttf_data.data()), file_size);
    file.close();

    // Bake printable ASCII (codepoints 32–127) into a grayscale atlas.
    std::vector<uint8_t>          bitmap(AtlasWidth * AtlasHeight);
    std::array<stbtt_packedchar, 96> packed_chars;

    stbtt_pack_context pc;
    // Padding must be ≥ oversampling factor to avoid neighbour bleed under linear sampling.
    if (!stbtt_PackBegin(&pc, bitmap.data(), AtlasWidth, AtlasHeight, 0, 2, nullptr)) {
        LOOM_CORE_ERROR("FontAsset: stbtt_PackBegin failed for '{}'", path);
        return nullptr;
    }
    stbtt_PackSetOversampling(&pc, 2, 2);
    stbtt_PackFontRange(&pc, ttf_data.data(), 0, static_cast<float>(BakedSize),
                        32, 96, packed_chars.data());
    stbtt_PackEnd(&pc);

    // Expand single-channel bitmap → RGBA (white pixels, glyph alpha).
    // Quad shader does: output = texture_sample * vertex_color.
    // With RGB=255 and A=glyph_mask: output = vertex_color with glyph alpha. ✓
    std::vector<uint8_t> rgba(AtlasWidth * AtlasHeight * 4);
    for (int i = 0; i < AtlasWidth * AtlasHeight; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }

    TextureSpecification spec;
    spec.Filter       = FilterMode::Linear;
    spec.Wrap         = WrapMode::Clamp;
    spec.GenerateMips = false;

    auto texture = Texture2D::Create(AtlasWidth, AtlasHeight, spec);
    texture->SetData(rgba.data(), static_cast<uint32_t>(rgba.size()));

    // Pre-compute GlyphData in GL UV / local-space conventions.
    // stb_truetype uses top-down Y and top-left UV; we flip both.
    auto font           = std::shared_ptr<FontAsset>(new FontAsset());
    font->mPath         = path;
    font->mAtlasTexture = texture;

    for (int i = 0; i < 96; i++) {
        stbtt_aligned_quad q;
        float cx = 0.0f, cy = 0.0f;
        stbtt_GetPackedQuad(packed_chars.data(), AtlasWidth, AtlasHeight, i, &cx, &cy, &q, 0);

        GlyphData& g = font->mGlyphs[i];
        // Store raw stb UV values. The atlas is uploaded without flipping, so the
        // texture is stored upside-down in GL. DrawText compensates by assigning
        // t1 to bottom quad vertices and t0 to top quad vertices.
        g.s0 = q.s0;
        g.s1 = q.s1;
        g.t0 = q.t0;
        g.t1 = q.t1;
        // Quad: negate y to convert screen-down to GL-up; normalize by baked size.
        g.QuadMin  = { q.x0 / static_cast<float>(BakedSize), -q.y1 / static_cast<float>(BakedSize) };
        g.QuadMax  = { q.x1 / static_cast<float>(BakedSize), -q.y0 / static_cast<float>(BakedSize) };
        // cx is advanced by stbtt_GetPackedQuad to the next cursor position.
        g.Advance  = cx / static_cast<float>(BakedSize);
    }

    // Compute line height from font vertical metrics.
    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf_data.data(), stbtt_GetFontOffsetForIndex(ttf_data.data(), 0));
    float scale = stbtt_ScaleForPixelHeight(&info, static_cast<float>(BakedSize));
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    font->mLineHeight = static_cast<float>(ascent - descent + line_gap) * scale
                        / static_cast<float>(BakedSize);

    LOOM_CORE_TRACE("FontAsset: loaded '{}' ({}x{} atlas, {} px baked)",
                    path, AtlasWidth, AtlasHeight, BakedSize);
    return font;
}

bool FontAsset::GetGlyphData(char c, GlyphData& out) const {
    if (c < 32 || c > 127) return false;
    out = mGlyphs[static_cast<int>(c) - 32];
    return true;
}

} // namespace Loom
