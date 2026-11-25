#pragma once

#include <cstdint>
#include <string>

namespace kailocale {

struct PickerView;

enum class ShellScreenKind {
    InitError,
};

struct ShellView {
    ShellScreenKind kind = ShellScreenKind::InitError;
    const char* app_name = "KaiLocale";
    const char* app_version = "0.1.1";
    std::string stage;
    std::uint32_t error_code = 0;
};

struct TouchSample {
    bool active = false;
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct InputSnapshot {
    bool applet_running = true;
    bool plus_held = false;
    bool resolution_changed = false;
    bool nav_up_held = false;
    bool nav_down_held = false;
    bool a_held = false;
    bool b_held = false;
    bool boundary_reset = false;
    TouchSample touch{};
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

    virtual std::uint32_t viewport_width() const = 0;
    virtual std::uint32_t viewport_height() const = 0;

    virtual InputSnapshot poll_frame() = 0;
    virtual void render_status(const ShellView& view) = 0;
    virtual void render_picker(const PickerView& view) = 0;
};

} // namespace kailocale
