#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kailocale/language_catalog.hpp"
#include "kailocale/language_service.hpp"

namespace {

using kailocale::AvailableLanguagesResult;
using kailocale::CatalogBuildResult;
using kailocale::LanguageCode;
using kailocale::LanguageMetadata;
using kailocale::LanguageReadResult;
using kailocale::LanguageService;
using kailocale::ServiceResult;
using kailocale::SystemTheme;
using kailocale::build_language_catalog;
using kailocale::describe_language;
using kailocale::format_language_tag;
using kailocale::pack_language_code;

class FakeLanguageService final : public LanguageService {
public:
    LanguageReadResult current_result{ServiceResult::success(), pack_language_code("en-US")};
    AvailableLanguagesResult available_result{ServiceResult::success(), {}};
    SystemTheme theme = SystemTheme::Dark;

    LanguageReadResult read_current_language() override {
        return current_result;
    }

    AvailableLanguagesResult read_available_languages() override {
        return available_result;
    }

    SystemTheme read_system_theme() override {
        return theme;
    }

    ServiceResult apply_language(LanguageCode) override {
        return ServiceResult::success();
    }
};

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::ostringstream oss;                                              \
            oss << __FILE__ << ":" << __LINE__ << " failed check: " << #cond;    \
            throw std::runtime_error(oss.str());                                 \
        }                                                                        \
    } while (false)

void test_bcp47_u64_pack_and_unpack() {
    CHECK(pack_language_code("ja") == 0x616AULL);
    CHECK(pack_language_code("en-US") == 0x000053552D6E65ULL);
    CHECK(pack_language_code("zh-Hans") == 0x736E61482D687AULL);
    CHECK(format_language_tag(0x616AULL) == "ja");
    CHECK(format_language_tag(0x000053552D6E65ULL) == "en-US");
    CHECK(format_language_tag(0x736E61482D687AULL) == "zh-Hans");
    CHECK(format_language_tag(0ULL).empty());
    CHECK(pack_language_code("") == 0ULL);
    CHECK(pack_language_code("too-long-tag") == 0ULL);
}

