#include "kailocale/language_picker.hpp"

#include <algorithm>
#include <cstdlib>

namespace kailocale {

namespace {

bool is_supported_target(const LanguageEntry& entry) {
    return entry.system_supported && entry.code != 0 && !format_language_tag(entry.code).empty();
}

} // namespace

LanguagePicker::LanguagePicker(LanguageService& service,
                               std::uint32_t viewport_width,
                               std::uint32_t viewport_height)
    : service_(service) {
    view_.theme = service.read_system_theme();
    view_.palette = theme_for_system(view_.theme);

    const CatalogBuildResult catalog = build_language_catalog(service);
    view_.applied_code = catalog.active_entry.code;
    view_.active_entry = catalog.active_entry;
    view_.active_missing_from_supported = !catalog.has_error && !catalog.active_in_supported_list;

    const UiLanguage ui_lang = ui_language_from_code(view_.applied_code);
    view_.strings = localized_strings(ui_lang);

    view_.layout.viewport_width = viewport_width > 0 ? viewport_width : 1920u;
    view_.layout.viewport_height = viewport_height > 0 ? viewport_height : 1080u;

    if (catalog.has_error) {
        view_.screen = PickerScreen::Error;
        view_.error_stage = catalog.error_stage;
        view_.error_code = catalog.error_code;
        refresh_layout();
        return;
    }

    view_.screen = PickerScreen::List;
    view_.entries = catalog.entries;
    view_.focused_index = 0;
    for (std::size_t i = 0; i < view_.entries.size(); ++i) {
        if (view_.entries[i].is_active) {
            view_.focused_index = i;
            break;
        }
    }

    if (view_.active_missing_from_supported) {
        view_.banner_kind = BannerKind::Warning;
        view_.banner_text = view_.strings.unsupported_active_warning;
    }

    ensure_focused_visible();
}

void LanguagePicker::refresh_layout() {
    const std::uint32_t w = view_.layout.viewport_width;
    const std::uint32_t h = view_.layout.viewport_height;
    view_.layout = compute_picker_layout(w, h, view_.entries.size(), view_.scroll_offset);
    view_.scroll_offset = view_.layout.scroll_offset;
}

void LanguagePicker::ensure_focused_visible() {
    refresh_layout();
    if (!view_.entries.empty() && view_.layout.visible_rows > 0) {
        if (view_.focused_index < view_.scroll_offset) {
            view_.scroll_offset = view_.focused_index;
        } else if (view_.focused_index >= view_.scroll_offset + view_.layout.visible_rows) {
            view_.scroll_offset = view_.focused_index - view_.layout.visible_rows + 1;
        }
        refresh_layout();
    }
}

void LanguagePicker::clear_transient_banner() {
    if (view_.banner_kind == BannerKind::Info) {
        if (view_.active_missing_from_supported) {
            view_.banner_kind = BannerKind::Warning;
            view_.banner_text = view_.strings.unsupported_active_warning;
        } else {
            view_.banner_kind = BannerKind::None;
            view_.banner_text.clear();
        }
    }
}

void LanguagePicker::move_focus(int delta) {
    if (view_.entries.empty() || delta == 0) {
        return;
    }
    const std::size_t count = view_.entries.size();
    if (delta < 0) {
        view_.focused_index = (view_.focused_index == 0) ? (count - 1) : (view_.focused_index - 1);
    } else {
        view_.focused_index = (view_.focused_index + 1 >= count) ? 0 : (view_.focused_index + 1);
    }
    clear_transient_banner();
    ensure_focused_visible();
}

bool LanguagePicker::open_dialog_for_focused() {
    if (view_.entries.empty() || view_.focused_index >= view_.entries.size()) {
        return false;
    }
    const LanguageEntry& focused = view_.entries[view_.focused_index];
    if (!is_supported_target(focused)) {
        return false;
    }
    if (focused.code == view_.applied_code) {
        const bool changed =
            (view_.banner_kind != BannerKind::Info || view_.banner_text != view_.strings.already_active_notice);
        view_.banner_kind = BannerKind::Info;
        view_.banner_text = view_.strings.already_active_notice;
        return changed;
    }
    view_.dialog_target_index = view_.focused_index;
    view_.screen = PickerScreen::ConfirmDialog;
    return true;
}

bool LanguagePicker::begin_apply_for_dialog_target() {
    if (view_.dialog_target_index >= view_.entries.size()) {
        cancel_dialog();
        return false;
    }
    const LanguageEntry& target = view_.entries[view_.dialog_target_index];
    if (!is_supported_target(target) || target.code == view_.applied_code) {
        cancel_dialog();
        return false;
    }
    view_.apply_report = ApplyReport{};
    view_.apply_report.target_entry = target;
    view_.screen = PickerScreen::Applying;
    return true;
}

void LanguagePicker::apply_active_language_state(LanguageCode code) {
    view_.applied_code = code;
    bool in_supported = false;
    for (auto& entry : view_.entries) {
        entry.is_active = (entry.code == code);
        if (entry.is_active) {
            in_supported = true;
        }
    }
    view_.active_missing_from_supported = !in_supported;

    view_.active_entry = make_language_entry(code, in_supported, true);

    const UiLanguage ui_lang = ui_language_from_code(code);
    view_.strings = localized_strings(ui_lang);

    if (view_.active_missing_from_supported) {
        view_.banner_kind = BannerKind::Warning;
        view_.banner_text = view_.strings.unsupported_active_warning;
    } else {
        view_.banner_kind = BannerKind::None;
        view_.banner_text.clear();
    }
}

void LanguagePicker::execute_pending_apply() {
    const LanguageEntry target = view_.apply_report.target_entry;
    if (!is_supported_target(target) || target.code == view_.applied_code) {
        cancel_dialog();
        return;
    }

    const ServiceResult write_res = service_.apply_language(target.code);
    if (!write_res.ok) {
        view_.apply_report.outcome = ApplyOutcome::Rejected;
        view_.apply_report.error_code = write_res.code;
        view_.apply_report.actual_entry = view_.active_entry;
        view_.screen = PickerScreen::Result;
        return;
    }

    const LanguageReadResult readback = service_.read_current_language();
    if (!readback.status.ok) {
        view_.apply_report.outcome = ApplyOutcome::ReadbackFailed;
        view_.apply_report.error_code = readback.status.code;
        view_.apply_report.actual_entry = LanguageEntry{};
        view_.screen = PickerScreen::Result;
        return;
    }

    if (readback.code == target.code) {
        apply_active_language_state(readback.code);
        view_.apply_report.outcome = ApplyOutcome::Verified;
        view_.apply_report.actual_entry = view_.active_entry;
        view_.apply_report.error_code = 0;
        view_.screen = PickerScreen::Result;
        refresh_layout();
        return;
    }

    // ReadbackMismatch: update applied state if readback.code != 0, and always report expected vs actual.
    view_.apply_report.outcome = ApplyOutcome::ReadbackMismatch;
    if (readback.code != 0) {
        apply_active_language_state(readback.code);
        view_.apply_report.actual_entry = view_.active_entry;
    } else {
        view_.apply_report.actual_entry = make_language_entry(0ULL, false, false);
    }
    view_.apply_report.error_code = 0;
    view_.screen = PickerScreen::Result;
    refresh_layout();
}

void LanguagePicker::dismiss_result(const PickerInput& input) {
    if (!view_.entries.empty() && view_.dialog_target_index < view_.entries.size()) {
        view_.focused_index = view_.dialog_target_index;
    }
    view_.screen = PickerScreen::List;
    ensure_focused_visible();
    reset_inputs_on_transition(input);
}

void LanguagePicker::cancel_dialog() {
    view_.focused_index = view_.dialog_target_index;
    view_.screen = PickerScreen::List;
    ensure_focused_visible();
}

void LanguagePicker::reset_inputs_on_transition(const PickerInput& input) {
    prev_a_held_ = input.a_held;
    prev_b_held_ = input.b_held;
    prev_plus_held_ = input.plus_held;
    if (input.a_held) {
        latch_a_ = true;
    }
    if (input.b_held) {
        latch_b_ = true;
    }
    if (input.plus_held) {
        latch_plus_ = true;
    }
    if (input.nav_up_held || input.nav_down_held) {
        latch_nav_ = true;
    }
    nav_dir_ = 0;
    nav_timer_ = 0;
    touch_in_progress_ = false;
    touch_is_tap_ = false;
    touch_suppressed_ = input.touch.active;
}

PickerStepResult LanguagePicker::step(const PickerInput& input) {
    PickerStepResult out;

    if (!input.applet_running) {
        out.exit_requested = true;
        return out;
    }

    const std::uint32_t target_w = input.viewport_width > 0 ? input.viewport_width : 1920u;
    const std::uint32_t target_h = input.viewport_height > 0 ? input.viewport_height : 1080u;
    if (target_w != view_.layout.viewport_width || target_h != view_.layout.viewport_height) {
        view_.layout.viewport_width = target_w;
        view_.layout.viewport_height = target_h;
        refresh_layout();
        out.dirty = true;
    }

    if (!input.a_held) {
        latch_a_ = false;
    }
    if (!input.b_held) {
        latch_b_ = false;
    }
    if (!input.plus_held) {
        latch_plus_ = false;
    }
    if (!input.nav_up_held && !input.nav_down_held) {
        latch_nav_ = false;
    }

    if (view_.screen == PickerScreen::Applying) {
        reset_inputs_on_transition(input);
        execute_pending_apply();
        out.dirty = true;
        return out;
    }

    if (input.boundary_reset) {
        reset_inputs_on_transition(input);
        return out;
    }

    const bool a_down = input.a_held && !prev_a_held_ && !latch_a_;
    const bool b_down = input.b_held && !prev_b_held_ && !latch_b_;
    const bool plus_down = input.plus_held && !prev_plus_held_ && !latch_plus_;

    prev_a_held_ = input.a_held;
    prev_b_held_ = input.b_held;
    prev_plus_held_ = input.plus_held;

    if (plus_down) {
        out.exit_requested = true;
        return out;
    }

    if (view_.screen == PickerScreen::Result) {
        if (a_down || b_down) {
            dismiss_result(input);
            out.dirty = true;
            return out;
        }
    }

    // Controlled navigation repeat strictly for vertical movement when list is active.
    int desired_dir = 0;
    if (view_.screen == PickerScreen::List && !latch_nav_) {
        if (input.nav_up_held && !input.nav_down_held) {
            desired_dir = -1;
        } else if (input.nav_down_held && !input.nav_up_held) {
            desired_dir = 1;
        }
    }

    if (desired_dir == 0) {
        nav_dir_ = 0;
        nav_timer_ = 0;
    } else if (desired_dir != nav_dir_) {
        nav_dir_ = desired_dir;
        nav_timer_ = -kRepeatDelayFrames;
        const std::size_t before = view_.focused_index;
        move_focus(nav_dir_);
        if (view_.focused_index != before) {
            out.dirty = true;
        }
    } else {
        ++nav_timer_;
        if (nav_timer_ >= 0) {
            nav_timer_ = -kRepeatIntervalFrames;
            const std::size_t before = view_.focused_index;
            move_focus(nav_dir_);
            if (view_.focused_index != before) {
                out.dirty = true;
            }
        }
    }

    // Controller face buttons: one press cannot both open and confirm a dialog.
    if (view_.screen == PickerScreen::ConfirmDialog) {
        if (a_down) {
            begin_apply_for_dialog_target();
            reset_inputs_on_transition(input);
            out.dirty = true;
            return out;
        }
        if (b_down) {
            cancel_dialog();
            reset_inputs_on_transition(input);
            out.dirty = true;
            return out;
        }
    } else if (view_.screen == PickerScreen::List) {
        if (a_down) {
            if (open_dialog_for_focused()) {
                out.dirty = true;
            }
            reset_inputs_on_transition(input);
            return out;
        }
    }

    // Touch handling (release-to-tap within tap slop + vertical drag-scroll, explicit confirmation preserved).
    if (!input.touch.active) {
        touch_suppressed_ = false;
        if (touch_in_progress_) {
            touch_in_progress_ = false;
            if (touch_is_tap_) {
                const int tap_x = touch_start_x_;
                const int tap_y = touch_start_y_;
                if (view_.layout.exit_button.contains(tap_x, tap_y) &&
                    (view_.screen == PickerScreen::List ||
                     view_.screen == PickerScreen::Result ||
                     view_.screen == PickerScreen::Error)) {
                    out.exit_requested = true;
                    return out;
                }
                if (view_.screen == PickerScreen::Result) {
                    if (view_.layout.result_continue_button.contains(tap_x, tap_y)) {
                        dismiss_result(input);
                        out.dirty = true;
                    }
                } else if (view_.screen == PickerScreen::ConfirmDialog) {
                    if (view_.layout.dialog_confirm_button.contains(tap_x, tap_y)) {
                        begin_apply_for_dialog_target();
                        reset_inputs_on_transition(input);
                        out.dirty = true;
                    } else if (view_.layout.dialog_cancel_button.contains(tap_x, tap_y) ||
                               !view_.layout.dialog_box.contains(tap_x, tap_y)) {
                        cancel_dialog();
                        out.dirty = true;
                    }
                } else if (view_.screen == PickerScreen::List) {
                    const auto hit_row = hit_test_row(view_.layout, tap_x, tap_y);
                    if (hit_row.has_value() && *hit_row < view_.entries.size()) {
                        if (*hit_row != view_.focused_index) {
                            view_.focused_index = *hit_row;
                            clear_transient_banner();
                            ensure_focused_visible();
                            out.dirty = true;
                        } else if (open_dialog_for_focused()) {
                            out.dirty = true;
                        }
                    } else if (view_.layout.select_button.contains(tap_x, tap_y)) {
                        if (open_dialog_for_focused()) {
                            out.dirty = true;
                        }
                    }
                }
            }
        }
    } else if (!touch_suppressed_) {
        if (!touch_in_progress_) {
            touch_in_progress_ = true;
            touch_is_tap_ = true;
            touch_started_in_list_ =
                (view_.screen == PickerScreen::List) &&
                view_.layout.list_area.contains(input.touch.x, input.touch.y);
            touch_start_x_ = input.touch.x;
            touch_start_y_ = input.touch.y;
            touch_last_x_ = input.touch.x;
            touch_last_y_ = input.touch.y;
            touch_drag_start_scroll_ = view_.scroll_offset;
        } else {
            touch_last_x_ = input.touch.x;
            touch_last_y_ = input.touch.y;
            if (std::abs(touch_last_x_ - touch_start_x_) > view_.layout.tap_slop ||
                std::abs(touch_last_y_ - touch_start_y_) > view_.layout.tap_slop) {
                touch_is_tap_ = false;
            }
            if (!touch_is_tap_ && touch_started_in_list_ &&
                view_.entries.size() > view_.layout.visible_rows &&
                view_.layout.row_pitch > 0) {
                const int dy = touch_start_y_ - touch_last_y_;
                const int row_delta = dy / view_.layout.row_pitch;
                const std::ptrdiff_t max_scroll = static_cast<std::ptrdiff_t>(
                    view_.entries.size() - view_.layout.visible_rows);
                const std::ptrdiff_t raw_target =
                    static_cast<std::ptrdiff_t>(touch_drag_start_scroll_) + row_delta;
                const std::size_t clamped = static_cast<std::size_t>(
                    std::clamp<std::ptrdiff_t>(raw_target, 0, max_scroll));
                if (clamped != view_.scroll_offset) {
                    view_.scroll_offset = clamped;
                    refresh_layout();
                    out.dirty = true;
                }
            }
        }
    }

    return out;
}

} // namespace kailocale
