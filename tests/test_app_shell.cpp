#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kailocale/app_shell.hpp"
#include "kailocale/language_catalog.hpp"
#include "kailocale/language_picker.hpp"

namespace {

using kailocale::ApplyOutcome;
using kailocale::AvailableLanguagesResult;
using kailocale::InputSnapshot;
using kailocale::LanguageCode;
using kailocale::LanguagePicker;
using kailocale::LanguageReadResult;
using kailocale::LanguageService;
using kailocale::PickerScreen;
using kailocale::PickerView;
using kailocale::PlatformServices;
using kailocale::ServiceResult;
using kailocale::ShellOutcome;
using kailocale::ShellScreenKind;
using kailocale::ShellView;
using kailocale::SystemTheme;
using kailocale::pack_language_code;

class FakeLanguageService final : public LanguageService {
public:
    LanguageReadResult current_result{ServiceResult::success(), pack_language_code("en-US")};
    AvailableLanguagesResult available_result{
        ServiceResult::success(),
        {pack_language_code("ja"), pack_language_code("en-US"), pack_language_code("zh-Hans")},
    };
    SystemTheme theme = SystemTheme::Dark;
    ServiceResult apply_result = ServiceResult::success();
    bool update_current_on_apply_success = true;
    std::vector<LanguageCode> apply_calls;

    LanguageReadResult read_current_language() override {
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

class FakePlatformServices final : public PlatformServices {
public:
    std::vector<InputSnapshot> frames;
    std::vector<std::string> events;
    std::vector<ShellView> rendered_status;
    std::vector<PickerView> rendered_picker;
    const std::vector<LanguageCode>* apply_calls_ptr = nullptr;
    std::vector<std::size_t> apply_count_at_render;

    ServiceResult exit_lock_result = ServiceResult::success();
    ServiceResult display_result = ServiceResult::success();
    ServiceResult resize_result = ServiceResult::success();
    ServiceResult input_result = ServiceResult::success();
    ServiceResult set_result = ServiceResult::success();
    ServiceResult setsys_result = ServiceResult::success();
    ServiceResult shared_font_result = ServiceResult::success();
    std::uint32_t width = 1920;
    std::uint32_t height = 1080;

    ServiceResult init_exit_lock() override {
        events.push_back("init:exit_lock");
        return exit_lock_result;
    }
    void exit_exit_lock() override { events.push_back("exit:exit_lock"); }

    ServiceResult init_display() override {
        events.push_back("init:display");
        return display_result;
    }
    ServiceResult resize_display() override {
        events.push_back("resize:display");
        width = 3840;
        height = 2160;
        return resize_result;
    }
    void exit_display() override { events.push_back("exit:display"); }

    ServiceResult init_input() override {
        events.push_back("init:input");
        return input_result;
    }
    void exit_input() override { events.push_back("exit:input"); }

    ServiceResult init_set() override {
        events.push_back("init:set");
        return set_result;
    }
    void exit_set() override { events.push_back("exit:set"); }

    ServiceResult init_setsys() override {
        events.push_back("init:setsys");
        return setsys_result;
    }
    void exit_setsys() override { events.push_back("exit:setsys"); }

    ServiceResult init_shared_font() override {
        events.push_back("init:shared_font");
        return shared_font_result;
    }
    void exit_shared_font() override { events.push_back("exit:shared_font"); }

    std::uint32_t viewport_width() const override { return width; }
    std::uint32_t viewport_height() const override { return height; }

    InputSnapshot poll_frame() override {
        ++poll_count;
        if (frame_cursor_ < frames.size()) {
            return frames[frame_cursor_++];
        }
        InputSnapshot stop;
        stop.applet_running = false;
        return stop;
    }

    void render_status(const ShellView& view) override {
        rendered_status.push_back(view);
    }

    void render_picker(const PickerView& view) override {
        rendered_picker.push_back(view);
        apply_count_at_render.push_back(apply_calls_ptr != nullptr ? apply_calls_ptr->size() : 0u);
    }

    std::size_t poll_count = 0;

private:
    std::size_t frame_cursor_ = 0;
};

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::ostringstream oss;                                              \
            oss << __FILE__ << ":" << __LINE__ << " failed check: " << #cond;    \
            throw std::runtime_error(oss.str());                                 \
        }                                                                        \
    } while (false)

void test_clean_startup_plus_exit_and_reverse_teardown() {
    FakePlatformServices fake;
    FakeLanguageService lang;
    InputSnapshot idle;
    InputSnapshot plus;
    plus.plus_held = true;
    fake.frames = {idle, plus};

    kailocale::AppShell shell(fake, lang);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::UserPlus);
    CHECK(outcome.succeeded());
    CHECK(fake.rendered_picker.size() == 1);
    CHECK(fake.rendered_picker[0].screen == PickerScreen::List);
    CHECK(std::string(fake.rendered_picker[0].app_name) == "KaiLocale");
    CHECK(std::string(fake.rendered_picker[0].app_version) == "0.1.1");

