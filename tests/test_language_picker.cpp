#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kailocale/language_catalog.hpp"
#include "kailocale/language_picker.hpp"
#include "kailocale/language_service.hpp"
#include "kailocale/picker_layout.hpp"

namespace {

using kailocale::ApplyOutcome;
using kailocale::AvailableLanguagesResult;
using kailocale::LanguageCode;
using kailocale::LanguagePicker;
using kailocale::LanguageReadResult;
using kailocale::LanguageService;
using kailocale::PickerInput;
using kailocale::PickerLayout;
using kailocale::PickerScreen;
using kailocale::PickerStepResult;
using kailocale::PickerView;
using kailocale::Rect;
using kailocale::ServiceResult;
using kailocale::SystemTheme;
using kailocale::UiLanguage;
using kailocale::compute_picker_layout;
using kailocale::hit_test_row;
using kailocale::pack_language_code;

class FakeLanguageService final : public LanguageService {
public:
    LanguageReadResult current_result{ServiceResult::success(), pack_language_code("en-US")};
    AvailableLanguagesResult available_result{
        ServiceResult::success(),
        {
            pack_language_code("ja"),
            pack_language_code("en-US"),
            pack_language_code("zh-Hans"),
            pack_language_code("zh-Hant"),
            pack_language_code("th"),
        },
    };
    SystemTheme theme = SystemTheme::Dark;
    ServiceResult apply_result = ServiceResult::success();
    bool update_current_on_apply_success = true;
    std::vector<LanguageCode> apply_calls;
    std::size_t read_current_calls = 0;

    LanguageReadResult read_current_language() override {
        ++read_current_calls;
        return current_result;
    }

    AvailableLanguagesResult read_available_languages() override {
        return available_result;
    }

    SystemTheme read_system_theme() override {
        return theme;
    }

    ServiceResult apply_language(LanguageCode code) override {
        apply_calls.push_back(code);
        if (apply_result.ok && update_current_on_apply_success) {
            current_result = {ServiceResult::success(), code};
        }
        return apply_result;
    }
};

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::ostringstream oss;                                              \
            oss << __FILE__ << ":" << __LINE__ << " failed check: " << #cond;    \
            throw std::runtime_error(oss.str());                                 \
        }                                                                        \
    } while (false)

void test_layout_scales_cleanly_across_1080p_1440p_4k_with_comfortable_hit_targets() {
    CHECK(kailocale::scale_px(100, 1080) == 100);
    CHECK(kailocale::scale_px(100, 2160) == 200);
    CHECK(kailocale::scale_px(1, 720) == 1);

    const PickerLayout l1080 = compute_picker_layout(1920, 1080, 18, 0);
    const PickerLayout l1440 = compute_picker_layout(2560, 1440, 18, 0);
    const PickerLayout l2160 = compute_picker_layout(3840, 2160, 18, 0);

    CHECK(!l1080.row_rects.empty());
    CHECK(l1080.row_rects[0].h >= 64);
    CHECK(l1080.list_area.y >= l1080.header.y + l1080.header.h);
    CHECK(l1080.footer.y >= l1080.list_area.y + l1080.list_area.h);

    CHECK(l1440.row_rects[0].h > l1080.row_rects[0].h);
    CHECK(l2160.row_rects[0].h >= l1080.row_rects[0].h * 2 - 2);
    CHECK(l2160.visible_rows == l1080.visible_rows);

    const Rect& row2 = l1080.row_rects[2];
    const auto hit = hit_test_row(l1080, row2.x + row2.w / 2, row2.y + row2.h / 2);
    CHECK(hit.has_value());
    CHECK(*hit == 2u);
}

void test_picker_initializes_focus_on_active_language_and_handles_absent_or_error() {
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("zh-Hans")};
    svc.theme = SystemTheme::Light;

    LanguagePicker picker(svc, 1920, 1080);
    const PickerView& view = picker.view();
    CHECK(view.screen == PickerScreen::List);
    CHECK(view.theme == SystemTheme::Light);
    CHECK(view.applied_code == pack_language_code("zh-Hans"));
    CHECK(view.focused_index == 2u);
    CHECK(view.entries[view.focused_index].tag == "zh-Hans");
    CHECK(!view.active_missing_from_supported);

    FakeLanguageService absent_svc;
    absent_svc.current_result = {ServiceResult::success(), pack_language_code("ru")};
    LanguagePicker absent_picker(absent_svc, 1920, 1080);
    CHECK(absent_picker.view().screen == PickerScreen::List);
    CHECK(absent_picker.view().applied_code == pack_language_code("ru"));
    CHECK(absent_picker.view().focused_index == 0u);
    CHECK(absent_picker.view().active_missing_from_supported);

    FakeLanguageService err_svc;
    err_svc.available_result = {ServiceResult::failure(0x7788), {}};
    LanguagePicker err_picker(err_svc, 1920, 1080);
    CHECK(err_picker.view().screen == PickerScreen::Error);
    CHECK(err_picker.view().error_code == 0x7788u);
}

