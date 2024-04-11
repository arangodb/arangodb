// © 2023 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// Fuzzer for ICU Calendar.

#include <cstring>

#include "fuzzer_utils.h"

#include "unicode/calendar.h"
#include "unicode/localebuilder.h"
#include "unicode/locid.h"

icu::TimeZone* CreateRandomTimeZone(uint16_t rnd) {
    icu::Locale und("und");
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::StringEnumeration> enumeration(
        icu::TimeZone::createEnumeration(status));
    if (U_SUCCESS(status)) {
        int32_t count = enumeration->count(status);
        if (U_SUCCESS(status)) {
            int32_t i = rnd % count;
            const icu::UnicodeString* id = nullptr;
            do {
              id = enumeration->snext(status);
            } while (U_SUCCESS(status) && --i > 0);
            if (U_SUCCESS(status)) {
                return icu::TimeZone::createTimeZone(*id);
            }
        }
    }
    return icu::TimeZone::getGMT()->clone();
}
const char* GetRandomCalendarType(uint8_t rnd) {
    icu::Locale und("und");
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::StringEnumeration> enumeration(
        icu::Calendar::getKeywordValuesForLocale("calendar", und, false, status));
    const char* type = "";
    if (U_SUCCESS(status)) {
        int32_t count = enumeration->count(status);
        if (U_SUCCESS(status)) {
            int32_t i = rnd % count;
            do {
              type = enumeration->next(nullptr, status);
            } while (U_SUCCESS(status) && --i > 0);
        }
    }
    type = uloc_toUnicodeLocaleType("ca", type);
    return type;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    uint16_t rnd;
    // Set the limit for the test data to 1000 bytes to avoid timeout for a
    // very long list of operations.
    if (size > 1000) { size = 1000; }
    if (size < 2*sizeof(rnd) + 1) return 0;
    icu::StringPiece fuzzData(reinterpret_cast<const char *>(data), size);
    // Byte 0 and 1 randomly select a TimeZone
    std::memcpy(&rnd, fuzzData.data(), sizeof(rnd));
    fuzzData.remove_prefix(sizeof(rnd));
    std::unique_ptr<icu::TimeZone> timeZone(CreateRandomTimeZone(rnd));

    // Byte 1 and 2 randomly select a Locale
    std::memcpy(&rnd, fuzzData.data(), sizeof(rnd));
    fuzzData.remove_prefix(sizeof(rnd));
    icu::Locale locale = GetRandomLocale(rnd);

    // Byte 4 randomly select a Calendar type
    const char* type = GetRandomCalendarType(*fuzzData.data());
    fuzzData.remove_prefix(1);

    UErrorCode status = U_ZERO_ERROR;
    icu::LocaleBuilder bld;
    bld.setLocale(locale);
    bld.setUnicodeLocaleKeyword("ca", type);
    locale = bld.build(status);
    if (U_FAILURE(status)) return 0;
    std::unique_ptr<icu::Calendar> cal(
        icu::Calendar::createInstance(*timeZone, locale, status));
    printf("locale = %s\n", locale.getName());
    if (U_FAILURE(status)) return 0;
    cal->clear();

    int32_t amount;
    double time;
    while (fuzzData.length() > 2 + static_cast<int32_t>(sizeof(time))) {
        UCalendarDateFields field = static_cast<UCalendarDateFields>(
            (*fuzzData.data()) % UCAL_FIELD_COUNT);
        fuzzData.remove_prefix(1);

        uint8_t command = *fuzzData.data();
        fuzzData.remove_prefix(1);

        std::memcpy(&time, fuzzData.data(), sizeof(time));
        std::memcpy(&amount, fuzzData.data(), sizeof(amount));
        fuzzData.remove_prefix(sizeof(time));

        status = U_ZERO_ERROR;
        switch (command % 7) {
            case 0:
                printf("setTime(%f)\n", time);
                cal->setTime(time, status);
                break;
            case 1:
                printf("getTime()\n");
                cal->getTime(status);
                break;
            case 2:
                printf("set(%d, %d)\n", field, amount);
                cal->set(field, amount);
                break;
            case 3:
                printf("add(%d, %d)\n", field, amount);
                cal->add(field, amount, status);
                break;
            case 4:
                printf("roll(%d, %d)\n", field, amount);
                cal->roll(field, amount, status);
                break;
            case 5:
                printf("fieldDifference(%f, %d)\n", time, field);
                cal->fieldDifference(time, field, status);
                break;
            case 6:
                printf("get(%d)\n", field);
                cal->get(field, status);
                break;
            default:
                break;
        }
    }

    return EXIT_SUCCESS;
}
