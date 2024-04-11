// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numbertest.h"
#include "unicode/numberrangeformatter.h"

#include <cmath>
#include <numparse_affixes.h>

// Horrible workaround for the lack of a status code in the constructor...
// (Also affects numbertest_api.cpp)
UErrorCode globalNumberRangeFormatterTestStatus = U_ZERO_ERROR;

NumberRangeFormatterTest::NumberRangeFormatterTest()
        : NumberRangeFormatterTest(globalNumberRangeFormatterTestStatus) {
}

NumberRangeFormatterTest::NumberRangeFormatterTest(UErrorCode& status)
        : USD(u"USD", status),
          CHF(u"CHF", status),
          GBP(u"GBP", status),
          PTE(u"PTE", status) {

    // Check for error on the first MeasureUnit in case there is no data
    LocalPointer<MeasureUnit> unit(MeasureUnit::createMeter(status));
    if (U_FAILURE(status)) {
        dataerrln("%s %d status = %s", __FILE__, __LINE__, u_errorName(status));
        return;
    }
    METER = *unit;

    KILOMETER = *LocalPointer<MeasureUnit>(MeasureUnit::createKilometer(status));
    FAHRENHEIT = *LocalPointer<MeasureUnit>(MeasureUnit::createFahrenheit(status));
    KELVIN = *LocalPointer<MeasureUnit>(MeasureUnit::createKelvin(status));
}

void NumberRangeFormatterTest::runIndexedTest(int32_t index, UBool exec, const char*& name, char*) {
    if (exec) {
        logln("TestSuite NumberRangeFormatterTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testSanity);
        TESTCASE_AUTO(testBasic);
        TESTCASE_AUTO(testCollapse);
        TESTCASE_AUTO(testIdentity);
        TESTCASE_AUTO(testDifferentFormatters);
        TESTCASE_AUTO(testNaNInfinity);
        TESTCASE_AUTO(testPlurals);
        TESTCASE_AUTO(testFieldPositions);
        TESTCASE_AUTO(testCopyMove);
        TESTCASE_AUTO(toObject);
        TESTCASE_AUTO(locale);
        TESTCASE_AUTO(testGetDecimalNumbers);
        TESTCASE_AUTO(test21684_Performance);
        TESTCASE_AUTO(test21358_SignPosition);
        TESTCASE_AUTO(test21683_StateLeak);
        TESTCASE_AUTO(testCreateLNRFFromNumberingSystemInSkeleton);
        TESTCASE_AUTO(test22288_DifferentStartEndSettings);
    TESTCASE_AUTO_END;
}

void NumberRangeFormatterTest::testSanity() {
    IcuTestErrorCode status(*this, "testSanity");
    LocalizedNumberRangeFormatter lnrf1 = NumberRangeFormatter::withLocale("en-us");
    LocalizedNumberRangeFormatter lnrf2 = NumberRangeFormatter::with().locale("en-us");
    assertEquals("Formatters should have same behavior 1",
        lnrf1.formatFormattableRange(4, 6, status).toString(status),
        lnrf2.formatFormattableRange(4, 6, status).toString(status));
}

void NumberRangeFormatterTest::testBasic() {
    assertFormatRange(
        u"Basic",
        NumberRangeFormatter::with(),
        Locale("en-us"),
        u"1–5",
        u"~5",
        u"~5",
        u"0–3",
        u"~0",
        u"3–3,000",
        u"3,000–5,000",
        u"4,999–5,001",
        u"~5,000",
        u"5,000–5,000,000");

    assertFormatRange(
        u"Basic with units",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().unit(METER)),
        Locale("en-us"),
        u"1–5 m",
        u"~5 m",
        u"~5 m",
        u"0–3 m",
        u"~0 m",
        u"3–3,000 m",
        u"3,000–5,000 m",
        u"4,999–5,001 m",
        u"~5,000 m",
        u"5,000–5,000,000 m");

    assertFormatRange(
        u"Basic with different units",
        NumberRangeFormatter::with()
            .numberFormatterFirst(NumberFormatter::with().unit(METER))
            .numberFormatterSecond(NumberFormatter::with().unit(KILOMETER)),
        Locale("en-us"),
        u"1 m – 5 km",
        u"5 m – 5 km",
        u"5 m – 5 km",
        u"0 m – 3 km",
        u"0 m – 0 km",
        u"3 m – 3,000 km",
        u"3,000 m – 5,000 km",
        u"4,999 m – 5,001 km",
        u"5,000 m – 5,000 km",
        u"5,000 m – 5,000,000 km");

    assertFormatRange(
        u"Basic long unit",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().unit(METER).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)),
        Locale("en-us"),
        u"1–5 meters",
        u"~5 meters",
        u"~5 meters",
        u"0–3 meters",
        u"~0 meters",
        u"3–3,000 meters",
        u"3,000–5,000 meters",
        u"4,999–5,001 meters",
        u"~5,000 meters",
        u"5,000–5,000,000 meters");

    assertFormatRange(
        u"Non-English locale and unit",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().unit(FAHRENHEIT).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)),
        Locale("fr-FR"),
        u"1–5\u00A0degrés Fahrenheit",
        u"≃5\u00A0degrés Fahrenheit",
        u"≃5\u00A0degrés Fahrenheit",
        u"0–3\u00A0degrés Fahrenheit",
        u"≃0\u00A0degré Fahrenheit",
        u"3–3\u202F000\u00A0degrés Fahrenheit",
        u"3\u202F000–5\u202F000\u00A0degrés Fahrenheit",
        u"4\u202F999–5\u202F001\u00A0degrés Fahrenheit",
        u"≃5\u202F000\u00A0degrés Fahrenheit",
        u"5\u202F000–5\u202F000\u202F000\u00A0degrés Fahrenheit");

    assertFormatRange(
        u"Locale with custom range separator",
        NumberRangeFormatter::with(),
        Locale("ja"),
        u"1～5",
        u"約5",
        u"約5",
        u"0～3",
        u"約0",
        u"3～3,000",
        u"3,000～5,000",
        u"4,999～5,001",
        u"約5,000",
        u"5,000～5,000,000");

    assertFormatRange(
        u"Locale that already has spaces around range separator",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_NONE)
            .numberFormatterBoth(NumberFormatter::with().unit(KELVIN)),
        Locale("hr"),
        u"1 K – 5 K",
        u"~5 K",
        u"~5 K",
        u"0 K – 3 K",
        u"~0 K",
        u"3 K – 3.000 K",
        u"3.000 K – 5.000 K",
        u"4.999 K – 5.001 K",
        u"~5.000 K",
        u"5.000 K – 5.000.000 K");

    assertFormatRange(
        u"Locale with custom numbering system and no plural ranges data",
        NumberRangeFormatter::with(),
        Locale("shn@numbers=beng"),
        // 012459 = ০১৩৪৫৯
        u"১–৫",
        u"~৫",
        u"~৫",
        u"০–৩",
        u"~০",
        u"৩–৩,০০০",
        u"৩,০০০–৫,০০০",
        u"৪,৯৯৯–৫,০০১",
        u"~৫,০০০",
        u"৫,০০০–৫,০০০,০০০");

    assertFormatRange(
        u"Portuguese currency",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().unit(PTE)),
        Locale("pt-PT"),
        u"1$00 - 5$00 \u200B",
        u"~5$00 \u200B",
        u"~5$00 \u200B",
        u"0$00 - 3$00 \u200B",
        u"~0$00 \u200B",
        u"3$00 - 3000$00 \u200B",
        u"3000$00 - 5000$00 \u200B",
        u"4999$00 - 5001$00 \u200B",
        u"~5000$00 \u200B",
        u"5000$00 - 5,000,000$00 \u200B");
}

