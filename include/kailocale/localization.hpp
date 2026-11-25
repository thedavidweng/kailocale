#pragma once

#include <string>

#include "kailocale/language_service.hpp"

namespace kailocale {

enum class UiLanguage {
    En = 0,
    ZhHans = 1,
    ZhHant = 2,
    Ja = 3,
    Ko = 4,
    Fr = 5,
    De = 6,
    It = 7,
    Es = 8,
    Nl = 9,
    Pt = 10,
    Ru = 11,
    Pl = 12,
    Th = 13,
};

struct LocalizedStrings {
    UiLanguage ui_language = UiLanguage::En;
    std::string subtitle;
    std::string apply_note;
    std::string current_label;
    std::string supported_header;
    std::string unsupported_active_warning;
    std::string already_active_notice;
    std::string confirm_title;
    std::string confirm_current_label;
    std::string confirm_target_label;
    std::string confirm_notice;
    std::string confirm_button;
    std::string cancel_button;
    std::string applying_title;
    std::string applying_detail;
    std::string result_verified_title;
    std::string result_rejected_title;
    std::string result_rejected_detail;
    std::string result_mismatch_title;
    std::string result_mismatch_detail;
    std::string result_expected_label;
    std::string result_actual_label;
    std::string result_readback_failed_title;
    std::string result_readback_failed_detail;
    std::string result_reopen_apps_note;
    std::string result_reboot_note;
    std::string result_continue_button;
    std::string service_error_title;
    std::string service_error_detail;
    std::string stage_label;
    std::string result_label;
    std::string hint_select;
    std::string hint_exit;
};

/// Selects the localized UI string set matching the console language code, falling back to English.
UiLanguage ui_language_from_code(LanguageCode code);

/// Returns immutable UI strings for the given interface language.
const LocalizedStrings& localized_strings(UiLanguage lang);

} // namespace kailocale
