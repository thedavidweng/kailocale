#include "framebuffer_renderer.hpp"

#include <algorithm>
#include <cmath>
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

void blend_pixel(const RenderSurface& surface, int x, int y, RgbaColor color, std::uint8_t coverage) {
    if (x < 0 || y < 0 || x >= static_cast<int>(surface.width) || y >= static_cast<int>(surface.height)) {
        return;
    }
    const std::uint32_t a = (static_cast<std::uint32_t>(coverage) * color.a + 127u) / 255u;
    if (a == 0) {
        return;
    }
    auto* dst = surface.buffer + static_cast<std::size_t>(y) * surface.stride + static_cast<std::size_t>(x) * 4;
    if (a == 255) {
        dst[0] = color.r;
        dst[1] = color.g;
        dst[2] = color.b;
        dst[3] = 255;
        return;
    }
    const std::uint32_t inv = 255u - a;
    dst[0] = static_cast<std::uint8_t>((dst[0] * inv + color.r * a) / 255u);
    dst[1] = static_cast<std::uint8_t>((dst[1] * inv + color.g * a) / 255u);
    dst[2] = static_cast<std::uint8_t>((dst[2] * inv + color.b * a) / 255u);
    dst[3] = 255;
}

float clamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

std::uint8_t to_coverage(float v) {
    return static_cast<std::uint8_t>(clamp01(v) * 255.f + 0.5f);
}

RgbaColor with_alpha(RgbaColor c, std::uint8_t a) {
    c.a = a;
    return c;
}

