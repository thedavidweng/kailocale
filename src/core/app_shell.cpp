#include "kailocale/app_shell.hpp"

#include <string>
#include <vector>

#include "kailocale/language_picker.hpp"

namespace kailocale {

namespace {

struct ScopedTeardowns {
    PlatformServices& platform;
    std::vector<void (PlatformServices::*)()> steps;

    ~ScopedTeardowns() {
        for (auto it = steps.rbegin(); it != steps.rend(); ++it) {
            (platform.*(*it))();
        }
    }
};

enum class LoopExit {
    UserPlus,
    AppletExit,
    ResizeFailed,
};

struct LoopResult {
    LoopExit exit = LoopExit::AppletExit;
    std::uint32_t error_code = 0;
};

LoopResult wait_for_error_exit(PlatformServices& platform, const ShellView& view) {
    platform.render_status(view);
    bool prev_plus = false;
    for (;;) {
        const InputSnapshot snap = platform.poll_frame();
        if (!snap.applet_running) {
            return {LoopExit::AppletExit, 0};
        }
        const bool plus_down = snap.plus_held && !prev_plus && !snap.boundary_reset;
        prev_plus = snap.plus_held;
        if (plus_down) {
            return {LoopExit::UserPlus, 0};
        }
        if (snap.resolution_changed) {
            const ServiceResult resize_res = platform.resize_display();
            if (!resize_res.ok) {
                return {LoopExit::ResizeFailed, resize_res.code};
            }
            platform.render_status(view);
        }
    }
}

LoopResult run_picker_loop(PlatformServices& platform, LanguageService& language_service) {
    LanguagePicker picker(language_service, platform.viewport_width(), platform.viewport_height());
    platform.render_picker(picker.view());

    for (;;) {
        const InputSnapshot snap = platform.poll_frame();
        if (!snap.applet_running) {
            return {LoopExit::AppletExit, 0};
        }

        bool resized = false;
        if (snap.resolution_changed) {
            const ServiceResult resize_res = platform.resize_display();
            if (!resize_res.ok) {
                return {LoopExit::ResizeFailed, resize_res.code};
            }
            resized = true;
        }

        PickerInput input;
        input.applet_running = snap.applet_running;
        input.nav_up_held = snap.nav_up_held;
        input.nav_down_held = snap.nav_down_held;
        input.a_held = snap.a_held;
        input.b_held = snap.b_held;
        input.plus_held = snap.plus_held;
        input.boundary_reset = snap.boundary_reset;
        input.viewport_width = platform.viewport_width();
        input.viewport_height = platform.viewport_height();
        input.touch = snap.touch;

        const PickerStepResult step_res = picker.step(input);
        if (step_res.exit_requested) {
            return {LoopExit::UserPlus, 0};
        }
        if (step_res.dirty || resized) {
            platform.render_picker(picker.view());
        }
    }
}

} // namespace

AppShell::AppShell(PlatformServices& platform, LanguageService& language_service)
    : platform_(platform), language_service_(language_service) {}

ShellOutcome AppShell::run() {
    ScopedTeardowns teardowns{platform_, {}};

    struct InitStep {
        const char* name;
        ServiceResult (PlatformServices::*init)();
        void (PlatformServices::*exit)();
    };

    const InitStep steps[] = {
        {"exit_lock", &PlatformServices::init_exit_lock, &PlatformServices::exit_exit_lock},
        {"display", &PlatformServices::init_display, &PlatformServices::exit_display},
        {"input", &PlatformServices::init_input, &PlatformServices::exit_input},
        {"set", &PlatformServices::init_set, &PlatformServices::exit_set},
        {"set:sys", &PlatformServices::init_setsys, &PlatformServices::exit_setsys},
        {"shared_font", &PlatformServices::init_shared_font, &PlatformServices::exit_shared_font},
    };

    bool display_ready = false;
    for (const auto& step : steps) {
        const ServiceResult res = (platform_.*step.init)();
        if (!res.ok) {
            if (display_ready) {
                ShellView err_view;
                err_view.kind = ShellScreenKind::InitError;
                err_view.stage = step.name;
                err_view.error_code = res.code;
                (void)wait_for_error_exit(platform_, err_view);
            }
            return ShellOutcome{ExitReason::InitFailed, step.name, res.code};
        }
        teardowns.steps.push_back(step.exit);
        if (std::string(step.name) == "display") {
            display_ready = true;
        }
    }

    const LoopResult loop = run_picker_loop(platform_, language_service_);
    if (loop.exit == LoopExit::ResizeFailed) {
        return ShellOutcome{ExitReason::InitFailed, "display_resize", loop.error_code};
    }
    return ShellOutcome{
        loop.exit == LoopExit::UserPlus ? ExitReason::UserPlus : ExitReason::AppletExit,
        {},
        0,
    };
}

} // namespace kailocale