void NumberRangeFormatterTest::testCollapse() {
    assertFormatRange(
        u"Default collapse on currency (default rounding)",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().unit(USD)),
        Locale("en-us"),
        u"$1.00 – $5.00",
        u"~$5.00",
        u"~$5.00",
        u"$0.00 – $3.00",
        u"~$0.00",
        u"$3.00 – $3,000.00",
        u"$3,000.00 – $5,000.00",
        u"$4,999.00 – $5,001.00",
        u"~$5,000.00",
        u"$5,000.00 – $5,000,000.00");

    assertFormatRange(
        u"Default collapse on currency",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().unit(USD).precision(Precision::integer())),
        Locale("en-us"),
        u"$1 – $5",
        u"~$5",
        u"~$5",
        u"$0 – $3",
        u"~$0",
        u"$3 – $3,000",
        u"$3,000 – $5,000",
        u"$4,999 – $5,001",
        u"~$5,000",
        u"$5,000 – $5,000,000");

    assertFormatRange(
        u"No collapse on currency",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_NONE)
            .numberFormatterBoth(NumberFormatter::with().unit(USD).precision(Precision::integer())),
        Locale("en-us"),
        u"$1 – $5",
        u"~$5",
        u"~$5",
        u"$0 – $3",
        u"~$0",
        u"$3 – $3,000",
        u"$3,000 – $5,000",
        u"$4,999 – $5,001",
        u"~$5,000",
        u"$5,000 – $5,000,000");

    assertFormatRange(
        u"Unit collapse on currency",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_UNIT)
            .numberFormatterBoth(NumberFormatter::with().unit(USD).precision(Precision::integer())),
        Locale("en-us"),
        u"$1–5",
        u"~$5",
        u"~$5",
        u"$0–3",
        u"~$0",
        u"$3–3,000",
        u"$3,000–5,000",
        u"$4,999–5,001",
        u"~$5,000",
        u"$5,000–5,000,000");

    assertFormatRange(
        u"All collapse on currency",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_ALL)
            .numberFormatterBoth(NumberFormatter::with().unit(USD).precision(Precision::integer())),
        Locale("en-us"),
        u"$1–5",
        u"~$5",
        u"~$5",
        u"$0–3",
        u"~$0",
        u"$3–3,000",
        u"$3,000–5,000",
        u"$4,999–5,001",
        u"~$5,000",
        u"$5,000–5,000,000");

    assertFormatRange(
        u"Default collapse on currency ISO code",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with()
                .unit(GBP)
                .unitWidth(UNUM_UNIT_WIDTH_ISO_CODE)
                .precision(Precision::integer())),
        Locale("en-us"),
        u"GBP 1–5",
        u"~GBP 5",  // TODO: Fix this at some point
        u"~GBP 5",
        u"GBP 0–3",
        u"~GBP 0",
        u"GBP 3–3,000",
        u"GBP 3,000–5,000",
        u"GBP 4,999–5,001",
        u"~GBP 5,000",
        u"GBP 5,000–5,000,000");

    assertFormatRange(
        u"No collapse on currency ISO code",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_NONE)
            .numberFormatterBoth(NumberFormatter::with()
                .unit(GBP)
                .unitWidth(UNUM_UNIT_WIDTH_ISO_CODE)
                .precision(Precision::integer())),
        Locale("en-us"),
        u"GBP 1 – GBP 5",
        u"~GBP 5",  // TODO: Fix this at some point
        u"~GBP 5",
        u"GBP 0 – GBP 3",
        u"~GBP 0",
        u"GBP 3 – GBP 3,000",
        u"GBP 3,000 – GBP 5,000",
        u"GBP 4,999 – GBP 5,001",
        u"~GBP 5,000",
        u"GBP 5,000 – GBP 5,000,000");

    assertFormatRange(
        u"Unit collapse on currency ISO code",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_UNIT)
            .numberFormatterBoth(NumberFormatter::with()
                .unit(GBP)
                .unitWidth(UNUM_UNIT_WIDTH_ISO_CODE)
                .precision(Precision::integer())),
        Locale("en-us"),
        u"GBP 1–5",
        u"~GBP 5",  // TODO: Fix this at some point
        u"~GBP 5",
        u"GBP 0–3",
        u"~GBP 0",
        u"GBP 3–3,000",
        u"GBP 3,000–5,000",
        u"GBP 4,999–5,001",
        u"~GBP 5,000",
        u"GBP 5,000–5,000,000");

    assertFormatRange(
        u"All collapse on currency ISO code",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_ALL)
            .numberFormatterBoth(NumberFormatter::with()
                .unit(GBP)
                .unitWidth(UNUM_UNIT_WIDTH_ISO_CODE)
                .precision(Precision::integer())),
        Locale("en-us"),
        u"GBP 1–5",
        u"~GBP 5",  // TODO: Fix this at some point
        u"~GBP 5",
        u"GBP 0–3",
        u"~GBP 0",
        u"GBP 3–3,000",
        u"GBP 3,000–5,000",
        u"GBP 4,999–5,001",
        u"~GBP 5,000",
        u"GBP 5,000–5,000,000");

    // Default collapse on measurement unit is in testBasic()

    assertFormatRange(
        u"No collapse on measurement unit",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_NONE)
            .numberFormatterBoth(NumberFormatter::with().unit(METER)),
        Locale("en-us"),
        u"1 m – 5 m",
        u"~5 m",
        u"~5 m",
        u"0 m – 3 m",
        u"~0 m",
        u"3 m – 3,000 m",
        u"3,000 m – 5,000 m",
        u"4,999 m – 5,001 m",
        u"~5,000 m",
        u"5,000 m – 5,000,000 m");

    assertFormatRange(
        u"Unit collapse on measurement unit",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_UNIT)
            .numberFormatterBoth(NumberFormatter::with().unit(METER)),
        Locale("en-us"),
        u"1–5 m",
        u"~5 m",
        u"~5 m",
        u"0–3 m",
        u"~0 m",
        u"3–3,000 m",
        u"3,000–5,000 m",
        u"4,999–5,001 m",
        u"~5,000 m",
        u"5,000–5,000,000 m");

    assertFormatRange(
        u"All collapse on measurement unit",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_ALL)
            .numberFormatterBoth(NumberFormatter::with().unit(METER)),
        Locale("en-us"),
        u"1–5 m",
        u"~5 m",
        u"~5 m",
        u"0–3 m",
        u"~0 m",
        u"3–3,000 m",
        u"3,000–5,000 m",
        u"4,999–5,001 m",
        u"~5,000 m",
        u"5,000–5,000,000 m");

    assertFormatRange(
        u"Default collapse, long-form compact notation",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::compactLong())),
        Locale("de-CH"),
        u"1–5",
        u"≈5",
        u"≈5",
        u"0–3",
        u"≈0",
        u"3–3 Tausend",
        u"3–5 Tausend",
        u"≈5 Tausend",
        u"≈5 Tausend",
        u"5 Tausend – 5 Millionen");

    assertFormatRange(
        u"Unit collapse, long-form compact notation",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_UNIT)
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::compactLong())),
        Locale("de-CH"),
        u"1–5",
        u"≈5",
        u"≈5",
        u"0–3",
        u"≈0",
        u"3–3 Tausend",
        u"3 Tausend – 5 Tausend",
        u"≈5 Tausend",
        u"≈5 Tausend",
        u"5 Tausend – 5 Millionen");

    assertFormatRange(
        u"Default collapse on measurement unit with compact-short notation",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::compactShort()).unit(METER)),
        Locale("en-us"),
        u"1–5 m",
        u"~5 m",
        u"~5 m",
        u"0–3 m",
        u"~0 m",
        u"3–3K m",
        u"3K – 5K m",
        u"~5K m",
        u"~5K m",
        u"5K – 5M m");

    assertFormatRange(
        u"No collapse on measurement unit with compact-short notation",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_NONE)
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::compactShort()).unit(METER)),
        Locale("en-us"),
        u"1 m – 5 m",
        u"~5 m",
        u"~5 m",
        u"0 m – 3 m",
        u"~0 m",
        u"3 m – 3K m",
        u"3K m – 5K m",
        u"~5K m",
        u"~5K m",
        u"5K m – 5M m");

    assertFormatRange(
        u"Unit collapse on measurement unit with compact-short notation",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_UNIT)
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::compactShort()).unit(METER)),
        Locale("en-us"),
        u"1–5 m",
        u"~5 m",
        u"~5 m",
        u"0–3 m",
        u"~0 m",
        u"3–3K m",
        u"3K – 5K m",
        u"~5K m",
        u"~5K m",
        u"5K – 5M m");

    assertFormatRange(
        u"All collapse on measurement unit with compact-short notation",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_ALL)
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::compactShort()).unit(METER)),
        Locale("en-us"),
        u"1–5 m",
        u"~5 m",
        u"~5 m",
        u"0–3 m",
        u"~0 m",
        u"3–3K m",
        u"3–5K m",  // this one is the key use case for ALL
        u"~5K m",
        u"~5K m",
        u"5K – 5M m");

    assertFormatRange(
        u"No collapse on scientific notation",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_NONE)
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::scientific())),
        Locale("en-us"),
        u"1E0 – 5E0",
        u"~5E0",
        u"~5E0",
        u"0E0 – 3E0",
        u"~0E0",
        u"3E0 – 3E3",
        u"3E3 – 5E3",
        u"4.999E3 – 5.001E3",
        u"~5E3",
        u"5E3 – 5E6");

    assertFormatRange(
        u"All collapse on scientific notation",
        NumberRangeFormatter::with()
            .collapse(UNUM_RANGE_COLLAPSE_ALL)
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::scientific())),
        Locale("en-us"),
        u"1–5E0",
        u"~5E0",
        u"~5E0",
        u"0–3E0",
        u"~0E0",
        u"3E0 – 3E3",
        u"3–5E3",
        u"4.999–5.001E3",
        u"~5E3",
        u"5E3 – 5E6");

    // TODO: Test compact currency?
    // The code is not smart enough to differentiate the notation from the unit.
}

