#include "framebuffer_renderer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace kailocale {

namespace {

constexpr std::uint32_t rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
    return static_cast<std::uint32_t>(r) |
           (static_cast<std::uint32_t>(g) << 8) |
           (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(a) << 24);
}

void fill_rect(std::uint8_t* buffer,
               std::uint32_t stride,
               std::uint32_t fb_w,
               std::uint32_t fb_h,
               int x,
               int y,
               int w,
               int h,
               std::uint32_t color) {
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(static_cast<int>(fb_w), x + w);
    const int y1 = std::min(static_cast<int>(fb_h), y + h);
    for (int py = y0; py < y1; ++py) {
        auto* row = reinterpret_cast<std::uint32_t*>(buffer + static_cast<std::size_t>(py) * stride);
        for (int px = x0; px < x1; ++px) {
            row[px] = color;
        }
    }
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
                       std::uint32_t color) {
    fill_rect(buffer, stride, fb_w, fb_h, x, y, w, thickness, color);
    fill_rect(buffer, stride, fb_w, fb_h, x, y + h - thickness, w, thickness, color);
    fill_rect(buffer, stride, fb_w, fb_h, x, y, thickness, h, color);
    fill_rect(buffer, stride, fb_w, fb_h, x + w - thickness, y, thickness, h, color);
}

void blend_pixel(std::uint8_t* buffer,
                 std::uint32_t stride,
                 std::uint32_t fb_w,
                 std::uint32_t fb_h,
                 int x,
                 int y,
                 std::uint8_t r,
                 std::uint8_t g,
                 std::uint8_t b,
                 std::uint8_t alpha) {
    if (x < 0 || y < 0 || x >= static_cast<int>(fb_w) || y >= static_cast<int>(fb_h) || alpha == 0) {
        return;
    }
    auto* dst = buffer + static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4;
    if (alpha == 255) {
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = 255;
        return;
    }
    const std::uint32_t a = alpha;
    const std::uint32_t inv = 255u - a;
    dst[0] = static_cast<std::uint8_t>((dst[0] * inv + r * a) / 255u);
    dst[1] = static_cast<std::uint8_t>((dst[1] * inv + g * a) / 255u);
    dst[2] = static_cast<std::uint8_t>((dst[2] * inv + b * a) / 255u);
    dst[3] = 255;
}

// Compact 5x7 uppercase/symbol fallback bitmap font (columns LSB=top, 5 bytes/glyph)
// used strictly when pl:u or FreeType failed to initialize so InitError remains readable.
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
                        const std::string& text,
                        std::uint8_t r,
                        std::uint8_t g,
                        std::uint8_t b) {
    int pen_x = x;
    const int px_scale = std::max(2, scale);
    const std::uint32_t color = rgba(r, g, b);
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

void draw_text_freetype(std::uint8_t* buffer,
                        std::uint32_t stride,
                        std::uint32_t fb_w,
                        std::uint32_t fb_h,
                        FT_Face face,
                        int x,
                        int baseline_y,
                        int pixel_size,
                        const std::string& text,
                        std::uint8_t r,
                        std::uint8_t g,
                        std::uint8_t b) {
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_size)) != 0) {
        return;
    }
    int pen_x = x;
    for (unsigned char ch : text) {
        if (FT_Load_Char(face, ch, FT_LOAD_RENDER) != 0) {
            continue;
        }
        const FT_GlyphSlot slot = face->glyph;
        const FT_Bitmap& bmp = slot->bitmap;
        const int gx = pen_x + slot->bitmap_left;
        const int gy = baseline_y - slot->bitmap_top;
        for (unsigned int row = 0; row < bmp.rows; ++row) {
            const std::uint8_t* src_row = bmp.buffer + row * static_cast<unsigned int>(std::abs(bmp.pitch));
            for (unsigned int col = 0; col < bmp.width; ++col) {
                blend_pixel(buffer, stride, fb_w, fb_h, gx + static_cast<int>(col), gy + static_cast<int>(row), r, g, b, src_row[col]);
            }
        }
        pen_x += static_cast<int>(slot->advance.x >> 6);
    }
}

} // namespace

void FramebufferRenderer::set_dimensions(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;
}

void FramebufferRenderer::set_freetype_face(FT_Face face) {
    face_ = face;
}

