#pragma once

#include <cstdint>
#include <string>

namespace kailocale {

enum class ShellScreenKind {
    Ready,
    InitError,
};

struct ShellView {
    ShellScreenKind kind = ShellScreenKind::Ready;
    const char* app_name = "KaiLocale";
    const char* app_version = "0.1.0";
    std::string stage;
    std::uint32_t error_code = 0;
};

struct InputSnapshot {
    bool applet_running = true;
    bool plus_pressed = false;
    bool resolution_changed = false;
};

struct ServiceResult {
    bool ok = true;
    std::uint32_t code = 0;

    static constexpr ServiceResult success() { return {true, 0}; }
    static constexpr ServiceResult failure(std::uint32_t err) { return {false, err}; }
};

/// Narrow platform seam isolating libounce/FreeType lifecycle from host-testable core logic.
class PlatformServices {
public:
    virtual ~PlatformServices() = default;

    virtual ServiceResult init_exit_lock() = 0;
    virtual void exit_exit_lock() = 0;

    virtual ServiceResult init_display() = 0;
    virtual ServiceResult resize_display() = 0;
    virtual void exit_display() = 0;

    virtual ServiceResult init_input() = 0;
    virtual void exit_input() = 0;

    virtual ServiceResult init_set() = 0;
    virtual void exit_set() = 0;

    virtual ServiceResult init_setsys() = 0;
    virtual void exit_setsys() = 0;

    virtual ServiceResult init_shared_font() = 0;
    virtual void exit_shared_font() = 0;

    virtual InputSnapshot poll_frame() = 0;
    virtual void render_status(const ShellView& view) = 0;
};

} // namespace kailocale