void NumberRangeFormatterTest::testIdentity() {
    assertFormatRange(
        u"Identity fallback Range",
        NumberRangeFormatter::with().identityFallback(UNUM_IDENTITY_FALLBACK_RANGE),
        Locale("en-us"),
        u"1–5",
        u"5–5",
        u"5–5",
        u"0–3",
        u"0–0",
        u"3–3,000",
        u"3,000–5,000",
        u"4,999–5,001",
        u"5,000–5,000",
        u"5,000–5,000,000");

    assertFormatRange(
        u"Identity fallback Approximately or Single Value",
        NumberRangeFormatter::with().identityFallback(UNUM_IDENTITY_FALLBACK_APPROXIMATELY_OR_SINGLE_VALUE),
        Locale("en-us"),
        u"1–5",
        u"~5",
        u"5",
        u"0–3",
        u"0",
        u"3–3,000",
        u"3,000–5,000",
        u"4,999–5,001",
        u"5,000",
        u"5,000–5,000,000");

    assertFormatRange(
        u"Identity fallback Single Value",
        NumberRangeFormatter::with().identityFallback(UNUM_IDENTITY_FALLBACK_SINGLE_VALUE),
        Locale("en-us"),
        u"1–5",
        u"5",
        u"5",
        u"0–3",
        u"0",
        u"3–3,000",
        u"3,000–5,000",
        u"4,999–5,001",
        u"5,000",
        u"5,000–5,000,000");

    assertFormatRange(
        u"Identity fallback Approximately or Single Value with compact notation",
        NumberRangeFormatter::with()
            .identityFallback(UNUM_IDENTITY_FALLBACK_APPROXIMATELY_OR_SINGLE_VALUE)
            .numberFormatterBoth(NumberFormatter::with().notation(Notation::compactShort())),
        Locale("en-us"),
        u"1–5",
        u"~5",
        u"5",
        u"0–3",
        u"0",
        u"3–3K",
        u"3K – 5K",
        u"~5K",
        u"5K",
        u"5K – 5M");

    assertFormatRange(
        u"Approximately in middle of unit string",
        NumberRangeFormatter::with().numberFormatterBoth(
            NumberFormatter::with().unit(FAHRENHEIT).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)),
        Locale("zh-Hant"),
        u"華氏 1-5 度",
        u"華氏 ~5 度",
        u"華氏 ~5 度",
        u"華氏 0-3 度",
        u"華氏 ~0 度",
        u"華氏 3-3,000 度",
        u"華氏 3,000-5,000 度",
        u"華氏 4,999-5,001 度",
        u"華氏 ~5,000 度",
        u"華氏 5,000-5,000,000 度");
}

