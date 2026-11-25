#include "switch2_platform.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "NotoSans-Regular_ttf.h"
#include "NotoSansThai-Regular_ttf.h"

namespace kailocale {

namespace {

constexpr std::uint32_t kDefaultWidth = 1920;
constexpr std::uint32_t kDefaultHeight = 1080;
constexpr s64 kIdlePollSleepNs = 16'000'000;

constexpr std::size_t kMaxAvailableLanguageCodes = 64;

} // namespace

LanguageReadResult Switch2LanguageService::read_current_language() {
    u64 code = 0;
    const Result rc = setGetLanguageCode(&code);
    if (R_FAILED(rc)) {
        return {ServiceResult::failure(static_cast<std::uint32_t>(rc)), 0ULL};
    }
    return {ServiceResult::success(), static_cast<LanguageCode>(code)};
}

AvailableLanguagesResult Switch2LanguageService::read_available_languages() {
    s32 count = 0;
    Result rc = setGetAvailableLanguageCodeCount(&count);
    if (R_FAILED(rc)) {
        return {ServiceResult::failure(static_cast<std::uint32_t>(rc)), {}};
    }
    if (count <= 0) {
        return {ServiceResult::success(), {}};
    }

    const std::size_t cap = std::min<std::size_t>(static_cast<std::size_t>(count), kMaxAvailableLanguageCodes);
    std::vector<u64> raw_codes(cap, 0);
    s32 total_entries = 0;
    rc = setGetAvailableLanguageCodes(&total_entries, raw_codes.data(), raw_codes.size());
    if (R_FAILED(rc)) {
        return {ServiceResult::failure(static_cast<std::uint32_t>(rc)), {}};
    }

    const std::size_t got =
        total_entries > 0 ? std::min<std::size_t>(static_cast<std::size_t>(total_entries), raw_codes.size()) : 0;
    std::vector<LanguageCode> out;
    out.reserve(got);
    for (std::size_t i = 0; i < got; ++i) {
        out.push_back(static_cast<LanguageCode>(raw_codes[i]));
    }
    return {ServiceResult::success(), std::move(out)};
}

SystemTheme Switch2LanguageService::read_system_theme() {
    ColorSetId id = ColorSetId_Dark;
    const Result rc = setsysGetColorSetId(&id);
    if (R_SUCCEEDED(rc) && id == ColorSetId_Light) {
        return SystemTheme::Light;
    }
    return SystemTheme::Dark;
}

void Switch2PlatformServices::on_applet_hook(AppletHookType hook, void* param) {
    auto* self = static_cast<Switch2PlatformServices*>(param);
    if (self == nullptr) {
        return;
    }
    switch (hook) {
    case AppletHookType_OnFocusState:
    case AppletHookType_OnResume:
        self->boundary_reset_pending_ = true;
        break;
    case AppletHookType_OnOperationMode:
        self->boundary_reset_pending_ = true;
        self->operation_mode_changed_pending_ = true;
        break;
    default:
        break;
    }
}

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
    fb_width_ = static_cast<std::uint32_t>(w);
    fb_height_ = static_cast<std::uint32_t>(h);
    renderer_.set_dimensions(fb_width_, fb_height_);
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
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    padInitializeAny(&pad_);
    hidSetNpadHandheldActivationMode(HidNpadHandheldActivationMode_Single);
    padUpdate(&pad_);
    was_pad_connected_ = padIsConnected(&pad_);

    // Request Switch 2 1080p digitizer output range before hidInitializeTouchScreen; fall back to Horizon 720p default if rejected.
    if (R_SUCCEEDED(hidSetTouchScreenOutputRanges(static_cast<s32>(kDefaultWidth),
                                                  static_cast<s32>(kDefaultHeight)))) {
        touch_domain_w_ = static_cast<std::int32_t>(kDefaultWidth);
        touch_domain_h_ = static_cast<std::int32_t>(kDefaultHeight);
    } else {
        touch_domain_w_ = 1280;
        touch_domain_h_ = 720;
    }
    hidInitializeTouchScreen();

    appletHook(&applet_hook_cookie_, &Switch2PlatformServices::on_applet_hook, this);
    hooks_registered_ = true;
    return ServiceResult::success();
}

