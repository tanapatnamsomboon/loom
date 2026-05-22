#include "loom/asset/font_manager.h"
#include "loom/project/project.h"

namespace Loom {

    std::unordered_map<FontType, ImFont*> FontManager::sFonts;

    static std::string EngineFont(const char* relative) {
        return Project::GetEngineAssetFileSystemPath(std::string("fonts/") + relative).generic_string();
    }

    void FontManager::Init(float dpi_scale) {
        ImGuiIO& io = ImGui::GetIO();

        io.Fonts->Clear();

        LoadUIFont(FontType::Small,      "inter/inter_regular.ttf",   "noto_sans_thai/noto_sans_thai_regular.ttf", 11.0f * dpi_scale);
        LoadUIFont(FontType::Medium,     "inter/inter_regular.ttf",   "noto_sans_thai/noto_sans_thai_regular.ttf", 12.0f * dpi_scale);
        LoadUIFont(FontType::MediumBold, "inter/inter_bold.ttf",      "noto_sans_thai/noto_sans_thai_bold.ttf",    12.0f * dpi_scale);
        LoadUIFont(FontType::Large,      "inter/inter_regular.ttf",   "noto_sans_thai/noto_sans_thai_regular.ttf", 16.0f * dpi_scale);
        LoadUIFont(FontType::LargeBold,  "inter/inter_bold.ttf",      "noto_sans_thai/noto_sans_thai_bold.ttf",    16.0f * dpi_scale);

        ImFontConfig mono_config;
        mono_config.PixelSnapH  = true;
        mono_config.OversampleH = 2;
        mono_config.OversampleV = 1;

        std::string mono_path = EngineFont("roboto_mono/roboto_mono_regular.ttf");
        sFonts[FontType::Monospace] =
            io.Fonts->AddFontFromFileTTF(mono_path.c_str(), 12.0f * dpi_scale, &mono_config);
        MergeIconFont(12.0f * dpi_scale);

        io.FontDefault = sFonts[FontType::Medium];

        io.Fonts->Build();
    }

    ImFont* FontManager::Get(FontType type) {
        auto it = sFonts.find(type);
        return (it != sFonts.end()) ? it->second : nullptr;
    }

    void FontManager::Push(FontType type) {
        if (auto* font = Get(type)) ImGui::PushFont(font);
        else                        ImGui::PushFont(ImGui::GetIO().FontDefault);
    }

    void FontManager::Pop() {
        ImGui::PopFont();
    }

    void FontManager::LoadUIFont(FontType type, const char* latin_font, const char* thai_font, float size, bool /*bold*/) {
        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig latin_config;
        latin_config.PixelSnapH  = true;
        latin_config.OversampleH = 2;
        latin_config.OversampleV = 1;

        std::string latin_path = EngineFont(latin_font);
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            latin_path.c_str(),
            size,
            &latin_config,
            io.Fonts->GetGlyphRangesDefault()
        );

        ImFontConfig thai_config;
        thai_config.MergeMode    = true;
        thai_config.PixelSnapH   = true;
        thai_config.OversampleH  = 2;
        thai_config.OversampleV  = 1;

        static constexpr ImWchar thai_ranges[] = {
            0x0E00, 0x0E7F,
            0
        };

        std::string thai_path = EngineFont(thai_font);
        io.Fonts->AddFontFromFileTTF(
            thai_path.c_str(),
            size,
            &thai_config,
            thai_ranges
        );

        MergeIconFont(size);

        sFonts[type] = font;
    }

    void FontManager::MergeIconFont(float size) {
        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig icon_config;
        icon_config.MergeMode        = true;          // fold glyphs into the previous font
        icon_config.PixelSnapH       = true;
        icon_config.GlyphMinAdvanceX = size;          // render icons monospaced
        icon_config.GlyphOffset      = ImVec2(0.0f, 1.0f); // nudge onto the text baseline

        // Fork Awesome packs its glyphs into the Unicode Private Use Area.
        static constexpr ImWchar icon_ranges[] = {
            0xF000, 0xF2FF,
            0
        };

        std::string icon_path = EngineFont("fork_awesome/fork_awesome.ttf");
        io.Fonts->AddFontFromFileTTF(
            icon_path.c_str(),
            size,
            &icon_config,
            icon_ranges
        );
    }

} // namespace Loom
