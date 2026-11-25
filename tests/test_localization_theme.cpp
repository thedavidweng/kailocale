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
    CHECK(ui_language_from_code(pack_language_code("ja")) == UiLanguage::Ja);
    CHECK(ui_language_from_code(pack_language_code("ko")) == UiLanguage::Ko);
    CHECK(ui_language_from_code(pack_language_code("fr")) == UiLanguage::Fr);
    CHECK(ui_language_from_code(pack_language_code("fr-CA")) == UiLanguage::Fr);
    CHECK(ui_language_from_code(pack_language_code("de")) == UiLanguage::De);
    CHECK(ui_language_from_code(pack_language_code("it")) == UiLanguage::It);
    CHECK(ui_language_from_code(pack_language_code("es")) == UiLanguage::Es);
    CHECK(ui_language_from_code(pack_language_code("es-419")) == UiLanguage::Es);
    CHECK(ui_language_from_code(pack_language_code("nl")) == UiLanguage::Nl);
    CHECK(ui_language_from_code(pack_language_code("pt")) == UiLanguage::Pt);
    CHECK(ui_language_from_code(pack_language_code("pt-BR")) == UiLanguage::Pt);
    CHECK(ui_language_from_code(pack_language_code("ru")) == UiLanguage::Ru);
    CHECK(ui_language_from_code(pack_language_code("pl")) == UiLanguage::Pl);
    CHECK(ui_language_from_code(pack_language_code("th")) == UiLanguage::Th);

    CHECK(ui_language_from_code(pack_language_code("en-US")) == UiLanguage::En);
    CHECK(ui_language_from_code(pack_language_code("en-GB")) == UiLanguage::En);
    CHECK(ui_language_from_code(pack_language_code("unknown")) == UiLanguage::En);
    CHECK(ui_language_from_code(0ULL) == UiLanguage::En);
}

std::string ascii_lower(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

void test_localized_strings_cover_all_ui_languages_distinctly() {
    const UiLanguage all_langs[] = {
        UiLanguage::En,
        UiLanguage::ZhHans,
        UiLanguage::ZhHant,
        UiLanguage::Ja,
        UiLanguage::Ko,
        UiLanguage::Fr,
        UiLanguage::De,
        UiLanguage::It,
        UiLanguage::Es,
        UiLanguage::Nl,
        UiLanguage::Pt,
        UiLanguage::Ru,
        UiLanguage::Pl,
        UiLanguage::Th,
    };

    for (UiLanguage lang : all_langs) {
        const LocalizedStrings& s = localized_strings(lang);
        CHECK(s.ui_language == lang);
        CHECK(!s.subtitle.empty());
        CHECK(!s.apply_note.empty());
        CHECK(!s.current_label.empty());
        CHECK(!s.supported_header.empty());
        CHECK(!s.unsupported_active_warning.empty());
        CHECK(!s.already_active_notice.empty());
        CHECK(!s.confirm_title.empty());
        CHECK(!s.confirm_current_label.empty());
        CHECK(!s.confirm_target_label.empty());
        CHECK(!s.confirm_notice.empty());
        CHECK(!s.confirm_button.empty());
        CHECK(!s.cancel_button.empty());
        CHECK(!s.applying_title.empty());
        CHECK(!s.applying_detail.empty());
        CHECK(!s.result_verified_title.empty());
        CHECK(!s.result_rejected_title.empty());
        CHECK(!s.result_rejected_detail.empty());
        CHECK(!s.result_mismatch_title.empty());
        CHECK(!s.result_mismatch_detail.empty());
        CHECK(!s.result_expected_label.empty());
        CHECK(!s.result_actual_label.empty());
        CHECK(!s.result_readback_failed_title.empty());
        CHECK(!s.result_readback_failed_detail.empty());
        CHECK(!s.result_reopen_apps_note.empty());
        CHECK(!s.result_reboot_note.empty());
        CHECK(!s.result_continue_button.empty());
        CHECK(!s.service_error_title.empty());
        CHECK(!s.service_error_detail.empty());
        CHECK(!s.stage_label.empty());
        CHECK(!s.result_label.empty());
        CHECK(!s.hint_select.empty());
        CHECK(!s.hint_exit.empty());
        if (lang != UiLanguage::En) {
            const LocalizedStrings& en = localized_strings(UiLanguage::En);
            CHECK(s.apply_note != en.apply_note);
            CHECK(s.confirm_title != en.confirm_title);
            CHECK(s.result_verified_title != en.result_verified_title);
            CHECK(s.result_reopen_apps_note != en.result_reopen_apps_note);
            CHECK(s.result_reboot_note != en.result_reboot_note);
        }
    }

    const LocalizedStrings& en = localized_strings(UiLanguage::En);
    const LocalizedStrings& sc = localized_strings(UiLanguage::ZhHans);
    const LocalizedStrings& tc = localized_strings(UiLanguage::ZhHant);

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

    CHECK(sc.subtitle != tc.subtitle);
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
        {"localized_strings_cover_all_ui_languages_distinctly", test_localized_strings_cover_all_ui_languages_distinctly},
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