void NumberRangeFormatterTest::testDifferentFormatters() {
    assertFormatRange(
        u"Different rounding rules",
        NumberRangeFormatter::with()
            .numberFormatterFirst(NumberFormatter::with().precision(Precision::integer()))
            .numberFormatterSecond(NumberFormatter::with().precision(Precision::fixedSignificantDigits(2))),
        Locale("en-us"),
        u"1–5.0",
        u"5–5.0",
        u"5–5.0",
        u"0–3.0",
        u"0–0.0",
        u"3–3,000",
        u"3,000–5,000",
        u"4,999–5,000",
        u"5,000–5,000",  // TODO: Should this one be ~5,000?
        u"5,000–5,000,000");
}

void NumberRangeFormatterTest::testNaNInfinity() {
    IcuTestErrorCode status(*this, "testNaNInfinity");

    auto lnf = NumberRangeFormatter::withLocale("en");
    auto result1 = lnf.formatFormattableRange(-uprv_getInfinity(), 0, status);
    auto result2 = lnf.formatFormattableRange(0, uprv_getInfinity(), status);
    auto result3 = lnf.formatFormattableRange(-uprv_getInfinity(), uprv_getInfinity(), status);
    auto result4 = lnf.formatFormattableRange(uprv_getNaN(), 0, status);
    auto result5 = lnf.formatFormattableRange(0, uprv_getNaN(), status);
    auto result6 = lnf.formatFormattableRange(uprv_getNaN(), uprv_getNaN(), status);
    auto result7 = lnf.formatFormattableRange({"1000", status}, {"Infinity", status}, status);
    auto result8 = lnf.formatFormattableRange({"-Infinity", status}, {"NaN", status}, status);

    assertEquals("0 - inf", u"-∞ – 0", result1.toTempString(status));
    assertEquals("-inf - 0", u"0–∞", result2.toTempString(status));
    assertEquals("-inf - inf", u"-∞ – ∞", result3.toTempString(status));
    assertEquals("NaN - 0", u"NaN–0", result4.toTempString(status));
    assertEquals("0 - NaN", u"0–NaN", result5.toTempString(status));
    assertEquals("NaN - NaN", u"~NaN", result6.toTempString(status));
    assertEquals("1000 - inf", u"1,000–∞", result7.toTempString(status));
    assertEquals("-inf - NaN", u"-∞ – NaN", result8.toTempString(status));
}

