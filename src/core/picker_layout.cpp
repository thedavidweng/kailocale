#include "kailocale/picker_layout.hpp"

#include <algorithm>

namespace kailocale {

int scale_px(int px, std::uint32_t viewport_height) {
    return std::max(1, static_cast<int>((static_cast<long long>(px) * viewport_height) / 1080LL));
}

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

    // Horizon proportions authored at 1080p: 132 px header, 108 px footer, ~1/3 width sidebar.
    layout.header = {0, 0, w, sc(132)};
    const int footer_h = sc(108);
    layout.footer = {0, h - footer_h, w, footer_h};

    const int body_y = layout.header.h;
    const int sidebar_w = std::max(sc(420), (w * 32) / 100);
    layout.sidebar = {0, body_y, sidebar_w, layout.footer.y - body_y};

    const int side_x = sc(80);
    const int side_w = std::max(sc(200), sidebar_w - side_x - sc(40));
    layout.sidebar_tab = {side_x, body_y + sc(36), side_w, sc(84)};
    const int banner_y = layout.sidebar_tab.y + layout.sidebar_tab.h + sc(320);
    layout.banner = {side_x, banner_y, side_w, std::max(sc(60), layout.footer.y - sc(30) - banner_y)};

    const int content_x = sidebar_w + sc(60);
    const int content_right = w - sc(96);
    const int content_w = std::max(sc(320), content_right - content_x);

    layout.section_header = {content_x, body_y + sc(28), content_w, sc(48)};

    const int list_y = body_y + sc(84);
    const int list_bottom = layout.footer.y - sc(24);
    const int list_h = std::max(sc(90), list_bottom - list_y);

    const int row_h = sc(90);
    layout.row_pitch = row_h;
    layout.tap_slop = sc(24);

    layout.list_area = {content_x, list_y, content_w, list_h};
    layout.scrollbar_track = {content_x + content_w + sc(36), list_y, sc(6), list_h};

    layout.visible_rows = std::max<std::size_t>(1, static_cast<std::size_t>(list_h / row_h));

    const std::size_t max_scroll =
        total_rows > layout.visible_rows ? (total_rows - layout.visible_rows) : 0;
    layout.scroll_offset = std::min(scroll_offset, max_scroll);

    const std::size_t rows_to_emit =
        total_rows > layout.scroll_offset
            ? std::min(layout.visible_rows, total_rows - layout.scroll_offset)
            : 0;

    layout.row_rects.reserve(rows_to_emit);
    for (std::size_t i = 0; i < rows_to_emit; ++i) {
        layout.row_rects.push_back(Rect{content_x, list_y + static_cast<int>(i) * row_h, content_w, row_h});
    }

    const int btn_h = sc(72);
    const int btn_y = layout.footer.y + (footer_h - btn_h) / 2;
    const int select_w = sc(280);
    const int exit_w = sc(240);
    layout.exit_button = {w - sc(66) - exit_w, btn_y, exit_w, btn_h};
    layout.select_button = {layout.exit_button.x - sc(12) - select_w, btn_y, select_w, btn_h};

    const int dlg_w = std::min(w - 2 * sc(96), sc(1040));
    const int dlg_btn_h = sc(108);

    const int dlg_h = sc(440);
    const int dlg_x = (w - dlg_w) / 2;
    const int dlg_y = (h - dlg_h) / 2;
    layout.dialog_box = {dlg_x, dlg_y, dlg_w, dlg_h};
    const int dlg_btn_y = dlg_y + dlg_h - dlg_btn_h;
    layout.dialog_cancel_button = {dlg_x, dlg_btn_y, dlg_w / 2, dlg_btn_h};
    layout.dialog_confirm_button = {dlg_x + dlg_w / 2, dlg_btn_y, dlg_w - dlg_w / 2, dlg_btn_h};

    const int res_h = sc(540);
    const int res_y = (h - res_h) / 2;
    layout.result_box = {dlg_x, res_y, dlg_w, res_h};
    layout.result_continue_button = {dlg_x, res_y + res_h - dlg_btn_h, dlg_w, dlg_btn_h};

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
