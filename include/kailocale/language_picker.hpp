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
    Applying,
    Result,
    Error,
};

enum class ApplyOutcome {
    None,
    Verified,
    Rejected,
    ReadbackMismatch,
    ReadbackFailed,
};

struct ApplyReport {
    ApplyOutcome outcome = ApplyOutcome::None;
    LanguageEntry target_entry{};
    LanguageEntry actual_entry{};
    std::uint32_t error_code = 0;
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

    static PickerInput from_snapshot(const InputSnapshot& snap,
                                     std::uint32_t width,
                                     std::uint32_t height) {
        return PickerInput{
            snap.applet_running,
            snap.nav_up_held,
            snap.nav_down_held,
            snap.a_held,
            snap.b_held,
            snap.plus_held,
            snap.boundary_reset,
            width,
            height,
            snap.touch,
        };
    }
};

struct PickerView {
    const char* app_name = "KaiLocale";
    const char* app_version = "0.1.1";
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
    ApplyReport apply_report{};
    PickerLayout layout{};
};

struct PickerStepResult {
    bool exit_requested = false;
    bool dirty = false;
};

/// Controller-first and touch-capable language picker state machine with confirmed apply and readback verification.
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
    bool begin_apply_for_dialog_target();
    void execute_pending_apply();
    void apply_active_language_state(LanguageCode code);
    void dismiss_result(const PickerInput& input);
    void cancel_dialog();
    void reset_inputs_on_transition(const PickerInput& input);

    LanguageService& service_;
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
