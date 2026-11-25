#include "framebuffer_renderer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace kailocale {

namespace {

void fill_rect(std::uint8_t* buffer,
               std::uint32_t stride,
               std::uint32_t fb_w,
               std::uint32_t fb_h,
               int x,
               int y,
               int w,
               int h,
               RgbaColor color) {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(static_cast<int>(fb_w), x + w);
    const int y1 = std::min(static_cast<int>(fb_h), y + h);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    if (color.a == 255) {
        const std::uint32_t px_val = color.packed();
        for (int py = y0; py < y1; ++py) {
            auto* row = reinterpret_cast<std::uint32_t*>(buffer + static_cast<std::size_t>(py) * stride);
            for (int px = x0; px < x1; ++px) {
                row[px] = px_val;
            }
        }
        return;
    }

    const std::uint32_t a = color.a;
    const std::uint32_t inv = 255u - a;
    for (int py = y0; py < y1; ++py) {
        auto* row = buffer + static_cast<std::size_t>(py) * stride;
        for (int px = x0; px < x1; ++px) {
            auto* dst = row + static_cast<std::size_t>(px) * 4;
            dst[0] = static_cast<std::uint8_t>((dst[0] * inv + color.r * a) / 255u);
            dst[1] = static_cast<std::uint8_t>((dst[1] * inv + color.g * a) / 255u);
            dst[2] = static_cast<std::uint8_t>((dst[2] * inv + color.b * a) / 255u);
            dst[3] = 255;
        }
    }
}

void fill_rect(std::uint8_t* buffer,
               std::uint32_t stride,
               std::uint32_t fb_w,
               std::uint32_t fb_h,
               const Rect& r,
               RgbaColor color) {
    fill_rect(buffer, stride, fb_w, fb_h, r.x, r.y, r.w, r.h, color);
}

void draw_rect_outline(std::uint8_t* buffer,
                       std::uint32_t stride,
                       std::uint32_t fb_w,
                       std::uint32_t fb_h,
                       int x,
                       int y,
                       int w,
                       int h,
                       int thickness,
                       RgbaColor color) {
    const int t = std::max(1, thickness);
    fill_rect(buffer, stride, fb_w, fb_h, x, y, w, t, color);
    fill_rect(buffer, stride, fb_w, fb_h, x, y + h - t, w, t, color);
    fill_rect(buffer, stride, fb_w, fb_h, x, y, t, h, color);
    fill_rect(buffer, stride, fb_w, fb_h, x + w - t, y, t, h, color);
}

void draw_rect_outline(std::uint8_t* buffer,
                       std::uint32_t stride,
                       std::uint32_t fb_w,
                       std::uint32_t fb_h,
                       const Rect& r,
                       int thickness,
                       RgbaColor color) {
    draw_rect_outline(buffer, stride, fb_w, fb_h, r.x, r.y, r.w, r.h, thickness, color);
}

void blend_pixel(std::uint8_t* buffer,
                 std::uint32_t stride,
                 std::uint32_t fb_w,
                 std::uint32_t fb_h,
                 int x,
                 int y,
                 RgbaColor color,
                 std::uint8_t coverage) {
    if (x < 0 || y < 0 || x >= static_cast<int>(fb_w) || y >= static_cast<int>(fb_h) || coverage == 0) {
        return;
    }
    auto* dst = buffer + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4;
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

void draw_text_fallback(std::uint8_t* buffer,
                        std::uint32_t stride,
                        std::uint32_t fb_w,
                        std::uint32_t fb_h,
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
                    fill_rect(buffer,
                              stride,
                              fb_w,
                              fb_h,
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

int scale_px(int base_at_1080p, std::uint32_t height) {
    return std::max(1, static_cast<int>((static_cast<long long>(base_at_1080p) * height) / 1080LL));
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

    const std::vector<TextRun> runs =
        segment_text_runs(text, pref, &FramebufferRenderer::has_glyph_cb, const_cast<FramebufferRenderer*>(this));

    int total_x = 0;
    for (const TextRun& run : runs) {
        const std::size_t idx = static_cast<std::size_t>(run.slot);
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
        const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
        for (unsigned int i = 0; i < glyph_count; ++i) {
            total_x += static_cast<int>(pos[i].x_advance >> 6);
        }
        hb_buffer_destroy(buf);
    }
    return total_x;
}

void FramebufferRenderer::draw_text(std::uint8_t* buffer,
                                    std::uint32_t stride,
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
        draw_text_fallback(buffer, stride, width_, height_, x, baseline_y - 7 * px_scale, px_scale, text, color);
        return;
    }

    const std::vector<TextRun> runs =
        segment_text_runs(text, pref, &FramebufferRenderer::has_glyph_cb, const_cast<FramebufferRenderer*>(this));

    int pen_x = x;
    const int x_limit = (max_width > 0) ? (x + max_width) : static_cast<int>(width_);

    for (const TextRun& run : runs) {
        const std::size_t idx = static_cast<std::size_t>(run.slot);
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

        for (unsigned int i = 0; i < glyph_count; ++i) {
            const int adv_x = static_cast<int>(pos[i].x_advance >> 6);
            if (max_width > 0 && pen_x + adv_x > x_limit) {
                break;
            }
            if (FT_Load_Glyph(face, info[i].codepoint, FT_LOAD_RENDER) == 0) {
                const FT_GlyphSlot slot = face->glyph;
                const FT_Bitmap& bmp = slot->bitmap;
                const int gx = pen_x + static_cast<int>(pos[i].x_offset >> 6) + slot->bitmap_left;
                const int gy = baseline_y - static_cast<int>(pos[i].y_offset >> 6) - slot->bitmap_top;
                for (unsigned int row = 0; row < bmp.rows; ++row) {
                    const std::uint8_t* src_row =
                        bmp.buffer + row * static_cast<unsigned int>(std::abs(bmp.pitch));
                    for (unsigned int col = 0; col < bmp.width; ++col) {
                        blend_pixel(buffer,
                                    stride,
                                    width_,
                                    height_,
                                    gx + static_cast<int>(col),
                                    gy + static_cast<int>(row),
                                    color,
                                    src_row[col]);
                    }
                }
            }
            pen_x += adv_x;
        }
        hb_buffer_destroy(buf);
        if (max_width > 0 && pen_x >= x_limit) {
            break;
        }
    }
}

void FramebufferRenderer::render_shell_view(std::uint8_t* buffer,
                                            std::uint32_t stride,
                                            const ShellView& view) const {
    if (buffer == nullptr) {
        return;
    }

    const bool is_error = (view.kind == ShellScreenKind::InitError);
    const ThemePalette& pal = theme_for_system(SystemTheme::Dark);
    const RgbaColor bg = is_error ? RgbaColor{38, 14, 20, 255} : pal.background;
    const RgbaColor card_bg = is_error ? RgbaColor{58, 21, 30, 255} : pal.surface;
    const RgbaColor card_border = is_error ? pal.error_border : pal.accent;

    fill_rect(buffer, stride, width_, height_, 0, 0, static_cast<int>(width_), static_cast<int>(height_), bg);

    const int margin_x = static_cast<int>(width_ / 10);
    const int margin_y = static_cast<int>(height_ / 6);
    const int card_w = static_cast<int>(width_) - 2 * margin_x;
    const int card_h = static_cast<int>(height_) - 2 * margin_y;
    const int border = std::max(3, static_cast<int>(height_ / 270));

    fill_rect(buffer, stride, width_, height_, margin_x, margin_y, card_w, card_h, card_bg);
    draw_rect_outline(buffer, stride, width_, height_, margin_x, margin_y, card_w, card_h, border, card_border);

    const int pad_x = margin_x + std::max(40, card_w / 14);
    const int title_px = scale_px(48, height_);
    const int body_px = scale_px(30, height_);
    const int line_step = body_px + scale_px(24, height_);

    const std::string title = std::string(view.app_name) + " v" + view.app_version;
    char detail_line[160] = {};
    std::snprintf(detail_line,
                  sizeof(detail_line),
                  "Stage: %s   Result: 0x%08X",
                  view.stage.c_str(),
                  view.error_code);

    int y = margin_y + std::max(64, card_h / 5);
    draw_text(buffer, stride, pad_x, y, title_px, title, ScriptPreference::Default, pal.text_primary);
    y += title_px + scale_px(36, height_);
    draw_text(buffer,
              stride,
              pad_x,
              y,
              body_px,
              is_error ? "Service initialization failed" : "Switch 2 native runtime initialized",
              ScriptPreference::Default,
              is_error ? pal.error_text : pal.accent);
    y += line_step;
    draw_text(buffer, stride, pad_x, y, body_px, detail_line, ScriptPreference::Default, pal.text_secondary);
    y += line_step * 2;
    draw_text(buffer,
              stride,
              pad_x,
              y,
              body_px,
              "Press + (Plus) to exit",
              ScriptPreference::Default,
              pal.text_primary);
}

void FramebufferRenderer::render_picker_view(std::uint8_t* buffer,
                                             std::uint32_t stride,
                                             const PickerView& view) const {
    if (buffer == nullptr) {
        return;
    }

    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const ScriptPreference ui_pref = script_pref_for_ui(str.ui_language);
    const auto sc = [this](int v) { return scale_px(v, height_); };

    fill_rect(buffer, stride, width_, height_, 0, 0, static_cast<int>(width_), static_cast<int>(height_), pal.background);

    // 1. Header bar
    fill_rect(buffer, stride, width_, height_, lay.header, pal.surface);
    fill_rect(buffer,
              stride,
              width_,
              height_,
              0,
              lay.header.y + lay.header.h - sc(2),
              lay.header.w,
              sc(2),
              pal.divider);

    const int margin_x = lay.banner.x;
    const int title_px = sc(40);
    const int sub_px = sc(22);
    const int body_px = sc(28);
    const int small_px = sc(21);
    const int border_px = sc(3);

    const std::string title = std::string(view.app_name) + "  v" + view.app_version;
    draw_text(buffer, stride, margin_x, sc(58), title_px, title, ScriptPreference::Default, pal.text_primary);
    draw_text(buffer, stride, margin_x, sc(98), sub_px, str.subtitle, ui_pref, pal.text_secondary);

    // Read-only badge next to subtitle
    const int sub_w = measure_text_width(str.subtitle, sub_px, ui_pref);
    const int ro_w = measure_text_width(str.readonly_pill, small_px, ui_pref) + sc(24);
    const Rect ro_rect{margin_x + sub_w + sc(20), sc(74), ro_w, sc(32)};
    if (ro_rect.x + ro_rect.w < static_cast<int>(width_) / 2) {
        fill_rect(buffer, stride, width_, height_, ro_rect, pal.info_bg);
        draw_rect_outline(buffer, stride, width_, height_, ro_rect, sc(1), pal.info_border);
        draw_text(buffer,
                  stride,
                  ro_rect.x + sc(12),
                  ro_rect.y + sc(23),
                  small_px,
                  str.readonly_pill,
                  ui_pref,
                  pal.info_text);
    }

    // Active language pill on the right side of header
    if (!view.active_entry.tag.empty()) {
        const std::string active_prefix = str.current_label + ": ";
        const int prefix_w = measure_text_width(active_prefix, sub_px, ui_pref);
        const ScriptPreference active_pref = script_preference_for_tag(view.active_entry.tag);
        const std::string active_value =
            view.active_entry.native_name + "  [" + view.active_entry.tag + "]";
        const int val_w = measure_text_width(active_value, body_px, active_pref);
        const int pill_pad = sc(24);
        const int pill_w = std::min(static_cast<int>(width_) / 2, prefix_w + val_w + pill_pad * 2);
        const Rect pill{static_cast<int>(width_) - margin_x - pill_w, sc(32), pill_w, sc(64)};

        fill_rect(buffer, stride, width_, height_, pill, pal.row_bg);
        draw_rect_outline(buffer, stride, width_, height_, pill, sc(2), pal.applied_badge_bg);
        draw_text(buffer,
                  stride,
                  pill.x + pill_pad,
                  pill.y + sc(42),
                  sub_px,
                  active_prefix,
                  ui_pref,
                  pal.text_secondary);
        draw_text(buffer,
                  stride,
                  pill.x + pill_pad + prefix_w,
                  pill.y + sc(42),
                  body_px,
                  active_value,
                  active_pref,
                  pal.text_primary,
                  pill.w - pill_pad * 2 - prefix_w);
    }

    // 2. Error Screen (when service read fails or supported list empty)
    if (view.screen == PickerScreen::Error) {
        const int card_w = std::min(lay.banner.w, sc(1100));
        const int card_h = sc(360);
        const Rect card{(static_cast<int>(width_) - card_w) / 2,
                        (static_cast<int>(height_) - card_h) / 2,
                        card_w,
                        card_h};
        fill_rect(buffer, stride, width_, height_, card, pal.error_bg);
        draw_rect_outline(buffer, stride, width_, height_, card, border_px, pal.error_border);

        const int tx = card.x + sc(48);
        int ty = card.y + sc(78);
        draw_text(buffer, stride, tx, ty, sc(36), str.service_error_title, ui_pref, pal.error_text);
        ty += sc(56);
        draw_text(buffer, stride, tx, ty, body_px, str.service_error_detail, ui_pref, pal.text_primary);
        ty += sc(64);
        char code_buf[64] = {};
        std::snprintf(code_buf, sizeof(code_buf), "0x%08X", view.error_code);
        const std::string detail =
            str.stage_label + ": " + view.error_stage + "   |   " + str.result_label + ": " + code_buf;
        draw_text(buffer, stride, tx, ty, sub_px, detail, ui_pref, pal.text_secondary);
    } else {
        // 3. Banner (warning/info)
        if (view.banner_kind != BannerKind::None && !view.banner_text.empty()) {
            const bool warn = (view.banner_kind == BannerKind::Warning);
            fill_rect(buffer, stride, width_, height_, lay.banner, warn ? pal.warning_bg : pal.info_bg);
            draw_rect_outline(buffer,
                              stride,
                              width_,
                              height_,
                              lay.banner,
                              sc(2),
                              warn ? pal.warning_border : pal.info_border);
            draw_text(buffer,
                      stride,
                      lay.banner.x + sc(20),
                      lay.banner.y + sc(34),
                      sub_px,
                      view.banner_text,
                      ui_pref,
                      warn ? pal.warning_text : pal.info_text,
                      lay.banner.w - sc(40));
        }

        // 4. Section header & position counter
        draw_text(buffer,
                  stride,
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
            draw_text(buffer,
                      stride,
                      lay.list_area.x + lay.list_area.w - cw,
                      lay.section_header.y + sc(24),
                      small_px,
                      count_buf,
                      ScriptPreference::Default,
                      pal.text_secondary);
        }

        // 5. Scrollable language rows
        for (std::size_t i = 0; i < lay.row_rects.size(); ++i) {
            const std::size_t idx = lay.scroll_offset + i;
            if (idx >= view.entries.size()) {
                break;
            }
            const LanguageEntry& entry = view.entries[idx];
            const Rect& row = lay.row_rects[i];
            const bool focused = (idx == view.focused_index);
            const bool applied = (entry.code == view.applied_code);

            fill_rect(buffer, stride, width_, height_, row, focused ? pal.focus_bg : pal.row_bg);
            if (focused) {
                draw_rect_outline(buffer, stride, width_, height_, row, border_px, pal.focus_border);
                fill_rect(buffer, stride, width_, height_, row.x, row.y, sc(8), row.h, pal.focus_border);
            } else {
                draw_rect_outline(buffer, stride, width_, height_, row, sc(1), pal.divider);
            }

            const ScriptPreference row_pref = script_preference_for_tag(entry.tag);
            const int text_baseline = row.y + (row.h + body_px) / 2 - sc(4);
            const int label_x = row.x + sc(28);

            // Right-aligned tag and applied badge
            int right_edge = row.x + row.w - sc(24);
            if (applied) {
                const int badge_text_w = measure_text_width(str.applied_badge, small_px, ui_pref);
                const int badge_w = badge_text_w + sc(28);
                const int badge_h = sc(36);
                const Rect badge{right_edge - badge_w, row.y + (row.h - badge_h) / 2, badge_w, badge_h};
                fill_rect(buffer, stride, width_, height_, badge, pal.applied_badge_bg);
                draw_text(buffer,
                          stride,
                          badge.x + sc(14),
                          badge.y + sc(25),
                          small_px,
                          str.applied_badge,
                          ui_pref,
                          pal.applied_badge_text);
                right_edge = badge.x - sc(20);
            }

            const int tag_w = measure_text_width(entry.tag, sub_px, ScriptPreference::Default);
            draw_text(buffer,
                      stride,
                      right_edge - tag_w,
                      text_baseline,
                      sub_px,
                      entry.tag,
                      ScriptPreference::Default,
                      pal.text_secondary);

            const int max_name_w = std::max(sc(120), (right_edge - tag_w - sc(24)) - label_x);
            draw_text(buffer,
                      stride,
                      label_x,
                      text_baseline,
                      body_px,
                      entry.native_name,
                      row_pref,
                      pal.text_primary,
                      max_name_w);
        }

        // 6. Scrollbar thumb when list exceeds viewport
        if (view.entries.size() > lay.visible_rows && lay.visible_rows > 0) {
            fill_rect(buffer, stride, width_, height_, lay.scrollbar_track, pal.divider);
            const std::size_t total = view.entries.size();
            const int thumb_h = std::max(
                sc(40),
                static_cast<int>((static_cast<long long>(lay.scrollbar_track.h) * lay.visible_rows) / total));
            const std::size_t max_scroll = total - lay.visible_rows;
            const int travel = lay.scrollbar_track.h - thumb_h;
            const int thumb_y =
                lay.scrollbar_track.y +
                (max_scroll > 0 ? static_cast<int>((static_cast<long long>(travel) * lay.scroll_offset) / max_scroll) : 0);
            fill_rect(buffer,
                      stride,
                      width_,
                      height_,
                      lay.scrollbar_track.x,
                      thumb_y,
                      lay.scrollbar_track.w,
                      thumb_h,
                      pal.accent);
        }
    }

    // 7. Footer bar with contextual Nintendo button hints + touchable action pills
    fill_rect(buffer, stride, width_, height_, lay.footer, pal.surface);
    fill_rect(buffer, stride, width_, height_, 0, lay.footer.y, lay.footer.w, sc(2), pal.divider);

    const int hint_y = lay.footer.y + sc(56);
    if (view.screen == PickerScreen::List) {
        const std::string hints = str.hint_navigate + "     " + str.hint_select + "     " + str.hint_exit;
        draw_text(buffer, stride, margin_x, hint_y, sub_px, hints, ui_pref, pal.text_secondary);

        fill_rect(buffer, stride, width_, height_, lay.select_button, pal.button_primary_bg);
        const int sw = measure_text_width(str.hint_select, sub_px, ui_pref);
        draw_text(buffer,
                  stride,
                  lay.select_button.x + std::max(sc(12), (lay.select_button.w - sw) / 2),
                  lay.select_button.y + sc(41),
                  sub_px,
                  str.hint_select,
                  ui_pref,
                  pal.button_primary_text,
                  lay.select_button.w - sc(24));
    } else if (view.screen == PickerScreen::ConfirmDialog) {
        const std::string hints = str.hint_confirm + "     " + str.hint_cancel + "     " + str.hint_exit;
        draw_text(buffer, stride, margin_x, hint_y, sub_px, hints, ui_pref, pal.text_secondary);
    } else {
        draw_text(buffer, stride, margin_x, hint_y, sub_px, str.hint_exit, ui_pref, pal.text_secondary);
    }

    if (view.screen != PickerScreen::ConfirmDialog) {
        fill_rect(buffer, stride, width_, height_, lay.exit_button, pal.button_secondary_bg);
        draw_rect_outline(buffer, stride, width_, height_, lay.exit_button, sc(1), pal.divider);
        const int ew = measure_text_width(str.hint_exit, sub_px, ui_pref);
        draw_text(buffer,
                  stride,
                  lay.exit_button.x + std::max(sc(12), (lay.exit_button.w - ew) / 2),
                  lay.exit_button.y + sc(41),
                  sub_px,
                  str.hint_exit,
                  ui_pref,
                  pal.text_primary,
                  lay.exit_button.w - sc(24));
    }

    // 8. Confirmation Dialog Overlay
    if (view.screen == PickerScreen::ConfirmDialog && view.dialog_target_index < view.entries.size()) {
        fill_rect(buffer, stride, width_, height_, 0, 0, static_cast<int>(width_), static_cast<int>(height_), pal.modal_scrim);
        fill_rect(buffer, stride, width_, height_, lay.dialog_box, pal.dialog_bg);
        draw_rect_outline(buffer, stride, width_, height_, lay.dialog_box, border_px, pal.focus_border);

        const LanguageEntry& target = view.entries[view.dialog_target_index];
        const int dx = lay.dialog_box.x + sc(40);
        const int max_dw = lay.dialog_box.w - sc(80);
        int dy = lay.dialog_box.y + sc(66);

        draw_text(buffer, stride, dx, dy, sc(34), str.confirm_title, ui_pref, pal.text_primary, max_dw);
        dy += sc(60);

        // Current language row
        const std::string cur_lbl = str.confirm_current_label + ":  ";
        const int cur_lbl_w = measure_text_width(cur_lbl, sub_px, ui_pref);
        draw_text(buffer, stride, dx, dy, sub_px, cur_lbl, ui_pref, pal.text_secondary);
        draw_text(buffer,
                  stride,
                  dx + cur_lbl_w,
                  dy,
                  body_px,
                  view.active_entry.native_name + "  [" + view.active_entry.tag + "]",
                  script_preference_for_tag(view.active_entry.tag),
                  pal.text_primary,
                  max_dw - cur_lbl_w);
        dy += sc(52);

        // Target language row
        const std::string tgt_lbl = str.confirm_target_label + ":  ";
        const int tgt_lbl_w = measure_text_width(tgt_lbl, sub_px, ui_pref);
        draw_text(buffer, stride, dx, dy, sub_px, tgt_lbl, ui_pref, pal.accent);
        draw_text(buffer,
                  stride,
                  dx + tgt_lbl_w,
                  dy,
                  body_px,
                  target.native_name + "  [" + target.tag + "]",
                  script_preference_for_tag(target.tag),
                  pal.text_primary,
                  max_dw - tgt_lbl_w);
        dy += sc(48);

        // Read-only notice box inside dialog
        const Rect note_box{dx, dy, max_dw, sc(54)};
        fill_rect(buffer, stride, width_, height_, note_box, pal.info_bg);
        draw_rect_outline(buffer, stride, width_, height_, note_box, sc(1), pal.info_border);
        draw_text(buffer,
                  stride,
                  note_box.x + sc(18),
                  note_box.y + sc(35),
                  small_px,
                  str.readonly_notice,
                  ui_pref,
                  pal.info_text,
                  note_box.w - sc(36));

        // Cancel & Confirm buttons
        fill_rect(buffer, stride, width_, height_, lay.dialog_cancel_button, pal.button_secondary_bg);
        draw_rect_outline(buffer, stride, width_, height_, lay.dialog_cancel_button, sc(2), pal.divider);
        const int cancel_w = measure_text_width(str.cancel_button, body_px, ui_pref);
        draw_text(buffer,
                  stride,
                  lay.dialog_cancel_button.x + std::max(sc(12), (lay.dialog_cancel_button.w - cancel_w) / 2),
                  lay.dialog_cancel_button.y + sc(48),
                  body_px,
                  str.cancel_button,
                  ui_pref,
                  pal.text_primary,
                  lay.dialog_cancel_button.w - sc(24));

        fill_rect(buffer, stride, width_, height_, lay.dialog_confirm_button, pal.button_primary_bg);
        const int confirm_w = measure_text_width(str.confirm_button, body_px, ui_pref);
        draw_text(buffer,
                  stride,
                  lay.dialog_confirm_button.x + std::max(sc(12), (lay.dialog_confirm_button.w - confirm_w) / 2),
                  lay.dialog_confirm_button.y + sc(48),
                  body_px,
                  str.confirm_button,
                  ui_pref,
                  pal.button_primary_text,
                  lay.dialog_confirm_button.w - sc(24));
    }
}

} // namespace kailocale