    const std::vector<std::string> expected_events = {
        "init:exit_lock",
        "init:display",
        "init:input",
        "init:set",
        "init:setsys",
        "init:shared_font",
        "exit:shared_font",
        "exit:setsys",
        "exit:set",
        "exit:input",
        "exit:display",
        "exit:exit_lock",
    };
    CHECK(fake.events == expected_events);
}

void test_setsys_init_failure_renders_visible_error_and_tears_down_acquired_subset() {
    FakePlatformServices fake;
    FakeLanguageService lang;
    fake.setsys_result = ServiceResult::failure(0x4201);
    InputSnapshot idle;
    InputSnapshot plus;
    plus.plus_held = true;
    fake.frames = {idle, plus};

    kailocale::AppShell shell(fake, lang);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::InitFailed);
    CHECK(!outcome.succeeded());
    CHECK(outcome.failed_stage == "set:sys");
    CHECK(outcome.error_code == 0x4201u);

    CHECK(fake.rendered_status.size() == 1);
    CHECK(fake.rendered_status[0].kind == ShellScreenKind::InitError);
    CHECK(fake.rendered_status[0].stage == "set:sys");
    CHECK(fake.rendered_status[0].error_code == 0x4201u);
    CHECK(fake.rendered_picker.empty());
    CHECK(fake.poll_count == 2);

    const std::vector<std::string> expected_events = {
        "init:exit_lock",
        "init:display",
        "init:input",
        "init:set",
        "init:setsys",
        "exit:set",
        "exit:input",
        "exit:display",
        "exit:exit_lock",
    };
    CHECK(fake.events == expected_events);
}

void test_resolution_change_resizes_and_rerenders_picker_only_when_dirty() {
    FakePlatformServices fake;
    FakeLanguageService lang;
    InputSnapshot res_change;
    res_change.resolution_changed = true;
    InputSnapshot idle;
    InputSnapshot plus;
    plus.plus_held = true;
    fake.frames = {res_change, idle, idle, plus};

    kailocale::AppShell shell(fake, lang);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::UserPlus);
    // Initial render + resolution-change render; idle frames must NOT trigger redundant draws.
    CHECK(fake.rendered_picker.size() == 2);
    CHECK(fake.rendered_picker[0].layout.viewport_width == 1920);
    CHECK(fake.rendered_picker[1].layout.viewport_width == 3840);
    CHECK(fake.rendered_picker[1].layout.viewport_height == 2160);

    const std::vector<std::string> expected_events = {
        "init:exit_lock",
        "init:display",
        "init:input",
        "init:set",
        "init:setsys",
        "init:shared_font",
        "resize:display",
        "exit:shared_font",
        "exit:setsys",
        "exit:set",
        "exit:input",
        "exit:display",
        "exit:exit_lock",
    };
    CHECK(fake.events == expected_events);
}

void test_display_init_failure_skips_render_and_unwinds_exit_lock() {
    FakePlatformServices fake;
    FakeLanguageService lang;
    fake.display_result = ServiceResult::failure(0x9001);

    kailocale::AppShell shell(fake, lang);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::InitFailed);
    CHECK(!outcome.succeeded());
    CHECK(outcome.failed_stage == "display");
    CHECK(outcome.error_code == 0x9001u);
    CHECK(fake.rendered_status.empty());
    CHECK(fake.rendered_picker.empty());
    CHECK(fake.poll_count == 0);

    const std::vector<std::string> expected_events = {
        "init:exit_lock",
        "init:display",
        "exit:exit_lock",
    };
    CHECK(fake.events == expected_events);
}

void test_input_init_failure_renders_error_and_unwinds_cleanly_on_applet_exit() {
    FakePlatformServices fake;
    FakeLanguageService lang;
    fake.input_result = ServiceResult::failure(0x5101);
    InputSnapshot idle;
    InputSnapshot stop;
    stop.applet_running = false;
    fake.frames = {idle, stop};

    kailocale::AppShell shell(fake, lang);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::InitFailed);
    CHECK(!outcome.succeeded());
    CHECK(outcome.failed_stage == "input");
    CHECK(outcome.error_code == 0x5101u);

    CHECK(fake.rendered_status.size() == 1);
    CHECK(fake.rendered_status[0].kind == ShellScreenKind::InitError);
    CHECK(fake.rendered_status[0].stage == "input");
    CHECK(fake.rendered_status[0].error_code == 0x5101u);
    CHECK(fake.rendered_picker.empty());
    CHECK(fake.poll_count == 2);

    const std::vector<std::string> expected_events = {
        "init:exit_lock",
        "init:display",
        "init:input",
        "exit:display",
        "exit:exit_lock",
    };
    CHECK(fake.events == expected_events);
}

