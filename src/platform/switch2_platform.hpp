#pragma once

#include <array>
#include <cstdint>

#include <switch.h>

#include "framebuffer_renderer.hpp"
#include "kailocale/language_service.hpp"
#include "kailocale/platform_services.hpp"

namespace kailocale {

/// Switch 2 LanguageService backed strictly by documented libounce set / set:sys APIs.
class Switch2LanguageService final : public LanguageService {
public:
    LanguageReadResult read_current_language() override;
    AvailableLanguagesResult read_available_languages() override;
    SystemTheme read_system_theme() override;
    ServiceResult apply_language(LanguageCode code) override;
};

/// Switch 2 PlatformServices adapter backed by libounce, VI2 linear Framebuffer, FreeType2, and HarfBuzz.
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

    std::uint32_t viewport_width() const override { return fb_width_; }
    std::uint32_t viewport_height() const override { return fb_height_; }

    InputSnapshot poll_frame() override;
    void render_status(const ShellView& view) override;
    void render_picker(const PickerView& view) override;

private:
    static void on_applet_hook(AppletHookType hook, void* param);
    ServiceResult open_framebuffer();
    void close_framebuffer();

    PadState pad_{};
    bool input_initialized_ = false;
    bool was_pad_connected_ = false;
    std::int32_t touch_domain_w_ = 1280;
    std::int32_t touch_domain_h_ = 720;

    AppletHookCookie applet_hook_cookie_{};
    bool hooks_registered_ = false;
    bool boundary_reset_pending_ = false;
    bool operation_mode_changed_pending_ = false;

    Framebuffer fb_{};
    bool fb_initialized_ = false;
    std::uint32_t fb_width_ = 1920;
    std::uint32_t fb_height_ = 1080;

    Event res_event_{};
    bool res_event_initialized_ = false;

    bool pl_initialized_ = false;
    FT_Library ft_library_ = nullptr;
    std::array<FT_Face, 8> ft_faces_{};
    FramebufferRenderer renderer_{};
};

} // namespace kailocale
