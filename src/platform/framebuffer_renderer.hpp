#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include "kailocale/language_picker.hpp"
#include "kailocale/platform_services.hpp"
#include "kailocale/text_runs.hpp"

namespace kailocale {

/// Renders KaiLocale shell errors and picker screens into a Switch 2 linear RGBA8 Framebuffer using FreeType + HarfBuzz.
class FramebufferRenderer {
public:
    ~FramebufferRenderer();

    void set_dimensions(std::uint32_t width, std::uint32_t height);
    void set_font_face(FontSlot slot, FT_Face face);
    void clear_fonts();

    void render_shell_view(std::uint8_t* buffer, std::uint32_t stride, const ShellView& view) const;
    void render_picker_view(std::uint8_t* buffer, std::uint32_t stride, const PickerView& view) const;

private:
    static bool has_glyph_cb(void* ctx, FontSlot slot, std::uint32_t codepoint);
    bool has_any_face() const;
    int measure_text_width(std::string_view text, int pixel_size, ScriptPreference pref) const;
    void draw_text(std::uint8_t* buffer,
                   std::uint32_t stride,
                   int x,
                   int baseline_y,
                   int pixel_size,
                   std::string_view text,
                   ScriptPreference pref,
                   RgbaColor color,
                   int max_width = 0) const;

    std::uint32_t width_ = 1920;
    std::uint32_t height_ = 1080;
    std::array<FT_Face, 8> faces_{};
    std::array<hb_font_t*, 8> hb_fonts_{};
};

} // namespace kailocale