void NumberRangeFormatterTest::testPlurals() {
    IcuTestErrorCode status(*this, "testPlurals");

    // Locale sl has interesting plural forms:
    // GBP{
    //     one{"britanski funt"}
    //     two{"britanska funta"}
    //     few{"britanski funti"}
    //     other{"britanskih funtov"}
    // }
    Locale locale("sl");

    UnlocalizedNumberFormatter unf = NumberFormatter::with()
        .unit(GBP)
        .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)
        .precision(Precision::integer());
    LocalizedNumberFormatter lnf = unf.locale(locale);

    // For comparison, run the non-range version of the formatter
    assertEquals(Int64ToUnicodeString(1), u"1 britanski funt", lnf.formatDouble(1, status).toString(status));
    assertEquals(Int64ToUnicodeString(2), u"2 britanska funta", lnf.formatDouble(2, status).toString(status));
    assertEquals(Int64ToUnicodeString(3), u"3 britanski funti", lnf.formatDouble(3, status).toString(status));
    assertEquals(Int64ToUnicodeString(5), u"5 britanskih funtov", lnf.formatDouble(5, status).toString(status));
    if (status.errIfFailureAndReset()) { return; }

    LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::with()
        .numberFormatterBoth(unf)
        .identityFallback(UNUM_IDENTITY_FALLBACK_RANGE)
        .locale(locale);

    struct TestCase {
        double first;
        double second;
        const char16_t* expected;
    } cases[] = {
        {1, 1, u"1–1 britanski funti"}, // one + one -> few
        {1, 2, u"1–2 britanska funta"}, // one + two -> two
        {1, 3, u"1–3 britanski funti"}, // one + few -> few
        {1, 5, u"1–5 britanskih funtov"}, // one + other -> other
        {2, 1, u"2–1 britanski funti"}, // two + one -> few
        {2, 2, u"2–2 britanska funta"}, // two + two -> two
        {2, 3, u"2–3 britanski funti"}, // two + few -> few
        {2, 5, u"2–5 britanskih funtov"}, // two + other -> other
        {3, 1, u"3–1 britanski funti"}, // few + one -> few
        {3, 2, u"3–2 britanska funta"}, // few + two -> two
        {3, 3, u"3–3 britanski funti"}, // few + few -> few
        {3, 5, u"3–5 britanskih funtov"}, // few + other -> other
        {5, 1, u"5–1 britanski funti"}, // other + one -> few
        {5, 2, u"5–2 britanska funta"}, // other + two -> two
        {5, 3, u"5–3 britanski funti"}, // other + few -> few
        {5, 5, u"5–5 britanskih funtov"}, // other + other -> other
    };
    for (auto& cas : cases) {
        UnicodeString message = Int64ToUnicodeString(static_cast<int64_t>(cas.first));
        message += u" ";
        message += Int64ToUnicodeString(static_cast<int64_t>(cas.second));
        status.setScope(message);
        UnicodeString actual = lnrf.formatFormattableRange(cas.first, cas.second, status).toString(status);
        assertEquals(message, cas.expected, actual);
        status.errIfFailureAndReset();
    }
}

void NumberRangeFormatterTest::testFieldPositions() {
    {
        const char16_t* message = u"Field position test 1";
        const char16_t* expectedString = u"3K – 5K m";
        FormattedNumberRange result = assertFormattedRangeEquals(
            message,
            NumberRangeFormatter::with()
                .numberFormatterBoth(NumberFormatter::with()
                    .unit(METER)
                    .notation(Notation::compactShort()))
                .locale("en-us"),
            3000,
            5000,
            expectedString);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_NUMBER_RANGE_SPAN, 0, 0, 2},
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 0, 1},
            {UFIELD_CATEGORY_NUMBER, UNUM_COMPACT_FIELD, 1, 2},
            {UFIELD_CATEGORY_NUMBER_RANGE_SPAN, 1, 5, 7},
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 5, 6},
            {UFIELD_CATEGORY_NUMBER, UNUM_COMPACT_FIELD, 6, 7},
            {UFIELD_CATEGORY_NUMBER, UNUM_MEASURE_UNIT_FIELD, 8, 9}};
        checkMixedFormattedValue(
            message,
            result,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Field position test 2";
        const char16_t* expectedString = u"87,654,321–98,765,432";
        FormattedNumberRange result = assertFormattedRangeEquals(
            message,
            NumberRangeFormatter::withLocale("en-us"),
            87654321,
            98765432,
            expectedString);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_NUMBER_RANGE_SPAN, 0, 0, 10},
            {UFIELD_CATEGORY_NUMBER, UNUM_GROUPING_SEPARATOR_FIELD, 2, 3},
            {UFIELD_CATEGORY_NUMBER, UNUM_GROUPING_SEPARATOR_FIELD, 6, 7},
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 0, 10},
            {UFIELD_CATEGORY_NUMBER_RANGE_SPAN, 1, 11, 21},
            {UFIELD_CATEGORY_NUMBER, UNUM_GROUPING_SEPARATOR_FIELD, 13, 14},
            {UFIELD_CATEGORY_NUMBER, UNUM_GROUPING_SEPARATOR_FIELD, 17, 18},
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 11, 21}};
        checkMixedFormattedValue(
            message,
            result,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Field position with approximately sign";
        const char16_t* expectedString = u"~-100";
        FormattedNumberRange result = assertFormattedRangeEquals(
            message,
            NumberRangeFormatter::withLocale("en-us"),
            -100,
            -100,
            expectedString);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_NUMBER, UNUM_APPROXIMATELY_SIGN_FIELD, 0, 1},
            {UFIELD_CATEGORY_NUMBER, UNUM_SIGN_FIELD, 1, 2},
            {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, 2, 5}};
        checkMixedFormattedValue(
            message,
            result,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
}

