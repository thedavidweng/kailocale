#pragma once

#include <cstdint>

#include "kailocale/language_service.hpp"

namespace kailocale {

struct RgbaColor {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr std::uint32_t packed() const {
        return static_cast<std::uint32_t>(r) |
               (static_cast<std::uint32_t>(g) << 8) |
               (static_cast<std::uint32_t>(b) << 16) |
               (static_cast<std::uint32_t>(a) << 24);
    }
};

/// Colors follow the Horizon (Switch System Settings) Basic Black and Basic White color sets.
struct ThemePalette {
    RgbaColor background;
    RgbaColor sidebar;
    RgbaColor header_rule;
    RgbaColor list_rule;
    RgbaColor text_primary;
    RgbaColor text_secondary;
    RgbaColor text_faint;
    RgbaColor accent;
    RgbaColor highlight;
    RgbaColor highlight_bg;
    RgbaColor highlight_shadow;
    RgbaColor dialog_bg;
    RgbaColor modal_scrim;
    RgbaColor warning;
    RgbaColor error;
    RgbaColor glyph_bg;
    RgbaColor glyph_fg;
};

inline const ThemePalette& theme_for_system(SystemTheme theme) {
    static constexpr ThemePalette kDark{
        {45, 45, 45, 255},     // background
        {50, 50, 50, 255},     // sidebar
        {255, 255, 255, 255},  // header_rule
        {78, 78, 78, 255},     // list_rule
        {255, 255, 255, 255},  // text_primary
        {163, 163, 163, 255},  // text_secondary
        {125, 125, 125, 255},  // text_faint
        {0, 255, 204, 255},    // accent
        {64, 200, 232, 255},   // highlight
        {31, 34, 39, 255},     // highlight_bg
        {0, 0, 0, 255},        // highlight_shadow
        {70, 70, 70, 255},     // dialog_bg
        {0, 0, 0, 178},        // modal_scrim
        {255, 196, 0, 255},    // warning
        {255, 92, 92, 255},    // error
        {255, 255, 255, 255},  // glyph_bg
        {45, 45, 45, 255},     // glyph_fg
    };

    static constexpr ThemePalette kLight{
        {235, 235, 235, 255},  // background
        {240, 240, 240, 255},  // sidebar
        {45, 45, 45, 255},     // header_rule
        {207, 207, 207, 255},  // list_rule
        {45, 45, 45, 255},     // text_primary
        {115, 115, 115, 255},  // text_secondary
        {150, 150, 150, 255},  // text_faint
        {50, 80, 240, 255},    // accent
        {20, 190, 214, 255},   // highlight
        {253, 255, 252, 255},  // highlight_bg
        {0, 0, 0, 255},        // highlight_shadow
        {240, 240, 240, 255},  // dialog_bg
        {0, 0, 0, 150},        // modal_scrim
        {214, 132, 0, 255},    // warning
        {214, 48, 48, 255},    // error
        {45, 45, 45, 255},     // glyph_bg
        {235, 235, 235, 255},  // glyph_fg
    };

    return theme == SystemTheme::Light ? kLight : kDark;
}

} // namespace kailocale
