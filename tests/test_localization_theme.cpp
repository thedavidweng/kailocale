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

void test_localized_strings_cover_en_sc_tc_distinctly() {
    const LocalizedStrings& en = localized_strings(UiLanguage::En);
    const LocalizedStrings& sc = localized_strings(UiLanguage::ZhHans);
    const LocalizedStrings& tc = localized_strings(UiLanguage::ZhHant);

    CHECK(!en.subtitle.empty());
    CHECK(!en.current_label.empty());
    CHECK(!en.applied_badge.empty());
    CHECK(!en.confirm_title.empty());
    CHECK(!en.readonly_notice.empty());
    CHECK(!en.service_error_title.empty());
    CHECK(!en.hint_exit.empty());

    CHECK(sc.subtitle != en.subtitle);
    CHECK(tc.subtitle != en.subtitle);
    CHECK(sc.subtitle != tc.subtitle);
    CHECK(sc.confirm_title != tc.confirm_title);
    CHECK(sc.applied_badge != en.applied_badge);
    CHECK(tc.applied_badge != en.applied_badge);
}

void test_light_and_dark_themes_maintain_contrast_and_distinct_indicators() {
    const ThemePalette& dark = theme_for_system(SystemTheme::Dark);
    const ThemePalette& light = theme_for_system(SystemTheme::Light);

    CHECK(luma(dark.background) < 60);
    CHECK(luma(dark.text_primary) > 200);
    CHECK(luma(light.background) > 200);
    CHECK(luma(light.text_primary) < 60);

    CHECK(!same_color(dark.focus_border, dark.row_bg));
    CHECK(!same_color(dark.focus_bg, dark.applied_badge_bg));
    CHECK(!same_color(light.focus_border, light.row_bg));
    CHECK(!same_color(light.focus_bg, light.applied_badge_bg));
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