void NumberRangeFormatterTest::testCopyMove() {
    IcuTestErrorCode status(*this, "testCopyMove");

    // Default constructors
    LocalizedNumberRangeFormatter l1;
    assertEquals("Initial behavior", u"1–5", l1.formatFormattableRange(1, 5, status).toString(status));
    if (status.errDataIfFailureAndReset()) { return; }

    // Setup
    l1 = NumberRangeFormatter::withLocale("fr-FR")
        .numberFormatterBoth(NumberFormatter::with().unit(USD));
    assertEquals("Currency behavior", u"1,00–5,00 $US", l1.formatFormattableRange(1, 5, status).toString(status));

    // Copy constructor
    LocalizedNumberRangeFormatter l2 = l1;
    assertEquals("Copy constructor", u"1,00–5,00 $US", l2.formatFormattableRange(1, 5, status).toString(status));

    // Move constructor
    LocalizedNumberRangeFormatter l3 = std::move(l1);
    assertEquals("Move constructor", u"1,00–5,00 $US", l3.formatFormattableRange(1, 5, status).toString(status));

    // Reset objects for assignment tests
    l1 = NumberRangeFormatter::withLocale("en-us");
    l2 = NumberRangeFormatter::withLocale("en-us");
    assertEquals("Rest behavior, l1", u"1–5", l1.formatFormattableRange(1, 5, status).toString(status));
    assertEquals("Rest behavior, l2", u"1–5", l2.formatFormattableRange(1, 5, status).toString(status));

    // Copy assignment
    l1 = l3;
    assertEquals("Copy constructor", u"1,00–5,00 $US", l1.formatFormattableRange(1, 5, status).toString(status));

    // Move assignment
    l2 = std::move(l3);
    assertEquals("Copy constructor", u"1,00–5,00 $US", l2.formatFormattableRange(1, 5, status).toString(status));

    // FormattedNumberRange
    FormattedNumberRange result = l1.formatFormattableRange(1, 5, status);
    assertEquals("FormattedNumberRange move constructor", u"1,00–5,00 $US", result.toString(status));
    result = l1.formatFormattableRange(3, 6, status);
    assertEquals("FormattedNumberRange move assignment", u"3,00–6,00 $US", result.toString(status));
    FormattedNumberRange fnrdefault;
    fnrdefault.toString(status);
    status.expectErrorAndReset(U_INVALID_STATE_ERROR);
}

void NumberRangeFormatterTest::toObject() {
    IcuTestErrorCode status(*this, "toObject");

    // const lvalue version
    {
        LocalizedNumberRangeFormatter lnf = NumberRangeFormatter::withLocale("en");
        LocalPointer<LocalizedNumberRangeFormatter> lnf2(lnf.clone());
        assertFalse("should create successfully, const lvalue", lnf2.isNull());
        assertEquals("object API test, const lvalue", u"5–7",
            lnf2->formatFormattableRange(5, 7, status).toString(status));
    }

    // rvalue reference version
    {
        LocalPointer<LocalizedNumberRangeFormatter> lnf(
            NumberRangeFormatter::withLocale("en").clone());
        assertFalse("should create successfully, rvalue reference", lnf.isNull());
        assertEquals("object API test, rvalue reference", u"5–7",
            lnf->formatFormattableRange(5, 7, status).toString(status));
    }

    // to std::unique_ptr via assignment
    {
        std::unique_ptr<LocalizedNumberRangeFormatter> lnf =
            NumberRangeFormatter::withLocale("en").clone();
        assertTrue("should create successfully, unique_ptr B", static_cast<bool>(lnf));
        assertEquals("object API test, unique_ptr B", u"5–7",
            lnf->formatFormattableRange(5, 7, status).toString(status));
    }

    // make sure no memory leaks
    {
        NumberRangeFormatter::with().clone();
    }
}

void NumberRangeFormatterTest::locale() {
    IcuTestErrorCode status(*this, "locale");

    LocalizedNumberRangeFormatter lnf = NumberRangeFormatter::withLocale("en")
        .identityFallback(UNUM_IDENTITY_FALLBACK_RANGE);
    UnlocalizedNumberRangeFormatter unf1 = lnf.withoutLocale();
    UnlocalizedNumberRangeFormatter unf2 = NumberRangeFormatter::with()
        .identityFallback(UNUM_IDENTITY_FALLBACK_RANGE)
        .locale("ar-EG")
        .withoutLocale();

    FormattedNumberRange res1 = unf1.locale("bn").formatFormattableRange(5, 5, status);
    assertEquals(u"res1", u"\u09EB\u2013\u09EB", res1.toTempString(status));
    FormattedNumberRange res2 = unf2.locale("ja-JP").formatFormattableRange(5, 5, status);
    assertEquals(u"res2", u"5\uFF5E5", res2.toTempString(status));
}

void NumberRangeFormatterTest::testGetDecimalNumbers() {
    IcuTestErrorCode status(*this, "testGetDecimalNumbers");

    LocalizedNumberRangeFormatter lnf = NumberRangeFormatter::withLocale("en")
        .numberFormatterBoth(NumberFormatter::with().unit(USD));

    // Range of numbers
    {
        FormattedNumberRange range = lnf.formatFormattableRange(1, 5, status);
        assertEquals("Range: Formatted string should be as expected",
            u"$1.00 \u2013 $5.00",
            range.toString(status));
        auto decimalNumbers = range.getDecimalNumbers<std::string>(status);
        // TODO(ICU-21281): DecNum doesn't retain trailing zeros. Is that a problem?
        if (logKnownIssue("ICU-21281")) {
            assertEquals("First decimal number", "1", decimalNumbers.first.c_str());
            assertEquals("Second decimal number", "5", decimalNumbers.second.c_str());
        } else {
            assertEquals("First decimal number", "1.00", decimalNumbers.first.c_str());
            assertEquals("Second decimal number", "5.00", decimalNumbers.second.c_str());
        }
    }

    // Identity fallback
    {
        FormattedNumberRange range = lnf.formatFormattableRange(3, 3, status);
        assertEquals("Identity: Formatted string should be as expected",
            u"~$3.00",
            range.toString(status));
        auto decimalNumbers = range.getDecimalNumbers<std::string>(status);
        // NOTE: DecNum doesn't retain trailing zeros. Is that a problem?
        // TODO(ICU-21281): DecNum doesn't retain trailing zeros. Is that a problem?
        if (logKnownIssue("ICU-21281")) {
            assertEquals("First decimal number", "3", decimalNumbers.first.c_str());
            assertEquals("Second decimal number", "3", decimalNumbers.second.c_str());
        } else {
            assertEquals("First decimal number", "3.00", decimalNumbers.first.c_str());
            assertEquals("Second decimal number", "3.00", decimalNumbers.second.c_str());
        }
    }
}

