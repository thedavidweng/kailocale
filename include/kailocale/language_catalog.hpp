#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "kailocale/language_service.hpp"

namespace kailocale {

struct LanguageMetadata {
    std::string tag;
    std::string native_name;
    bool known = false;
};

struct LanguageEntry {
    LanguageCode code = 0;
    std::string tag;
    std::string native_name;
    bool known = false;
    bool system_supported = true;
    bool is_active = false;
};

struct CatalogBuildResult {
    std::vector<LanguageEntry> entries;
    LanguageEntry active_entry;
    bool active_in_supported_list = false;
    bool has_error = false;
    std::string error_stage;
    std::uint32_t error_code = 0;
};

/// Packs an ASCII BCP-47 tag (1..8 printable non-space chars) into a little-endian u64; returns 0 if invalid.
constexpr LanguageCode pack_language_code(std::string_view tag) {
    if (tag.empty() || tag.size() > 8) {
        return 0;
    }
    LanguageCode packed = 0;
    for (std::size_t i = 0; i < tag.size(); ++i) {
        const auto ch = static_cast<unsigned char>(tag[i]);
        if (ch < 0x21 || ch > 0x7E) {
            return 0;
        }
        packed |= static_cast<LanguageCode>(ch) << (i * 8u);
    }
    return packed;
}

/// Validates and unpacks a packed little-endian u64 BCP-47 language code into ASCII; returns empty string if invalid.
std::string format_language_tag(LanguageCode code);

/// Maps a packed u64 BCP-47 tag to its native autonym and metadata without SetLanguage index assumptions.
LanguageMetadata describe_language(LanguageCode code);

/// Builds a display-ready LanguageEntry for code, using a canonical 0x... fallback tag if invalid/zero.
LanguageEntry make_language_entry(LanguageCode code, bool system_supported, bool is_active);

/// Discovers active and runtime-supported languages via LanguageService without modifying system settings.
CatalogBuildResult build_language_catalog(LanguageService& service);

} // namespace kailocale