std::vector<LanguageCode> eighteen_horizon_codes() {
    return {
        pack_language_code("ja"),
        pack_language_code("en-US"),
        pack_language_code("fr"),
        pack_language_code("de"),
        pack_language_code("it"),
        pack_language_code("es"),
        pack_language_code("ko"),
        pack_language_code("nl"),
        pack_language_code("pt"),
        pack_language_code("ru"),
        pack_language_code("en-GB"),
        pack_language_code("fr-CA"),
        pack_language_code("es-419"),
        pack_language_code("zh-Hans"),
        pack_language_code("zh-Hant"),
        pack_language_code("pt-BR"),
        pack_language_code("pl"),
        pack_language_code("th"),
    };
}

void test_controller_navigation_repeat_and_scrolling_keep_applied_code_separate() {
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("ja")};
    svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};

    LanguagePicker picker(svc, 1920, 1080);
    CHECK(picker.view().focused_index == 0u);
    CHECK(picker.view().applied_code == pack_language_code("ja"));

    // Initial Down press moves immediately by 1; holding through delay triggers controlled repeat.
    PickerInput down;
    down.nav_down_held = true;
    CHECK(picker.step(down).dirty);
    CHECK(picker.view().focused_index == 1u);

    for (int f = 0; f < 19; ++f) {
        CHECK(!picker.step(down).dirty);
        CHECK(picker.view().focused_index == 1u);
    }
    CHECK(picker.step(down).dirty);
    CHECK(picker.view().focused_index == 2u);
    CHECK(picker.view().applied_code == pack_language_code("ja"));

    // Scroll far down beyond visible rows (9 at 1080p) and check scroll_offset advances.
    PickerInput release;
    picker.step(release);
    for (int i = 0; i < 12; ++i) {
        picker.step(down);
        picker.step(release);
    }
    CHECK(picker.view().focused_index == 14u);
    CHECK(picker.view().scroll_offset > 0u);
    CHECK(picker.view().applied_code == pack_language_code("ja"));

    // Wrap around from first row -> last row on Up.
    PickerInput up;
    up.nav_up_held = true;
    for (int i = 0; i < 15; ++i) {
        picker.step(up);
        picker.step(release);
    }
    CHECK(picker.view().focused_index == 17u);
}

void test_dialog_confirm_cancel_active_noop_and_held_button_guards() {
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
    svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};

    LanguagePicker picker(svc, 1920, 1080);
    CHECK(picker.view().focused_index == 1u); // en-US

    // Pressing A on already-active language is a safe no-op (never opens confirmation).
    PickerInput a_press;
    a_press.a_held = true;
    CHECK(picker.step(a_press).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().applied_code == pack_language_code("en-US"));

    PickerInput idle;
    picker.step(idle);

    // Move focus to index 2 ("fr") and press A: opens dialog. Holding A must NEVER confirm!
    PickerInput down;
    down.nav_down_held = true;
    picker.step(down);
    picker.step(idle);
    CHECK(picker.view().focused_index == 2u);

    picker.step(a_press);
    CHECK(picker.view().screen == PickerScreen::ConfirmDialog);
    CHECK(picker.view().dialog_target_index == 2u);

    for (int f = 0; f < 30; ++f) {
        CHECK(!picker.step(a_press).dirty);
        CHECK(picker.view().screen == PickerScreen::ConfirmDialog);
    }

    // Cancel with B restores focus to pre-dialog row and keeps settings untouched.
    PickerInput b_press;
    b_press.b_held = true;
    CHECK(picker.step(b_press).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().focused_index == 2u);
    CHECK(picker.view().applied_code == pack_language_code("en-US"));

    picker.step(idle);

    // Held A across boundary_reset (sleep/wake, focus loss, dock/undock) cannot leak.
    PickerInput boundary_a;
    boundary_a.boundary_reset = true;
    boundary_a.a_held = true;
    picker.step(boundary_a);
    CHECK(!picker.step(a_press).dirty);
    CHECK(picker.view().screen == PickerScreen::List);

    // Release and press A cleanly -> opens dialog; release and press A again -> enters Applying, then verifies Result.
    picker.step(idle);
    picker.step(a_press);
    CHECK(picker.view().screen == PickerScreen::ConfirmDialog);
    picker.step(idle);
    CHECK(picker.step(a_press).dirty);
    CHECK(picker.view().screen == PickerScreen::Applying);
    CHECK(svc.apply_calls.empty());
    CHECK(picker.step(idle).dirty);
    CHECK(picker.view().screen == PickerScreen::Result);
    CHECK(picker.view().apply_report.outcome == ApplyOutcome::Verified);
    CHECK(picker.view().applied_code == pack_language_code("fr"));
    CHECK(svc.apply_calls.size() == 1u);

    // Dismiss Result -> returns to List with focused_index preserved and transient banner clear.
    CHECK(picker.step(a_press).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().focused_index == 2u);
    CHECK(picker.view().banner_text.empty());

    // Selecting already-active "fr" is now a zero-write no-op.
    picker.step(idle);
    CHECK(picker.step(a_press).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(svc.apply_calls.size() == 1u);
    picker.step(idle);
    picker.step(down);
    CHECK(picker.view().banner_text.empty());
}

