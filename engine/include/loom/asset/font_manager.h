#pragma once

#include "loom/core/core.h"
#include <imgui.h>

namespace Loom {

    enum class FontType {
        Small,
        Medium,
        MediumBold,
        Large,
        LargeBold,

        Monospace
    };

    class LOOM_API FontManager {
    public:
        static void Init(float dpi_scale = 1.0f);

        static ImFont* Get(FontType type);

        static void Push(FontType type);
        static void Pop();

    private:
        static void LoadUIFont(
            FontType type,
            const char* latin_font,
            const char* thai_font,
            float size,
            bool bold = false
        );

        // Merges the Fork Awesome icon glyphs into the most recently added font.
        // Call immediately after an AddFontFromFileTTF so the icons land in that font.
        static void MergeIconFont(float size);

    private:
        static std::unordered_map<FontType, ImFont*> sFonts;
    };

} // namespace Loom