void NumberRangeFormatterTest::test21684_Performance() {
    IcuTestErrorCode status(*this, "test21684_Performance");
    LocalizedNumberRangeFormatter lnf = NumberRangeFormatter::withLocale("en");
    // The following two lines of code should finish quickly.
    lnf.formatFormattableRange({"-1e99999", status}, {"0", status}, status);
    lnf.formatFormattableRange({"0", status}, {"1e99999", status}, status);
}

void NumberRangeFormatterTest::test21358_SignPosition() {
    IcuTestErrorCode status(*this, "test21358_SignPosition");

    // de-CH has currency pattern "¤ #,##0.00;¤-#,##0.00"
    assertFormatRange(
        u"Approximately sign position with spacing from pattern",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().unit(CHF)),
        Locale("de-CH"),
        u"CHF 1.00–5.00",
        u"CHF≈5.00",
        u"CHF≈5.00",
        u"CHF 0.00–3.00",
        u"CHF≈0.00",
        u"CHF 3.00–3’000.00",
        u"CHF 3’000.00–5’000.00",
        u"CHF 4’999.00–5’001.00",
        u"CHF≈5’000.00",
        u"CHF 5’000.00–5’000’000.00");

    // TODO(ICU-21420): Move the sign to the inside of the number
    assertFormatRange(
        u"Approximately sign position with currency spacing",
        NumberRangeFormatter::with()
            .numberFormatterBoth(NumberFormatter::with().unit(CHF)),
        Locale("en-US"),
        u"CHF 1.00–5.00",
        u"~CHF 5.00",
        u"~CHF 5.00",
        u"CHF 0.00–3.00",
        u"~CHF 0.00",
        u"CHF 3.00–3,000.00",
        u"CHF 3,000.00–5,000.00",
        u"CHF 4,999.00–5,001.00",
        u"~CHF 5,000.00",
        u"CHF 5,000.00–5,000,000.00");

    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("de-CH");
        UnicodeString actual = lnrf.formatFormattableRange(-2, 3, status).toString(status);
        assertEquals("Negative to positive range", u"-2 – 3", actual);
    }

    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("de-CH")
            .numberFormatterBoth(NumberFormatter::forSkeleton(u"%", status));
        UnicodeString actual = lnrf.formatFormattableRange(-2, 3, status).toString(status);
        assertEquals("Negative to positive percent", u"-2% – 3%", actual);
    }

    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("de-CH");
        UnicodeString actual = lnrf.formatFormattableRange(2, -3, status).toString(status);
        assertEquals("Positive to negative range", u"2–-3", actual);
    }

    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("de-CH")
            .numberFormatterBoth(NumberFormatter::forSkeleton(u"%", status));
        UnicodeString actual = lnrf.formatFormattableRange(2, -3, status).toString(status);
        assertEquals("Positive to negative percent", u"2% – -3%", actual);
    }
}

void NumberRangeFormatterTest::testCreateLNRFFromNumberingSystemInSkeleton() {
    IcuTestErrorCode status(*this, "testCreateLNRFFromNumberingSystemInSkeleton");
    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("en")
            .numberFormatterBoth(NumberFormatter::forSkeleton(
                u".### rounding-mode-half-up", status));
        UnicodeString actual = lnrf.formatFormattableRange(1, 234, status).toString(status);
        assertEquals("default numbering system", u"1–234", actual);
        status.errIfFailureAndReset("default numbering system");
    }
    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("th")
            .numberFormatterBoth(NumberFormatter::forSkeleton(
                u".### rounding-mode-half-up numbering-system/thai", status));
        UnicodeString actual = lnrf.formatFormattableRange(1, 234, status).toString(status);
        assertEquals("Thai numbering system", u"๑-๒๓๔", actual);
        status.errIfFailureAndReset("thai numbering system");
    }
    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("en")
            .numberFormatterBoth(NumberFormatter::forSkeleton(
                u".### rounding-mode-half-up numbering-system/arab", status));
        UnicodeString actual = lnrf.formatFormattableRange(1, 234, status).toString(status);
        assertEquals("Arabic numbering system", u"١–٢٣٤", actual);
        status.errIfFailureAndReset("arab numbering system");
    }
    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("en")
            .numberFormatterFirst(NumberFormatter::forSkeleton(u"numbering-system/arab", status))
            .numberFormatterSecond(NumberFormatter::forSkeleton(u"numbering-system/arab", status));
        UnicodeString actual = lnrf.formatFormattableRange(1, 234, status).toString(status);
        assertEquals("Double Arabic numbering system", u"١–٢٣٤", actual);
        status.errIfFailureAndReset("double arab numbering system");
    }
    {
        LocalizedNumberRangeFormatter lnrf = NumberRangeFormatter::withLocale("en")
            .numberFormatterFirst(NumberFormatter::forSkeleton(u"numbering-system/arab", status))
            .numberFormatterSecond(NumberFormatter::forSkeleton(u"numbering-system/latn", status));
        // Note: The error is not set until `formatFormattableRange` because this is where the
        // formatter object gets built.
        lnrf.formatFormattableRange(1, 234, status);
        status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    }
}