void test_known_horizon_autonyms_and_regional_differentiation() {
    struct Expected {
        const char* tag;
        const char* autonym;
        bool known;
    };
    const Expected table[] = {
        {"ja", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", true},
        {"en-US", "English (US)", true},
        {"en-GB", "English (UK)", true},
        {"fr", "Fran\xC3\xA7" "ais", true},
        {"fr-CA", "Fran\xC3\xA7" "ais (Canada)", true},
        {"de", "Deutsch", true},
        {"it", "Italiano", true},
        {"es", "Espa\xC3\xB1ol", true},
        {"es-419", "Espa\xC3\xB1ol (Latinoam\xC3\xA9rica)", true},
        {"ko", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4", true},
        {"nl", "Nederlands", true},
        {"pt", "Portugu\xC3\xAAs (Portugal)", true},
        {"pt-BR", "Portugu\xC3\xAAs (Brasil)", true},
        {"ru", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9", true},
        {"zh-Hans", "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87", true},
        {"zh-Hant", "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87", true},
        {"pl", "Polski", true},
        {"th", "\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2", true},
    };

    for (const auto& item : table) {
        const LanguageMetadata meta = describe_language(pack_language_code(item.tag));
        CHECK(meta.tag == item.tag);
        CHECK(meta.native_name == item.autonym);
        CHECK(meta.known == item.known);
    }

    const LanguageMetadata zh_cn = describe_language(pack_language_code("zh-CN"));
    CHECK(zh_cn.tag == "zh-CN");
    CHECK(zh_cn.native_name != describe_language(pack_language_code("zh-Hans")).native_name);
    CHECK(zh_cn.native_name.find("zh-CN") != std::string::npos);

    const LanguageMetadata zh_tw = describe_language(pack_language_code("zh-TW"));
    CHECK(zh_tw.tag == "zh-TW");
    CHECK(zh_tw.native_name != describe_language(pack_language_code("zh-Hant")).native_name);
    CHECK(zh_tw.native_name.find("zh-TW") != std::string::npos);

    const LanguageMetadata unknown = describe_language(pack_language_code("sv"));
    CHECK(!unknown.known);
    CHECK(unknown.tag == "sv");
    CHECK(unknown.native_name == "sv");

    const kailocale::LanguageEntry known_entry =
        kailocale::make_language_entry(pack_language_code("en-US"), true, true);
    CHECK(known_entry.code == pack_language_code("en-US"));
    CHECK(known_entry.tag == "en-US");
    CHECK(known_entry.native_name == "English (US)");
    CHECK(known_entry.known);
    CHECK(known_entry.system_supported);
    CHECK(known_entry.is_active);

    const kailocale::LanguageEntry invalid_entry =
        kailocale::make_language_entry(0x0001020304050607ULL, false, false);
    CHECK(invalid_entry.code == 0x0001020304050607ULL);
    CHECK(invalid_entry.tag == "0x0001020304050607");
    CHECK(invalid_entry.native_name == "0x0001020304050607");
    CHECK(!invalid_entry.known);
    CHECK(!invalid_entry.system_supported);
    CHECK(!invalid_entry.is_active);
}

void test_catalog_deduplicates_filters_invalid_and_marks_active() {
    FakeLanguageService service;
    service.current_result = {ServiceResult::success(), pack_language_code("zh-Hans")};
    service.available_result = {
        ServiceResult::success(),
        {
            0ULL,
            pack_language_code("ja"),
            pack_language_code("en-US"),
            pack_language_code("ja"),
            0x0001020304050607ULL,
            pack_language_code("zh-Hans"),
            pack_language_code("th"),
            pack_language_code("en-US"),
        },
    };

    const CatalogBuildResult cat = build_language_catalog(service);
    CHECK(!cat.has_error);
    CHECK(cat.active_in_supported_list);
    CHECK(cat.active_entry.code == pack_language_code("zh-Hans"));
    CHECK(cat.entries.size() == 4);
    CHECK(cat.entries[0].tag == "ja");
    CHECK(!cat.entries[0].is_active);
    CHECK(cat.entries[1].tag == "en-US");
    CHECK(cat.entries[2].tag == "zh-Hans");
    CHECK(cat.entries[2].is_active);
    CHECK(cat.entries[3].tag == "th");
}

void test_active_language_absent_from_supported_list_sets_notice_safely() {
    FakeLanguageService service;
    service.current_result = {ServiceResult::success(), pack_language_code("ja")};
    service.available_result = {
        ServiceResult::success(),
        {pack_language_code("en-US"), pack_language_code("zh-Hant")},
    };

    const CatalogBuildResult cat = build_language_catalog(service);
    CHECK(!cat.has_error);
    CHECK(!cat.active_in_supported_list);
    CHECK(cat.active_entry.code == pack_language_code("ja"));
    CHECK(cat.active_entry.native_name == "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
    CHECK(!cat.active_entry.system_supported);
    CHECK(cat.entries.size() == 2);
    CHECK(!cat.entries[0].is_active);
    CHECK(!cat.entries[1].is_active);
}

void test_service_failure_and_empty_available_list_produce_error_state() {
    FakeLanguageService fail_read;
    fail_read.current_result = {ServiceResult::failure(0x2345), 0ULL};
    const CatalogBuildResult res1 = build_language_catalog(fail_read);
    CHECK(res1.has_error);
    CHECK(res1.error_code == 0x2345u);
    CHECK(res1.error_stage == "setGetLanguageCode");

    FakeLanguageService fail_avail;
    fail_avail.available_result = {ServiceResult::failure(0x5511), {}};
    const CatalogBuildResult res2 = build_language_catalog(fail_avail);
    CHECK(res2.has_error);
    CHECK(res2.error_code == 0x5511u);
    CHECK(res2.error_stage == "setGetAvailableLanguageCodes");

    FakeLanguageService empty_avail;
    empty_avail.available_result = {ServiceResult::success(), {0ULL}};
    const CatalogBuildResult res3 = build_language_catalog(empty_avail);
    CHECK(res3.has_error);
    CHECK(res3.error_stage == "available_languages_empty");
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*fn)();
    };
    const TestCase cases[] = {
        {"bcp47_u64_pack_and_unpack", test_bcp47_u64_pack_and_unpack},
        {"known_horizon_autonyms_and_regional_differentiation", test_known_horizon_autonyms_and_regional_differentiation},
        {"catalog_deduplicates_filters_invalid_and_marks_active", test_catalog_deduplicates_filters_invalid_and_marks_active},
        {"active_language_absent_from_supported_list_sets_notice_safely", test_active_language_absent_from_supported_list_sets_notice_safely},
        {"service_failure_and_empty_available_list_produce_error_state", test_service_failure_and_empty_available_list_produce_error_state},
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