void test_plus_exits_cleanly_from_list_dialog_and_error() {
    FakeLanguageService svc;
    LanguagePicker picker(svc, 1920, 1080);

    PickerInput plus;
    plus.plus_held = true;
    CHECK(picker.step(plus).exit_requested);

    // Held Plus across boundary_reset must not immediately exit on resume.
    LanguagePicker dialog_picker(svc, 1920, 1080);
    PickerInput down;
    down.nav_down_held = true;
    dialog_picker.step(down);
    PickerInput idle;
    dialog_picker.step(idle);
    PickerInput a;
    a.a_held = true;
    dialog_picker.step(a);
    dialog_picker.step(idle);
    CHECK(dialog_picker.view().screen == PickerScreen::ConfirmDialog);

    PickerInput boundary_plus;
    boundary_plus.boundary_reset = true;
    boundary_plus.plus_held = true;
    CHECK(!dialog_picker.step(boundary_plus).exit_requested);
    CHECK(!dialog_picker.step(plus).exit_requested);
    dialog_picker.step(idle);
    CHECK(dialog_picker.step(plus).exit_requested);

    FakeLanguageService err_svc;
    err_svc.current_result = {ServiceResult::failure(0x1111), 0ULL};
    LanguagePicker err_picker(err_svc, 1920, 1080);
    CHECK(err_picker.view().screen == PickerScreen::Error);
    CHECK(err_picker.step(plus).exit_requested);
}

void test_touch_selects_row_requires_explicit_confirm_and_supports_drag_scroll() {
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("ja")};
    svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};

    LanguagePicker picker(svc, 1920, 1080);
    CHECK(picker.view().focused_index == 0u);
    CHECK(picker.view().scroll_offset == 0u);

    // Tap row 3: moves focus only, does NOT open confirmation dialog.
    const Rect r3 = picker.view().layout.row_rects[3];
    const int rx = r3.x + r3.w / 2;
    const int ry = r3.y + r3.h / 2;
    PickerInput tap_down;
    tap_down.touch = {true, rx, ry};
    picker.step(tap_down);
    PickerInput idle;
    CHECK(picker.step(idle).dirty);
    CHECK(picker.view().focused_index == 3u);
    CHECK(picker.view().screen == PickerScreen::List);

    // Tap already-focused row 3: opens confirmation dialog (never bypasses confirmation).
    picker.step(tap_down);
    CHECK(picker.step(idle).dirty);
    CHECK(picker.view().screen == PickerScreen::ConfirmDialog);
    CHECK(picker.view().dialog_target_index == 3u);

    // Tap Cancel button inside dialog: cancels and restores focus to row 3.
    const Rect cancel_btn = picker.view().layout.dialog_cancel_button;
    PickerInput tap_cancel;
    tap_cancel.touch = {true, cancel_btn.x + cancel_btn.w / 2, cancel_btn.y + cancel_btn.h / 2};
    picker.step(tap_cancel);
    CHECK(picker.step(idle).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().focused_index == 3u);
    CHECK(picker.view().applied_code == pack_language_code("ja"));

    // Tap Select footer button, then Confirm button: enters Applying -> verifies Result -> dismiss back to List.
    const Rect sel_btn = picker.view().layout.select_button;
    PickerInput tap_select;
    tap_select.touch = {true, sel_btn.x + sel_btn.w / 2, sel_btn.y + sel_btn.h / 2};
    picker.step(tap_select);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::ConfirmDialog);

    const Rect ok_btn = picker.view().layout.dialog_confirm_button;
    PickerInput tap_ok;
    tap_ok.touch = {true, ok_btn.x + ok_btn.w / 2, ok_btn.y + ok_btn.h / 2};
    picker.step(tap_ok);
    CHECK(picker.step(idle).dirty);
    CHECK(picker.view().screen == PickerScreen::Applying);
    CHECK(picker.step(idle).dirty);
    CHECK(picker.view().screen == PickerScreen::Result);
    CHECK(picker.view().applied_code == pack_language_code("de"));
    PickerInput b_dismiss;
    b_dismiss.b_held = true;
    CHECK(picker.step(b_dismiss).dirty);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::List);

    // Drag upward inside list_area scrolls rows down without tapping or changing focus.
    const int drag_x = picker.view().layout.list_area.x + 100;
    const int drag_start_y = picker.view().layout.list_area.y + 300;
    PickerInput drag0;
    drag0.touch = {true, drag_x, drag_start_y};
    picker.step(drag0);
    PickerInput drag1;
    drag1.touch = {true, drag_x, drag_start_y - picker.view().layout.row_pitch * 3};
    CHECK(picker.step(drag1).dirty);
    CHECK(picker.view().scroll_offset >= 2u);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().focused_index == 3u);

    // Boundary reset during touch suppresses tap upon release.
    const Rect r5 = picker.view().layout.row_rects[5];
    PickerInput held_touch;
    held_touch.touch = {true, r5.x + r5.w / 2, r5.y + r5.h / 2};
    picker.step(held_touch);
    PickerInput reset_touch = held_touch;
    reset_touch.boundary_reset = true;
    picker.step(reset_touch);
    CHECK(!picker.step(idle).dirty);
    CHECK(picker.view().focused_index == 3u);
}

