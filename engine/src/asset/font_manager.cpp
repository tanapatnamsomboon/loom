#include "loom/asset/font_manager.h"

namespace Loom {

    std::unordered_map<FontType, ImFont*> FontManager::sFonts;

    void FontManager::Init(float dpi_scale) {
        ImGuiIO& io = ImGui::GetIO();

        io.Fonts->Clear();

        LoadUIFont(
            FontType::Small,
            "assets/fonts/inter/inter-regular.ttf",
            "assets/fonts/noto_sans_thai/noto_sans_thai_regular.ttf",
            11.0f * dpi_scale
        );

        LoadUIFont(
            FontType::Medium,
            "assets/fonts/inter/inter_regular.ttf",
            "assets/fonts/noto_sans_thai/noto_sans_thai_regular.ttf",
            12.0f * dpi_scale
        );

        LoadUIFont(
            FontType::MediumBold,
            "assets/fonts/inter/inter_regular.ttf",
            "assets/fonts/noto_sans_thai/noto_sans_thai_regular.ttf",
            12.0f * dpi_scale
        );

        LoadUIFont(
            FontType::Large,
            "assets/fonts/inter/inter_regular.ttf",
            "assets/fonts/noto_sans_thai/noto_sans_thai_regular.ttf",
            16.0f * dpi_scale
        );

        LoadUIFont(
            FontType::LargeBold,
            "assets/fonts/inter/inter_regular.ttf",
            "assets/fonts/noto_sans_thai/noto_sans_thai_regular.ttf",
            16.0f * dpi_scale
        );

        ImFontConfig config;
        config.PixelSnapH = true;

        sFonts[FontType::Monospace] =
            io.Fonts->AddFontFromFileTTF(
                "assets/fonts/roboto_mono/roboto_mono_regular.ttf",
                12.0f * dpi_scale,
                &config
            );

        io.FontDefault = sFonts[FontType::Medium];

        io.Fonts->Build();
    }

    ImFont* FontManager::Get(FontType type) {
        return sFonts[type];
    }

    void FontManager::Push(FontType type) {
        ImGui::PushFont(Get(type));
    }

    void FontManager::Pop() {
        ImGui::PopFont();
    }

    void FontManager::LoadUIFont(FontType type, const char* latin_font, const char* thai_font, float size, bool bold) {
        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig config;
        config.PixelSnapH = true;
        config.OversampleH = 2;
        config.OversampleV = 2;

        ImFont* font =
            io.Fonts->AddFontFromFileTTF(
                latin_font,
                size,
                &config,
                io.Fonts->GetGlyphRangesDefault()
            );

        ImFontConfig thai_config;
        thai_config.MergeMode = true;
        thai_config.PixelSnapH = true;

        static constexpr ImWchar thai_ranges[] = {
            0x0E00, 0x0E7F,
            0
        };

        io.Fonts->AddFontFromFileTTF(
            thai_font,
            size,
            &thai_config,
            thai_ranges
        );

        sFonts[type] = font;
    }

} // namespace Loom