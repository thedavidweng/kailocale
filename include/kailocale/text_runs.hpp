#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace kailocale {

enum class FontSlot {
    Standard = 0,
    ChineseSimplified = 1,
    ExtChineseSimplified = 2,
    ChineseTraditional = 3,
    Korean = 4,
    FallbackThai = 5,
    FallbackSans = 6,
    NintendoExt = 7,
};

enum class ScriptPreference {
    Default = 0,
    Japanese,
    SimplifiedChinese,
    TraditionalChinese,
    Korean,
    Thai,
};

struct TextRun {
    FontSlot slot = FontSlot::Standard;
    std::vector<std::uint32_t> codepoints;
};

using GlyphCheckFn = bool (*)(void* ctx, FontSlot slot, std::uint32_t codepoint);

/// Decodes UTF-8 into Unicode codepoints, substituting U+FFFD for invalid or truncated sequences.
std::vector<std::uint32_t> decode_utf8(std::string_view utf8);

/// Maps a BCP-47 tag string to a preferred font lookup order.
ScriptPreference script_preference_for_tag(std::string_view tag);

/// Splits UTF-8 text into contiguous same-face runs using the caller's glyph coverage predicate.
std::vector<TextRun> segment_text_runs(std::string_view utf8,
                                       ScriptPreference pref,
                                       GlyphCheckFn has_glyph,
                                       void* ctx);

} // namespace kailocale
