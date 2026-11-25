#include "kailocale/text_runs.hpp"

#include <array>

namespace kailocale {

namespace {

constexpr std::uint32_t kReplacementChar = 0xFFFDu;

bool is_continuation(unsigned char b) {
    return (b & 0xC0u) == 0x80u;
}

bool is_combining_mark(std::uint32_t cp) {
    return (cp >= 0x0300u && cp <= 0x036Fu) ||
           (cp >= 0x0E31u && cp <= 0x0E3Au) ||
           (cp >= 0x0E47u && cp <= 0x0E4Eu);
}

const std::array<FontSlot, 8>& priority_order(ScriptPreference pref) {
    static constexpr std::array<FontSlot, 8> kDefaultOrder{
        FontSlot::Standard,
        FontSlot::FallbackSans,
        FontSlot::FallbackThai,
        FontSlot::ChineseSimplified,
        FontSlot::ExtChineseSimplified,
        FontSlot::ChineseTraditional,
        FontSlot::Korean,
        FontSlot::NintendoExt,
    };
    static constexpr std::array<FontSlot, 8> kJapaneseOrder{
        FontSlot::Standard,
        FontSlot::ChineseSimplified,
        FontSlot::ExtChineseSimplified,
        FontSlot::ChineseTraditional,
        FontSlot::Korean,
        FontSlot::FallbackThai,
        FontSlot::FallbackSans,
        FontSlot::NintendoExt,
    };
    static constexpr std::array<FontSlot, 8> kSimplifiedChineseOrder{
        FontSlot::ChineseSimplified,
        FontSlot::ExtChineseSimplified,
        FontSlot::Standard,
        FontSlot::ChineseTraditional,
        FontSlot::Korean,
        FontSlot::FallbackThai,
        FontSlot::FallbackSans,
        FontSlot::NintendoExt,
    };
    static constexpr std::array<FontSlot, 8> kTraditionalChineseOrder{
        FontSlot::ChineseTraditional,
        FontSlot::Standard,
        FontSlot::ChineseSimplified,
        FontSlot::ExtChineseSimplified,
        FontSlot::Korean,
        FontSlot::FallbackThai,
        FontSlot::FallbackSans,
        FontSlot::NintendoExt,
    };
    static constexpr std::array<FontSlot, 8> kKoreanOrder{
        FontSlot::Korean,
        FontSlot::Standard,
        FontSlot::ChineseSimplified,
        FontSlot::ExtChineseSimplified,
        FontSlot::ChineseTraditional,
        FontSlot::FallbackThai,
        FontSlot::FallbackSans,
        FontSlot::NintendoExt,
    };
    static constexpr std::array<FontSlot, 8> kThaiOrder{
        FontSlot::Standard,
        FontSlot::FallbackThai,
        FontSlot::FallbackSans,
        FontSlot::ChineseSimplified,
        FontSlot::ExtChineseSimplified,
        FontSlot::ChineseTraditional,
        FontSlot::Korean,
        FontSlot::NintendoExt,
    };

    switch (pref) {
    case ScriptPreference::Japanese:
        return kJapaneseOrder;
    case ScriptPreference::SimplifiedChinese:
        return kSimplifiedChineseOrder;
    case ScriptPreference::TraditionalChinese:
        return kTraditionalChineseOrder;
    case ScriptPreference::Korean:
        return kKoreanOrder;
    case ScriptPreference::Thai:
        return kThaiOrder;
    case ScriptPreference::Default:
    default:
        return kDefaultOrder;
    }
}

FontSlot pick_slot(std::uint32_t cp,
                   const std::array<FontSlot, 8>& order,
                   const TextRun* active_run,
                   GlyphCheckFn has_glyph,
                   void* ctx) {
    if (active_run != nullptr && is_combining_mark(cp) && has_glyph(ctx, active_run->slot, cp)) {
        return active_run->slot;
    }
    for (const FontSlot slot : order) {
        if (has_glyph(ctx, slot, cp)) {
            return slot;
        }
    }
    return order.front();
}

} // namespace

std::vector<std::uint32_t> decode_utf8(std::string_view utf8) {
    std::vector<std::uint32_t> out;
    out.reserve(utf8.size());

    std::size_t i = 0;
    while (i < utf8.size()) {
        const auto b0 = static_cast<unsigned char>(utf8[i]);
        if (b0 < 0x80u) {
            out.push_back(b0);
            ++i;
            continue;
        }
        if ((b0 & 0xE0u) == 0xC0u) {
            if (i + 1 < utf8.size()) {
                const auto b1 = static_cast<unsigned char>(utf8[i + 1]);
                const std::uint32_t cp = ((b0 & 0x1Fu) << 6u) | (b1 & 0x3Fu);
                if (is_continuation(b1) && cp >= 0x80u) {
                    out.push_back(cp);
                    i += 2;
                    continue;
                }
            }
            out.push_back(kReplacementChar);
            ++i;
            continue;
        }
        if ((b0 & 0xF0u) == 0xE0u) {
            if (i + 2 < utf8.size()) {
                const auto b1 = static_cast<unsigned char>(utf8[i + 1]);
                const auto b2 = static_cast<unsigned char>(utf8[i + 2]);
                const std::uint32_t cp =
                    ((b0 & 0x0Fu) << 12u) | ((b1 & 0x3Fu) << 6u) | (b2 & 0x3Fu);
                if (is_continuation(b1) && is_continuation(b2) &&
                    cp >= 0x800u && !(cp >= 0xD800u && cp <= 0xDFFFu)) {
                    out.push_back(cp);
                    i += 3;
                    continue;
                }
            }
            out.push_back(kReplacementChar);
            ++i;
            continue;
        }
        if ((b0 & 0xF8u) == 0xF0u) {
            if (i + 3 < utf8.size()) {
                const auto b1 = static_cast<unsigned char>(utf8[i + 1]);
                const auto b2 = static_cast<unsigned char>(utf8[i + 2]);
                const auto b3 = static_cast<unsigned char>(utf8[i + 3]);
                const std::uint32_t cp = ((b0 & 0x07u) << 18u) |
                                         ((b1 & 0x3Fu) << 12u) |
                                         ((b2 & 0x3Fu) << 6u) |
                                         (b3 & 0x3Fu);
                if (is_continuation(b1) && is_continuation(b2) && is_continuation(b3) &&
                    cp >= 0x10000u && cp <= 0x10FFFFu) {
                    out.push_back(cp);
                    i += 4;
                    continue;
                }
            }
            out.push_back(kReplacementChar);
            ++i;
            continue;
        }
        out.push_back(kReplacementChar);
        ++i;
    }

    return out;
}

ScriptPreference script_preference_for_tag(std::string_view tag) {
    if (tag == "zh-Hans" || tag == "zh-CN") {
        return ScriptPreference::SimplifiedChinese;
    }
    if (tag == "zh-Hant" || tag == "zh-TW") {
        return ScriptPreference::TraditionalChinese;
    }
    if (tag == "ja") {
        return ScriptPreference::Japanese;
    }
    if (tag == "ko") {
        return ScriptPreference::Korean;
    }
    if (tag == "th") {
        return ScriptPreference::Thai;
    }
    return ScriptPreference::Default;
}

std::vector<TextRun> segment_text_runs(std::string_view utf8,
                                       ScriptPreference pref,
                                       GlyphCheckFn has_glyph,
                                       void* ctx) {
    std::vector<TextRun> runs;
    if (utf8.empty() || has_glyph == nullptr) {
        return runs;
    }

    const std::vector<std::uint32_t> codepoints = decode_utf8(utf8);
    const auto& order = priority_order(pref);

    for (const std::uint32_t cp : codepoints) {
        const TextRun* active = runs.empty() ? nullptr : &runs.back();
        const FontSlot slot = pick_slot(cp, order, active, has_glyph, ctx);
        if (runs.empty() || runs.back().slot != slot) {
            runs.push_back(TextRun{slot, {cp}});
        } else {
            runs.back().codepoints.push_back(cp);
        }
    }

    return runs;
}

} // namespace kailocale