void test_applet_exit_request_closes_cleanly() {
    FakePlatformServices fake;
    FakeLanguageService lang;
    InputSnapshot stop;
    stop.applet_running = false;
    fake.frames = {stop};

    kailocale::AppShell shell(fake, lang);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::AppletExit);
    CHECK(outcome.succeeded());
    CHECK(fake.rendered_picker.size() == 1);

    const std::vector<std::string> expected_events = {
        "init:exit_lock",
        "init:display",
        "init:input",
        "init:set",
        "init:setsys",
        "init:shared_font",
        "exit:shared_font",
        "exit:setsys",
        "exit:set",
        "exit:input",
        "exit:display",
        "exit:exit_lock",
    };
    CHECK(fake.events == expected_events);
}

void test_shared_font_init_failure_still_renders_visible_error_and_exits_on_applet_close() {
    FakePlatformServices fake;
    FakeLanguageService lang;
    fake.shared_font_result = ServiceResult::failure(0xA502);
    InputSnapshot res_change;
    res_change.resolution_changed = true;
    InputSnapshot stop;
    stop.applet_running = false;
    fake.frames = {res_change, stop};

    kailocale::AppShell shell(fake, lang);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::InitFailed);
    CHECK(outcome.failed_stage == "shared_font");
    CHECK(outcome.error_code == 0xA502u);
    CHECK(fake.rendered_status.size() == 2);
    CHECK(fake.rendered_status[0].kind == ShellScreenKind::InitError);
    CHECK(fake.rendered_status[1].kind == ShellScreenKind::InitError);

    const std::vector<std::string> expected_events = {
        "init:exit_lock",
        "init:display",
        "init:input",
        "init:set",
        "init:setsys",
        "init:shared_font",
        "resize:display",
        "exit:setsys",
        "exit:set",
        "exit:input",
        "exit:display",
        "exit:exit_lock",
    };
    CHECK(fake.events == expected_events);
}

void test_navigation_dialog_and_service_error_picker_flow_through_app_shell() {
    FakePlatformServices fake;
    FakeLanguageService lang;
    InputSnapshot down;
    down.nav_down_held = true;
    InputSnapshot idle;
    InputSnapshot a;
    a.a_held = true;
    InputSnapshot plus;
    plus.plus_held = true;

    fake.frames = {down, idle, a, idle, a, idle, plus};

    kailocale::AppShell shell(fake, lang);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::UserPlus);
    CHECK(fake.rendered_picker.size() == 5);
    CHECK(fake.rendered_picker[0].focused_index == 1u); // en-US
    CHECK(fake.rendered_picker[1].focused_index == 2u); // moved to zh-Hans
    CHECK(fake.rendered_picker[2].screen == PickerScreen::ConfirmDialog);
    CHECK(fake.rendered_picker[3].screen == PickerScreen::Applying);
    CHECK(fake.rendered_picker[4].screen == PickerScreen::Result);
    CHECK(fake.rendered_picker[4].apply_report.outcome == ApplyOutcome::Verified);
    CHECK(fake.rendered_picker[4].applied_code == pack_language_code("zh-Hans"));

    FakePlatformServices err_fake;
    FakeLanguageService err_lang;
    err_lang.current_result = {ServiceResult::failure(0x3133), 0ULL};
    err_fake.frames = {plus};
    kailocale::AppShell err_shell(err_fake, err_lang);
    const ShellOutcome err_outcome = err_shell.run();
    CHECK(err_outcome.reason == kailocale::ExitReason::UserPlus);
    CHECK(err_fake.rendered_picker.size() == 1);
    CHECK(err_fake.rendered_picker[0].screen == PickerScreen::Error);
    CHECK(err_fake.rendered_picker[0].error_code == 0x3133u);
}

