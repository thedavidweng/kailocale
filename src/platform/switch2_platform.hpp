#pragma once

#include <switch.h>

#include "framebuffer_renderer.hpp"
#include "kailocale/platform_services.hpp"

namespace kailocale {

/// Switch 2 PlatformServices adapter backed by libounce, VI2 linear Framebuffer, and FreeType2.
class Switch2PlatformServices final : public PlatformServices {
public:
    ServiceResult init_exit_lock() override;
    void exit_exit_lock() override;

    ServiceResult init_display() override;
    ServiceResult resize_display() override;
    void exit_display() override;

    ServiceResult init_input() override;
    void exit_input() override;

    ServiceResult init_set() override;
    void exit_set() override;

    ServiceResult init_setsys() override;
    void exit_setsys() override;

    ServiceResult init_shared_font() override;
    void exit_shared_font() override;

    InputSnapshot poll_frame() override;
    void render_status(const ShellView& view) override;

private:
    ServiceResult open_framebuffer();
    void close_framebuffer();

    PadState pad_{};
    Framebuffer fb_{};
    bool fb_initialized_ = false;
    Event res_event_{};
    bool res_event_initialized_ = false;
    FT_Library ft_library_ = nullptr;
    FT_Face ft_face_ = nullptr;
    FramebufferRenderer renderer_{};
};

} // namespace kailocale
