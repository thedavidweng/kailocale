#pragma once

#include <cstdint>
#include <vector>

#include "kailocale/platform_services.hpp"

namespace kailocale {

/// Packed little-endian ASCII BCP-47 language tag (up to 8 bytes, NUL-padded), matching libounce set APIs.
using LanguageCode = std::uint64_t;

enum class SystemTheme {
    Dark = 0,
    Light = 1,
};

struct LanguageReadResult {
    ServiceResult status;
    LanguageCode code = 0;
};

struct AvailableLanguagesResult {
    ServiceResult status;
    std::vector<LanguageCode> codes;
};

/// Narrow boundary isolating libounce set / set:sys queries and confirmed writes from host-testable core logic.
class LanguageService {
public:
    virtual ~LanguageService() = default;

    virtual LanguageReadResult read_current_language() = 0;
    virtual AvailableLanguagesResult read_available_languages() = 0;
    virtual SystemTheme read_system_theme() = 0;
    virtual ServiceResult apply_language(LanguageCode code) = 0;
};

} // namespace kailocale
