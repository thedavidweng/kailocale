#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kailocale/text_runs.hpp"

namespace {

using kailocale::FontSlot;
using kailocale::ScriptPreference;
using kailocale::TextRun;
using kailocale::decode_utf8;
using kailocale::script_preference_for_tag;
using kailocale::segment_text_runs;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::ostringstream oss;                                              \
            oss << __FILE__ << ":" << __LINE__ << " failed check: " << #cond;    \
            throw std::runtime_error(oss.str());                                 \
        }                                                                        \
    } while (false)

struct FakeFaceSet {
    bool standard_has_thai = false;
    bool pl_available = true;
};

bool fake_has_glyph(void* ctx, FontSlot slot, std::uint32_t cp) {
    const auto* fs = static_cast<const FakeFaceSet*>(ctx);
    const bool is_thai = (cp >= 0x0E00u && cp <= 0x0E7Fu);
    const bool is_hangul = (cp >= 0xAC00u && cp <= 0xD7AFu);
    const bool is_cjk = (cp >= 0x4E00u && cp <= 0x9FFFu) || (cp >= 0x3040u && cp <= 0x30FFu);

    switch (slot) {
    case FontSlot::Standard:
        if (!fs->pl_available) return false;
        if (is_thai) return fs->standard_has_thai;
        return !is_hangul;
    case FontSlot::ChineseSimplified:
    case FontSlot::ExtChineseSimplified:
        return fs->pl_available && (is_cjk || cp < 0x80u);
    case FontSlot::ChineseTraditional:
        return fs->pl_available && (is_cjk || cp < 0x80u);
    case FontSlot::Korean:
        return fs->pl_available && (is_hangul || cp < 0x80u);
    case FontSlot::FallbackThai:
        return is_thai;
    case FontSlot::FallbackSans:
        return !is_thai && !is_cjk && !is_hangul;
    case FontSlot::NintendoExt:
        return fs->pl_available && (cp >= 0xE000u && cp <= 0xE1FFu);
    }
    return false;
}

void test_utf8_decoding_handles_multibyte_and_malformed_inputs() {
    // "ไทย" -> U+0E44, U+0E17, U+0E22
    const std::vector<std::uint32_t> thai = decode_utf8("\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2");
    CHECK((thai == std::vector<std::uint32_t>{0x0E44u, 0x0E17u, 0x0E22u}));

    // "Русский"
    const std::vector<std::uint32_t> ru = decode_utf8("\xD0\xA0\xD1\x83\xD1\x81");
    CHECK((ru == std::vector<std::uint32_t>{0x0420u, 0x0443u, 0x0441u}));

    // Malformed / truncated UTF-8 produces replacement U+FFFD without hanging.
    const std::vector<std::uint32_t> bad = decode_utf8("\xFF\xE0\xB8" "A");
    CHECK(!bad.empty());
    CHECK(bad.back() == static_cast<std::uint32_t>('A'));
}

void test_run_segmentation_selects_script_faces_and_falls_back_for_thai() {
    FakeFaceSet faces;
    faces.standard_has_thai = false;
    faces.pl_available = true;

    // Thai falls back to FallbackThai when Standard lacks Thai glyphs.
    const auto thai_runs = segment_text_runs(
        "\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2",
        ScriptPreference::Thai,
        fake_has_glyph,
        &faces);
    CHECK(thai_runs.size() == 1);
    CHECK(thai_runs[0].slot == FontSlot::FallbackThai);
    CHECK(thai_runs[0].codepoints.size() == 3);

    // Simplified vs Traditional Chinese prefer their respective faces.
    const auto sc_runs = segment_text_runs(
        "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87",
        ScriptPreference::SimplifiedChinese,
        fake_has_glyph,
        &faces);
    CHECK(sc_runs.size() == 1);
    CHECK(sc_runs[0].slot == FontSlot::ChineseSimplified);

    const auto tc_runs = segment_text_runs(
        "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87",
        ScriptPreference::TraditionalChinese,
        fake_has_glyph,
        &faces);
    CHECK(tc_runs.size() == 1);
    CHECK(tc_runs[0].slot == FontSlot::ChineseTraditional);

    // Korean uses KO face even when Standard lacks Hangul.
    const auto ko_runs = segment_text_runs(
        "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4",
        ScriptPreference::Korean,
        fake_has_glyph,
        &faces);
    CHECK(ko_runs.size() == 1);
    CHECK(ko_runs[0].slot == FontSlot::Korean);

    // When pl is completely unavailable, Latin/Cyrillic still render via FallbackSans.
    faces.pl_available = false;
    const auto ru_fallback = segment_text_runs(
        "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9",
        ScriptPreference::Default,
        fake_has_glyph,
        &faces);
    CHECK(ru_fallback.size() == 1);
    CHECK(ru_fallback[0].slot == FontSlot::FallbackSans);

    CHECK(script_preference_for_tag("zh-Hans") == ScriptPreference::SimplifiedChinese);
    CHECK(script_preference_for_tag("zh-Hant") == ScriptPreference::TraditionalChinese);
    CHECK(script_preference_for_tag("ja") == ScriptPreference::Japanese);
    CHECK(script_preference_for_tag("ko") == ScriptPreference::Korean);
    CHECK(script_preference_for_tag("th") == ScriptPreference::Thai);
    CHECK(script_preference_for_tag("en-US") == ScriptPreference::Default);
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*fn)();
    };
    const TestCase cases[] = {
        {"utf8_decoding_handles_multibyte_and_malformed_inputs", test_utf8_decoding_handles_multibyte_and_malformed_inputs},
        {"run_segmentation_selects_script_faces_and_falls_back_for_thai", test_run_segmentation_selects_script_faces_and_falls_back_for_thai},
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