float rounded_rect_sdf(float px, float py, const Rect& r, float radius) {
    const float hx = r.w * 0.5f;
    const float hy = r.h * 0.5f;
    const float qx = std::fabs(px - (r.x + hx)) - (hx - radius);
    const float qy = std::fabs(py - (r.y + hy)) - (hy - radius);
    const float ox = std::max(qx, 0.f);
    const float oy = std::max(qy, 0.f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - radius;
}

void fill_rounded_rect(const RenderSurface& surface, const Rect& r, int radius, RgbaColor color) {
    radius = std::min(radius, std::min(r.w, r.h) / 2);
    if (radius <= 0) {
        fill_rect(surface, r, color);
        return;
    }
    fill_rect(surface, r.x, r.y + radius, r.w, r.h - 2 * radius, color);
    for (int i = 0; i < radius; ++i) {
        for (const int py : {r.y + i, r.y + r.h - 1 - i}) {
            fill_rect(surface, r.x + radius, py, r.w - 2 * radius, 1, color);
            for (int j = 0; j < radius; ++j) {
                for (const int px : {r.x + j, r.x + r.w - 1 - j}) {
                    const float d = rounded_rect_sdf(px + 0.5f, py + 0.5f, r, static_cast<float>(radius));
                    blend_pixel(surface, px, py, color, to_coverage(0.5f - d));
                }
            }
        }
    }
}

void stroke_rounded_rect(const RenderSurface& surface, const Rect& r, int radius, int thickness, RgbaColor color) {
    radius = std::min(radius, std::min(r.w, r.h) / 2);
    const int t = std::max(1, std::min(thickness, std::min(r.w, r.h) / 2));
    const int band = std::max(radius, t);
    fill_rect(surface, r.x + band, r.y, r.w - 2 * band, t, color);
    fill_rect(surface, r.x + band, r.y + r.h - t, r.w - 2 * band, t, color);
    fill_rect(surface, r.x, r.y + band, t, r.h - 2 * band, color);
    fill_rect(surface, r.x + r.w - t, r.y + band, t, r.h - 2 * band, color);

    const Rect inner{r.x + t, r.y + t, r.w - 2 * t, r.h - 2 * t};
    const float inner_radius = static_cast<float>(std::max(0, radius - t));
    for (int i = 0; i < band; ++i) {
        for (const int py : {r.y + i, r.y + r.h - 1 - i}) {
            for (int j = 0; j < band; ++j) {
                for (const int px : {r.x + j, r.x + r.w - 1 - j}) {
                    const float fx = px + 0.5f;
                    const float fy = py + 0.5f;
                    float cov = clamp01(0.5f - rounded_rect_sdf(fx, fy, r, static_cast<float>(radius)));
                    if (inner.w > 0 && inner.h > 0) {
                        cov -= clamp01(0.5f - rounded_rect_sdf(fx, fy, inner, inner_radius));
                    }
                    blend_pixel(surface, px, py, color, to_coverage(cov));
                }
            }
        }
    }
}

void fill_circle(const RenderSurface& surface, int cx, int cy, int diameter, RgbaColor color) {
    fill_rounded_rect(surface, Rect{cx - diameter / 2, cy - diameter / 2, diameter, diameter}, diameter / 2, color);
}

void draw_segment(const RenderSurface& surface,
                  float x0,
                  float y0,
                  float x1,
                  float y1,
                  float thickness,
                  RgbaColor color) {
    const float half = thickness * 0.5f;
    const int bx0 = static_cast<int>(std::floor(std::min(x0, x1) - half - 1.f));
    const int by0 = static_cast<int>(std::floor(std::min(y0, y1) - half - 1.f));
    const int bx1 = static_cast<int>(std::ceil(std::max(x0, x1) + half + 1.f));
    const int by1 = static_cast<int>(std::ceil(std::max(y0, y1) + half + 1.f));
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len2 = dx * dx + dy * dy;
    for (int py = by0; py < by1; ++py) {
        for (int px = bx0; px < bx1; ++px) {
            const float fx = px + 0.5f;
            const float fy = py + 0.5f;
            const float t = len2 > 0.f ? clamp01(((fx - x0) * dx + (fy - y0) * dy) / len2) : 0.f;
            const float ex = fx - (x0 + t * dx);
            const float ey = fy - (y0 + t * dy);
            blend_pixel(surface, px, py, color, to_coverage(half - std::sqrt(ex * ex + ey * ey) + 0.5f));
        }
    }
}

void draw_ellipse_ring(const RenderSurface& surface,
                       float cx,
                       float cy,
                       float a,
                       float b,
                       float thickness,
                       RgbaColor color) {
    const float half = thickness * 0.5f;
    const int bx0 = static_cast<int>(std::floor(cx - a - half - 1.f));
    const int by0 = static_cast<int>(std::floor(cy - b - half - 1.f));
    const int bx1 = static_cast<int>(std::ceil(cx + a + half + 1.f));
    const int by1 = static_cast<int>(std::ceil(cy + b + half + 1.f));
    for (int py = by0; py < by1; ++py) {
        for (int px = bx0; px < bx1; ++px) {
            const float nx = (px + 0.5f - cx) / a;
            const float ny = (py + 0.5f - cy) / b;
            const float k = std::sqrt(nx * nx + ny * ny);
            if (k < 1e-4f) {
                continue;
            }
            // First-order distance to the ellipse: implicit value divided by its gradient length.
            const float gx = nx / (a * k);
            const float gy = ny / (b * k);
            const float d = (k - 1.f) / std::sqrt(gx * gx + gy * gy);
            blend_pixel(surface, px, py, color, to_coverage(half - std::fabs(d) + 0.5f));
        }
    }
}

void draw_check_mark(const RenderSurface& surface, int x, int y, int size, RgbaColor color) {
    const float s = static_cast<float>(size);
    const float t = std::max(2.f, s * 0.15f);
    draw_segment(surface, x + s * 0.08f, y + s * 0.52f, x + s * 0.38f, y + s * 0.82f, t, color);
    draw_segment(surface, x + s * 0.38f, y + s * 0.82f, x + s * 0.94f, y + s * 0.2f, t, color);
}

void draw_globe_icon(const RenderSurface& surface, int cx, int cy, int radius, int thickness, RgbaColor color) {
    const float r = static_cast<float>(radius);
    const float t = static_cast<float>(thickness);
    const float fx = static_cast<float>(cx);
    const float fy = static_cast<float>(cy);
    draw_ellipse_ring(surface, fx, fy, r, r, t, color);
    draw_ellipse_ring(surface, fx, fy, r * 0.42f, r, t, color);
    draw_segment(surface, fx - r, fy, fx + r, fy, t, color);
    draw_segment(surface, fx - r * 0.86f, fy - r * 0.5f, fx + r * 0.86f, fy - r * 0.5f, t, color);
    draw_segment(surface, fx - r * 0.86f, fy + r * 0.5f, fx + r * 0.86f, fy + r * 0.5f, t, color);
}

int centered_baseline(int y, int h, int pixel_size) {
    return y + (h + pixel_size * 7 / 10) / 2;
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

void FramebufferRenderer::draw_text_centered(const RenderSurface& surface,
                                             int center_x,
                                             int baseline_y,
                                             int pixel_size,
                                             std::string_view text,
                                             ScriptPreference pref,
                                             RgbaColor color,
                                             int max_width) const {
    const int w = std::min(measure_text_width(text, pixel_size, pref), max_width);
    draw_text(surface, center_x - w / 2, baseline_y, pixel_size, text, pref, color, max_width);
}

std::vector<std::string> FramebufferRenderer::wrap_text(std::string_view text,
                                                        int pixel_size,
                                                        ScriptPreference pref,
                                                        int max_width) const {
    std::vector<std::string> lines;
    std::size_t start = 0;
    std::size_t last_space = std::string_view::npos;
    std::size_t i = 0;
    while (i < text.size()) {
        std::size_t next = i + 1;
        while (next < text.size() && (static_cast<unsigned char>(text[next]) & 0xC0u) == 0x80u) {
            ++next;
        }
        if (text[i] == ' ') {
            last_space = i;
        }
        if (i > start && measure_text_width(text.substr(start, next - start), pixel_size, pref) > max_width) {
            const bool at_space = last_space != std::string_view::npos && last_space > start;
            const std::size_t brk = at_space ? last_space : i;
            lines.emplace_back(text.substr(start, brk - start));
            start = at_space ? brk + 1 : brk;
            last_space = std::string_view::npos;
            i = start;
            continue;
        }
        i = next;
    }
    if (start < text.size()) {
        lines.emplace_back(text.substr(start));
    }
    return lines;
}

void FramebufferRenderer::draw_status_icon(const RenderSurface& surface,
                                           int cx,
                                           int cy,
                                           int diameter,
                                           char symbol,
                                           RgbaColor color,
                                           RgbaColor symbol_color) const {
    const float d = static_cast<float>(diameter);
    const float fx = static_cast<float>(cx);
    const float fy = static_cast<float>(cy);
    fill_circle(surface, cx, cy, diameter, color);
    if (symbol == 'v') {
        const int size = static_cast<int>(d * 0.56f);
        draw_check_mark(surface, cx - size / 2, cy - size / 2, size, symbol_color);
    } else if (symbol == '!') {
        draw_segment(surface, fx, fy - d * 0.25f, fx, fy + d * 0.05f, d * 0.12f, symbol_color);
        fill_circle(surface, cx, cy + static_cast<int>(d * 0.24f), std::max(2, static_cast<int>(d * 0.14f)), symbol_color);
    } else if (symbol == 'i') {
        fill_circle(surface, cx, cy - static_cast<int>(d * 0.24f), std::max(2, static_cast<int>(d * 0.14f)), symbol_color);
        draw_segment(surface, fx, fy - d * 0.05f, fx, fy + d * 0.25f, d * 0.12f, symbol_color);
    }
}

void FramebufferRenderer::draw_key_glyph(const RenderSurface& surface,
                                         int cx,
                                         int cy,
                                         int diameter,
                                         char key,
                                         const ThemePalette& pal) const {
    fill_circle(surface, cx, cy, diameter, pal.glyph_bg);
    const float d = static_cast<float>(diameter);
    if (key == '+') {
        const float fx = static_cast<float>(cx);
        const float fy = static_cast<float>(cy);
        draw_segment(surface, fx - d * 0.26f, fy, fx + d * 0.26f, fy, d * 0.12f, pal.glyph_fg);
        draw_segment(surface, fx, fy - d * 0.26f, fx, fy + d * 0.26f, d * 0.12f, pal.glyph_fg);
        return;
    }
    const char label[2] = {key, '\0'};
    const int px = static_cast<int>(d * 0.66f);
    const int w = measure_text_width(label, px, ScriptPreference::Default);
    draw_text(surface, cx - w / 2, cy + px * 36 / 100, px, label, ScriptPreference::Default, pal.glyph_fg);
}

void FramebufferRenderer::draw_hint(const RenderSurface& surface,
                                    const Rect& rect,
                                    char key,
                                    std::string_view label,
                                    ScriptPreference pref,
                                    const ThemePalette& pal) const {
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const int d = sc(36);
    const int gap = sc(10);
    const int px = sc(28);
    const int label_w = std::min(measure_text_width(label, px, pref), rect.w - d - gap);
    const int x0 = rect.x + std::max(0, (rect.w - d - gap - label_w) / 2);
    draw_key_glyph(surface, x0 + d / 2, rect.y + rect.h / 2, d, key, pal);
    draw_text(surface, x0 + d + gap, centered_baseline(rect.y, rect.h, px), px, label, pref, pal.text_primary, label_w);
}

void FramebufferRenderer::draw_language_line(const RenderSurface& surface,
                                             int center_x,
                                             int baseline_y,
                                             int max_width,
                                             ScriptPreference ui_pref,
                                             const std::string& label,
                                             const LanguageEntry& entry,
                                             const ThemePalette& pal) const {
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const int label_px = sc(26);
    const int name_px = sc(30);
    const int tag_px = sc(24);
    const int space = sc(18);
    const ScriptPreference name_pref = script_preference_for_tag(entry.tag);
    const bool show_tag = entry.native_name != entry.tag;

    const int label_w = measure_text_width(label, label_px, ui_pref);
    const int tag_w = show_tag ? measure_text_width(entry.tag, tag_px, ScriptPreference::Default) : 0;
    const int fixed_w = label_w + space + (show_tag ? space + tag_w : 0);
    const int name_w = std::min(measure_text_width(entry.native_name, name_px, name_pref),
                                std::max(0, max_width - fixed_w));

    int x = center_x - (fixed_w + name_w) / 2;
    draw_text(surface, x, baseline_y, label_px, label, ui_pref, pal.text_secondary);
    x += label_w + space;
    draw_text(surface, x, baseline_y, name_px, entry.native_name, name_pref, pal.text_primary, name_w);
    if (show_tag) {
        x += name_w + space;
        draw_text(surface, x, baseline_y, tag_px, entry.tag, ScriptPreference::Default, pal.text_faint);
    }
}

void FramebufferRenderer::draw_dialog_content(const RenderSurface& surface,
                                              const Rect& area,
                                              const std::vector<DialogItem>& items,
                                              ScriptPreference ui_pref,
                                              const ThemePalette& pal,
                                              RgbaColor surface_color) const {
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const int icon_d = sc(72);
    const int title_px = sc(36);
    const int title_lh = sc(52);
    const int lang_lh = sc(48);
    const int note_px = sc(24);
    const int note_lh = sc(36);
    const int max_w = area.w - 2 * sc(56);
    const int cx = area.x + area.w / 2;

    std::vector<std::vector<std::string>> wrapped(items.size());
    int total_h = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const DialogItem& item = items[i];
        switch (item.kind) {
        case DialogItem::Kind::Icon:
            total_h += icon_d + sc(24);
            break;
        case DialogItem::Kind::Title:
            wrapped[i] = wrap_text(item.text, title_px, ui_pref, max_w);
            total_h += static_cast<int>(wrapped[i].size()) * title_lh;
            break;
        case DialogItem::Kind::Language:
            total_h += lang_lh;
            break;
        case DialogItem::Kind::Note:
            wrapped[i] = wrap_text(item.text, note_px, ui_pref, max_w);
            total_h += static_cast<int>(wrapped[i].size()) * note_lh;
            break;
        case DialogItem::Kind::Gap:
            total_h += item.gap;
            break;
        }
    }

    int y = area.y + std::max(sc(24), (area.h - total_h) / 2);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const DialogItem& item = items[i];
        switch (item.kind) {
        case DialogItem::Kind::Icon:
            draw_status_icon(surface, cx, y + icon_d / 2, icon_d, item.symbol, item.color, surface_color);
            y += icon_d + sc(24);
            break;
        case DialogItem::Kind::Title:
            for (const std::string& line : wrapped[i]) {
                draw_text_centered(surface, cx, centered_baseline(y, title_lh, title_px), title_px, line, ui_pref, pal.text_primary, max_w);
                y += title_lh;
            }
            break;
        case DialogItem::Kind::Language:
            draw_language_line(surface, cx, centered_baseline(y, lang_lh, sc(30)), max_w, ui_pref, item.text, *item.entry, pal);
            y += lang_lh;
            break;
        case DialogItem::Kind::Note:
            for (const std::string& line : wrapped[i]) {
                draw_text_centered(surface, cx, centered_baseline(y, note_lh, note_px), note_px, line, ui_pref, item.color, max_w);
                y += note_lh;
            }
            break;
        case DialogItem::Kind::Gap:
            y += item.gap;
            break;
        }
    }
}