void test_confirmed_apply_verifies_readback_and_refreshes_indicator_and_ui_language() {
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
    svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};

    LanguagePicker picker(svc, 1920, 1080);
    CHECK(svc.apply_calls.empty());
    CHECK(svc.read_current_calls == 1u);
    CHECK(picker.view().strings.ui_language == UiLanguage::En);

    // Move to zh-Hans (index 13) and open ConfirmDialog.
    PickerInput down;
    down.nav_down_held = true;
    PickerInput idle;
    for (int i = 0; i < 12; ++i) {
        picker.step(down);
        picker.step(idle);
    }
    CHECK(picker.view().entries[picker.view().focused_index].tag == "zh-Hans");

    PickerInput a;
    a.a_held = true;
    picker.step(a);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::ConfirmDialog);
    CHECK(svc.apply_calls.empty());

    // Confirm transitions to Applying FIRST with zero writes so UI renders Applying before IPC.
    CHECK(picker.step(a).dirty);
    CHECK(picker.view().screen == PickerScreen::Applying);
    CHECK(picker.view().apply_report.target_entry.code == pack_language_code("zh-Hans"));
    CHECK(picker.view().apply_report.outcome == ApplyOutcome::None);
    CHECK(svc.apply_calls.empty());

    // Next step executes apply_language ONCE and readback ONCE, then enters Result(Verified).
    CHECK(picker.step(idle).dirty);
    CHECK(svc.apply_calls.size() == 1u);
    CHECK(svc.apply_calls[0] == pack_language_code("zh-Hans"));
    CHECK(svc.read_current_calls == 2u);

    const PickerView& res_view = picker.view();
    CHECK(res_view.screen == PickerScreen::Result);
    CHECK(res_view.apply_report.outcome == ApplyOutcome::Verified);
    CHECK(res_view.apply_report.target_entry.code == pack_language_code("zh-Hans"));
    CHECK(res_view.apply_report.actual_entry.code == pack_language_code("zh-Hans"));
    CHECK(res_view.applied_code == pack_language_code("zh-Hans"));
    CHECK(res_view.active_entry.code == pack_language_code("zh-Hans"));
    CHECK(!res_view.active_missing_from_supported);
    CHECK(res_view.strings.ui_language == UiLanguage::ZhHans);
    CHECK(res_view.entries[13].is_active);
    CHECK(!res_view.entries[1].is_active);

    // Dismissing Result with A returns cleanly to List.
    CHECK(picker.step(a).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().focused_index == 13u);
}

void test_rejected_apply_reports_exact_error_without_readback_or_mutation() {
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
    svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};
    svc.apply_result = ServiceResult::failure(0x9922);

    LanguagePicker picker(svc, 1920, 1080);
    PickerInput down;
    down.nav_down_held = true;
    PickerInput idle;
    PickerInput a;
    a.a_held = true;
    PickerInput b;
    b.b_held = true;

    // Move from en-US (1) to fr (2), open dialog, confirm.
    picker.step(down);
    picker.step(idle);
    picker.step(a);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::ConfirmDialog);

    CHECK(picker.step(a).dirty);
    CHECK(picker.view().screen == PickerScreen::Applying);
    CHECK(svc.apply_calls.empty());

    CHECK(picker.step(idle).dirty);
    CHECK(svc.apply_calls.size() == 1u);
    CHECK(svc.apply_calls[0] == pack_language_code("fr"));
    // Rejected write must NOT perform post-write readback or fallback writes.
    CHECK(svc.read_current_calls == 1u);

    const PickerView& v = picker.view();
    CHECK(v.screen == PickerScreen::Result);
    CHECK(v.apply_report.outcome == ApplyOutcome::Rejected);
    CHECK(v.apply_report.error_code == 0x9922u);
    CHECK(v.apply_report.target_entry.code == pack_language_code("fr"));
    CHECK(v.applied_code == pack_language_code("en-US"));
    CHECK(v.active_entry.code == pack_language_code("en-US"));
    CHECK(v.strings.ui_language == UiLanguage::En);
    CHECK(v.entries[1].is_active);
    CHECK(!v.entries[2].is_active);

    // Dismissing with B returns to List with focus preserved on attempted row.
    CHECK(picker.step(b).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().focused_index == 2u);
}

