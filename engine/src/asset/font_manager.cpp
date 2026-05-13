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

        sFonts[type] = font;
    }

} // namespace Loom
