#include "kailocale/language_catalog.hpp"

#include <cstdio>
#include <unordered_set>

namespace kailocale {

namespace {

struct KnownLanguageSpec {
    LanguageCode code;
    const char* tag;
    const char* native_name;
};

constexpr KnownLanguageSpec kKnownLanguages[] = {
    {pack_language_code("ja"), "ja", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"},
    {pack_language_code("en-US"), "en-US", "English (US)"},
    {pack_language_code("en-GB"), "en-GB", "English (UK)"},
    {pack_language_code("fr"), "fr", "Fran\xC3\xA7" "ais"},
    {pack_language_code("fr-CA"), "fr-CA", "Fran\xC3\xA7" "ais (Canada)"},
    {pack_language_code("de"), "de", "Deutsch"},
    {pack_language_code("it"), "it", "Italiano"},
    {pack_language_code("es"), "es", "Espa\xC3\xB1ol"},
    {pack_language_code("es-419"), "es-419", "Espa\xC3\xB1ol (Latinoam\xC3\xA9rica)"},
    {pack_language_code("ko"), "ko", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4"},
    {pack_language_code("nl"), "nl", "Nederlands"},
    {pack_language_code("pt"), "pt", "Portugu\xC3\xAAs (Portugal)"},
    {pack_language_code("pt-BR"), "pt-BR", "Portugu\xC3\xAAs (Brasil)"},
    {pack_language_code("ru"), "ru", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9"},
    {pack_language_code("zh-Hans"), "zh-Hans", "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"},
    {pack_language_code("zh-Hant"), "zh-Hant", "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87"},
    {pack_language_code("pl"), "pl", "Polski"},
    {pack_language_code("th"), "th", "\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2"},
    // Legacy Switch 1 compatibility tags distinguished explicitly from zh-Hans / zh-Hant.
    {pack_language_code("zh-CN"), "zh-CN", "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87 (zh-CN)"},
    {pack_language_code("zh-TW"), "zh-TW", "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87 (zh-TW)"},
};

} // namespace

LanguageEntry make_language_entry(LanguageCode code, bool system_supported, bool is_active) {
    LanguageMetadata meta = describe_language(code);
    if (meta.tag.empty()) {
        char buf[24] = {};
        std::snprintf(buf, sizeof(buf), "0x%016llX", static_cast<unsigned long long>(code));
        meta.tag = buf;
        meta.native_name = buf;
    }
    return LanguageEntry{
        code,
        meta.tag,
        meta.native_name,
        meta.known,
        system_supported,
        is_active,
    };
}

std::string format_language_tag(LanguageCode code) {
    if (code == 0) {
        return {};
    }
    std::string out;
    out.reserve(8);
    bool seen_nul = false;
    for (unsigned i = 0; i < 8; ++i) {
        const auto byte = static_cast<unsigned char>((code >> (i * 8u)) & 0xFFu);
        if (byte == 0) {
            seen_nul = true;
            continue;
        }
        if (seen_nul || byte < 0x21 || byte > 0x7E) {
            return {};
        }
        out.push_back(static_cast<char>(byte));
    }
    return out;
}

LanguageMetadata describe_language(LanguageCode code) {
    const std::string tag = format_language_tag(code);
    if (tag.empty()) {
        return LanguageMetadata{"", "", false};
    }
    for (const auto& spec : kKnownLanguages) {
        if (spec.code == code) {
            return LanguageMetadata{spec.tag, spec.native_name, true};
        }
    }
    return LanguageMetadata{tag, tag, false};
}

CatalogBuildResult build_language_catalog(LanguageService& service) {
    CatalogBuildResult result;

    const LanguageReadResult current = service.read_current_language();
    if (!current.status.ok) {
        result.has_error = true;
        result.error_stage = "setGetLanguageCode";
        result.error_code = current.status.code;
        return result;
    }

    const std::string active_tag = format_language_tag(current.code);
    if (active_tag.empty()) {
        result.has_error = true;
        result.error_stage = "current_language_invalid";
        result.error_code = 0;
        return result;
    }

    const AvailableLanguagesResult available = service.read_available_languages();
    if (!available.status.ok) {
        result.has_error = true;
        result.error_stage = "setGetAvailableLanguageCodes";
        result.error_code = available.status.code;
        return result;
    }

    std::unordered_set<LanguageCode> seen;
    seen.reserve(available.codes.size());

    for (const LanguageCode raw_code : available.codes) {
        if (format_language_tag(raw_code).empty()) {
            continue;
        }
        if (!seen.insert(raw_code).second) {
            continue;
        }
        const bool is_active = (raw_code == current.code);
        if (is_active) {
            result.active_in_supported_list = true;
        }
        result.entries.push_back(make_language_entry(raw_code, true, is_active));
    }

    if (result.entries.empty()) {
        result.has_error = true;
        result.error_stage = "available_languages_empty";
        result.error_code = 0;
        return result;
    }

    result.active_entry = make_language_entry(current.code, result.active_in_supported_list, true);
    return result;
}

} // namespace kailocale