void test_readback_failure_and_mismatch_report_accurately() {
    // 1. Write succeeds, readback IPC fails -> ReadbackFailed (never false success).
    {
        FakeLanguageService svc;
        svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
        svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};
        svc.update_current_on_apply_success = false;

        LanguagePicker picker(svc, 1920, 1080);
        PickerInput down;
        down.nav_down_held = true;
        PickerInput idle;
        PickerInput a;
        a.a_held = true;

        picker.step(down);
        picker.step(idle);
        picker.step(a);
        picker.step(idle);
        picker.step(a);
        CHECK(picker.view().screen == PickerScreen::Applying);

        svc.current_result = {ServiceResult::failure(0x5A10), 0ULL};
        CHECK(picker.step(idle).dirty);
        CHECK(svc.apply_calls.size() == 1u);
        CHECK(svc.read_current_calls == 2u);
        CHECK(picker.view().screen == PickerScreen::Result);
        CHECK(picker.view().apply_report.outcome == ApplyOutcome::ReadbackFailed);
        CHECK(picker.view().apply_report.error_code == 0x5A10u);
        CHECK(picker.view().apply_report.target_entry.code == pack_language_code("fr"));
        CHECK(picker.view().applied_code == pack_language_code("en-US"));
    }

    // 2. Write succeeds, readback returns different non-zero code -> ReadbackMismatch & state refreshed.
    {
        FakeLanguageService svc;
        svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
        svc.available_result = {
            ServiceResult::success(),
            {pack_language_code("ja"), pack_language_code("en-US"), pack_language_code("zh-Hans")},
        };
        svc.update_current_on_apply_success = false;

        LanguagePicker picker(svc, 1920, 1080);
        PickerInput down;
        down.nav_down_held = true;
        PickerInput idle;
        PickerInput a;
        a.a_held = true;

        picker.step(down); // zh-Hans (index 2)
        picker.step(idle);
        picker.step(a);
        picker.step(idle);
        picker.step(a);
        CHECK(picker.view().screen == PickerScreen::Applying);

        // Simulate firmware keeping "ja" instead of requested "zh-Hans".
        svc.current_result = {ServiceResult::success(), pack_language_code("ja")};
        CHECK(picker.step(idle).dirty);
        CHECK(picker.view().screen == PickerScreen::Result);
        CHECK(picker.view().apply_report.outcome == ApplyOutcome::ReadbackMismatch);
        CHECK(picker.view().apply_report.target_entry.code == pack_language_code("zh-Hans"));
        CHECK(picker.view().apply_report.actual_entry.code == pack_language_code("ja"));
        CHECK(picker.view().apply_report.actual_entry.tag == "ja");
        CHECK(picker.view().applied_code == pack_language_code("ja"));
        CHECK(picker.view().active_entry.code == pack_language_code("ja"));
        CHECK(picker.view().entries[0].is_active);
        CHECK(!picker.view().entries[1].is_active);
        CHECK(!picker.view().entries[2].is_active);
    }

    // 3. Write succeeds, readback returns 0 -> ReadbackMismatch, applied_code not overwritten with 0.
    {
        FakeLanguageService svc;
        svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
        svc.available_result = {
            ServiceResult::success(),
            {pack_language_code("ja"), pack_language_code("en-US")},
        };
        svc.update_current_on_apply_success = false;

        LanguagePicker picker(svc, 1920, 1080);
        PickerInput up;
        up.nav_up_held = true;
        PickerInput idle;
        PickerInput a;
        a.a_held = true;

        picker.step(up); // ja (index 0)
        picker.step(idle);
        picker.step(a);
        picker.step(idle);
        picker.step(a);
        svc.current_result = {ServiceResult::success(), 0ULL};
        CHECK(picker.step(idle).dirty);
        CHECK(picker.view().screen == PickerScreen::Result);
        CHECK(picker.view().apply_report.outcome == ApplyOutcome::ReadbackMismatch);
        CHECK(picker.view().apply_report.actual_entry.code == 0ULL);
        CHECK(!picker.view().apply_report.actual_entry.tag.empty());
        CHECK(picker.view().applied_code == pack_language_code("en-US"));
    }

    // 4. Write succeeds, readback returns malformed non-zero tag -> ReadbackMismatch with safe hex display tag.
    {
        FakeLanguageService svc;
        svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
        svc.available_result = {
            ServiceResult::success(),
            {pack_language_code("ja"), pack_language_code("en-US")},
        };
        svc.update_current_on_apply_success = false;

        LanguagePicker picker(svc, 1920, 1080);
        PickerInput up;
        up.nav_up_held = true;
        PickerInput idle;
        PickerInput a;
        a.a_held = true;

        picker.step(up);
        picker.step(idle);
        picker.step(a);
        picker.step(idle);
        picker.step(a);
        svc.current_result = {ServiceResult::success(), 0x0001020304050607ULL};
        CHECK(picker.step(idle).dirty);
        CHECK(picker.view().screen == PickerScreen::Result);
        CHECK(picker.view().apply_report.outcome == ApplyOutcome::ReadbackMismatch);
        CHECK(picker.view().apply_report.actual_entry.code == 0x0001020304050607ULL);
        CHECK(picker.view().apply_report.actual_entry.tag == "0x0001020304050607");
        CHECK(picker.view().applied_code == 0x0001020304050607ULL);
        CHECK(picker.view().active_missing_from_supported);
    }
}

