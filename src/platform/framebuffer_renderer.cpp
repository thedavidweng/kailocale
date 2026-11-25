#include "framebuffer_renderer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "kailocale/picker_layout.hpp"

namespace kailocale {

namespace {

void fill_rect(const RenderSurface& surface, int x, int y, int w, int h, RgbaColor color) {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(static_cast<int>(surface.width), x + w);
    const int y1 = std::min(static_cast<int>(surface.height), y + h);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    if (color.a == 255) {
        const std::uint32_t px_val = color.packed();
        for (int py = y0; py < y1; ++py) {
            auto* row = reinterpret_cast<std::uint32_t*>(
                surface.buffer + static_cast<std::size_t>(py) * surface.stride);
            for (int px = x0; px < x1; ++px) {
                row[px] = px_val;
            }
        }
        return;
    }

    const std::uint32_t a = color.a;
    const std::uint32_t inv = 255u - a;
    for (int py = y0; py < y1; ++py) {
        auto* row = surface.buffer + static_cast<std::size_t>(py) * surface.stride;
        for (int px = x0; px < x1; ++px) {
            auto* dst = row + static_cast<std::size_t>(px) * 4;
            dst[0] = static_cast<std::uint8_t>((dst[0] * inv + color.r * a) / 255u);
            dst[1] = static_cast<std::uint8_t>((dst[1] * inv + color.g * a) / 255u);
            dst[2] = static_cast<std::uint8_t>((dst[2] * inv + color.b * a) / 255u);
            dst[3] = 255;
        }
    }
}

void fill_rect(const RenderSurface& surface, const Rect& r, RgbaColor color) {
    fill_rect(surface, r.x, r.y, r.w, r.h, color);
}

void draw_rect_outline(const RenderSurface& surface,
                       int x,
                       int y,
                       int w,
                       int h,
                       int thickness,
                       RgbaColor color) {
    const int t = std::max(1, thickness);
    fill_rect(surface, x, y, w, t, color);
    fill_rect(surface, x, y + h - t, w, t, color);
    fill_rect(surface, x, y, t, h, color);
    fill_rect(surface, x + w - t, y, t, h, color);
}

void draw_rect_outline(const RenderSurface& surface, const Rect& r, int thickness, RgbaColor color) {
    draw_rect_outline(surface, r.x, r.y, r.w, r.h, thickness, color);
}

void blend_pixel(const RenderSurface& surface, int x, int y, RgbaColor color, std::uint8_t coverage) {
    if (x < 0 || y < 0 || x >= static_cast<int>(surface.width) || y >= static_cast<int>(surface.height) ||
        coverage == 0) {
        return;
    }
    auto* dst = surface.buffer + static_cast<std::size_t>(y) * surface.stride + static_cast<std::size_t>(x) * 4;
    if (coverage == 255) {
        dst[0] = color.r;
        dst[1] = color.g;
        dst[2] = color.b;
        dst[3] = 255;
        return;
    }
    const std::uint32_t a = coverage;
    const std::uint32_t inv = 255u - a;
    dst[0] = static_cast<std::uint8_t>((dst[0] * inv + color.r * a) / 255u);
    dst[1] = static_cast<std::uint8_t>((dst[1] * inv + color.g * a) / 255u);
    dst[2] = static_cast<std::uint8_t>((dst[2] * inv + color.b * a) / 255u);
    dst[3] = 255;
}

// Compact 5x7 uppercase/symbol fallback bitmap font used strictly when FreeType itself failed to init.
const std::uint8_t* fallback_glyph_5x7(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        ch = static_cast<char>(ch - 'a' + 'A');
    }
    struct Entry {
        char ch;
        std::uint8_t cols[5];
    };
    static constexpr Entry kTable[] = {
        {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
        {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}},
        {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
        {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
        {'/', {0x20, 0x10, 0x08, 0x04, 0x02}},
        {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
        {'_', {0x40, 0x40, 0x40, 0x40, 0x40}},
        {'(', {0x00, 0x1C, 0x22, 0x41, 0x00}},
        {')', {0x00, 0x41, 0x22, 0x1C, 0x00}},
        {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
        {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
        {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
        {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
        {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
        {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
        {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
        {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
        {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
        {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
        {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
        {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
        {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
        {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
        {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
        {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
        {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
        {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
        {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
        {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
        {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
        {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
        {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
        {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
        {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
        {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
        {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}},
        {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
        {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
        {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
        {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
        {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
        {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}},
        {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
        {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}},
        {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
    };
    static constexpr std::uint8_t kFallbackBox[5] = {0x7F, 0x41, 0x41, 0x41, 0x7F};
    for (const auto& entry : kTable) {
        if (entry.ch == ch) {
            return entry.cols;
        }
    }
    return kFallbackBox;
}

void draw_text_fallback(const RenderSurface& surface,
                        int x,
                        int y,
                        int scale,
                        std::string_view text,
                        RgbaColor color) {
    int pen_x = x;
    const int px_scale = std::max(2, scale);
    for (char ch : text) {
        const std::uint8_t* cols = fallback_glyph_5x7(ch);
        for (int col = 0; col < 5; ++col) {
            for (int row = 0; row < 7; ++row) {
                if ((cols[col] & (1u << row)) != 0) {
                    fill_rect(surface,
                              pen_x + col * px_scale,
                              y + row * px_scale,
                              px_scale,
                              px_scale,
                              color);
                }
            }
        }
        pen_x += 6 * px_scale;
    }
}

ScriptPreference script_pref_for_ui(UiLanguage ui) {
    switch (ui) {
    case UiLanguage::ZhHans:
        return ScriptPreference::SimplifiedChinese;
    case UiLanguage::ZhHant:
        return ScriptPreference::TraditionalChinese;
    case UiLanguage::En:
    default:
        return ScriptPreference::Default;
    }
}

} // namespace

FramebufferRenderer::~FramebufferRenderer() {
    clear_fonts();
}

void FramebufferRenderer::set_dimensions(std::uint32_t width, std::uint32_t height) {
    width_ = width > 0 ? width : 1920u;
    height_ = height > 0 ? height : 1080u;
}

void FramebufferRenderer::set_font_face(FontSlot slot, FT_Face face) {
    const std::size_t idx = static_cast<std::size_t>(slot);
    if (idx >= faces_.size()) {
        return;
    }
    if (hb_fonts_[idx] != nullptr) {
        hb_font_destroy(hb_fonts_[idx]);
        hb_fonts_[idx] = nullptr;
    }
    faces_[idx] = face;
    if (face != nullptr) {
        FT_Set_Pixel_Sizes(face, 0, 28);
        hb_fonts_[idx] = hb_ft_font_create_referenced(face);
        if (hb_fonts_[idx] != nullptr) {
            hb_ft_font_set_load_flags(hb_fonts_[idx], FT_LOAD_DEFAULT);
        }
    }
}

void FramebufferRenderer::clear_fonts() {
    for (std::size_t i = 0; i < faces_.size(); ++i) {
        if (hb_fonts_[i] != nullptr) {
            hb_font_destroy(hb_fonts_[i]);
            hb_fonts_[i] = nullptr;
        }
        faces_[i] = nullptr;
    }
}

bool FramebufferRenderer::has_glyph_cb(void* ctx, FontSlot slot, std::uint32_t codepoint) {
    const auto* self = static_cast<const FramebufferRenderer*>(ctx);
    const std::size_t idx = static_cast<std::size_t>(slot);
    if (idx >= self->faces_.size() || self->faces_[idx] == nullptr) {
        return false;
    }
    return FT_Get_Char_Index(self->faces_[idx], codepoint) != 0;
}

bool FramebufferRenderer::has_any_face() const {
    for (FT_Face f : faces_) {
        if (f != nullptr) {
            return true;
        }
    }
    return false;
}

std::vector<FramebufferRenderer::ShapedGlyph> FramebufferRenderer::shape_text(
    std::string_view text,
    int pixel_size,
    ScriptPreference pref) const {
    std::vector<ShapedGlyph> glyphs;
    if (text.empty() || !has_any_face()) {
        return glyphs;
    }

    const std::vector<TextRun> runs =
        segment_text_runs(text, pref, &FramebufferRenderer::has_glyph_cb, const_cast<FramebufferRenderer*>(this));

    for (const TextRun& run : runs) {
        const std::size_t idx = static_cast<std::size_t>(run.slot);
        if (idx >= faces_.size()) {
            continue;
        }
        FT_Face face = faces_[idx];
        hb_font_t* hb_font = hb_fonts_[idx];
        if (face == nullptr || hb_font == nullptr) {
            continue;
        }
        if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size)) != 0) {
            continue;
        }
        hb_ft_font_changed(hb_font);

        hb_buffer_t* buf = hb_buffer_create();
        hb_buffer_add_utf32(buf,
                            run.codepoints.data(),
                            static_cast<int>(run.codepoints.size()),
                            0,
                            static_cast<int>(run.codepoints.size()));
        hb_buffer_guess_segment_properties(buf);
        hb_shape(hb_font, buf, nullptr, 0);

        unsigned int glyph_count = 0;
        const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buf, &glyph_count);
        const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
        if (info != nullptr && pos != nullptr) {
            glyphs.reserve(glyphs.size() + glyph_count);
            for (unsigned int i = 0; i < glyph_count; ++i) {
                // Skip .notdef (glyph index 0) gracefully when no loaded face provides a glyph.
                if (info[i].codepoint == 0) {
                    continue;
                }
                glyphs.push_back(ShapedGlyph{
                    face,
                    info[i].codepoint,
                    static_cast<int>(pos[i].x_advance >> 6),
                    static_cast<int>(pos[i].x_offset >> 6),
                    static_cast<int>(pos[i].y_offset >> 6),
                });
            }
        }
        hb_buffer_destroy(buf);
    }
    return glyphs;
}

int FramebufferRenderer::measure_text_width(std::string_view text,
                                              int pixel_size,
                                              ScriptPreference pref) const {
    if (text.empty()) {
        return 0;
    }
    if (!has_any_face()) {
        const int px_scale = std::max(2, pixel_size / 8);
        return static_cast<int>(text.size()) * 6 * px_scale;
    }

    const std::vector<ShapedGlyph> glyphs = shape_text(text, pixel_size, pref);
    int total_x = 0;
    for (const ShapedGlyph& glyph : glyphs) {
        total_x += glyph.x_advance;
    }
    return total_x;
}

void FramebufferRenderer::draw_text(const RenderSurface& surface,
                                    int x,
                                    int baseline_y,
                                    int pixel_size,
                                    std::string_view text,
                                    ScriptPreference pref,
                                    RgbaColor color,
                                    int max_width) const {
    if (text.empty()) {
        return;
    }
    if (!has_any_face()) {
        const int px_scale = std::max(2, pixel_size / 8);
        draw_text_fallback(surface, x, baseline_y - 7 * px_scale, px_scale, text, color);
        return;
    }

    const std::vector<ShapedGlyph> glyphs = shape_text(text, pixel_size, pref);
    int pen_x = x;
    const int x_limit = (max_width > 0) ? (x + max_width) : static_cast<int>(surface.width);

    for (const ShapedGlyph& glyph : glyphs) {
        if (max_width > 0 && pen_x + glyph.x_advance > x_limit) {
            break;
        }
        if (FT_Load_Glyph(glyph.face, glyph.glyph_index, FT_LOAD_RENDER | FT_LOAD_NO_BITMAP) == 0) {
            const FT_GlyphSlot slot = glyph.face->glyph;
            const FT_Bitmap& bmp = slot->bitmap;
            if (bmp.buffer != nullptr && bmp.pixel_mode == FT_PIXEL_MODE_GRAY) {
                const int gx = pen_x + glyph.x_offset + slot->bitmap_left;
                const int gy = baseline_y - glyph.y_offset - slot->bitmap_top;
                for (unsigned int row = 0; row < bmp.rows; ++row) {
                    const std::uint8_t* src_row =
                        bmp.buffer + row * static_cast<unsigned int>(std::abs(bmp.pitch));
                    for (unsigned int col = 0; col < bmp.width; ++col) {
                        blend_pixel(surface,
                                    gx + static_cast<int>(col),
                                    gy + static_cast<int>(row),
                                    color,
                                    src_row[col]);
                    }
                }
            }
        }
        pen_x += glyph.x_advance;
    }
}

void FramebufferRenderer::draw_button(const RenderSurface& surface,
                                      const Rect& rect,
                                      const ButtonStyle& style,
                                      std::string_view label,
                                      ScriptPreference pref) const {
    fill_rect(surface, rect, style.bg);
    if (style.border_thickness > 0) {
        draw_rect_outline(surface, rect, style.border_thickness, style.border);
    }
    const int pad_x = scale_px(12, surface.height);
    const int label_w = measure_text_width(label, style.pixel_size, pref);
    const int text_x = rect.x + std::max(pad_x, (rect.w - label_w) / 2);
    draw_text(surface,
              text_x,
              rect.y + style.baseline_offset,
              style.pixel_size,
              label,
              pref,
              style.text_color,
              std::max(0, rect.w - pad_x * 2));
}

void FramebufferRenderer::draw_labeled_language(const RenderSurface& surface,
                                                int x,
                                                int y,
                                                int max_w,
                                                ScriptPreference ui_pref,
                                                const std::string& label,
                                                RgbaColor label_color,
                                                const LanguageEntry& entry,
                                                RgbaColor value_color) const {
    const int sub_px = scale_px(22, surface.height);
    const int body_px = scale_px(28, surface.height);
    const std::string prefix = label + ":  ";
    const int lbl_w = measure_text_width(prefix, sub_px, ui_pref);
    const std::string val =
        (entry.native_name == entry.tag) ? entry.tag : (entry.native_name + "  [" + entry.tag + "]");
    draw_text(surface, x, y, sub_px, prefix, ui_pref, label_color);
    draw_text(surface,
              x + lbl_w,
              y,
              body_px,
              val,
              script_preference_for_tag(entry.tag),
              value_color,
              max_w - lbl_w);
}

void FramebufferRenderer::draw_notice_box(const RenderSurface& surface,
                                          const Rect& box,
                                          RgbaColor box_bg,
                                          RgbaColor box_border,
                                          RgbaColor text_color,
                                          std::string_view msg,
                                          ScriptPreference pref,
                                          int font_px) const {
    const int pad_x = scale_px(18, surface.height);
    fill_rect(surface, box, box_bg);
    draw_rect_outline(surface, box, scale_px(1, surface.height), box_border);
    draw_text(surface,
              box.x + pad_x,
              box.y + (box.h + font_px) / 2 - scale_px(4, surface.height),
              font_px,
              msg,
              pref,
              text_color,
              box.w - pad_x * 2);
}

void FramebufferRenderer::render_shell_view(std::uint8_t* buffer,
                                            std::uint32_t stride,
                                            const ShellView& view) const {
    if (buffer == nullptr) {
        return;
    }

    const RenderSurface surface{buffer, stride, width_, height_};
    const ThemePalette& pal = theme_for_system(SystemTheme::Dark);
    const RgbaColor bg{38, 14, 20, 255};
    const RgbaColor card_bg{58, 21, 30, 255};

    fill_rect(surface, 0, 0, static_cast<int>(surface.width), static_cast<int>(surface.height), bg);

    const int margin_x = static_cast<int>(surface.width / 10);
    const int margin_y = static_cast<int>(surface.height / 6);
    const int card_w = static_cast<int>(surface.width) - 2 * margin_x;
    const int card_h = static_cast<int>(surface.height) - 2 * margin_y;
    const int border = std::max(3, static_cast<int>(surface.height / 270));

    fill_rect(surface, margin_x, margin_y, card_w, card_h, card_bg);
    draw_rect_outline(surface, margin_x, margin_y, card_w, card_h, border, pal.error_border);

    const int pad_x = margin_x + std::max(40, card_w / 14);
    const int title_px = scale_px(48, surface.height);
    const int body_px = scale_px(30, surface.height);
    const int line_step = body_px + scale_px(24, surface.height);

    const std::string title = std::string(view.app_name) + " v" + view.app_version;
    char detail_line[160] = {};
    std::snprintf(detail_line,
                  sizeof(detail_line),
                  "Stage: %s   Result: 0x%08X",
                  view.stage.c_str(),
                  view.error_code);

    int y = margin_y + std::max(64, card_h / 5);
    draw_text(surface, pad_x, y, title_px, title, ScriptPreference::Default, pal.text_primary);
    y += title_px + scale_px(36, surface.height);
    draw_text(surface,
              pad_x,
              y,
              body_px,
              "Service initialization failed",
              ScriptPreference::Default,
              pal.error_text);
    y += line_step;
    draw_text(surface, pad_x, y, body_px, detail_line, ScriptPreference::Default, pal.text_secondary);
    y += line_step * 2;
    draw_text(surface,
              pad_x,
              y,
              body_px,
              "Press + (Plus) to exit",
              ScriptPreference::Default,
              pal.text_primary);
}

void FramebufferRenderer::render_header(const RenderSurface& surface,
                                        const PickerView& view,
                                        ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    fill_rect(surface, lay.header, pal.surface);
    fill_rect(surface, 0, lay.header.y + lay.header.h - sc(2), lay.header.w, sc(2), pal.divider);

    const int margin_x = lay.banner.x;
    const int title_px = sc(40);
    const int sub_px = sc(22);
    const int body_px = sc(28);
    const int small_px = sc(21);

    const std::string title = std::string(view.app_name) + "  v" + view.app_version;
    draw_text(surface, margin_x, sc(58), title_px, title, ScriptPreference::Default, pal.text_primary);
    draw_text(surface, margin_x, sc(98), sub_px, str.subtitle, ui_pref, pal.text_secondary);

    const int sub_w = measure_text_width(str.subtitle, sub_px, ui_pref);
    const int pill_w = measure_text_width(str.mode_pill, small_px, ui_pref) + sc(24);
    const Rect pill_rect{margin_x + sub_w + sc(20), sc(74), pill_w, sc(32)};
    if (pill_rect.x + pill_rect.w < static_cast<int>(surface.width) / 2) {
        fill_rect(surface, pill_rect, pal.info_bg);
        draw_rect_outline(surface, pill_rect, sc(1), pal.info_border);
        draw_text(surface,
                  pill_rect.x + sc(12),
                  pill_rect.y + sc(23),
                  small_px,
                  str.mode_pill,
                  ui_pref,
                  pal.info_text);
    }

    if (!view.active_entry.tag.empty()) {
        const std::string active_prefix = str.current_label + ": ";
        const int prefix_w = measure_text_width(active_prefix, sub_px, ui_pref);
        const ScriptPreference active_pref = script_preference_for_tag(view.active_entry.tag);
        const std::string active_value =
            view.active_entry.native_name + "  [" + view.active_entry.tag + "]";
        const int val_w = measure_text_width(active_value, body_px, active_pref);
        const int pill_pad = sc(24);
        const int active_pill_w = std::min(static_cast<int>(surface.width) / 2, prefix_w + val_w + pill_pad * 2);
        const Rect active_pill_rect{
            static_cast<int>(surface.width) - margin_x - active_pill_w,
            sc(32),
            active_pill_w,
            sc(64),
        };

        fill_rect(surface, active_pill_rect, pal.row_bg);
        draw_rect_outline(surface, active_pill_rect, sc(2), pal.applied_badge_bg);
        draw_text(surface,
                  active_pill_rect.x + pill_pad,
                  active_pill_rect.y + sc(42),
                  sub_px,
                  active_prefix,
                  ui_pref,
                  pal.text_secondary);
        draw_text(surface,
                  active_pill_rect.x + pill_pad + prefix_w,
                  active_pill_rect.y + sc(42),
                  body_px,
                  active_value,
                  active_pref,
                  pal.text_primary,
                  active_pill_rect.w - pill_pad * 2 - prefix_w);
    }
}

void FramebufferRenderer::render_error_screen(const RenderSurface& surface,
                                              const PickerView& view,
                                              ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    const int card_w = std::min(lay.banner.w, sc(1100));
    const int card_h = sc(360);
    const Rect card{(static_cast<int>(surface.width) - card_w) / 2,
                    (static_cast<int>(surface.height) - card_h) / 2,
                    card_w,
                    card_h};
    fill_rect(surface, card, pal.error_bg);
    draw_rect_outline(surface, card, sc(3), pal.error_border);

    const int tx = card.x + sc(48);
    int ty = card.y + sc(78);
    draw_text(surface, tx, ty, sc(36), str.service_error_title, ui_pref, pal.error_text);
    ty += sc(56);
    draw_text(surface, tx, ty, sc(28), str.service_error_detail, ui_pref, pal.text_primary);
    ty += sc(64);
    char code_buf[64] = {};
    std::snprintf(code_buf, sizeof(code_buf), "0x%08X", view.error_code);
    const std::string detail =
        str.stage_label + ": " + view.error_stage + "   |   " + str.result_label + ": " + code_buf;
    draw_text(surface, tx, ty, sc(22), detail, ui_pref, pal.text_secondary);
}

void FramebufferRenderer::render_list_view(const RenderSurface& surface,
                                           const PickerView& view,
                                           ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    const int sub_px = sc(22);
    const int body_px = sc(28);
    const int small_px = sc(21);
    const int border_px = sc(3);

    if (view.banner_kind != BannerKind::None && !view.banner_text.empty()) {
        const bool warn = (view.banner_kind == BannerKind::Warning);
        fill_rect(surface, lay.banner, warn ? pal.warning_bg : pal.info_bg);
        draw_rect_outline(surface, lay.banner, sc(2), warn ? pal.warning_border : pal.info_border);
        draw_text(surface,
                  lay.banner.x + sc(20),
                  lay.banner.y + sc(34),
                  sub_px,
                  view.banner_text,
                  ui_pref,
                  warn ? pal.warning_text : pal.info_text,
                  lay.banner.w - sc(40));
    }

    draw_text(surface,
              lay.section_header.x,
              lay.section_header.y + sc(24),
              small_px,
              str.supported_header,
              ui_pref,
              pal.text_secondary);
    if (!view.entries.empty()) {
        char count_buf[48] = {};
        std::snprintf(count_buf,
                      sizeof(count_buf),
                      "%zu / %zu",
                      view.focused_index + 1,
                      view.entries.size());
        const int cw = measure_text_width(count_buf, small_px, ScriptPreference::Default);
        draw_text(surface,
                  lay.list_area.x + lay.list_area.w - cw,
                  lay.section_header.y + sc(24),
                  small_px,
                  count_buf,
                  ScriptPreference::Default,
                  pal.text_secondary);
    }

    for (std::size_t i = 0; i < lay.row_rects.size(); ++i) {
        const std::size_t idx = lay.scroll_offset + i;
        if (idx >= view.entries.size()) {
            break;
        }
        const LanguageEntry& entry = view.entries[idx];
        const Rect& row = lay.row_rects[i];
        const bool focused = (idx == view.focused_index);
        const bool applied = (entry.code == view.applied_code);

        fill_rect(surface, row, focused ? pal.focus_bg : pal.row_bg);
        if (focused) {
            draw_rect_outline(surface, row, border_px, pal.focus_border);
            fill_rect(surface, row.x, row.y, sc(8), row.h, pal.focus_border);
        } else {
            draw_rect_outline(surface, row, sc(1), pal.divider);
        }

        const ScriptPreference row_pref = script_preference_for_tag(entry.tag);
        const int text_baseline = row.y + (row.h + body_px) / 2 - sc(4);
        const int label_x = row.x + sc(28);

        int right_edge = row.x + row.w - sc(24);
        if (applied) {
            const int badge_text_w = measure_text_width(str.applied_badge, small_px, ui_pref);
            const int badge_w = badge_text_w + sc(28);
            const int badge_h = sc(36);
            const Rect badge{right_edge - badge_w, row.y + (row.h - badge_h) / 2, badge_w, badge_h};
            fill_rect(surface, badge, pal.applied_badge_bg);
            draw_text(surface,
                      badge.x + sc(14),
                      badge.y + sc(25),
                      small_px,
                      str.applied_badge,
                      ui_pref,
                      pal.applied_badge_text);
            right_edge = badge.x - sc(20);
        }

        const int tag_w = measure_text_width(entry.tag, sub_px, ScriptPreference::Default);
        draw_text(surface,
                  right_edge - tag_w,
                  text_baseline,
                  sub_px,
                  entry.tag,
                  ScriptPreference::Default,
                  pal.text_secondary);

        const int max_name_w = std::max(sc(120), (right_edge - tag_w - sc(24)) - label_x);
        draw_text(surface,
                  label_x,
                  text_baseline,
                  body_px,
                  entry.native_name,
                  row_pref,
                  pal.text_primary,
                  max_name_w);
    }

    if (view.entries.size() > lay.visible_rows && lay.visible_rows > 0) {
        fill_rect(surface, lay.scrollbar_track, pal.divider);
        const std::size_t total = view.entries.size();
        const int thumb_h = std::max(
            sc(40),
            static_cast<int>((static_cast<long long>(lay.scrollbar_track.h) * lay.visible_rows) / total));
        const std::size_t max_scroll = total - lay.visible_rows;
        const int travel = lay.scrollbar_track.h - thumb_h;
        const int thumb_y =
            lay.scrollbar_track.y +
            (max_scroll > 0 ? static_cast<int>((static_cast<long long>(travel) * lay.scroll_offset) / max_scroll) : 0);
        fill_rect(surface, lay.scrollbar_track.x, thumb_y, lay.scrollbar_track.w, thumb_h, pal.accent);
    }
}

void FramebufferRenderer::render_footer(const RenderSurface& surface,
                                        const PickerView& view,
                                        ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    const int margin_x = lay.banner.x;
    const int sub_px = sc(22);

    fill_rect(surface, lay.footer, pal.surface);
    fill_rect(surface, 0, lay.footer.y, lay.footer.w, sc(2), pal.divider);

    const int hint_y = lay.footer.y + sc(56);
    if (view.screen == PickerScreen::List) {
        const std::string hints = str.hint_navigate + "     " + str.hint_select + "     " + str.hint_exit;
        draw_text(surface, margin_x, hint_y, sub_px, hints, ui_pref, pal.text_secondary);
        draw_button(surface,
                    lay.select_button,
                    {pal.button_primary_bg, {}, 0, sub_px, sc(41), pal.button_primary_text},
                    str.hint_select,
                    ui_pref);
    } else if (view.screen == PickerScreen::ConfirmDialog) {
        const std::string hints = str.hint_confirm + "     " + str.hint_cancel + "     " + str.hint_exit;
        draw_text(surface, margin_x, hint_y, sub_px, hints, ui_pref, pal.text_secondary);
    } else if (view.screen == PickerScreen::Applying) {
        draw_text(surface, margin_x, hint_y, sub_px, str.applying_title, ui_pref, pal.text_secondary);
    } else if (view.screen == PickerScreen::Result) {
        const std::string hints = str.hint_continue + "     " + str.hint_exit;
        draw_text(surface, margin_x, hint_y, sub_px, hints, ui_pref, pal.text_secondary);
    } else {
        draw_text(surface, margin_x, hint_y, sub_px, str.hint_exit, ui_pref, pal.text_secondary);
    }

    if (view.screen == PickerScreen::List ||
        view.screen == PickerScreen::Result ||
        view.screen == PickerScreen::Error) {
        draw_button(surface,
                    lay.exit_button,
                    {pal.button_secondary_bg, pal.divider, sc(1), sub_px, sc(41), pal.text_primary},
                    str.hint_exit,
                    ui_pref);
    }
}

void FramebufferRenderer::render_confirm_dialog(const RenderSurface& surface,
                                                const PickerView& view,
                                                ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    const int body_px = sc(28);
    const int small_px = sc(21);

    fill_rect(surface, 0, 0, static_cast<int>(surface.width), static_cast<int>(surface.height), pal.modal_scrim);
    fill_rect(surface, lay.dialog_box, pal.dialog_bg);
    draw_rect_outline(surface, lay.dialog_box, sc(3), pal.focus_border);

    const LanguageEntry& target = view.entries[view.dialog_target_index];
    const int dx = lay.dialog_box.x + sc(40);
    const int max_dw = lay.dialog_box.w - sc(80);
    int dy = lay.dialog_box.y + sc(66);

    draw_text(surface, dx, dy, sc(34), str.confirm_title, ui_pref, pal.text_primary, max_dw);
    dy += sc(60);

    draw_labeled_language(
        surface, dx, dy, max_dw, ui_pref, str.confirm_current_label, pal.text_secondary, view.active_entry, pal.text_primary);
    dy += sc(52);
    draw_labeled_language(
        surface, dx, dy, max_dw, ui_pref, str.confirm_target_label, pal.accent, target, pal.text_primary);
    dy += sc(48);

    draw_notice_box(
        surface, Rect{dx, dy, max_dw, sc(54)}, pal.info_bg, pal.info_border, pal.info_text, str.confirm_notice, ui_pref, small_px);

    draw_button(surface,
                lay.dialog_cancel_button,
                {pal.button_secondary_bg, pal.divider, sc(2), body_px, sc(48), pal.text_primary},
                str.cancel_button,
                ui_pref);
    draw_button(surface,
                lay.dialog_confirm_button,
                {pal.button_primary_bg, {}, 0, body_px, sc(48), pal.button_primary_text},
                str.confirm_button,
                ui_pref);
}

void FramebufferRenderer::render_applying_dialog(const RenderSurface& surface,
                                                 const PickerView& view,
                                                 ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    fill_rect(surface, 0, 0, static_cast<int>(surface.width), static_cast<int>(surface.height), pal.modal_scrim);
    fill_rect(surface, lay.result_box, pal.dialog_bg);
    draw_rect_outline(surface, lay.result_box, sc(3), pal.accent);

    const int dx = lay.result_box.x + sc(40);
    const int max_dw = lay.result_box.w - sc(80);
    int dy = lay.result_box.y + sc(78);

    draw_text(surface, dx, dy, sc(34), str.applying_title, ui_pref, pal.text_primary, max_dw);
    dy += sc(66);
    draw_labeled_language(
        surface, dx, dy, max_dw, ui_pref, str.confirm_target_label, pal.accent, view.apply_report.target_entry, pal.text_primary);
    dy += sc(66);
    draw_notice_box(
        surface, Rect{dx, dy, max_dw, sc(72)}, pal.info_bg, pal.info_border, pal.info_text, str.applying_detail, ui_pref, sc(22));
}

void FramebufferRenderer::render_result_dialog(const RenderSurface& surface,
                                               const PickerView& view,
                                               ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const ApplyReport& rep = view.apply_report;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    const int sub_px = sc(22);
    const int body_px = sc(28);
    const int small_px = sc(21);

    RgbaColor box_border = pal.error_border;
    const std::string* title_str = &str.result_rejected_title;
    if (rep.outcome == ApplyOutcome::Verified) {
        box_border = pal.applied_badge_bg;
        title_str = &str.result_verified_title;
    } else if (rep.outcome == ApplyOutcome::ReadbackMismatch) {
        box_border = pal.warning_border;
        title_str = &str.result_mismatch_title;
    } else if (rep.outcome == ApplyOutcome::ReadbackFailed) {
        box_border = pal.error_border;
        title_str = &str.result_readback_failed_title;
    }

    fill_rect(surface, 0, 0, static_cast<int>(surface.width), static_cast<int>(surface.height), pal.modal_scrim);
    fill_rect(surface, lay.result_box, pal.dialog_bg);
    draw_rect_outline(surface, lay.result_box, sc(3), box_border);
    fill_rect(surface, lay.result_box.x, lay.result_box.y, lay.result_box.w, sc(8), box_border);

    const int dx = lay.result_box.x + sc(40);
    const int max_dw = lay.result_box.w - sc(80);
    int dy = lay.result_box.y + sc(64);

    draw_text(surface, dx, dy, sc(34), *title_str, ui_pref, pal.text_primary, max_dw);
    dy += sc(56);

    if (rep.outcome == ApplyOutcome::Verified) {
        draw_labeled_language(
            surface, dx, dy, max_dw, ui_pref, str.confirm_target_label, pal.applied_badge_bg, rep.target_entry, pal.text_primary);
        dy += sc(48);

        const Rect note_box{dx, dy, max_dw, sc(108)};
        fill_rect(surface, note_box, pal.info_bg);
        draw_rect_outline(surface, note_box, sc(1), pal.info_border);
        draw_text(surface,
                  note_box.x + sc(18),
                  note_box.y + sc(40),
                  small_px,
                  str.result_reopen_apps_note,
                  ui_pref,
                  pal.info_text,
                  note_box.w - sc(36));
        draw_text(surface,
                  note_box.x + sc(18),
                  note_box.y + sc(80),
                  small_px,
                  str.result_reboot_note,
                  ui_pref,
                  pal.info_text,
                  note_box.w - sc(36));
    } else if (rep.outcome == ApplyOutcome::ReadbackMismatch) {
        draw_labeled_language(
            surface, dx, dy, max_dw, ui_pref, str.result_expected_label, pal.text_secondary, rep.target_entry, pal.text_primary);
        dy += sc(48);
        draw_labeled_language(
            surface, dx, dy, max_dw, ui_pref, str.result_actual_label, pal.warning_text, rep.actual_entry, pal.text_primary);
        dy += sc(44);
        draw_notice_box(surface,
                        Rect{dx, dy, max_dw, sc(64)},
                        pal.warning_bg,
                        pal.warning_border,
                        pal.warning_text,
                        str.result_mismatch_detail,
                        ui_pref,
                        small_px);
    } else {
        draw_labeled_language(
            surface, dx, dy, max_dw, ui_pref, str.confirm_target_label, pal.text_secondary, rep.target_entry, pal.text_primary);
        dy += sc(48);

        char code_buf[64] = {};
        std::snprintf(code_buf, sizeof(code_buf), "0x%08X", rep.error_code);
        draw_text(surface, dx, dy, sub_px, str.result_label + ": " + code_buf, ui_pref, pal.error_text, max_dw);
        dy += sc(44);

        const std::string& detail_str =
            (rep.outcome == ApplyOutcome::ReadbackFailed)
                ? str.result_readback_failed_detail
                : str.result_rejected_detail;
        draw_notice_box(surface,
                        Rect{dx, dy, max_dw, sc(64)},
                        pal.error_bg,
                        pal.error_border,
                        pal.error_text,
                        detail_str,
                        ui_pref,
                        small_px);
    }

    draw_button(surface,
                lay.result_continue_button,
                {pal.button_primary_bg, {}, 0, body_px, sc(48), pal.button_primary_text},
                str.result_continue_button,
                ui_pref);
}

void FramebufferRenderer::render_picker_view(std::uint8_t* buffer,
                                             std::uint32_t stride,
                                             const PickerView& view) const {
    if (buffer == nullptr) {
        return;
    }

    const RenderSurface surface{buffer, stride, width_, height_};
    const ScriptPreference ui_pref = script_pref_for_ui(view.strings.ui_language);

    fill_rect(surface, 0, 0, static_cast<int>(surface.width), static_cast<int>(surface.height), view.palette.background);

    render_header(surface, view, ui_pref);
    if (view.screen == PickerScreen::Error) {
        render_error_screen(surface, view, ui_pref);
    } else {
        render_list_view(surface, view, ui_pref);
    }
    render_footer(surface, view, ui_pref);

    if (view.screen == PickerScreen::ConfirmDialog && view.dialog_target_index < view.entries.size()) {
        render_confirm_dialog(surface, view, ui_pref);
    } else if (view.screen == PickerScreen::Applying) {
        render_applying_dialog(surface, view, ui_pref);
    } else if (view.screen == PickerScreen::Result) {
        render_result_dialog(surface, view, ui_pref);
    }
}

} // namespace kailocale