void FramebufferRenderer::draw_dialog_frame(const RenderSurface& surface, const Rect& box, const ThemePalette& pal) const {
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    fill_rect(surface, 0, 0, static_cast<int>(surface.width), static_cast<int>(surface.height), pal.modal_scrim);
    const int radius = sc(10);
    const int spread = sc(18);
    for (int i = 1; i <= spread; ++i) {
        const float k = 1.f - static_cast<float>(i) / static_cast<float>(spread + 1);
        const Rect ring{box.x - i, box.y - i + sc(4), box.w + 2 * i, box.h + 2 * i};
        stroke_rounded_rect(surface, ring, radius + i, 1, with_alpha(pal.highlight_shadow, static_cast<std::uint8_t>(60.f * k * k)));
    }
    fill_rounded_rect(surface, box, radius, pal.dialog_bg);
}

void FramebufferRenderer::draw_dialog_button(const RenderSurface& surface,
                                             const Rect& rect,
                                             std::string_view label,
                                             bool focused,
                                             ScriptPreference pref,
                                             const ThemePalette& pal) const {
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    if (focused) {
        const int inset = sc(8);
        const Rect frame{rect.x + inset, rect.y + inset, rect.w - 2 * inset, rect.h - 2 * inset};
        fill_rounded_rect(surface, frame, sc(6), pal.highlight_bg);
        stroke_rounded_rect(surface, frame, sc(6), sc(5), pal.highlight);
    }
    const int px = sc(30);
    draw_text_centered(surface, rect.x + rect.w / 2, centered_baseline(rect.y, rect.h, px), px, label, pref, pal.accent, rect.w - sc(48));
}