void test_applying_and_result_guards_touch_controls_and_abort_before_write() {
    // 1. If applet closes while on Applying before write starts, exit cleanly with ZERO writes.
    {
        FakeLanguageService svc;
        svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
        svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};
        LanguagePicker picker(svc, 1920, 1080);

        PickerInput down;
        down.nav_down_held = true;
        PickerInput idle;
        PickerInput a;
        a.a_held = true;
        picker.step(down);
        picker.step(idle);
        picker.step(a);
        picker.step(idle);
        picker.step(a);
        CHECK(picker.view().screen == PickerScreen::Applying);

        PickerInput closing;
        closing.applet_running = false;
        const PickerStepResult step = picker.step(closing);
        CHECK(step.exit_requested);
        CHECK(svc.apply_calls.empty());
    }

    // 2. Applying blocks Plus/A/B/nav/touch & handles boundary_reset; Result blocks held buttons and supports touch Continue/Exit.
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
    svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};
    LanguagePicker picker(svc, 1920, 1080);

    PickerInput down;
    down.nav_down_held = true;
    PickerInput idle;
    PickerInput a;
    a.a_held = true;

    picker.step(down); // fr (index 2)
    picker.step(idle);
    picker.step(a);
    picker.step(idle);
    picker.step(a); // Enter Applying
    CHECK(picker.view().screen == PickerScreen::Applying);

    // Send boundary_reset + Plus + A + B + nav + active touch during Applying step.
    const Rect cont_btn = picker.view().layout.result_continue_button;
    CHECK(cont_btn.w >= 200 && cont_btn.h >= 64);
    CHECK(picker.view().layout.result_box.contains(cont_btn.x + cont_btn.w / 2, cont_btn.y + cont_btn.h / 2));

    PickerInput noisy_apply;
    noisy_apply.boundary_reset = true;
    noisy_apply.plus_held = true;
    noisy_apply.a_held = true;
    noisy_apply.b_held = true;
    noisy_apply.nav_down_held = true;
    noisy_apply.touch = {true, cont_btn.x + cont_btn.w / 2, cont_btn.y + cont_btn.h / 2};

    const PickerStepResult apply_step = picker.step(noisy_apply);
    CHECK(!apply_step.exit_requested);
    CHECK(apply_step.dirty);
    CHECK(svc.apply_calls.size() == 1u);
    CHECK(picker.view().screen == PickerScreen::Result);

    // Continuing to hold A/B/Plus or releasing the suppressed touch on Result must NOT dismiss or exit.
    PickerInput held_after_apply = noisy_apply;
    held_after_apply.boundary_reset = false;
    held_after_apply.touch.active = false;
    const PickerStepResult held_step = picker.step(held_after_apply);
    CHECK(!held_step.exit_requested);
    CHECK(!held_step.dirty);
    CHECK(picker.view().screen == PickerScreen::Result);

    // Release all buttons, then cleanly tap result_continue_button -> returns to List.
    picker.step(idle);
    PickerInput tap_cont;
    tap_cont.touch = {true, cont_btn.x + cont_btn.w / 2, cont_btn.y + cont_btn.h / 2};
    picker.step(tap_cont);
    CHECK(picker.step(idle).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().focused_index == 2u);

    // Confirm another language, then tap exit_button from Result screen -> clean exit.
    picker.step(down); // de (index 3)
    picker.step(idle);
    picker.step(a);
    picker.step(idle);
    picker.step(a);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::Result);

    const Rect exit_btn = picker.view().layout.exit_button;
    PickerInput tap_exit;
    tap_exit.touch = {true, exit_btn.x + exit_btn.w / 2, exit_btn.y + exit_btn.h / 2};
    picker.step(tap_exit);
    CHECK(picker.step(idle).exit_requested);
    CHECK(svc.apply_calls.size() == 2u);
}