void Switch2PlatformServices::exit_input() {
    if (hooks_registered_) {
        appletUnhook(&applet_hook_cookie_);
        hooks_registered_ = false;
    }
}

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
    const FT_Error ft_init_err = FT_Init_FreeType(&ft_library_);
    if (ft_init_err != 0) {
        return ServiceResult::failure(static_cast<std::uint32_t>(ft_init_err));
    }

    auto load_memory_face = [this](FontSlot slot, const void* addr, std::size_t size) {
        if (addr == nullptr || size == 0) {
            return;
        }
        FT_Face face = nullptr;
        if (FT_New_Memory_Face(ft_library_,
                               static_cast<const FT_Byte*>(addr),
                               static_cast<FT_Long>(size),
                               0,
                               &face) == 0 &&
            face != nullptr) {
            ft_faces_[static_cast<std::size_t>(slot)] = face;
            renderer_.set_font_face(slot, face);
        }
    };

    const Result pl_rc = plInitialize(PlServiceType_User);
    if (R_SUCCEEDED(pl_rc)) {
        pl_initialized_ = true;
        struct SharedFontBinding {
            FontSlot slot;
            PlSharedFontType pl_type;
        };
        static constexpr SharedFontBinding kBindings[] = {
            {FontSlot::Standard, PlSharedFontType_Standard},
            {FontSlot::ChineseSimplified, PlSharedFontType_ChineseSimplified},
            {FontSlot::ExtChineseSimplified, PlSharedFontType_ExtChineseSimplified},
            {FontSlot::ChineseTraditional, PlSharedFontType_ChineseTraditional},
            {FontSlot::Korean, PlSharedFontType_KO},
            {FontSlot::NintendoExt, PlSharedFontType_NintendoExt},
        };
        for (const auto& binding : kBindings) {
            PlFontData data{};
            if (R_SUCCEEDED(plGetSharedFontByType(&data, binding.pl_type))) {
                load_memory_face(binding.slot, data.address, data.size);
            }
        }
    }

    // Licensed SIL OFL 1.1 embedded fallbacks (Thai & Latin/Cyrillic) if pl faces omit glyphs or fail to initialize.
    load_memory_face(FontSlot::FallbackThai, NotoSansThai_Regular_ttf, NotoSansThai_Regular_ttf_size);
    load_memory_face(FontSlot::FallbackSans, NotoSans_Regular_ttf, NotoSans_Regular_ttf_size);

    bool any_loaded = false;
    for (FT_Face f : ft_faces_) {
        if (f != nullptr) {
            any_loaded = true;
            break;
        }
    }
    if (!any_loaded) {
        exit_shared_font();
        return ServiceResult::failure(R_FAILED(pl_rc) ? static_cast<std::uint32_t>(pl_rc) : 1u);
    }

    return ServiceResult::success();
}

void Switch2PlatformServices::exit_shared_font() {
    renderer_.clear_fonts();
    for (FT_Face& f : ft_faces_) {
        if (f != nullptr) {
            FT_Done_Face(f);
            f = nullptr;
        }
    }
    if (ft_library_ != nullptr) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
    }
    if (pl_initialized_) {
        plExit();
        pl_initialized_ = false;
    }
}

InputSnapshot Switch2PlatformServices::poll_frame() {
    const bool running = appletMainLoop();
    padUpdate(&pad_);

    InputSnapshot snap;
    snap.applet_running = running;

    const bool connected = padIsConnected(&pad_);
    if (!was_pad_connected_ && connected) {
        snap.boundary_reset = true;
    }
    was_pad_connected_ = connected;

    if (boundary_reset_pending_) {
        snap.boundary_reset = true;
        boundary_reset_pending_ = false;
    }

    const bool in_focus = (appletGetFocusState() == AppletFocusState_InFocus);
    if (!in_focus) {
        snap.boundary_reset = true;
    } else {
        const u64 held = padGetButtons(&pad_);
        snap.nav_up_held = (held & (HidNpadButton_Up | HidNpadButton_StickLUp)) != 0;
        snap.nav_down_held = (held & (HidNpadButton_Down | HidNpadButton_StickLDown)) != 0;
        snap.a_held = (held & HidNpadButton_A) != 0;
        snap.b_held = (held & HidNpadButton_B) != 0;
        snap.plus_held = (held & HidNpadButton_Plus) != 0;

        HidTouchScreenState touch_state{};
        if (hidGetTouchScreenStates(&touch_state, 1) > 0 && touch_state.count > 0) {
            const std::int64_t raw_x = static_cast<std::int64_t>(touch_state.touches[0].x);
            const std::int64_t raw_y = static_cast<std::int64_t>(touch_state.touches[0].y);
            const std::int32_t sx = static_cast<std::int32_t>(
                std::clamp<std::int64_t>((raw_x * fb_width_) / touch_domain_w_, 0, fb_width_ - 1));
            const std::int32_t sy = static_cast<std::int32_t>(
                std::clamp<std::int64_t>((raw_y * fb_height_) / touch_domain_h_, 0, fb_height_ - 1));
            snap.touch = TouchSample{true, sx, sy};
        }
    }

    bool res_changed = false;
    if (res_event_initialized_ && R_SUCCEEDED(eventWait(&res_event_, 0))) {
        res_changed = true;
    }
    if (operation_mode_changed_pending_) {
        operation_mode_changed_pending_ = false;
        s32 new_w = 0;
        s32 new_h = 0;
        if (R_SUCCEEDED(appletGetDefaultDisplayResolution(&new_w, &new_h)) && new_w > 0 && new_h > 0) {
            if (static_cast<std::uint32_t>(new_w) != fb_width_ ||
                static_cast<std::uint32_t>(new_h) != fb_height_) {
                res_changed = true;
            }
        }
    }
    snap.resolution_changed = res_changed;

    if (running && !snap.plus_held && !snap.resolution_changed) {
        svcSleepThread(kIdlePollSleepNs);
    }

    return snap;
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

void Switch2PlatformServices::render_picker(const PickerView& view) {
    if (!fb_initialized_) {
        return;
    }
    u32 stride = 0;
    auto* pixels = static_cast<std::uint8_t*>(framebufferBegin(&fb_, &stride));
    if (pixels == nullptr) {
        return;
    }
    renderer_.render_picker_view(pixels, stride, view);
    framebufferEnd(&fb_);
}

} // namespace kailocale
