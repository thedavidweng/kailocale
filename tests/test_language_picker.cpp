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

    LanguageReadResult read_current_language() override {
        return current_result;
    }

    AvailableLanguagesResult read_available_languages() override {
        return available_result;
    }

    SystemTheme read_system_theme() override {
        return theme;
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

    // Release and press A cleanly -> opens dialog; release and press A again -> read-only preview confirms.
    picker.step(idle);
    picker.step(a_press);
    CHECK(picker.view().screen == PickerScreen::ConfirmDialog);
    picker.step(idle);
    CHECK(picker.step(a_press).dirty);
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().focused_index == 2u);
    CHECK(picker.view().applied_code == pack_language_code("en-US"));
    CHECK(!picker.view().banner_text.empty());

    // Moving focus clears transient Info notice without disturbing Warning notice when active is unsupported.
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

    // Tap Select footer button, then Confirm button: completes read-only preview without write.
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
    CHECK(picker.view().screen == PickerScreen::List);
    CHECK(picker.view().applied_code == pack_language_code("ja"));

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
