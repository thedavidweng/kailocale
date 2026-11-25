#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "kailocale/language_catalog.hpp"
#include "kailocale/localization.hpp"
#include "kailocale/theme.hpp"

namespace {

using kailocale::LocalizedStrings;
using kailocale::RgbaColor;
using kailocale::SystemTheme;
using kailocale::ThemePalette;
using kailocale::UiLanguage;
using kailocale::localized_strings;
using kailocale::pack_language_code;
using kailocale::theme_for_system;
using kailocale::ui_language_from_code;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::ostringstream oss;                                              \
            oss << __FILE__ << ":" << __LINE__ << " failed check: " << #cond;    \
            throw std::runtime_error(oss.str());                                 \
        }                                                                        \
    } while (false)

int luma(RgbaColor c) {
    return (c.r * 299 + c.g * 587 + c.b * 114) / 1000;
}

bool same_color(RgbaColor a, RgbaColor b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void test_ui_language_selection_and_english_fallback() {
    CHECK(ui_language_from_code(pack_language_code("zh-Hans")) == UiLanguage::ZhHans);
    CHECK(ui_language_from_code(pack_language_code("zh-CN")) == UiLanguage::ZhHans);
    CHECK(ui_language_from_code(pack_language_code("zh-Hant")) == UiLanguage::ZhHant);
    CHECK(ui_language_from_code(pack_language_code("zh-TW")) == UiLanguage::ZhHant);

    CHECK(ui_language_from_code(pack_language_code("en-US")) == UiLanguage::En);
    CHECK(ui_language_from_code(pack_language_code("ja")) == UiLanguage::En);
    CHECK(ui_language_from_code(pack_language_code("th")) == UiLanguage::En);
    CHECK(ui_language_from_code(pack_language_code("unknown")) == UiLanguage::En);
    CHECK(ui_language_from_code(0ULL) == UiLanguage::En);
}

std::string ascii_lower(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

void test_localized_strings_cover_en_sc_tc_distinctly() {
    const LocalizedStrings& en = localized_strings(UiLanguage::En);
    const LocalizedStrings& sc = localized_strings(UiLanguage::ZhHans);
    const LocalizedStrings& tc = localized_strings(UiLanguage::ZhHant);

    CHECK(!en.subtitle.empty());
    CHECK(!en.apply_note.empty());
    CHECK(!en.current_label.empty());
    CHECK(!en.confirm_title.empty());
    CHECK(!en.confirm_notice.empty());
    CHECK(!en.applying_title.empty());
    CHECK(!en.applying_detail.empty());
    CHECK(!en.result_verified_title.empty());
    CHECK(!en.result_rejected_title.empty());
    CHECK(!en.result_rejected_detail.empty());
    CHECK(!en.result_mismatch_title.empty());
    CHECK(!en.result_mismatch_detail.empty());
    CHECK(!en.result_expected_label.empty());
    CHECK(!en.result_actual_label.empty());
    CHECK(!en.result_readback_failed_title.empty());
    CHECK(!en.result_readback_failed_detail.empty());
    CHECK(!en.result_reopen_apps_note.empty());
    CHECK(!en.result_reboot_note.empty());
    CHECK(!en.result_continue_button.empty());
    CHECK(!en.service_error_title.empty());
    CHECK(!en.hint_exit.empty());

    // Ensure Ticket #3 read-only preview placeholders are gone across all UI languages.
    CHECK(ascii_lower(en.apply_note).find("read-only") == std::string::npos);
    CHECK(ascii_lower(en.apply_note).find("preview") == std::string::npos);
    CHECK(ascii_lower(en.confirm_notice).find("read-only") == std::string::npos);
    CHECK(ascii_lower(en.confirm_notice).find("preview") == std::string::npos);
    CHECK(ascii_lower(en.confirm_button).find("preview") == std::string::npos);
    CHECK(sc.apply_note.find("\xE5\x8F\xAA\xE8\xAF\xBB") == std::string::npos);
    CHECK(sc.apply_note.find("\xE9\xA2\x84\xE8\xA7\x88") == std::string::npos);
    CHECK(tc.apply_note.find("\xE5\x94\xAF\xE8\xAE\x80") == std::string::npos);
    CHECK(tc.apply_note.find("\xE9\xA0\x90\xE8\xA6\xBD") == std::string::npos);

    // Ensure reopen guidance is present and no reboot persistence promise is made.
    CHECK(ascii_lower(en.result_reopen_apps_note).find("reopen") != std::string::npos);
    CHECK(ascii_lower(en.result_reboot_note).find("not promised") != std::string::npos ||
          ascii_lower(en.result_reboot_note).find("not guaranteed") != std::string::npos);

    CHECK(sc.subtitle != en.subtitle);
    CHECK(tc.subtitle != en.subtitle);
    CHECK(sc.subtitle != tc.subtitle);
    CHECK(sc.apply_note != en.apply_note);
    CHECK(tc.apply_note != en.apply_note);
    CHECK(sc.apply_note != tc.apply_note);
    CHECK(sc.confirm_title != tc.confirm_title);
    CHECK(sc.applying_title != tc.applying_title);
    CHECK(sc.result_verified_title != tc.result_verified_title);
    CHECK(sc.result_rejected_title != tc.result_rejected_title);
    CHECK(sc.result_mismatch_title != tc.result_mismatch_title);
    CHECK(sc.result_readback_failed_title != tc.result_readback_failed_title);
    CHECK(sc.result_reopen_apps_note != tc.result_reopen_apps_note);
    CHECK(sc.result_reboot_note != tc.result_reboot_note);
    CHECK(sc.result_continue_button != tc.result_continue_button);
}

void test_light_and_dark_themes_maintain_contrast_and_distinct_indicators() {
    const ThemePalette& dark = theme_for_system(SystemTheme::Dark);
    const ThemePalette& light = theme_for_system(SystemTheme::Light);

    CHECK(luma(dark.background) < 60);
    CHECK(luma(dark.text_primary) > 200);
    CHECK(luma(light.background) > 200);
    CHECK(luma(light.text_primary) < 60);

    for (const ThemePalette* pal : {&dark, &light}) {
        CHECK(!same_color(pal->highlight, pal->background));
        CHECK(!same_color(pal->highlight_bg, pal->background));
        CHECK(!same_color(pal->accent, pal->text_primary));
        CHECK(!same_color(pal->glyph_bg, pal->glyph_fg));
        CHECK(pal->modal_scrim.a < 255);
    }
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*fn)();
    };
    const TestCase cases[] = {
        {"ui_language_selection_and_english_fallback", test_ui_language_selection_and_english_fallback},
        {"localized_strings_cover_en_sc_tc_distinctly", test_localized_strings_cover_en_sc_tc_distinctly},
        {"light_and_dark_themes_maintain_contrast_and_distinct_indicators", test_light_and_dark_themes_maintain_contrast_and_distinct_indicators},
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
