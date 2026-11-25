#include "kailocale/picker_layout.hpp"

#include <algorithm>

namespace kailocale {

namespace {

int scale_px(int base_at_1080p, std::uint32_t height) {
    return std::max(1, static_cast<int>((static_cast<long long>(base_at_1080p) * height) / 1080LL));
}

} // namespace

PickerLayout compute_picker_layout(std::uint32_t width,
                                   std::uint32_t height,
                                   std::size_t total_rows,
                                   std::size_t scroll_offset) {
    PickerLayout layout;
    layout.viewport_width = width > 0 ? width : 1920u;
    layout.viewport_height = height > 0 ? height : 1080u;
    const int w = static_cast<int>(layout.viewport_width);
    const int h = static_cast<int>(layout.viewport_height);

    const auto sc = [h](int v) { return scale_px(v, static_cast<std::uint32_t>(h)); };

    const int margin_x = std::max(sc(96), w / 16);
    const int content_w = std::max(sc(320), w - 2 * margin_x);

    layout.header = {0, 0, w, sc(128)};
    layout.banner = {margin_x, sc(142), content_w, sc(50)};
    layout.section_header = {margin_x, sc(202), content_w, sc(32)};

    const int footer_h = sc(96);
    layout.footer = {0, h - footer_h, w, footer_h};

    const int list_y = sc(242);
    const int list_bottom = layout.footer.y - sc(24);
    const int list_h = std::max(sc(80), list_bottom - list_y);

    const int row_h = sc(68);
    const int row_gap = sc(8);
    const int pitch = row_h + row_gap;
    layout.row_pitch = pitch;
    layout.tap_slop = sc(24);

    const int scrollbar_w = sc(10);
    const int scrollbar_gap = sc(16);
    const int row_w = std::max(sc(240), content_w - scrollbar_w - scrollbar_gap);

    layout.list_area = {margin_x, list_y, row_w, list_h};
    layout.scrollbar_track = {margin_x + row_w + scrollbar_gap, list_y, scrollbar_w, list_h};

    layout.visible_rows = std::max<std::size_t>(
        1,
        static_cast<std::size_t>((list_h + row_gap) / pitch));

    const std::size_t max_scroll =
        total_rows > layout.visible_rows ? (total_rows - layout.visible_rows) : 0;
    layout.scroll_offset = std::min(scroll_offset, max_scroll);

    const std::size_t rows_to_emit =
        total_rows > layout.scroll_offset
            ? std::min(layout.visible_rows, total_rows - layout.scroll_offset)
            : 0;

    layout.row_rects.reserve(rows_to_emit);
    for (std::size_t i = 0; i < rows_to_emit; ++i) {
        layout.row_rects.push_back(Rect{
            margin_x,
            list_y + static_cast<int>(i) * pitch,
            row_w,
            row_h,
        });
    }

    const int btn_h = sc(64);
    const int btn_y = layout.footer.y + (footer_h - btn_h) / 2;
    const int exit_w = sc(210);
    const int select_w = sc(230);
    const int btn_gap = sc(20);
    layout.exit_button = {w - margin_x - exit_w, btn_y, exit_w, btn_h};
    layout.select_button = {layout.exit_button.x - btn_gap - select_w, btn_y, select_w, btn_h};

    const int dlg_w = std::min(content_w, sc(900));
    const int dlg_h = sc(430);
    const int dlg_x = (w - dlg_w) / 2;
    const int dlg_y = (h - dlg_h) / 2;
    layout.dialog_box = {dlg_x, dlg_y, dlg_w, dlg_h};

    const int dlg_pad = sc(40);
    const int dlg_btn_h = sc(76);
    const int dlg_btn_gap = sc(24);
    const int dlg_btn_w = (dlg_w - 2 * dlg_pad - dlg_btn_gap) / 2;
    const int dlg_btn_y = dlg_y + dlg_h - dlg_pad - dlg_btn_h;
    layout.dialog_cancel_button = {dlg_x + dlg_pad, dlg_btn_y, dlg_btn_w, dlg_btn_h};
    layout.dialog_confirm_button = {dlg_x + dlg_pad + dlg_btn_w + dlg_btn_gap, dlg_btn_y, dlg_btn_w, dlg_btn_h};

    const int res_w = std::min(content_w, sc(1020));
    const int res_h = sc(470);
    const int res_x = (w - res_w) / 2;
    const int res_y = (h - res_h) / 2;
    layout.result_box = {res_x, res_y, res_w, res_h};
    const int res_btn_w = std::min(res_w - 2 * sc(40), sc(420));
    const int res_btn_h = sc(76);
    layout.result_continue_button = {
        res_x + (res_w - res_btn_w) / 2,
        res_y + res_h - sc(36) - res_btn_h,
        res_btn_w,
        res_btn_h,
    };

    return layout;
}

std::optional<std::size_t> hit_test_row(const PickerLayout& layout, int px, int py) {
    for (std::size_t i = 0; i < layout.row_rects.size(); ++i) {
        if (layout.row_rects[i].contains(px, py)) {
            return layout.scroll_offset + i;
        }
    }
    return std::nullopt;
}

} // namespace kailocale
