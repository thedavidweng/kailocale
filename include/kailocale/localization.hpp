#pragma once

#include <string>

#include "kailocale/language_service.hpp"

namespace kailocale {

enum class UiLanguage {
    En = 0,
    ZhHans = 1,
    ZhHant = 2,
};

struct LocalizedStrings {
    UiLanguage ui_language = UiLanguage::En;
    std::string subtitle;
    std::string readonly_pill;
    std::string current_label;
    std::string supported_header;
    std::string applied_badge;
    std::string unsupported_active_warning;
    std::string already_active_notice;
    std::string preview_confirmed_notice;
    std::string confirm_title;
    std::string confirm_current_label;
    std::string confirm_target_label;
    std::string readonly_notice;
    std::string confirm_button;
    std::string cancel_button;
    std::string service_error_title;
    std::string service_error_detail;
    std::string stage_label;
    std::string result_label;
    std::string hint_navigate;
    std::string hint_select;
    std::string hint_confirm;
    std::string hint_cancel;
    std::string hint_exit;
};

/// Selects English, Simplified Chinese, or Traditional Chinese UI strings with English fallback.
UiLanguage ui_language_from_code(LanguageCode code);

/// Returns immutable UI strings for the given interface language.
const LocalizedStrings& localized_strings(UiLanguage lang);

} // namespace kailocale
