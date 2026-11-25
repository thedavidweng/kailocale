#pragma once

#include <cstdint>
#include <string>

#include "kailocale/platform_services.hpp"

namespace kailocale {

enum class ExitReason {
    UserPlus,
    AppletExit,
    InitFailed,
};

struct ShellOutcome {
    ExitReason reason = ExitReason::InitFailed;
    std::string failed_stage;
    std::uint32_t error_code = 0;

    bool succeeded() const { return reason != ExitReason::InitFailed; }
};

/// Coordinates ordered service initialization, visible init-error/ready presentation, and reverse teardown.
class AppShell {
public:
    explicit AppShell(PlatformServices& platform);
    ShellOutcome run();

private:
    PlatformServices& platform_;
};

} // namespace kailocale
