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

struct ThemePalette {
    RgbaColor background;
    RgbaColor surface;
    RgbaColor divider;
    RgbaColor row_bg;
    RgbaColor focus_bg;
    RgbaColor focus_border;
    RgbaColor applied_badge_bg;
    RgbaColor applied_badge_text;
    RgbaColor text_primary;
    RgbaColor text_secondary;
    RgbaColor accent;
    RgbaColor warning_bg;
    RgbaColor warning_border;
    RgbaColor warning_text;
    RgbaColor error_bg;
    RgbaColor error_border;
    RgbaColor error_text;
    RgbaColor info_bg;
    RgbaColor info_border;
    RgbaColor info_text;
    RgbaColor modal_scrim;
    RgbaColor dialog_bg;
    RgbaColor button_secondary_bg;
    RgbaColor button_primary_bg;
    RgbaColor button_primary_text;
};

inline const ThemePalette& theme_for_system(SystemTheme theme) {
    static constexpr ThemePalette kDark{
        {24, 26, 31, 255},     // background
        {34, 37, 44, 255},     // surface
        {54, 60, 72, 255},     // divider
        {38, 42, 51, 255},     // row_bg
        {40, 59, 78, 255},     // focus_bg
        {0, 212, 224, 255},    // focus_border
        {16, 158, 122, 255},   // applied_badge_bg
        {255, 255, 255, 255},  // applied_badge_text
        {245, 247, 250, 255},  // text_primary
        {168, 177, 194, 255},  // text_secondary
        {0, 212, 224, 255},    // accent
        {66, 44, 18, 255},     // warning_bg
        {245, 158, 11, 255},   // warning_border
        {254, 243, 199, 255},  // warning_text
        {63, 27, 34, 255},     // error_bg
        {248, 113, 113, 255},  // error_border
        {254, 226, 226, 255},  // error_text
        {22, 50, 68, 255},     // info_bg
        {56, 189, 248, 255},   // info_border
        {224, 242, 254, 255},  // info_text
        {8, 10, 14, 196},      // modal_scrim
        {36, 40, 49, 255},     // dialog_bg
        {52, 58, 70, 255},     // button_secondary_bg
        {0, 184, 212, 255},    // button_primary_bg
        {12, 18, 24, 255},     // button_primary_text
    };

    static constexpr ThemePalette kLight{
        {235, 236, 238, 255},  // background
        {255, 255, 255, 255},  // surface
        {210, 215, 224, 255},  // divider
        {247, 248, 250, 255},  // row_bg
        {222, 243, 252, 255},  // focus_bg
        {0, 145, 196, 255},    // focus_border
        {13, 148, 136, 255},   // applied_badge_bg
        {255, 255, 255, 255},  // applied_badge_text
        {31, 36, 45, 255},     // text_primary
        {92, 101, 119, 255},   // text_secondary
        {0, 145, 196, 255},    // accent
        {254, 243, 199, 255},  // warning_bg
        {217, 119, 6, 255},    // warning_border
        {120, 53, 15, 255},    // warning_text
        {254, 226, 226, 255},  // error_bg
        {220, 38, 38, 255},    // error_border
        {127, 29, 29, 255},    // error_text
        {224, 242, 254, 255},  // info_bg
        {2, 132, 199, 255},    // info_border
        {12, 74, 110, 255},    // info_text
        {15, 23, 42, 165},     // modal_scrim
        {255, 255, 255, 255},  // dialog_bg
        {229, 233, 240, 255},  // button_secondary_bg
        {0, 145, 196, 255},    // button_primary_bg
        {255, 255, 255, 255},  // button_primary_text
    };

    return theme == SystemTheme::Light ? kLight : kDark;
}

} // namespace kailocale