void test_same_session_multiple_switches_and_switch_back() {
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("en-US")};
    svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};

    LanguagePicker picker(svc, 1920, 1080);
    PickerInput down;
    down.nav_down_held = true;
    PickerInput up;
    up.nav_up_held = true;
    PickerInput idle;
    PickerInput a;
    a.a_held = true;
    PickerInput b;
    b.b_held = true;
    PickerInput plus;
    plus.plus_held = true;

    // 1. Switch en-US (1) -> zh-Hans (13)
    for (int i = 0; i < 12; ++i) {
        picker.step(down);
        picker.step(idle);
    }
    picker.step(a);
    picker.step(idle);
    picker.step(a);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::Result);
    CHECK(picker.view().apply_report.outcome == ApplyOutcome::Verified);
    CHECK(picker.view().applied_code == pack_language_code("zh-Hans"));
    CHECK(picker.view().strings.ui_language == UiLanguage::ZhHans);

    // Holding A after dismissing Result with A must not reopen ConfirmDialog.
    CHECK(picker.step(a).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(!picker.step(a).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    picker.step(idle);

    // 2. Switch zh-Hans (13) -> zh-Hant (14)
    picker.step(down);
    picker.step(idle);
    picker.step(a);
    picker.step(idle);
    picker.step(a);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::Result);
    CHECK(picker.view().applied_code == pack_language_code("zh-Hant"));
    CHECK(picker.view().strings.ui_language == UiLanguage::ZhHant);
    picker.step(b);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::List);

    // 3. Switch back zh-Hant (14) -> en-US (1)
    for (int i = 0; i < 13; ++i) {
        picker.step(up);
        picker.step(idle);
    }
    CHECK(picker.view().focused_index == 1u);
    picker.step(a);
    picker.step(idle);
    picker.step(a);
    picker.step(idle);
    CHECK(picker.view().screen == PickerScreen::Result);
    CHECK(picker.view().applied_code == pack_language_code("en-US"));
    CHECK(picker.view().strings.ui_language == UiLanguage::En);
    CHECK(picker.view().entries[1].is_active);
    CHECK(!picker.view().entries[13].is_active);
    CHECK(!picker.view().entries[14].is_active);

    CHECK(svc.apply_calls.size() == 3u);
    CHECK(svc.apply_calls[0] == pack_language_code("zh-Hans"));
    CHECK(svc.apply_calls[1] == pack_language_code("zh-Hant"));
    CHECK(svc.apply_calls[2] == pack_language_code("en-US"));

    // Clean Plus exit directly from Result after verified switch-back.
    CHECK(picker.step(plus).exit_requested);
}

