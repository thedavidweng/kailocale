#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "kailocale/language_catalog.hpp"
#include "kailocale/language_service.hpp"
#include "kailocale/localization.hpp"
#include "kailocale/picker_layout.hpp"
#include "kailocale/platform_services.hpp"
#include "kailocale/theme.hpp"

namespace kailocale {

enum class PickerScreen {
    List,
    ConfirmDialog,
    Error,
};

enum class BannerKind {
    None,
    Warning,
    Info,
};

struct PickerInput {
    bool applet_running = true;
    bool nav_up_held = false;
    bool nav_down_held = false;
    bool a_held = false;
    bool b_held = false;
    bool plus_held = false;
    bool boundary_reset = false;
    std::uint32_t viewport_width = 1920;
    std::uint32_t viewport_height = 1080;
    TouchSample touch{};
};

struct PickerView {
    const char* app_name = "KaiLocale";
    const char* app_version = "0.1.0";
    PickerScreen screen = PickerScreen::List;
    SystemTheme theme = SystemTheme::Dark;
    ThemePalette palette{};
    LocalizedStrings strings{};
    LanguageCode applied_code = 0;
    LanguageEntry active_entry{};
    bool active_missing_from_supported = false;
    std::vector<LanguageEntry> entries;
    std::size_t focused_index = 0;
    std::size_t scroll_offset = 0;
    std::size_t dialog_target_index = 0;
    BannerKind banner_kind = BannerKind::None;
    std::string banner_text;
    std::string error_stage;
    std::uint32_t error_code = 0;
    PickerLayout layout{};
};

struct PickerStepResult {
    bool exit_requested = false;
    bool dirty = false;
};

/// Controller-first and touch-capable read-only language picker state machine.
class LanguagePicker {
public:
    explicit LanguagePicker(LanguageService& service,
                            std::uint32_t viewport_width = 1920,
                            std::uint32_t viewport_height = 1080);

    PickerStepResult step(const PickerInput& input);
    const PickerView& view() const { return view_; }

private:
    static constexpr int kRepeatDelayFrames = 20;
    static constexpr int kRepeatIntervalFrames = 8;

    void refresh_layout();
    void ensure_focused_visible();
    void clear_transient_banner();
    void move_focus(int delta);
    bool open_dialog_for_focused();
    void confirm_readonly_preview();
    void cancel_dialog();
    void latch_held_buttons(const PickerInput& input);

    PickerView view_{};

    bool prev_a_held_ = false;
    bool prev_b_held_ = false;
    bool prev_plus_held_ = false;

    bool latch_a_ = false;
    bool latch_b_ = false;
    bool latch_plus_ = false;
    bool latch_nav_ = false;

    int nav_dir_ = 0;
    int nav_timer_ = 0;

    bool touch_in_progress_ = false;
    bool touch_is_tap_ = false;
    bool touch_suppressed_ = false;
    bool touch_started_in_list_ = false;
    std::int32_t touch_start_x_ = 0;
    std::int32_t touch_start_y_ = 0;
    std::int32_t touch_last_x_ = 0;
    std::int32_t touch_last_y_ = 0;
    std::size_t touch_drag_start_scroll_ = 0;
};

} // namespace kailocale
