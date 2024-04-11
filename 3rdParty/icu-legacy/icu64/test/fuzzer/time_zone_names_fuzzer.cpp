// © 2023 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// Fuzzer for TimeZoneNames.

#include <cstring>

#include "fuzzer_utils.h"

#include "unicode/tznames.h"
#include "unicode/locid.h"

void TestNames(icu::TimeZoneNames* names, const icu::UnicodeString& text, UDate date, UTimeZoneNameType type) {
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::StringEnumeration> enumeration(
        names->getAvailableMetaZoneIDs(status));
    status = U_ZERO_ERROR;
    enumeration.reset(
        names->getAvailableMetaZoneIDs(text, status));
    icu::UnicodeString output;
    names->getMetaZoneID(text, date, output);
    names->getMetaZoneDisplayName(text, type, output);
    names->getTimeZoneDisplayName(text, type, output);
    names->getExemplarLocationName(text, output);
    names->getDisplayName(text, type, date, output);
}
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    uint16_t rnd;
    UDate date;

    UTimeZoneNameType type;
    if (size < sizeof(rnd) + sizeof(date) + sizeof(type)) return 0;
    icu::StringPiece fuzzData(reinterpret_cast<const char *>(data), size);

    std::memcpy(&rnd, fuzzData.data(), sizeof(rnd));
    icu::Locale locale = GetRandomLocale(rnd);
    fuzzData.remove_prefix(sizeof(rnd));

    std::memcpy(&date, fuzzData.data(), sizeof(date));
    fuzzData.remove_prefix(sizeof(date));

    std::memcpy(&type, fuzzData.data(), sizeof(type));
    fuzzData.remove_prefix(sizeof(type));

    size_t len = fuzzData.size() / sizeof(char16_t);
    icu::UnicodeString text(false, reinterpret_cast<const char16_t*>(fuzzData.data()), len);

    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::TimeZoneNames> names(
        icu::TimeZoneNames::createInstance(locale, status));
    if (U_SUCCESS(status)) {
        TestNames(names.get(), text, date, type);
    }

    status = U_ZERO_ERROR;
    names.reset(
        icu::TimeZoneNames::createTZDBInstance(locale, status));
    if (U_SUCCESS(status)) {
        TestNames(names.get(), text, date, type);
    }

    return EXIT_SUCCESS;
}
