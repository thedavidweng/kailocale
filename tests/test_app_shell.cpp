#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kailocale/app_shell.hpp"

namespace {

using kailocale::InputSnapshot;
using kailocale::PlatformServices;
using kailocale::ServiceResult;
using kailocale::ShellOutcome;
using kailocale::ShellScreenKind;
using kailocale::ShellView;

class FakePlatformServices final : public PlatformServices {
public:
    std::vector<InputSnapshot> frames;
    std::vector<std::string> events;
    std::vector<ShellView> rendered;

    ServiceResult exit_lock_result = ServiceResult::success();
    ServiceResult display_result = ServiceResult::success();
    ServiceResult resize_result = ServiceResult::success();
    ServiceResult input_result = ServiceResult::success();
    ServiceResult set_result = ServiceResult::success();
    ServiceResult setsys_result = ServiceResult::success();
    ServiceResult shared_font_result = ServiceResult::success();

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

    InputSnapshot poll_frame() override {
        ++poll_count;
        if (frame_cursor_ < frames.size()) {
            return frames[frame_cursor_++];
        }
        return InputSnapshot{false, false, false};
    }

    void render_status(const ShellView& view) override {
        rendered.push_back(view);
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
    fake.frames = {
        InputSnapshot{true, false, false},
        InputSnapshot{true, true, false},
    };

    kailocale::AppShell shell(fake);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::UserPlus);
    CHECK(outcome.succeeded());
    CHECK(!fake.rendered.empty());
    CHECK(fake.rendered.front().kind == ShellScreenKind::Ready);
    CHECK(std::string(fake.rendered.front().app_name) == "KaiLocale");
    CHECK(std::string(fake.rendered.front().app_version) == "0.1.0");

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
    fake.setsys_result = ServiceResult::failure(0x4201);
    fake.frames = {
        InputSnapshot{true, false, false},
        InputSnapshot{true, true, false},
    };

    kailocale::AppShell shell(fake);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::InitFailed);
    CHECK(!outcome.succeeded());
    CHECK(outcome.failed_stage == "set:sys");
    CHECK(outcome.error_code == 0x4201u);

    CHECK(fake.rendered.size() == 1);
    CHECK(fake.rendered[0].kind == ShellScreenKind::InitError);
    CHECK(fake.rendered[0].stage == "set:sys");
    CHECK(fake.rendered[0].error_code == 0x4201u);
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

void test_resolution_change_resizes_and_rerenders_ready_screen() {
    FakePlatformServices fake;
    fake.frames = {
        InputSnapshot{true, false, true},
        InputSnapshot{true, false, false},
        InputSnapshot{true, true, false},
    };

    kailocale::AppShell shell(fake);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::UserPlus);
    CHECK(fake.rendered.size() == 2);
    CHECK(fake.rendered[0].kind == ShellScreenKind::Ready);
    CHECK(fake.rendered[1].kind == ShellScreenKind::Ready);

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
    fake.display_result = ServiceResult::failure(0x9001);

    kailocale::AppShell shell(fake);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::InitFailed);
    CHECK(!outcome.succeeded());
    CHECK(outcome.failed_stage == "display");
    CHECK(outcome.error_code == 0x9001u);
    CHECK(fake.rendered.empty());
    CHECK(fake.poll_count == 0);

    const std::vector<std::string> expected_events = {
        "init:exit_lock",
        "init:display",
        "exit:exit_lock",
    };
    CHECK(fake.events == expected_events);
}

void test_applet_exit_request_closes_cleanly() {
    FakePlatformServices fake;
    fake.frames = {
        InputSnapshot{false, false, false},
    };

    kailocale::AppShell shell(fake);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::AppletExit);
    CHECK(outcome.succeeded());
    CHECK(fake.rendered.size() == 1);
    CHECK(fake.rendered[0].kind == ShellScreenKind::Ready);

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
    fake.shared_font_result = ServiceResult::failure(0xA502);
    fake.frames = {
        InputSnapshot{true, false, true},
        InputSnapshot{false, false, false},
    };

    kailocale::AppShell shell(fake);
    const ShellOutcome outcome = shell.run();

    CHECK(outcome.reason == kailocale::ExitReason::InitFailed);
    CHECK(outcome.failed_stage == "shared_font");
    CHECK(outcome.error_code == 0xA502u);
    CHECK(fake.rendered.size() == 2);
    CHECK(fake.rendered[0].kind == ShellScreenKind::InitError);
    CHECK(fake.rendered[1].kind == ShellScreenKind::InitError);

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

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*fn)();
    };
    const TestCase cases[] = {
        {"clean_startup_plus_exit_and_reverse_teardown", test_clean_startup_plus_exit_and_reverse_teardown},
        {"setsys_init_failure_renders_visible_error_and_tears_down_acquired_subset", test_setsys_init_failure_renders_visible_error_and_tears_down_acquired_subset},
        {"resolution_change_resizes_and_rerenders_ready_screen", test_resolution_change_resizes_and_rerenders_ready_screen},
        {"display_init_failure_skips_render_and_unwinds_exit_lock", test_display_init_failure_skips_render_and_unwinds_exit_lock},
        {"applet_exit_request_closes_cleanly", test_applet_exit_request_closes_cleanly},
        {"shared_font_init_failure_still_renders_visible_error_and_exits_on_applet_close", test_shared_font_init_failure_still_renders_visible_error_and_exits_on_applet_close},
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