void test_frame0_boundary_reset_suppresses_held_a_plus_and_touch_until_release() {
    FakeLanguageService svc;
    svc.current_result = {ServiceResult::success(), pack_language_code("ja")};
    svc.available_result = {ServiceResult::success(), eighteen_horizon_codes()};

    {
        kailocale::InputSnapshot snap;
        snap.applet_running = true;
        snap.nav_down_held = true;
        snap.a_held = true;
        snap.boundary_reset = true;
        snap.touch = {true, 120, 340};
        const PickerInput converted = PickerInput::from_snapshot(snap, 2560, 1440);
        CHECK(converted.applet_running);
        CHECK(!converted.nav_up_held);
        CHECK(converted.nav_down_held);
        CHECK(converted.a_held);
        CHECK(!converted.b_held);
        CHECK(!converted.plus_held);
        CHECK(converted.boundary_reset);
        CHECK(converted.viewport_width == 2560u);
        CHECK(converted.viewport_height == 1440u);
        CHECK(converted.touch.active);
        CHECK(converted.touch.x == 120);
        CHECK(converted.touch.y == 340);
    }

    // 1. Held A, Plus, nav_down, and touch on Exit button at Frame 0 (hbmenu launch carryover).
    {
        LanguagePicker picker(svc, 1920, 1080);
        const Rect exit_btn = picker.view().layout.exit_button;
        PickerInput frame0;
        frame0.boundary_reset = true;
        frame0.a_held = true;
        frame0.plus_held = true;
        frame0.nav_down_held = true;
        frame0.touch = {true, exit_btn.x + exit_btn.w / 2, exit_btn.y + exit_btn.h / 2};

        const PickerStepResult r0 = picker.step(frame0);
        CHECK(!r0.exit_requested);
        CHECK(!r0.dirty);
        CHECK(picker.view().screen == PickerScreen::List);
        CHECK(picker.view().focused_index == 0u);

        // Continuing to hold on Frame 1 (boundary_reset cleared) must still suppress everything.
        PickerInput frame1 = frame0;
        frame1.boundary_reset = false;
        const PickerStepResult r1 = picker.step(frame1);
        CHECK(!r1.exit_requested);
        CHECK(!r1.dirty);
        CHECK(picker.view().screen == PickerScreen::List);
        CHECK(picker.view().focused_index == 0u);

        // Releasing touch while A and Plus remain held must NOT trigger an Exit tap.
        PickerInput frame2 = frame1;
        frame2.touch = {};
        const PickerStepResult r2 = picker.step(frame2);
        CHECK(!r2.exit_requested);
        CHECK(!r2.dirty);
        CHECK(picker.view().screen == PickerScreen::List);

        // Release all buttons -> still idle, zero writes.
        PickerInput idle;
        const PickerStepResult r3 = picker.step(idle);
        CHECK(!r3.exit_requested);
        CHECK(!r3.dirty);
        CHECK(svc.apply_calls.empty());

        // Fresh nav_down moves focus, fresh A opens ConfirmDialog, fresh Plus exits cleanly.
        PickerInput down;
        down.nav_down_held = true;
        CHECK(picker.step(down).dirty);
        CHECK(picker.view().focused_index == 1u);
        picker.step(idle);

        PickerInput a;
        a.a_held = true;
        CHECK(picker.step(a).dirty);
        CHECK(picker.view().screen == PickerScreen::ConfirmDialog);
        picker.step(idle);

        PickerInput plus;
        plus.plus_held = true;
        CHECK(picker.step(plus).exit_requested);
    }

    // 2. Held touch over a non-active list row or Select button on Frame 0 must not select or confirm on release.
    {
        LanguagePicker picker(svc, 1920, 1080);
        const Rect row2 = picker.view().layout.row_rects[2];
        PickerInput frame0;
        frame0.boundary_reset = true;
        frame0.touch = {true, row2.x + row2.w / 2, row2.y + row2.h / 2};
        CHECK(!picker.step(frame0).dirty);

        PickerInput held = frame0;
        held.boundary_reset = false;
        CHECK(!picker.step(held).dirty);

        PickerInput idle;
        CHECK(!picker.step(idle).dirty);
        CHECK(picker.view().focused_index == 0u);
        CHECK(picker.view().screen == PickerScreen::List);
    }
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*fn)();
    };
    const TestCase cases[] = {
        {"layout_scales_cleanly_across_1080p_1440p_4k_with_comfortable_hit_targets", test_layout_scales_cleanly_across_1080p_1440p_4k_with_comfortable_hit_targets},
        {"picker_initializes_focus_on_active_language_and_handles_absent_or_error", test_picker_initializes_focus_on_active_language_and_handles_absent_or_error},
        {"controller_navigation_repeat_and_scrolling_keep_applied_code_separate", test_controller_navigation_repeat_and_scrolling_keep_applied_code_separate},
        {"dialog_confirm_cancel_active_noop_and_held_button_guards", test_dialog_confirm_cancel_active_noop_and_held_button_guards},
        {"plus_exits_cleanly_from_list_dialog_and_error", test_plus_exits_cleanly_from_list_dialog_and_error},
        {"touch_selects_row_requires_explicit_confirm_and_supports_drag_scroll", test_touch_selects_row_requires_explicit_confirm_and_supports_drag_scroll},
        {"confirmed_apply_verifies_readback_and_refreshes_indicator_and_ui_language", test_confirmed_apply_verifies_readback_and_refreshes_indicator_and_ui_language},
        {"rejected_apply_reports_exact_error_without_readback_or_mutation", test_rejected_apply_reports_exact_error_without_readback_or_mutation},
        {"readback_failure_and_mismatch_report_accurately", test_readback_failure_and_mismatch_report_accurately},
        {"applying_and_result_guards_touch_controls_and_abort_before_write", test_applying_and_result_guards_touch_controls_and_abort_before_write},
        {"same_session_multiple_switches_and_switch_back", test_same_session_multiple_switches_and_switch_back},
        {"frame0_boundary_reset_suppresses_held_a_plus_and_touch_until_release", test_frame0_boundary_reset_suppresses_held_a_plus_and_touch_until_release},
    };

    int passed = 0;
    for (const auto& tc : cases) {
        try {
            tc.fn();
            std::cout << "[PASS] " << tc.name << "\n";
            ++passed;
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << tc.name << ": " << ex.what() << "\n";
            return 1;
        }
    }
    std::cout << "Passed " << passed << " test(s).\n";
    return 0;
}