void FramebufferRenderer::render_shell_view(std::uint8_t* buffer,
                                            std::uint32_t stride,
                                            const ShellView& view) const {
    if (buffer == nullptr) {
        return;
    }

    const RenderSurface surface{buffer, stride, width_, height_};
    const ThemePalette& pal = theme_for_system(SystemTheme::Dark);
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const int w = static_cast<int>(surface.width);
    const int h = static_cast<int>(surface.height);
    const int rule_t = std::max(1, sc(1));

    fill_rect(surface, 0, 0, w, h, pal.background);

    draw_globe_icon(surface, sc(92), sc(72), sc(25), std::max(2, sc(3)), pal.text_primary);
    draw_text(surface, sc(140), sc(88), sc(42), view.app_name, ScriptPreference::Default, pal.text_primary);
    const std::string version = std::string("v") + view.app_version;
    const int version_px = sc(24);
    const int version_w = measure_text_width(version, version_px, ScriptPreference::Default);
    draw_text(surface, w - sc(66) - version_w, sc(88), version_px, version, ScriptPreference::Default, pal.text_faint);
    fill_rect(surface, sc(45), sc(132) - rule_t, w - sc(90), rule_t, pal.header_rule);

    char detail_line[160] = {};
    std::snprintf(detail_line, sizeof(detail_line), "Stage: %s   Result: 0x%08X", view.stage.c_str(), view.error_code);

    const int cx = w / 2;
    const int cy = h / 2;
    draw_status_icon(surface, cx, cy - sc(110), sc(72), '!', pal.error, pal.background);
    const int title_px = sc(36);
    const int body_px = sc(26);
    draw_text_centered(surface, cx, cy + sc(12), title_px, "Service initialization failed", ScriptPreference::Default, pal.text_primary, w);
    draw_text_centered(surface, cx, cy + sc(70), body_px, detail_line, ScriptPreference::Default, pal.text_secondary, w);

    const int footer_y = h - sc(108);
    fill_rect(surface, sc(45), footer_y, w - sc(90), rule_t, pal.header_rule);
    draw_hint(surface, Rect{w - sc(66) - sc(190), footer_y + sc(18), sc(190), sc(72)}, '+', "Exit", ScriptPreference::Default, pal);
}