void FramebufferRenderer::render_shell_view(std::uint8_t* buffer,
                                            std::uint32_t stride,
                                            const ShellView& view) const {
    if (buffer == nullptr) {
        return;
    }

    const bool is_error = (view.kind == ShellScreenKind::InitError);
    const std::uint32_t bg = is_error ? rgba(38, 14, 20) : rgba(15, 23, 42);
    const std::uint32_t card_bg = is_error ? rgba(58, 21, 30) : rgba(30, 41, 59);
    const std::uint32_t card_border = is_error ? rgba(248, 113, 113) : rgba(56, 189, 248);

    fill_rect(buffer, stride, width_, height_, 0, 0, static_cast<int>(width_), static_cast<int>(height_), bg);

    const int margin_x = static_cast<int>(width_ / 10);
    const int margin_y = static_cast<int>(height_ / 6);
    const int card_w = static_cast<int>(width_) - 2 * margin_x;
    const int card_h = static_cast<int>(height_) - 2 * margin_y;
    const int border = std::max(3, static_cast<int>(height_ / 270));

    fill_rect(buffer, stride, width_, height_, margin_x, margin_y, card_w, card_h, card_bg);
    draw_rect_outline(buffer, stride, width_, height_, margin_x, margin_y, card_w, card_h, border, card_border);

    const int pad_x = margin_x + std::max(40, card_w / 14);
    const int title_px = std::max(28, static_cast<int>(height_ / 18));
    const int body_px = std::max(20, static_cast<int>(height_ / 30));
    const int line_step = body_px + std::max(18, static_cast<int>(height_ / 45));

    const std::string title = std::string(view.app_name) + " v" + view.app_version;
    char status_line[128] = {};
    char detail_line[128] = {};
    char hint_line[128] = {};

    if (is_error) {
        std::snprintf(status_line, sizeof(status_line), "Service initialization failed");
        std::snprintf(detail_line,
                      sizeof(detail_line),
                      "Stage: %s   Result: 0x%08X",
                      view.stage.c_str(),
                      view.error_code);
        std::snprintf(hint_line, sizeof(hint_line), "Press + (Plus) to exit");
    } else {
        std::snprintf(status_line, sizeof(status_line), "Switch 2 native runtime initialized");
        std::snprintf(detail_line,
                      sizeof(detail_line),
                      "Mode: %s (zero settings writes)  |  Framebuffer + FreeType",
                      view.stage.c_str());
        std::snprintf(hint_line, sizeof(hint_line), "Press + (Plus) to return cleanly to hbmenu");
    }

    int y = margin_y + std::max(64, card_h / 6);
    if (face_ != nullptr) {
        draw_text_freetype(buffer, stride, width_, height_, face_, pad_x, y, title_px, title, 248, 250, 252);
        y += title_px + std::max(36, static_cast<int>(height_ / 24));
        draw_text_freetype(buffer,
                           stride,
                           width_,
                           height_,
                           face_,
                           pad_x,
                           y,
                           body_px,
                           status_line,
                           is_error ? 254 : 56,
                           is_error ? 202 : 189,
                           is_error ? 202 : 248);
        y += line_step;
        draw_text_freetype(buffer, stride, width_, height_, face_, pad_x, y, body_px, detail_line, 203, 213, 225);
        y += line_step * 2;
        draw_text_freetype(buffer, stride, width_, height_, face_, pad_x, y, body_px, hint_line, 248, 250, 252);
    } else {
        const int title_scale = std::max(3, static_cast<int>(height_ / 216));
        const int body_scale = std::max(2, static_cast<int>(height_ / 320));
        draw_text_fallback(buffer, stride, width_, height_, pad_x, y, title_scale, title, 248, 250, 252);
        y += title_scale * 10 + std::max(24, static_cast<int>(height_ / 30));
        draw_text_fallback(buffer,
                           stride,
                           width_,
                           height_,
                           pad_x,
                           y,
                           body_scale,
                           status_line,
                           is_error ? 254 : 56,
                           is_error ? 202 : 189,
                           is_error ? 202 : 248);
        y += body_scale * 12;
        draw_text_fallback(buffer, stride, width_, height_, pad_x, y, body_scale, detail_line, 203, 213, 225);
        y += body_scale * 20;
        draw_text_fallback(buffer, stride, width_, height_, pad_x, y, body_scale, hint_line, 248, 250, 252);
    }
}

} // namespace kailocale