void NumberRangeFormatterTest::test21683_StateLeak() {
    IcuTestErrorCode status(*this, "test21683_StateLeak");
    UNumberRangeFormatter* nrf = nullptr;
    UFormattedNumberRange* result = nullptr;
    UConstrainedFieldPosition* fpos = nullptr;

    struct Range {
        double start;
        double end;
        const char16_t* expected;
        int numFields;
    } ranges[] = {
        {1, 2, u"1\u20132", 4},
        {1, 1, u"~1", 2},
    };

    UParseError* perror = nullptr;
    nrf = unumrf_openForSkeletonWithCollapseAndIdentityFallback(
        u"", -1,
        UNUM_RANGE_COLLAPSE_AUTO,
        UNUM_IDENTITY_FALLBACK_APPROXIMATELY,
        "en", perror, status);
    if (status.errIfFailureAndReset("unumrf_openForSkeletonWithCollapseAndIdentityFallback")) {
        goto cleanup;
    }

    result = unumrf_openResult(status);
    if (status.errIfFailureAndReset("unumrf_openResult")) { goto cleanup; }

    for (auto range : ranges) {
        unumrf_formatDoubleRange(nrf, range.start, range.end, result, status);
        if (status.errIfFailureAndReset("unumrf_formatDoubleRange")) { goto cleanup; }

        const auto* formattedValue = unumrf_resultAsValue(result, status);
        if (status.errIfFailureAndReset("unumrf_resultAsValue")) { goto cleanup; }

        int32_t utf16Length;
        const char16_t* utf16Str = ufmtval_getString(formattedValue, &utf16Length, status);
        if (status.errIfFailureAndReset("ufmtval_getString")) { goto cleanup; }

        assertEquals("Format", range.expected, utf16Str);

        ucfpos_close(fpos);
        fpos = ucfpos_open(status);
        if (status.errIfFailureAndReset("ucfpos_open")) { goto cleanup; }

        int numFields = 0;
        while (true) {
            bool hasMore = ufmtval_nextPosition(formattedValue, fpos, status);
            if (status.errIfFailureAndReset("ufmtval_nextPosition")) { goto cleanup; }
            if (!hasMore) {
                break;
            }
            numFields++;
        }
        assertEquals("numFields", range.numFields, numFields);
    }

cleanup:
    unumrf_close(nrf);
    unumrf_closeResult(result);
    ucfpos_close(fpos);
}

void NumberRangeFormatterTest::test22288_DifferentStartEndSettings() {
    IcuTestErrorCode status(*this, "test22288_DifferentStartEndSettings");
    LocalizedNumberRangeFormatter lnrf(NumberRangeFormatter
            ::withLocale("en")
            .collapse(UNUM_RANGE_COLLAPSE_UNIT)
            .numberFormatterFirst(
                NumberFormatter::with()
                    .unit(CurrencyUnit("USD", status))
                    .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)
                    .precision(Precision::integer())
                    .roundingMode(UNUM_ROUND_FLOOR))
            .numberFormatterSecond(
                NumberFormatter::with()
                    .unit(CurrencyUnit("USD", status))
                    .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)
                    .precision(Precision::integer())
                    .roundingMode(UNUM_ROUND_CEILING)));
        FormattedNumberRange result = lnrf.formatFormattableRange(2.5, 2.5, status);
        assertEquals("Should format successfully", u"2–3 US dollars", result.toString(status));
}

void  NumberRangeFormatterTest::assertFormatRange(
      const char16_t* message,
      const UnlocalizedNumberRangeFormatter& f,
      Locale locale,
      const char16_t* expected_10_50,
      const char16_t* expected_49_51,
      const char16_t* expected_50_50,
      const char16_t* expected_00_30,
      const char16_t* expected_00_00,
      const char16_t* expected_30_3K,
      const char16_t* expected_30K_50K,
      const char16_t* expected_49K_51K,
      const char16_t* expected_50K_50K,
      const char16_t* expected_50K_50M) {
    LocalizedNumberRangeFormatter l = f.locale(locale);
    assertFormattedRangeEquals(message, l, 1, 5, expected_10_50);
    assertFormattedRangeEquals(message, l, 4.9999999, 5.0000001, expected_49_51);
    assertFormattedRangeEquals(message, l, 5, 5, expected_50_50);
    assertFormattedRangeEquals(message, l, 0, 3, expected_00_30);
    assertFormattedRangeEquals(message, l, 0, 0, expected_00_00);
    assertFormattedRangeEquals(message, l, 3, 3000, expected_30_3K);
    assertFormattedRangeEquals(message, l, 3000, 5000, expected_30K_50K);
    assertFormattedRangeEquals(message, l, 4999, 5001, expected_49K_51K);
    assertFormattedRangeEquals(message, l, 5000, 5000, expected_50K_50K);
    assertFormattedRangeEquals(message, l, 5e3, 5e6, expected_50K_50M);
}

FormattedNumberRange NumberRangeFormatterTest::assertFormattedRangeEquals(
      const char16_t* message,
      const LocalizedNumberRangeFormatter& l,
      double first,
      double second,
      const char16_t* expected) {
    IcuTestErrorCode status(*this, "assertFormattedRangeEquals");
    UnicodeString fullMessage = UnicodeString(message) + u": " + DoubleToUnicodeString(first) + u", " + DoubleToUnicodeString(second);
    status.setScope(fullMessage);
    FormattedNumberRange fnr = l.formatFormattableRange(first, second, status);
    UnicodeString actual = fnr.toString(status);
    assertEquals(fullMessage, expected, actual);
    return fnr;
}


#endif