void FramebufferRenderer::render_header(const RenderSurface& surface, const PickerView& view) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const int w = static_cast<int>(surface.width);
    const int rule_t = std::max(1, sc(1));

    draw_globe_icon(surface, sc(92), sc(72), sc(25), std::max(2, sc(3)), pal.text_primary);
    draw_text(surface, sc(140), sc(88), sc(42), view.app_name, ScriptPreference::Default, pal.text_primary);

    const std::string version = std::string("v") + view.app_version;
    const int version_px = sc(24);
    const int version_w = measure_text_width(version, version_px, ScriptPreference::Default);
    draw_text(surface, w - sc(66) - version_w, sc(88), version_px, version, ScriptPreference::Default, pal.text_faint);

    fill_rect(surface, sc(45), lay.header.y + lay.header.h - rule_t, w - sc(90), rule_t, pal.header_rule);
}

void FramebufferRenderer::render_sidebar(const RenderSurface& surface,
                                         const PickerView& view,
                                         ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const int rule_t = std::max(1, sc(1));
    const Rect& tab = lay.sidebar_tab;

    fill_rect(surface, lay.sidebar, pal.sidebar);

    fill_rounded_rect(surface, Rect{tab.x, tab.y + sc(16), sc(6), tab.h - sc(32)}, sc(3), pal.accent);
    const int tab_px = sc(30);
    draw_text(surface, tab.x + sc(26), centered_baseline(tab.y, tab.h, tab_px), tab_px, str.subtitle, ui_pref, pal.accent, tab.w - sc(30));

    const int rule_y = tab.y + tab.h + sc(24);
    fill_rect(surface, tab.x, rule_y, tab.w, rule_t, pal.list_rule);

    int y = rule_y + sc(64);
    if (!view.active_entry.tag.empty()) {
        draw_text(surface, tab.x, y, sc(24), str.current_label, ui_pref, pal.text_secondary, tab.w);
        y += sc(54);
        draw_text(surface,
                  tab.x,
                  y,
                  sc(34),
                  view.active_entry.native_name,
                  script_preference_for_tag(view.active_entry.tag),
                  pal.text_primary,
                  tab.w);
        if (view.active_entry.native_name != view.active_entry.tag) {
            y += sc(42);
            draw_text(surface, tab.x, y, sc(24), view.active_entry.tag, ScriptPreference::Default, pal.text_faint, tab.w);
        }
        y += sc(60);
    }
    for (const std::string& line : wrap_text(str.apply_note, sc(24), ui_pref, tab.w)) {
        draw_text(surface, tab.x, y, sc(24), line, ui_pref, pal.text_secondary, tab.w);
        y += sc(34);
    }

    if (view.banner_kind != BannerKind::None && !view.banner_text.empty()) {
        const Rect& banner = lay.banner;
        const bool warn = view.banner_kind == BannerKind::Warning;
        fill_rect(surface, banner.x, banner.y, banner.w, rule_t, pal.list_rule);
        const int icon_d = sc(32);
        const int text_x = banner.x + icon_d + sc(16);
        const int line_h = sc(34);
        const int text_px = sc(24);
        const int top = banner.y + sc(28);
        draw_status_icon(surface, banner.x + icon_d / 2, top + line_h / 2, icon_d, warn ? '!' : 'i', warn ? pal.warning : pal.accent, pal.sidebar);
        int line_top = top;
        for (const std::string& line : wrap_text(view.banner_text, text_px, ui_pref, banner.x + banner.w - text_x)) {
            draw_text(surface, text_x, centered_baseline(line_top, line_h, text_px), text_px, line, ui_pref, pal.text_primary);
            line_top += line_h;
        }
    }
}

