#pragma once

#include <cstdint>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "kailocale/platform_services.hpp"

namespace kailocale {

/// Renders KaiLocale shell screens into a Switch 2 linear RGBA8 Framebuffer via FreeType or fallback glyphs.
class FramebufferRenderer {
public:
    void set_dimensions(std::uint32_t width, std::uint32_t height);
    void set_freetype_face(FT_Face face);

    void render_shell_view(std::uint8_t* buffer, std::uint32_t stride, const ShellView& view) const;

private:
    std::uint32_t width_ = 1920;
    std::uint32_t height_ = 1080;
    FT_Face face_ = nullptr;
};

} // namespace kailocale
