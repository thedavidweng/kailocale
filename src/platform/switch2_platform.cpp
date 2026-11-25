#include "switch2_platform.hpp"

#include <cstdint>

namespace kailocale {

namespace {

constexpr std::uint32_t kDefaultWidth = 1920;
constexpr std::uint32_t kDefaultHeight = 1080;
constexpr s64 kIdlePollSleepNs = 16'000'000;

} // namespace

ServiceResult Switch2PlatformServices::init_exit_lock() {
    const Result rc = appletLockExit();
    return R_SUCCEEDED(rc) ? ServiceResult::success()
                           : ServiceResult::failure(static_cast<std::uint32_t>(rc));
}

void Switch2PlatformServices::exit_exit_lock() {
    appletUnlockExit();
}

ServiceResult Switch2PlatformServices::open_framebuffer() {
    s32 w = 0;
    s32 h = 0;
    if (R_FAILED(appletGetDefaultDisplayResolution(&w, &h)) || w <= 0 || h <= 0) {
        w = static_cast<s32>(kDefaultWidth);
        h = static_cast<s32>(kDefaultHeight);
    }

    Result rc = framebufferCreate(&fb_,
                                  nwindowGetDefault(),
                                  static_cast<u32>(w),
                                  static_cast<u32>(h),
                                  PIXEL_FORMAT_RGBA_8888,
                                  2);
    if (R_FAILED(rc)) {
        return ServiceResult::failure(static_cast<std::uint32_t>(rc));
    }

    rc = framebufferMakeLinear(&fb_);
    if (R_FAILED(rc)) {
        framebufferClose(&fb_);
        return ServiceResult::failure(static_cast<std::uint32_t>(rc));
    }

    fb_initialized_ = true;
    renderer_.set_dimensions(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
    return ServiceResult::success();
}

void Switch2PlatformServices::close_framebuffer() {
    if (fb_initialized_) {
        framebufferClose(&fb_);
        fb_initialized_ = false;
    }
}

ServiceResult Switch2PlatformServices::init_display() {
    if (R_SUCCEEDED(appletGetDefaultDisplayResolutionChangeEvent(&res_event_))) {
        res_event_initialized_ = true;
    }
    const ServiceResult fb_res = open_framebuffer();
    if (!fb_res.ok && res_event_initialized_) {
        eventClose(&res_event_);
        res_event_initialized_ = false;
    }
    return fb_res;
}

ServiceResult Switch2PlatformServices::resize_display() {
    close_framebuffer();
    return open_framebuffer();
}

void Switch2PlatformServices::exit_display() {
    close_framebuffer();
    if (res_event_initialized_) {
        eventClose(&res_event_);
        res_event_initialized_ = false;
    }
}

ServiceResult Switch2PlatformServices::init_input() {
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad_);
    return ServiceResult::success();
}

void Switch2PlatformServices::exit_input() {}

ServiceResult Switch2PlatformServices::init_set() {
    const Result rc = setInitialize();
    return R_SUCCEEDED(rc) ? ServiceResult::success()
                           : ServiceResult::failure(static_cast<std::uint32_t>(rc));
}

void Switch2PlatformServices::exit_set() {
    setExit();
}

ServiceResult Switch2PlatformServices::init_setsys() {
    const Result rc = setsysInitialize();
    return R_SUCCEEDED(rc) ? ServiceResult::success()
                           : ServiceResult::failure(static_cast<std::uint32_t>(rc));
}

void Switch2PlatformServices::exit_setsys() {
    setsysExit();
}

ServiceResult Switch2PlatformServices::init_shared_font() {
    Result rc = plInitialize(PlServiceType_User);
    if (R_FAILED(rc)) {
        return ServiceResult::failure(static_cast<std::uint32_t>(rc));
    }

    PlFontData std_font{};
    rc = plGetSharedFontByType(&std_font, PlSharedFontType_Standard);
    if (R_FAILED(rc) || std_font.address == nullptr || std_font.size == 0) {
        plExit();
        return ServiceResult::failure(R_FAILED(rc) ? static_cast<std::uint32_t>(rc) : 1u);
    }

    const FT_Error ft_init_err = FT_Init_FreeType(&ft_library_);
    if (ft_init_err != 0) {
        plExit();
        return ServiceResult::failure(static_cast<std::uint32_t>(ft_init_err));
    }

    const FT_Error ft_face_err = FT_New_Memory_Face(
        ft_library_,
        static_cast<const FT_Byte*>(std_font.address),
        static_cast<FT_Long>(std_font.size),
        0,
        &ft_face_);
    if (ft_face_err != 0) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
        plExit();
        return ServiceResult::failure(static_cast<std::uint32_t>(ft_face_err));
    }

    renderer_.set_freetype_face(ft_face_);
    return ServiceResult::success();
}

void Switch2PlatformServices::exit_shared_font() {
    renderer_.set_freetype_face(nullptr);
    if (ft_face_ != nullptr) {
        FT_Done_Face(ft_face_);
        ft_face_ = nullptr;
    }
    if (ft_library_ != nullptr) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
    }
    plExit();
}

InputSnapshot Switch2PlatformServices::poll_frame() {
    const bool running = appletMainLoop();
    padUpdate(&pad_);
    const u64 down = padGetButtonsDown(&pad_);
    const bool plus_pressed = (down & HidNpadButton_Plus) != 0;

    bool res_changed = false;
    if (res_event_initialized_ && R_SUCCEEDED(eventWait(&res_event_, 0))) {
        res_changed = true;
    }

    if (running && !plus_pressed && !res_changed) {
        svcSleepThread(kIdlePollSleepNs);
    }

    return InputSnapshot{running, plus_pressed, res_changed};
}

void Switch2PlatformServices::render_status(const ShellView& view) {
    if (!fb_initialized_) {
        return;
    }
    u32 stride = 0;
    auto* pixels = static_cast<std::uint8_t*>(framebufferBegin(&fb_, &stride));
    if (pixels == nullptr) {
        return;
    }
    renderer_.render_shell_view(pixels, stride, view);
    framebufferEnd(&fb_);
}

} // namespace kailocale