void FramebufferRenderer::render_error_screen(const RenderSurface& surface,
                                              const PickerView& view,
                                              ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    char code_buf[64] = {};
    std::snprintf(code_buf, sizeof(code_buf), "0x%08X", view.error_code);

    std::vector<DialogItem> items;
    items.push_back({DialogItem::Kind::Icon, {}, nullptr, pal.error, '!', 0});
    items.push_back({DialogItem::Kind::Title, str.service_error_title, nullptr, {}, 0, 0});
    items.push_back({DialogItem::Kind::Gap, {}, nullptr, {}, 0, sc(8)});
    items.push_back({DialogItem::Kind::Note, str.service_error_detail, nullptr, pal.text_secondary, 0, 0});
    items.push_back({DialogItem::Kind::Gap, {}, nullptr, {}, 0, sc(8)});
    items.push_back({DialogItem::Kind::Note,
                     str.stage_label + ": " + view.error_stage + "    " + str.result_label + ": " + code_buf,
                     nullptr,
                     pal.text_faint,
                     0,
                     0});

    const Rect area{lay.list_area.x, lay.section_header.y, lay.list_area.w, lay.list_area.y + lay.list_area.h - lay.section_header.y};
    draw_dialog_content(surface, area, items, ui_pref, pal, pal.background);
}

void FramebufferRenderer::render_list_view(const RenderSurface& surface,
                                           const PickerView& view,
                                           ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };

    const int rule_t = std::max(1, sc(1));
    const int text_inset = sc(30);
    const int small_px = sc(24);
    const int name_px = sc(30);
    const int tag_px = sc(26);

    const int header_baseline = lay.section_header.y + sc(34);
    draw_text(surface, lay.list_area.x + text_inset, header_baseline, small_px, str.supported_header, ui_pref, pal.text_secondary);
    if (!view.entries.empty()) {
        char count_buf[48] = {};
        std::snprintf(count_buf, sizeof(count_buf), "%zu / %zu", view.focused_index + 1, view.entries.size());
        const int cw = measure_text_width(count_buf, small_px, ScriptPreference::Default);
        draw_text(surface,
                  lay.list_area.x + lay.list_area.w - text_inset - cw,
                  header_baseline,
                  small_px,
                  count_buf,
                  ScriptPreference::Default,
                  pal.text_faint);
    }

    for (const Rect& row : lay.row_rects) {
        fill_rect(surface, row.x, row.y, row.w, rule_t, pal.list_rule);
    }
    if (!lay.row_rects.empty()) {
        const Rect& last = lay.row_rects.back();
        fill_rect(surface, last.x, last.y + last.h, last.w, rule_t, pal.list_rule);
    }

    for (std::size_t i = 0; i < lay.row_rects.size(); ++i) {
        const std::size_t idx = lay.scroll_offset + i;
        if (idx >= view.entries.size()) {
            break;
        }
        const LanguageEntry& entry = view.entries[idx];
        const Rect& row = lay.row_rects[i];
        const bool applied = entry.code == view.applied_code;

        if (idx == view.focused_index) {
            const Rect frame{row.x - sc(4), row.y - sc(2), row.w + sc(8), row.h + sc(4)};
            const int radius = sc(4);
            const int spread = sc(12);
            const float peak = view.theme == SystemTheme::Light ? 34.f : 80.f;
            for (int s = 1; s <= spread; ++s) {
                const float k = 1.f - static_cast<float>(s) / static_cast<float>(spread + 1);
                const Rect ring{frame.x - s, frame.y - s + sc(2), frame.w + 2 * s, frame.h + 2 * s};
                stroke_rounded_rect(surface, ring, radius + s, 1, with_alpha(pal.highlight_shadow, static_cast<std::uint8_t>(peak * k * k)));
            }
            fill_rounded_rect(surface, frame, radius, pal.highlight_bg);
            stroke_rounded_rect(surface, frame, radius, sc(5), pal.highlight);
        }

        int right_edge = row.x + row.w - text_inset;
        if (applied) {
            const int check = sc(30);
            draw_check_mark(surface, right_edge - check, row.y + (row.h - check) / 2, check, pal.accent);
            right_edge -= check + sc(22);
        }

        const int tag_w = measure_text_width(entry.tag, tag_px, ScriptPreference::Default);
        if (entry.native_name != entry.tag) {
            draw_text(surface, right_edge - tag_w, centered_baseline(row.y, row.h, tag_px), tag_px, entry.tag, ScriptPreference::Default, pal.text_faint);
        }

        const int label_x = row.x + text_inset;
        const int max_name_w = std::max(sc(120), right_edge - tag_w - sc(24) - label_x);
        draw_text(surface,
                  label_x,
                  centered_baseline(row.y, row.h, name_px),
                  name_px,
                  entry.native_name,
                  script_preference_for_tag(entry.tag),
                  applied ? pal.accent : pal.text_primary,
                  max_name_w);
    }

    if (view.entries.size() > lay.visible_rows && lay.visible_rows > 0) {
        const Rect& track = lay.scrollbar_track;
        const std::size_t total = view.entries.size();
        const int thumb_h = std::max(
            sc(48),
            static_cast<int>((static_cast<long long>(track.h) * lay.visible_rows) / total));
        const std::size_t max_scroll = total - lay.visible_rows;
        const int travel = track.h - thumb_h;
        const int thumb_y =
            track.y + (max_scroll > 0 ? static_cast<int>((static_cast<long long>(travel) * lay.scroll_offset) / max_scroll) : 0);
        fill_rounded_rect(surface, Rect{track.x, thumb_y, track.w, thumb_h}, track.w / 2, pal.text_faint);
    }
}

