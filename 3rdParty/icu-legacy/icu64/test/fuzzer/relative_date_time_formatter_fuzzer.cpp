// © 2023 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// Fuzzer for ICU RelativeDateTimeFormatter.

#include <cstring>
#include <memory>

#include "fuzzer_utils.h"

#include "unicode/reldatefmt.h"
#include "unicode/locid.h"

const UDisplayContext validCapitalizations[] = {
    UDISPCTX_CAPITALIZATION_NONE,
    UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
    UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
    UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,
    UDISPCTX_CAPITALIZATION_FOR_STANDALONE
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    uint16_t rnd;
    UDateRelativeDateTimeFormatterStyle style;
    UDisplayContext context;
    if (size < sizeof(rnd) + sizeof(style) + sizeof(context) ) return 0;
    icu::StringPiece fuzzData(reinterpret_cast<const char *>(data), size);

    std::memcpy(&rnd, fuzzData.data(), sizeof(rnd));
    icu::Locale locale = GetRandomLocale(rnd);
    fuzzData.remove_prefix(sizeof(rnd));

    std::memcpy(&style, fuzzData.data(), sizeof(style));
    fuzzData.remove_prefix(sizeof(style));

    std::memcpy(&context, fuzzData.data(), sizeof(context));
    fuzzData.remove_prefix(sizeof(context));

    UErrorCode status = U_ZERO_ERROR;
    // Test all range of style and context
    std::unique_ptr<icu::RelativeDateTimeFormatter> formatter(
        new icu::RelativeDateTimeFormatter(
            locale, nullptr, style, context, status));
    status = U_ZERO_ERROR;

    // Then we test with only valid style and context
    style = static_cast<UDateRelativeDateTimeFormatterStyle>(
        static_cast<int>(style) % UDAT_STYLE_COUNT);
    context = validCapitalizations[
        static_cast<int>(context) %
            (sizeof(validCapitalizations) / sizeof(UDisplayContext))];
    formatter =
        std::make_unique<icu::RelativeDateTimeFormatter>(locale, nullptr, style, context, status);

    if (U_SUCCESS(status)) {
        URelativeDateTimeUnit unit;
        UDateAbsoluteUnit aunit;
        UDateDirection dir;
        double number;
        while (fuzzData.length() >
               static_cast<int>(sizeof(unit) + sizeof(number))) {
            // unit and aunit share the same bytes.
            std::memcpy(&unit, fuzzData.data(), sizeof(unit));
            std::memcpy(&aunit, fuzzData.data(), sizeof(aunit));
            fuzzData.remove_prefix(sizeof(unit));

            // number and dir share the same bytes.
            std::memcpy(&number, fuzzData.data(), sizeof(number));
            std::memcpy(&dir, fuzzData.data(), sizeof(dir));
            fuzzData.remove_prefix(sizeof(number));

            // Test with any values for unit, aunit and dir
            status = U_ZERO_ERROR;
            icu::FormattedRelativeDateTime formatted =
                formatter->formatNumericToValue(number, unit, status);
            status = U_ZERO_ERROR;
            formatted = formatter->formatToValue(number, unit, status);

            status = U_ZERO_ERROR;
            formatted = formatter->formatToValue(dir, aunit, status);

            // Test with valid values for unit and aunit
            unit = static_cast<URelativeDateTimeUnit>(
                static_cast<int>(unit) % UDAT_REL_UNIT_COUNT);
            aunit = static_cast<UDateAbsoluteUnit>(
                static_cast<int>(aunit) % UDAT_ABSOLUTE_UNIT_COUNT);
            dir = static_cast<UDateDirection>(
                static_cast<int>(dir) % UDAT_DIRECTION_COUNT);

            status = U_ZERO_ERROR;
            formatted = formatter->formatNumericToValue(number, unit, status);
            status = U_ZERO_ERROR;
            formatted = formatter->formatToValue(number, unit, status);
            status = U_ZERO_ERROR;
            formatted = formatter->formatToValue(dir, aunit, status);
        }
    }

    return EXIT_SUCCESS;
}
