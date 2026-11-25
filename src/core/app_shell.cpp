#include "kailocale/app_shell.hpp"

#include <string>
#include <vector>

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

LoopResult wait_for_exit_or_resize(PlatformServices& platform, const ShellView& view) {
    platform.render_status(view);
    for (;;) {
        const InputSnapshot snap = platform.poll_frame();
        if (!snap.applet_running) {
            return {LoopExit::AppletExit, 0};
        }
        if (snap.plus_pressed) {
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

} // namespace

AppShell::AppShell(PlatformServices& platform) : platform_(platform) {}

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
                (void)wait_for_exit_or_resize(platform_, err_view);
            }
            return ShellOutcome{ExitReason::InitFailed, step.name, res.code};
        }
        teardowns.steps.push_back(step.exit);
        if (std::string(step.name) == "display") {
            display_ready = true;
        }
    }

    ShellView ready_view;
    ready_view.kind = ShellScreenKind::Ready;
    ready_view.stage = "read-only";
    const LoopResult loop = wait_for_exit_or_resize(platform_, ready_view);
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