void FramebufferRenderer::render_footer(const RenderSurface& surface,
                                        const PickerView& view,
                                        ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const int rule_t = std::max(1, sc(1));

    fill_rect(surface, sc(45), lay.footer.y, lay.footer.w - sc(90), rule_t, pal.header_rule);

    switch (view.screen) {
    case PickerScreen::List:
        draw_hint(surface, lay.select_button, 'A', str.hint_select, ui_pref, pal);
        draw_hint(surface, lay.exit_button, '+', str.hint_exit, ui_pref, pal);
        break;
    case PickerScreen::ConfirmDialog:
        draw_hint(surface, lay.select_button, 'A', str.confirm_button, ui_pref, pal);
        draw_hint(surface, lay.exit_button, 'B', str.cancel_button, ui_pref, pal);
        break;
    case PickerScreen::Result:
        draw_hint(surface, lay.select_button, 'A', str.result_continue_button, ui_pref, pal);
        draw_hint(surface, lay.exit_button, '+', str.hint_exit, ui_pref, pal);
        break;
    case PickerScreen::Error:
        draw_hint(surface, lay.exit_button, '+', str.hint_exit, ui_pref, pal);
        break;
    case PickerScreen::Applying:
        break;
    }
}

void FramebufferRenderer::render_confirm_dialog(const RenderSurface& surface,
                                                const PickerView& view,
                                                ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const Rect& box = lay.dialog_box;
    const Rect& confirm = lay.dialog_confirm_button;
    const int rule_t = std::max(1, sc(1));
    const RgbaColor rule = with_alpha(pal.text_faint, 110);

    draw_dialog_frame(surface, box, pal);

    const LanguageEntry& target = view.entries[view.dialog_target_index];
    const std::vector<DialogItem> items{
        {DialogItem::Kind::Title, str.confirm_title, nullptr, {}, 0, 0},
        {DialogItem::Kind::Gap, {}, nullptr, {}, 0, sc(16)},
        {DialogItem::Kind::Language, str.confirm_current_label, &view.active_entry, {}, 0, 0},
        {DialogItem::Kind::Language, str.confirm_target_label, &target, {}, 0, 0},
        {DialogItem::Kind::Gap, {}, nullptr, {}, 0, sc(12)},
        {DialogItem::Kind::Note, str.confirm_notice, nullptr, pal.text_secondary, 0, 0},
    };
    draw_dialog_content(surface, Rect{box.x, box.y, box.w, confirm.y - box.y}, items, ui_pref, pal, pal.dialog_bg);

    fill_rect(surface, box.x, confirm.y, box.w, rule_t, rule);
    fill_rect(surface, confirm.x, confirm.y + sc(22), rule_t, confirm.h - sc(44), rule);
    draw_dialog_button(surface, lay.dialog_cancel_button, str.cancel_button, false, ui_pref, pal);
    draw_dialog_button(surface, confirm, str.confirm_button, true, ui_pref, pal);
}

