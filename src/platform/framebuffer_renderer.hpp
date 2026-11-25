#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include "kailocale/language_picker.hpp"
#include "kailocale/platform_services.hpp"
#include "kailocale/text_runs.hpp"

namespace kailocale {

struct RenderSurface {
    std::uint8_t* buffer = nullptr;
    std::uint32_t stride = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

/// Renders KaiLocale shell errors and Horizon-styled picker screens into a Switch 2 linear RGBA8 Framebuffer using FreeType + HarfBuzz.
class FramebufferRenderer {
public:
    ~FramebufferRenderer();

    void set_dimensions(std::uint32_t width, std::uint32_t height);
    void set_font_face(FontSlot slot, FT_Face face);
    void clear_fonts();

    void render_shell_view(std::uint8_t* buffer, std::uint32_t stride, const ShellView& view) const;
    void render_picker_view(std::uint8_t* buffer, std::uint32_t stride, const PickerView& view) const;

private:
    struct ShapedGlyph {
        FT_Face face = nullptr;
        std::uint32_t glyph_index = 0;
        int x_advance = 0;
        int x_offset = 0;
        int y_offset = 0;
    };

    struct DialogItem {
        enum class Kind { Icon, Title, Language, Note, Gap };
        Kind kind = Kind::Gap;
        std::string text;
        const LanguageEntry* entry = nullptr;
        RgbaColor color{};
        char symbol = 0;
        int gap = 0;
    };

    static bool has_glyph_cb(void* ctx, FontSlot slot, std::uint32_t codepoint);
    bool has_any_face() const;
    std::vector<ShapedGlyph> shape_text(std::string_view text,
                                        int pixel_size,
                                        ScriptPreference pref) const;
    int measure_text_width(std::string_view text, int pixel_size, ScriptPreference pref) const;
    void draw_text(const RenderSurface& surface,
                   int x,
                   int baseline_y,
                   int pixel_size,
                   std::string_view text,
                   ScriptPreference pref,
                   RgbaColor color,
                   int max_width = 0) const;
    void draw_text_centered(const RenderSurface& surface,
                            int center_x,
                            int baseline_y,
                            int pixel_size,
                            std::string_view text,
                            ScriptPreference pref,
                            RgbaColor color,
                            int max_width) const;
    std::vector<std::string> wrap_text(std::string_view text,
                                       int pixel_size,
                                       ScriptPreference pref,
                                       int max_width) const;
    void draw_status_icon(const RenderSurface& surface,
                          int cx,
                          int cy,
                          int diameter,
                          char symbol,
                          RgbaColor color,
                          RgbaColor symbol_color) const;
    void draw_key_glyph(const RenderSurface& surface, int cx, int cy, int diameter, char key, const ThemePalette& pal) const;
    void draw_hint(const RenderSurface& surface,
                   const Rect& rect,
                   char key,
                   std::string_view label,
                   ScriptPreference pref,
                   const ThemePalette& pal) const;
    void draw_language_line(const RenderSurface& surface,
                            int center_x,
                            int baseline_y,
                            int max_width,
                            ScriptPreference ui_pref,
                            const std::string& label,
                            const LanguageEntry& entry,
                            const ThemePalette& pal) const;
    void draw_dialog_content(const RenderSurface& surface,
                             const Rect& area,
                             const std::vector<DialogItem>& items,
                             ScriptPreference ui_pref,
                             const ThemePalette& pal,
                             RgbaColor surface_color) const;
    void draw_dialog_frame(const RenderSurface& surface, const Rect& box, const ThemePalette& pal) const;
    void draw_dialog_button(const RenderSurface& surface,
                            const Rect& rect,
                            std::string_view label,
                            bool focused,
                            ScriptPreference pref,
                            const ThemePalette& pal) const;

    void render_header(const RenderSurface& surface, const PickerView& view) const;
    void render_sidebar(const RenderSurface& surface, const PickerView& view, ScriptPreference ui_pref) const;
    void render_error_screen(const RenderSurface& surface, const PickerView& view, ScriptPreference ui_pref) const;
    void render_list_view(const RenderSurface& surface, const PickerView& view, ScriptPreference ui_pref) const;
    void render_footer(const RenderSurface& surface, const PickerView& view, ScriptPreference ui_pref) const;
    void render_confirm_dialog(const RenderSurface& surface, const PickerView& view, ScriptPreference ui_pref) const;
    void render_applying_dialog(const RenderSurface& surface, const PickerView& view, ScriptPreference ui_pref) const;
    void render_result_dialog(const RenderSurface& surface, const PickerView& view, ScriptPreference ui_pref) const;

    std::uint32_t width_ = 1920;
    std::uint32_t height_ = 1080;
    std::array<FT_Face, 8> faces_{};
    std::array<hb_font_t*, 8> hb_fonts_{};
};

} // namespace kailocale