void test_same_session_roundtrip_switch_and_rejected_apply_render_applying_before_ipc() {
    // 1. Switch en-US -> zh-Hans -> en-US -> clean Plus exit.
    {
        FakePlatformServices fake;
        FakeLanguageService lang;
        fake.apply_calls_ptr = &lang.apply_calls;

        InputSnapshot down;
        down.nav_down_held = true;
        InputSnapshot up;
        up.nav_up_held = true;
        InputSnapshot idle;
        InputSnapshot a;
        a.a_held = true;
        InputSnapshot plus;
        plus.plus_held = true;

        // Initial: en-US (idx 1). Move down -> zh-Hans (idx 2), A -> Confirm, A -> Applying, idle -> Result,
        // A -> List, up -> en-US (idx 1), A -> Confirm, A -> Applying, idle -> Result, Plus -> Exit.
        fake.frames = {
            down, idle,
            a, idle,
            a, idle,
            a, idle,
            up, idle,
            a, idle,
            a, idle,
            plus,
        };

        kailocale::AppShell shell(fake, lang);
        const ShellOutcome outcome = shell.run();
        CHECK(outcome.reason == kailocale::ExitReason::UserPlus);
        CHECK(outcome.succeeded());
        CHECK(lang.apply_calls.size() == 2u);
        CHECK(lang.apply_calls[0] == pack_language_code("zh-Hans"));
        CHECK(lang.apply_calls[1] == pack_language_code("en-US"));

        // Verify exact render sequence and that Applying ALWAYS renders before the write runs.
        CHECK(fake.rendered_picker.size() == 10u);
        CHECK(fake.rendered_picker[3].screen == PickerScreen::Applying);
        CHECK(fake.apply_count_at_render[3] == 0u);
        CHECK(fake.rendered_picker[4].screen == PickerScreen::Result);
        CHECK(fake.rendered_picker[4].apply_report.outcome == ApplyOutcome::Verified);
        CHECK(fake.apply_count_at_render[4] == 1u);
        CHECK(fake.rendered_picker[5].screen == PickerScreen::List);
        CHECK(fake.rendered_picker[5].applied_code == pack_language_code("zh-Hans"));
        CHECK(fake.rendered_picker[8].screen == PickerScreen::Applying);
        CHECK(fake.apply_count_at_render[8] == 1u);
        CHECK(fake.rendered_picker[9].screen == PickerScreen::Result);
        CHECK(fake.rendered_picker[9].apply_report.outcome == ApplyOutcome::Verified);
        CHECK(fake.rendered_picker[9].applied_code == pack_language_code("en-US"));
        CHECK(fake.apply_count_at_render[9] == 2u);
    }

    // 2. Rejected write renders Applying then Result(Rejected), keeps original language, and exits cleanly.
    {
        FakePlatformServices fake;
        FakeLanguageService lang;
        lang.apply_result = ServiceResult::failure(0x8811);
        fake.apply_calls_ptr = &lang.apply_calls;

        InputSnapshot down;
        down.nav_down_held = true;
        InputSnapshot idle;
        InputSnapshot a;
        a.a_held = true;
        InputSnapshot plus;
        plus.plus_held = true;
        fake.frames = {down, idle, a, idle, a, idle, plus};

        kailocale::AppShell shell(fake, lang);
        const ShellOutcome outcome = shell.run();
        CHECK(outcome.reason == kailocale::ExitReason::UserPlus);
        CHECK(fake.rendered_picker.size() == 5u);
        CHECK(fake.rendered_picker[3].screen == PickerScreen::Applying);
        CHECK(fake.apply_count_at_render[3] == 0u);
        CHECK(fake.rendered_picker[4].screen == PickerScreen::Result);
        CHECK(fake.rendered_picker[4].apply_report.outcome == ApplyOutcome::Rejected);
        CHECK(fake.rendered_picker[4].apply_report.error_code == 0x8811u);
        CHECK(fake.rendered_picker[4].applied_code == pack_language_code("en-US"));
    }
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*fn)();
    };
    const TestCase cases[] = {
        {"clean_startup_plus_exit_and_reverse_teardown", test_clean_startup_plus_exit_and_reverse_teardown},
        {"setsys_init_failure_renders_visible_error_and_tears_down_acquired_subset", test_setsys_init_failure_renders_visible_error_and_tears_down_acquired_subset},
        {"resolution_change_resizes_and_rerenders_picker_only_when_dirty", test_resolution_change_resizes_and_rerenders_picker_only_when_dirty},
        {"display_init_failure_skips_render_and_unwinds_exit_lock", test_display_init_failure_skips_render_and_unwinds_exit_lock},
        {"input_init_failure_renders_error_and_unwinds_cleanly_on_applet_exit", test_input_init_failure_renders_error_and_unwinds_cleanly_on_applet_exit},
        {"applet_exit_request_closes_cleanly", test_applet_exit_request_closes_cleanly},
        {"shared_font_init_failure_still_renders_visible_error_and_exits_on_applet_close", test_shared_font_init_failure_still_renders_visible_error_and_exits_on_applet_close},
        {"navigation_dialog_and_service_error_picker_flow_through_app_shell", test_navigation_dialog_and_service_error_picker_flow_through_app_shell},
        {"same_session_roundtrip_switch_and_rejected_apply_render_applying_before_ipc", test_same_session_roundtrip_switch_and_rejected_apply_render_applying_before_ipc},
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