void FramebufferRenderer::render_applying_dialog(const RenderSurface& surface,
                                                 const PickerView& view,
                                                 ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const LocalizedStrings& str = view.strings;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const Rect& box = view.layout.result_box;

    draw_dialog_frame(surface, box, pal);
    const std::vector<DialogItem> items{
        {DialogItem::Kind::Title, str.applying_title, nullptr, {}, 0, 0},
        {DialogItem::Kind::Gap, {}, nullptr, {}, 0, sc(16)},
        {DialogItem::Kind::Language, str.confirm_target_label, &view.apply_report.target_entry, {}, 0, 0},
        {DialogItem::Kind::Gap, {}, nullptr, {}, 0, sc(12)},
        {DialogItem::Kind::Note, str.applying_detail, nullptr, pal.text_secondary, 0, 0},
    };
    draw_dialog_content(surface, box, items, ui_pref, pal, pal.dialog_bg);
}

void FramebufferRenderer::render_result_dialog(const RenderSurface& surface,
                                               const PickerView& view,
                                               ScriptPreference ui_pref) const {
    const ThemePalette& pal = view.palette;
    const PickerLayout& lay = view.layout;
    const LocalizedStrings& str = view.strings;
    const ApplyReport& rep = view.apply_report;
    const auto sc = [&surface](int v) { return scale_px(v, surface.height); };
    const Rect& box = lay.result_box;
    const Rect& button = lay.result_continue_button;
    const int rule_t = std::max(1, sc(1));

    draw_dialog_frame(surface, box, pal);

    const DialogItem gap{DialogItem::Kind::Gap, {}, nullptr, {}, 0, sc(14)};
    std::vector<DialogItem> items;
    if (rep.outcome == ApplyOutcome::Verified) {
        items = {
            {DialogItem::Kind::Icon, {}, nullptr, pal.accent, 'v', 0},
            {DialogItem::Kind::Title, str.result_verified_title, nullptr, {}, 0, 0},
            gap,
            {DialogItem::Kind::Language, str.confirm_target_label, &rep.target_entry, {}, 0, 0},
            gap,
            {DialogItem::Kind::Note, str.result_reopen_apps_note, nullptr, pal.text_secondary, 0, 0},
            {DialogItem::Kind::Note, str.result_reboot_note, nullptr, pal.text_secondary, 0, 0},
        };
    } else if (rep.outcome == ApplyOutcome::ReadbackMismatch) {
        items = {
            {DialogItem::Kind::Icon, {}, nullptr, pal.warning, '!', 0},
            {DialogItem::Kind::Title, str.result_mismatch_title, nullptr, {}, 0, 0},
            gap,
            {DialogItem::Kind::Language, str.result_expected_label, &rep.target_entry, {}, 0, 0},
            {DialogItem::Kind::Language, str.result_actual_label, &rep.actual_entry, {}, 0, 0},
            gap,
            {DialogItem::Kind::Note, str.result_mismatch_detail, nullptr, pal.text_secondary, 0, 0},
        };
    } else {
        char code_buf[64] = {};
        std::snprintf(code_buf, sizeof(code_buf), "0x%08X", rep.error_code);
        const bool readback_failed = rep.outcome == ApplyOutcome::ReadbackFailed;
        items = {
            {DialogItem::Kind::Icon, {}, nullptr, pal.error, '!', 0},
            {DialogItem::Kind::Title, readback_failed ? str.result_readback_failed_title : str.result_rejected_title, nullptr, {}, 0, 0},
            gap,
            {DialogItem::Kind::Language, str.confirm_target_label, &rep.target_entry, {}, 0, 0},
            gap,
            {DialogItem::Kind::Note,
             readback_failed ? str.result_readback_failed_detail : str.result_rejected_detail,
             nullptr,
             pal.text_secondary,
             0,
             0},
            {DialogItem::Kind::Note, str.result_label + ": " + code_buf, nullptr, pal.text_faint, 0, 0},
        };
    }
    draw_dialog_content(surface, Rect{box.x, box.y, box.w, button.y - box.y}, items, ui_pref, pal, pal.dialog_bg);

    fill_rect(surface, box.x, button.y, box.w, rule_t, with_alpha(pal.text_faint, 110));
    draw_dialog_button(surface, button, str.result_continue_button, true, ui_pref, pal);
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

    render_header(surface, view);
    render_sidebar(surface, view, ui_pref);
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
