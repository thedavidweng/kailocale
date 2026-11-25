#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace kailocale {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    constexpr bool contains(int px, int py) const {
        return px >= x && px < (x + w) && py >= y && py < (y + h);
    }
};

struct PickerLayout {
    std::uint32_t viewport_width = 1920;
    std::uint32_t viewport_height = 1080;
    Rect header{};
    Rect sidebar{};
    Rect sidebar_tab{};
    Rect banner{};
    Rect section_header{};
    Rect list_area{};
    Rect scrollbar_track{};
    Rect footer{};
    Rect select_button{};
    Rect exit_button{};
    Rect dialog_box{};
    Rect dialog_confirm_button{};
    Rect dialog_cancel_button{};
    Rect result_box{};
    Rect result_continue_button{};
    int row_pitch = 0;
    int tap_slop = 24;
    std::size_t visible_rows = 0;
    std::size_t scroll_offset = 0;
    std::vector<Rect> row_rects;
};

/// Scales a 1080p-authored pixel value proportionally for the active viewport height (clamped >= 1).
int scale_px(int px, std::uint32_t viewport_height);

/// Computes responsive Horizon-style layout geometry and touch hit targets for any Switch 2 viewport.
PickerLayout compute_picker_layout(std::uint32_t width,
                                   std::uint32_t height,
                                   std::size_t total_rows,
                                   std::size_t scroll_offset);

/// Returns the catalog row index under (px, py), or std::nullopt if outside visible rows.
std::optional<std::size_t> hit_test_row(const PickerLayout& layout, int px, int py);

} // namespace kailocale
