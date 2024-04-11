// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <cmath>
#include <cstdarg>
#include <memory>

#include "unicode/displayoptions.h"
#include "unicode/numberformatter.h"
#include "unicode/testlog.h"
#include "unicode/unum.h"
#include "unicode/utypes.h"

#include "charstr.h"
#include "number_asformat.h"
#include "number_microprops.h"
#include "number_types.h"
#include "number_utils.h"
#include "number_utypes.h"
#include "numbertest.h"

using number::impl::UFormattedNumberData;

// Horrible workaround for the lack of a status code in the constructor...
// (Also affects numbertest_range.cpp)
UErrorCode globalNumberFormatterApiTestStatus = U_ZERO_ERROR;

NumberFormatterApiTest::NumberFormatterApiTest()
        : NumberFormatterApiTest(globalNumberFormatterApiTestStatus) {
}

NumberFormatterApiTest::NumberFormatterApiTest(UErrorCode& status)
        : USD(u"USD", status),
          GBP(u"GBP", status),
          CZK(u"CZK", status),
          CAD(u"CAD", status),
          ESP(u"ESP", status),
          PTE(u"PTE", status),
          RON(u"RON", status),
          TWD(u"TWD", status),
          TRY(u"TRY", status),
          CNY(u"CNY", status),
          FRENCH_SYMBOLS(Locale::getFrench(), status),
          SWISS_SYMBOLS(Locale("de-CH"), status),
          MYANMAR_SYMBOLS(Locale("my"), status) {

    // Check for error on the first MeasureUnit in case there is no data
    LocalPointer<MeasureUnit> unit(MeasureUnit::createMeter(status));
    if (U_FAILURE(status)) {
        dataerrln("%s %d status = %s", __FILE__, __LINE__, u_errorName(status));
        return;
    }
    METER = *unit;

    METER_PER_SECOND = *LocalPointer<MeasureUnit>(MeasureUnit::createMeterPerSecond(status));
    DAY = *LocalPointer<MeasureUnit>(MeasureUnit::createDay(status));
    SQUARE_METER = *LocalPointer<MeasureUnit>(MeasureUnit::createSquareMeter(status));
    FAHRENHEIT = *LocalPointer<MeasureUnit>(MeasureUnit::createFahrenheit(status));
    SECOND = *LocalPointer<MeasureUnit>(MeasureUnit::createSecond(status));
    POUND = *LocalPointer<MeasureUnit>(MeasureUnit::createPound(status));
    POUND_FORCE = *LocalPointer<MeasureUnit>(MeasureUnit::createPoundForce(status));
    SQUARE_MILE = *LocalPointer<MeasureUnit>(MeasureUnit::createSquareMile(status));
    SQUARE_INCH = *LocalPointer<MeasureUnit>(MeasureUnit::createSquareInch(status));
    JOULE = *LocalPointer<MeasureUnit>(MeasureUnit::createJoule(status));
    FURLONG = *LocalPointer<MeasureUnit>(MeasureUnit::createFurlong(status));
    KELVIN = *LocalPointer<MeasureUnit>(MeasureUnit::createKelvin(status));

    MATHSANB = *LocalPointer<NumberingSystem>(NumberingSystem::createInstanceByName("mathsanb", status));
    LATN = *LocalPointer<NumberingSystem>(NumberingSystem::createInstanceByName("latn", status));
}

void NumberFormatterApiTest::runIndexedTest(int32_t index, UBool exec, const char*& name, char*) {
    if (exec) {
        logln("TestSuite NumberFormatterApiTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(notationSimple);
        TESTCASE_AUTO(notationScientific);
        TESTCASE_AUTO(notationCompact);
        TESTCASE_AUTO(unitMeasure);
        TESTCASE_AUTO(unitCompoundMeasure);
        TESTCASE_AUTO(unitArbitraryMeasureUnits);
        TESTCASE_AUTO(unitSkeletons);
        TESTCASE_AUTO(unitUsage);
        TESTCASE_AUTO(unitUsageErrorCodes);
        TESTCASE_AUTO(unitUsageSkeletons);
        TESTCASE_AUTO(unitCurrency);
        TESTCASE_AUTO(unitInflections);
        TESTCASE_AUTO(unitNounClass);
        TESTCASE_AUTO(unitNotConvertible);
        TESTCASE_AUTO(unitPercent);
        TESTCASE_AUTO(unitLocaleTags);
        if (!quick) {
            // Slow test: run in exhaustive mode only
            TESTCASE_AUTO(percentParity);
        }
        TESTCASE_AUTO(roundingFraction);
        TESTCASE_AUTO(roundingFigures);
        TESTCASE_AUTO(roundingFractionFigures);
        TESTCASE_AUTO(roundingOther);
        TESTCASE_AUTO(roundingIncrementRegressionTest);
        TESTCASE_AUTO(roundingPriorityCoverageTest);
        TESTCASE_AUTO(grouping);
        TESTCASE_AUTO(padding);
        TESTCASE_AUTO(integerWidth);
        TESTCASE_AUTO(symbols);
        // TODO: Add this method if currency symbols override support is added.
        //TESTCASE_AUTO(symbolsOverride);
        TESTCASE_AUTO(sign);
        TESTCASE_AUTO(signNearZero);
        TESTCASE_AUTO(signCoverage);
        TESTCASE_AUTO(decimal);
        TESTCASE_AUTO(scale);
        TESTCASE_AUTO(locale);
        TESTCASE_AUTO(skeletonUserGuideExamples);
        TESTCASE_AUTO(formatTypes);
        TESTCASE_AUTO(fieldPositionLogic);
        TESTCASE_AUTO(fieldPositionCoverage);
        TESTCASE_AUTO(toFormat);
        TESTCASE_AUTO(errors);
        if (!quick) {
            // Slow test: run in exhaustive mode only
            // (somewhat slow to check all permutations of settings)
            TESTCASE_AUTO(validRanges);
        }
        TESTCASE_AUTO(copyMove);
        TESTCASE_AUTO(localPointerCAPI);
        TESTCASE_AUTO(toObject);
        TESTCASE_AUTO(toDecimalNumber);
        TESTCASE_AUTO(microPropsInternals);
        TESTCASE_AUTO(formatUnitsAliases);
        TESTCASE_AUTO(testIssue22378);
    TESTCASE_AUTO_END;
}

void NumberFormatterApiTest::notationSimple() {
    assertFormatDescending(
            u"Basic",
            u"",
            u"",
            NumberFormatter::with(),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0");

    assertFormatDescendingBig(
            u"Big Simple",
            u"notation-simple",
            u"",
            NumberFormatter::with().notation(Notation::simple()),
            Locale::getEnglish(),
            u"87,650,000",
            u"8,765,000",
            u"876,500",
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0");

    assertFormatSingle(
            u"Basic with Negative Sign",
            u"",
            u"",
            NumberFormatter::with(),
            Locale::getEnglish(),
            -9876543.21,
            u"-9,876,543.21");
}


void NumberFormatterApiTest::notationScientific() {
    assertFormatDescending(
            u"Scientific",
            u"scientific",
            u"E0",
            NumberFormatter::with().notation(Notation::scientific()),
            Locale::getEnglish(),
            u"8.765E4",
            u"8.765E3",
            u"8.765E2",
            u"8.765E1",
            u"8.765E0",
            u"8.765E-1",
            u"8.765E-2",
            u"8.765E-3",
            u"0E0");

    assertFormatDescending(
            u"Engineering",
            u"engineering",
            u"EE0",
            NumberFormatter::with().notation(Notation::engineering()),
            Locale::getEnglish(),
            u"87.65E3",
            u"8.765E3",
            u"876.5E0",
            u"87.65E0",
            u"8.765E0",
            u"876.5E-3",
            u"87.65E-3",
            u"8.765E-3",
            u"0E0");

    assertFormatDescending(
            u"Scientific sign always shown",
            u"scientific/sign-always",
            u"E+!0",
            NumberFormatter::with().notation(
                    Notation::scientific().withExponentSignDisplay(UNumberSignDisplay::UNUM_SIGN_ALWAYS)),
            Locale::getEnglish(),
            u"8.765E+4",
            u"8.765E+3",
            u"8.765E+2",
            u"8.765E+1",
            u"8.765E+0",
            u"8.765E-1",
            u"8.765E-2",
            u"8.765E-3",
            u"0E+0");

    assertFormatDescending(
            u"Scientific min exponent digits",
            u"scientific/*ee",
            u"E00",
            NumberFormatter::with().notation(Notation::scientific().withMinExponentDigits(2)),
            Locale::getEnglish(),
            u"8.765E04",
            u"8.765E03",
            u"8.765E02",
            u"8.765E01",
            u"8.765E00",
            u"8.765E-01",
            u"8.765E-02",
            u"8.765E-03",
            u"0E00");

    assertFormatSingle(
            u"Scientific Negative",
            u"scientific",
            u"E0",
            NumberFormatter::with().notation(Notation::scientific()),
            Locale::getEnglish(),
            -1000000,
            u"-1E6");

    assertFormatSingle(
            u"Scientific Infinity",
            u"scientific",
            u"E0",
            NumberFormatter::with().notation(Notation::scientific()),
            Locale::getEnglish(),
            -uprv_getInfinity(),
            u"-∞");

    assertFormatSingle(
            u"Scientific NaN",
            u"scientific",
            u"E0",
            NumberFormatter::with().notation(Notation::scientific()),
            Locale::getEnglish(),
            uprv_getNaN(),
            u"NaN");
}

void NumberFormatterApiTest::notationCompact() {
    assertFormatDescending(
            u"Compact Short",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            u"88K",
            u"8.8K",
            u"876",
            u"88",
            u"8.8",
            u"0.88",
            u"0.088",
            u"0.0088",
            u"0");

    assertFormatDescending(
            u"Compact Long",
            u"compact-long",
            u"KK",
            NumberFormatter::with().notation(Notation::compactLong()),
            Locale::getEnglish(),
            u"88 thousand",
            u"8.8 thousand",
            u"876",
            u"88",
            u"8.8",
            u"0.88",
            u"0.088",
            u"0.0088",
            u"0");

    assertFormatDescending(
            u"Compact Short Currency",
            u"compact-short currency/USD",
            u"K currency/USD",
            NumberFormatter::with().notation(Notation::compactShort()).unit(USD),
            Locale::getEnglish(),
            u"$88K",
            u"$8.8K",
            u"$876",
            u"$88",
            u"$8.8",
            u"$0.88",
            u"$0.088",
            u"$0.0088",
            u"$0");

    assertFormatDescending(
            u"Compact Short with ISO Currency",
            u"compact-short currency/USD unit-width-iso-code",
            u"K currency/USD unit-width-iso-code",
            NumberFormatter::with().notation(Notation::compactShort())
                    .unit(USD)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_ISO_CODE),
            Locale::getEnglish(),
            u"USD 88K",
            u"USD 8.8K",
            u"USD 876",
            u"USD 88",
            u"USD 8.8",
            u"USD 0.88",
            u"USD 0.088",
            u"USD 0.0088",
            u"USD 0");

    assertFormatDescending(
            u"Compact Short with Long Name Currency",
            u"compact-short currency/USD unit-width-full-name",
            u"K currency/USD unit-width-full-name",
            NumberFormatter::with().notation(Notation::compactShort())
                    .unit(USD)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getEnglish(),
            u"88K US dollars",
            u"8.8K US dollars",
            u"876 US dollars",
            u"88 US dollars",
            u"8.8 US dollars",
            u"0.88 US dollars",
            u"0.088 US dollars",
            u"0.0088 US dollars",
            u"0 US dollars");

    // Note: Most locales don't have compact long currency, so this currently falls back to short.
    // This test case should be fixed when proper compact long currency patterns are added.
    assertFormatDescending(
            u"Compact Long Currency",
            u"compact-long currency/USD",
            u"KK currency/USD",
            NumberFormatter::with().notation(Notation::compactLong()).unit(USD),
            Locale::getEnglish(),
            u"$88K", // should be something like "$88 thousand"
            u"$8.8K",
            u"$876",
            u"$88",
            u"$8.8",
            u"$0.88",
            u"$0.088",
            u"$0.0088",
            u"$0");

    // Note: Most locales don't have compact long currency, so this currently falls back to short.
    // This test case should be fixed when proper compact long currency patterns are added.
    assertFormatDescending(
            u"Compact Long with ISO Currency",
            u"compact-long currency/USD unit-width-iso-code",
            u"KK currency/USD unit-width-iso-code",
            NumberFormatter::with().notation(Notation::compactLong())
                    .unit(USD)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_ISO_CODE),
            Locale::getEnglish(),
            u"USD 88K", // should be something like "USD 88 thousand"
            u"USD 8.8K",
            u"USD 876",
            u"USD 88",
            u"USD 8.8",
            u"USD 0.88",
            u"USD 0.088",
            u"USD 0.0088",
            u"USD 0");

    // TODO: This behavior could be improved and should be revisited.
    assertFormatDescending(
            u"Compact Long with Long Name Currency",
            u"compact-long currency/USD unit-width-full-name",
            u"KK currency/USD unit-width-full-name",
            NumberFormatter::with().notation(Notation::compactLong())
                    .unit(USD)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getEnglish(),
            u"88 thousand US dollars",
            u"8.8 thousand US dollars",
            u"876 US dollars",
            u"88 US dollars",
            u"8.8 US dollars",
            u"0.88 US dollars",
            u"0.088 US dollars",
            u"0.0088 US dollars",
            u"0 US dollars");

    assertFormatSingle(
            u"Compact Plural One",
            u"compact-long",
            u"KK",
            NumberFormatter::with().notation(Notation::compactLong()),
            Locale::createFromName("es"),
            1000000,
            u"1 millón");

    assertFormatSingle(
            u"Compact Plural Other",
            u"compact-long",
            u"KK",
            NumberFormatter::with().notation(Notation::compactLong()),
            Locale::createFromName("es"),
            2000000,
            u"2 millones");

    assertFormatSingle(
            u"Compact with Negative Sign",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            -9876543.21,
            u"-9.9M");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            990000,
            u"990K");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            999000,
            u"999K");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            999900,
            u"1M");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            9900000,
            u"9.9M");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            9990000,
            u"10M");

    assertFormatSingle(
            u"Compact in zh-Hant-HK",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale("zh-Hant-HK"),
            1e7,
            u"10M");

    assertFormatSingle(
            u"Compact in zh-Hant",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale("zh-Hant"),
            1e7,
            u"1000\u842C");

    assertFormatSingle(
            u"Compact with plural form =1 (ICU-21258)",
            u"compact-long",
            u"KK",
            NumberFormatter::with().notation(Notation::compactLong()),
            Locale("fr-FR"),
            1e3,
            u"mille");

    assertFormatSingle(
            u"Compact Infinity",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            -uprv_getInfinity(),
            u"-∞");

    assertFormatSingle(
            u"Compact NaN",
            u"compact-short",
            u"K",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            uprv_getNaN(),
            u"NaN");

    // NOTE: There is no API for compact custom data in C++
    // and thus no "Compact Somali No Figure" test
}

void NumberFormatterApiTest::unitMeasure() {
    IcuTestErrorCode status(*this, "unitMeasure()");

    assertFormatDescending(
            u"Meters Short and unit() method",
            u"measure-unit/length-meter",
            u"unit/meter",
            NumberFormatter::with().unit(MeasureUnit::getMeter()),
            Locale::getEnglish(),
            u"87,650 m",
            u"8,765 m",
            u"876.5 m",
            u"87.65 m",
            u"8.765 m",
            u"0.8765 m",
            u"0.08765 m",
            u"0.008765 m",
            u"0 m");

    assertFormatDescending(
            u"Meters Long and adoptUnit() method",
            u"measure-unit/length-meter unit-width-full-name",
            u"unit/meter unit-width-full-name",
            NumberFormatter::with().adoptUnit(new MeasureUnit(METER))
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getEnglish(),
            u"87,650 meters",
            u"8,765 meters",
            u"876.5 meters",
            u"87.65 meters",
            u"8.765 meters",
            u"0.8765 meters",
            u"0.08765 meters",
            u"0.008765 meters",
            u"0 meters");

    assertFormatDescending(
            u"Compact Meters Long",
            u"compact-long measure-unit/length-meter unit-width-full-name",
            u"KK unit/meter unit-width-full-name",
            NumberFormatter::with().notation(Notation::compactLong())
                    .unit(METER)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getEnglish(),
            u"88 thousand meters",
            u"8.8 thousand meters",
            u"876 meters",
            u"88 meters",
            u"8.8 meters",
            u"0.88 meters",
            u"0.088 meters",
            u"0.0088 meters",
            u"0 meters");

    assertFormatDescending(
            u"Hectometers",
            u"unit/hectometer",
            u"unit/hectometer",
            NumberFormatter::with().unit(MeasureUnit::forIdentifier("hectometer", status)),
            Locale::getEnglish(),
            u"87,650 hm",
            u"8,765 hm",
            u"876.5 hm",
            u"87.65 hm",
            u"8.765 hm",
            u"0.8765 hm",
            u"0.08765 hm",
            u"0.008765 hm",
            u"0 hm");

//    TODO: Implement Measure in C++
//    assertFormatSingleMeasure(
//            u"Meters with Measure Input",
//            NumberFormatter::with().unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
//            Locale::getEnglish(),
//            new Measure(5.43, new MeasureUnit(METER)),
//            u"5.43 meters");

//    TODO: Implement Measure in C++
//    assertFormatSingleMeasure(
//            u"Measure format method takes precedence over fluent chain",
//            NumberFormatter::with().unit(METER),
//            Locale::getEnglish(),
//            new Measure(5.43, USD),
//            u"$5.43");

    assertFormatSingle(
            u"Meters with Negative Sign",
            u"measure-unit/length-meter",
            u"unit/meter",
            NumberFormatter::with().unit(METER),
            Locale::getEnglish(),
            -9876543.21,
            u"-9,876,543.21 m");

    // The locale string "सान" appears only in brx.txt:
    assertFormatSingle(
            u"Interesting Data Fallback 1",
            u"measure-unit/duration-day unit-width-full-name",
            u"unit/day unit-width-full-name",
            NumberFormatter::with().unit(DAY).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::createFromName("brx"),
            5.43,
            u"5.43 सान");

    // Requires following the alias from unitsNarrow to unitsShort:
    assertFormatSingle(
            u"Interesting Data Fallback 2",
            u"measure-unit/duration-day unit-width-narrow",
            u"unit/day unit-width-narrow",
            NumberFormatter::with().unit(DAY).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW),
            Locale::createFromName("brx"),
            5.43,
            u"5.43 d");

    // en_001.txt has a unitsNarrow/area/square-meter table, but table does not contain the OTHER unit,
    // requiring fallback to the root.
    assertFormatSingle(
            u"Interesting Data Fallback 3",
            u"measure-unit/area-square-meter unit-width-narrow",
            u"unit/square-meter unit-width-narrow",
            NumberFormatter::with().unit(SQUARE_METER).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW),
            Locale::createFromName("en-GB"),
            5.43,
            u"5.43m²");

    // Try accessing a narrow unit directly from root.
    assertFormatSingle(
            u"Interesting Data Fallback 4",
            u"measure-unit/area-square-meter unit-width-narrow",
            u"unit/square-meter unit-width-narrow",
            NumberFormatter::with().unit(SQUARE_METER).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW),
            Locale::createFromName("root"),
            5.43,
            u"5.43 m²");

    // es_US has "{0}°" for unitsNarrow/temperature/FAHRENHEIT.
    // NOTE: This example is in the documentation.
    assertFormatSingle(
            u"Difference between Narrow and Short (Narrow Version)",
            u"measure-unit/temperature-fahrenheit unit-width-narrow",
            u"unit/fahrenheit unit-width-narrow",
            NumberFormatter::with().unit(FAHRENHEIT).unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale("es-US"),
            5.43,
            u"5.43°");

    assertFormatSingle(
            u"Difference between Narrow and Short (Short Version)",
            u"measure-unit/temperature-fahrenheit unit-width-short",
            u"unit/fahrenheit unit-width-short",
            NumberFormatter::with().unit(FAHRENHEIT).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("es-US"),
            5.43,
            u"5.43 °F");

    assertFormatSingle(
            u"MeasureUnit form without {0} in CLDR pattern",
            u"measure-unit/temperature-kelvin unit-width-full-name",
            u"unit/kelvin unit-width-full-name",
            NumberFormatter::with().unit(KELVIN).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("es-MX"),
            1,
            u"kelvin");

    assertFormatSingle(
            u"MeasureUnit form without {0} in CLDR pattern and wide base form",
            u"measure-unit/temperature-kelvin .00000000000000000000 unit-width-full-name",
            u"unit/kelvin .00000000000000000000 unit-width-full-name",
            NumberFormatter::with().precision(Precision::fixedFraction(20))
                    .unit(KELVIN)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("es-MX"),
            1,
            u"kelvin");

    assertFormatSingle(
            u"Person unit not in short form",
            u"measure-unit/duration-year-person unit-width-full-name",
            u"unit/year-person unit-width-full-name",
            NumberFormatter::with().unit(MeasureUnit::getYearPerson())
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("es-MX"),
            5,
            u"5 a\u00F1os");

    assertFormatSingle(
            u"Hubble Constant - usually expressed in km/s/Mpc",
            u"unit/kilometer-per-megaparsec-second",
            u"unit/kilometer-per-megaparsec-second",
            NumberFormatter::with().unit(MeasureUnit::forIdentifier("kilometer-per-second-per-megaparsec", status)),
            Locale("en"),
            74, // Approximate 2019-03-18 measurement
            u"74 km/Mpc⋅sec");

    assertFormatSingle(
            u"Mixed unit",
            u"unit/yard-and-foot-and-inch",
            u"unit/yard-and-foot-and-inch",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("yard-and-foot-and-inch", status)),
            Locale("en-US"),
            3.65,
            "3 yd, 1 ft, 11.4 in");

    assertFormatSingle(
            u"Mixed unit, Scientific",
            u"unit/yard-and-foot-and-inch E0",
            u"unit/yard-and-foot-and-inch E0",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("yard-and-foot-and-inch", status))
                .notation(Notation::scientific()),
            Locale("en-US"),
            3.65,
            "3 yd, 1 ft, 1.14E1 in");

    assertFormatSingle(
            u"Mixed Unit (Narrow Version)",
            u"unit/tonne-and-kilogram-and-gram unit-width-narrow",
            u"unit/tonne-and-kilogram-and-gram unit-width-narrow",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("tonne-and-kilogram-and-gram", status))
                .unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale("en-US"),
            4.28571,
            u"4t 285kg 710g");

    assertFormatSingle(
            u"Mixed Unit (Short Version)",
            u"unit/tonne-and-kilogram-and-gram unit-width-short",
            u"unit/tonne-and-kilogram-and-gram unit-width-short",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("tonne-and-kilogram-and-gram", status))
                .unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("en-US"),
            4.28571,
            u"4 t, 285 kg, 710 g");

    assertFormatSingle(
            u"Mixed Unit (Full Name Version)",
            u"unit/tonne-and-kilogram-and-gram unit-width-full-name",
            u"unit/tonne-and-kilogram-and-gram unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("tonne-and-kilogram-and-gram", status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-US"),
            4.28571,
            u"4 metric tons, 285 kilograms, 710 grams");

    assertFormatSingle(u"Mixed Unit (Not Sorted) [metric]",                               //
                       u"unit/gram-and-kilogram unit-width-full-name",                    //
                       u"unit/gram-and-kilogram unit-width-full-name",                    //
                       NumberFormatter::with()                                            //
                           .unit(MeasureUnit::forIdentifier("gram-and-kilogram", status)) //
                           .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),                         //
                       Locale("en-US"),                                                   //
                       4.28571,                                                           //
                       u"285.71 grams, 4 kilograms");                                     //

    assertFormatSingle(u"Mixed Unit (Not Sorted) [imperial]",                                  //
                       u"unit/inch-and-yard-and-foot unit-width-full-name",                    //
                       u"unit/inch-and-yard-and-foot unit-width-full-name",                    //
                       NumberFormatter::with()                                                 //
                           .unit(MeasureUnit::forIdentifier("inch-and-yard-and-foot", status)) //
                           .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),                              //
                       Locale("en-US"),                                                        //
                       4.28571,                                                                //
                       u"10.28556 inches, 4 yards, 0 feet");                                   //

    assertFormatSingle(u"Mixed Unit (Not Sorted) [imperial full]",                             //
                       u"unit/inch-and-yard-and-foot unit-width-full-name",                    //
                       u"unit/inch-and-yard-and-foot unit-width-full-name",                    //
                       NumberFormatter::with()                                                 //
                           .unit(MeasureUnit::forIdentifier("inch-and-yard-and-foot", status)) //
                           .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),                              //
                       Locale("en-US"),                                                        //
                       4.38571,                                                                //
                       u"1.88556 inches, 4 yards, 1 foot");                                    //

    assertFormatSingle(u"Mixed Unit (Not Sorted) [imperial full integers]",                    //
                       u"unit/inch-and-yard-and-foot @# unit-width-full-name",                 //
                       u"unit/inch-and-yard-and-foot @# unit-width-full-name",                 //
                       NumberFormatter::with()                                                 //
                           .unit(MeasureUnit::forIdentifier("inch-and-yard-and-foot", status)) //
                           .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)                               //
                           .precision(Precision::maxSignificantDigits(2)),                     //
                       Locale("en-US"),                                                        //
                       4.36112,                                                                //
                       u"1 inch, 4 yards, 1 foot");                                            //

    assertFormatSingle(u"Mixed Unit (Not Sorted) [imperial full] with `And` in the end",       //
                       u"unit/inch-and-yard-and-foot unit-width-full-name",                    //
                       u"unit/inch-and-yard-and-foot unit-width-full-name",                    //
                       NumberFormatter::with()                                                 //
                           .unit(MeasureUnit::forIdentifier("inch-and-yard-and-foot", status)) //
                           .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),                              //
                       Locale("fr-FR"),                                                        //
                       4.38571,                                                                //
                       u"1,88556\u00A0pouce, 4\u00A0yards et 1\u00A0pied");                    //

    assertFormatSingle(u"Mixed unit, Scientific [Not in Order]",                               //
                       u"unit/foot-and-inch-and-yard E0",                                      //
                       u"unit/foot-and-inch-and-yard E0",                                      //
                       NumberFormatter::with()                                                 //
                           .unit(MeasureUnit::forIdentifier("foot-and-inch-and-yard", status)) //
                           .notation(Notation::scientific()),                                  //
                       Locale("en-US"),                                                        //
                       3.65,                                                                   //
                       "1 ft, 1.14E1 in, 3 yd");                                               //

    assertFormatSingle(
            u"Testing  \"1 foot 12 inches\"",
            u"unit/foot-and-inch @### unit-width-full-name",
            u"unit/foot-and-inch @### unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("foot-and-inch", status))
                .precision(Precision::maxSignificantDigits(4))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-US"),
            1.9999,
            u"2 feet, 0 inches");

    assertFormatSingle(
            u"Negative numbers: temperature",
            u"measure-unit/temperature-celsius",
            u"unit/celsius",
            NumberFormatter::with().unit(MeasureUnit::forIdentifier("celsius", status)),
            Locale("nl-NL"),
            -6.5,
            u"-6,5°C");

    assertFormatSingle(
            u"Negative numbers: time",
            u"unit/hour-and-minute-and-second",
            u"unit/hour-and-minute-and-second",
            NumberFormatter::with().unit(MeasureUnit::forIdentifier("hour-and-minute-and-second", status)),
            Locale("de-DE"),
            -1.24,
            u"-1 Std., 14 Min. und 24 Sek.");

    assertFormatSingle(
            u"Zero out the unit field",
            u"",
            u"",
            NumberFormatter::with().unit(KELVIN).unit(MeasureUnit()),
            Locale("en"),
            100,
            u"100");

    // TODO: desired behaviour for this "pathological" case?
    // Since this is pointless, we don't test that its behaviour doesn't change.
    // As of January 2021, the produced result has a missing sign: 23.5 Kelvin
    // is "23 Kelvin and -272.65 degrees Celsius":
//     assertFormatSingle(
//             u"Meaningless: kelvin-and-celcius",
//             u"unit/kelvin-and-celsius",
//             u"unit/kelvin-and-celsius",
//             NumberFormatter::with().unit(MeasureUnit::forIdentifier("kelvin-and-celsius", status)),
//             Locale("en"),
//             23.5,
//             u"23 K, 272.65°C");

    if (uprv_getNaN() != 0.0) {
        assertFormatSingle(
                u"Measured -Inf",
                u"measure-unit/electric-ampere",
                u"unit/ampere",
                NumberFormatter::with().unit(MeasureUnit::getAmpere()),
                Locale("en"),
                -uprv_getInfinity(),
                u"-∞ A");

        assertFormatSingle(
                u"Measured NaN",
                u"measure-unit/temperature-celsius",
                u"unit/celsius",
                NumberFormatter::with().unit(MeasureUnit::forIdentifier("celsius", status)),
                Locale("en"),
                uprv_getNaN(),
                u"NaN°C");
    }
}

void NumberFormatterApiTest::unitCompoundMeasure() {
    IcuTestErrorCode status(*this, "unitCompoundMeasure()");

    assertFormatDescending(
            u"Meters Per Second Short (unit that simplifies) and perUnit method",
            u"measure-unit/length-meter per-measure-unit/duration-second",
            u"unit/meter-per-second",
            NumberFormatter::with().unit(METER).perUnit(SECOND),
            Locale::getEnglish(),
            u"87,650 m/s",
            u"8,765 m/s",
            u"876.5 m/s",
            u"87.65 m/s",
            u"8.765 m/s",
            u"0.8765 m/s",
            u"0.08765 m/s",
            u"0.008765 m/s",
            u"0 m/s");

    assertFormatDescending(
            u"Meters Per Second Short, built-in m/s",
            u"measure-unit/speed-meter-per-second",
            u"unit/meter-per-second",
            NumberFormatter::with().unit(METER_PER_SECOND),
            Locale::getEnglish(),
            u"87,650 m/s",
            u"8,765 m/s",
            u"876.5 m/s",
            u"87.65 m/s",
            u"8.765 m/s",
            u"0.8765 m/s",
            u"0.08765 m/s",
            u"0.008765 m/s",
            u"0 m/s");

    assertFormatDescending(
            u"Pounds Per Square Mile Short (secondary unit has per-format) and adoptPerUnit method",
            u"measure-unit/mass-pound per-measure-unit/area-square-mile",
            u"unit/pound-per-square-mile",
            NumberFormatter::with().unit(POUND).adoptPerUnit(new MeasureUnit(SQUARE_MILE)),
            Locale::getEnglish(),
            u"87,650 lb/mi²",
            u"8,765 lb/mi²",
            u"876.5 lb/mi²",
            u"87.65 lb/mi²",
            u"8.765 lb/mi²",
            u"0.8765 lb/mi²",
            u"0.08765 lb/mi²",
            u"0.008765 lb/mi²",
            u"0 lb/mi²");

    assertFormatDescending(
            u"Joules Per Furlong Short (unit with no simplifications or special patterns)",
            u"measure-unit/energy-joule per-measure-unit/length-furlong",
            u"unit/joule-per-furlong",
            NumberFormatter::with().unit(JOULE).perUnit(FURLONG),
            Locale::getEnglish(),
            u"87,650 J/fur",
            u"8,765 J/fur",
            u"876.5 J/fur",
            u"87.65 J/fur",
            u"8.765 J/fur",
            u"0.8765 J/fur",
            u"0.08765 J/fur",
            u"0.008765 J/fur",
            u"0 J/fur");

    assertFormatDescending(
            u"Joules Per Furlong Short with unit identifier via API",
            u"measure-unit/energy-joule per-measure-unit/length-furlong",
            u"unit/joule-per-furlong",
            NumberFormatter::with().unit(MeasureUnit::forIdentifier("joule-per-furlong", status)),
            Locale::getEnglish(),
            u"87,650 J/fur",
            u"8,765 J/fur",
            u"876.5 J/fur",
            u"87.65 J/fur",
            u"8.765 J/fur",
            u"0.8765 J/fur",
            u"0.08765 J/fur",
            u"0.008765 J/fur",
            u"0 J/fur");

    assertFormatDescending(
            u"Pounds per Square Inch: composed",
            u"measure-unit/force-pound-force per-measure-unit/area-square-inch",
            u"unit/pound-force-per-square-inch",
            NumberFormatter::with().unit(POUND_FORCE).perUnit(SQUARE_INCH),
            Locale::getEnglish(),
            u"87,650 psi",
            u"8,765 psi",
            u"876.5 psi",
            u"87.65 psi",
            u"8.765 psi",
            u"0.8765 psi",
            u"0.08765 psi",
            u"0.008765 psi",
            u"0 psi");

    assertFormatDescending(
            u"Pounds per Square Inch: built-in",
            u"measure-unit/force-pound-force per-measure-unit/area-square-inch",
            u"unit/pound-force-per-square-inch",
            NumberFormatter::with().unit(MeasureUnit::getPoundPerSquareInch()),
            Locale::getEnglish(),
            u"87,650 psi",
            u"8,765 psi",
            u"876.5 psi",
            u"87.65 psi",
            u"8.765 psi",
            u"0.8765 psi",
            u"0.08765 psi",
            u"0.008765 psi",
            u"0 psi");

    assertFormatSingle(
            u"m/s/s simplifies to m/s^2",
            u"measure-unit/speed-meter-per-second per-measure-unit/duration-second",
            u"unit/meter-per-square-second",
            NumberFormatter::with().unit(METER_PER_SECOND).perUnit(SECOND),
            Locale("en-GB"),
            2.4,
            u"2.4 m/s\u00B2");

    assertFormatSingle(
            u"Negative numbers: acceleration",
            u"measure-unit/acceleration-meter-per-square-second",
            u"unit/meter-per-second-second",
            NumberFormatter::with().unit(MeasureUnit::forIdentifier("meter-per-pow2-second", status)),
            Locale("af-ZA"),
            -9.81,
            u"-9,81 m/s\u00B2");

    // Testing the rejection of invalid specifications

    // If .unit() is not given a built-in type, .perUnit() is not allowed
    // (because .unit is now flexible enough to handle compound units,
    // .perUnit() is supported for backward compatibility).
    LocalizedNumberFormatter nf = NumberFormatter::with()
             .unit(MeasureUnit::forIdentifier("furlong-pascal", status))
             .perUnit(METER)
             .locale("en-GB");
    status.assertSuccess(); // Error is only returned once we try to format.
    FormattedNumber num = nf.formatDouble(2.4, status);
    if (!status.expectErrorAndReset(U_UNSUPPORTED_ERROR)) {
        errln(UnicodeString("Expected failure for unit/furlong-pascal per-unit/length-meter, got: \"") +
              nf.formatDouble(2.4, status).toString(status) + "\".");
        status.assertSuccess();
    }

    // .perUnit() may only be passed a built-in type, or something that combines
    // to a built-in type together with .unit().
    MeasureUnit SQUARE_SECOND = MeasureUnit::forIdentifier("square-second", status);
    nf = NumberFormatter::with().unit(FURLONG).perUnit(SQUARE_SECOND).locale("en-GB");
    status.assertSuccess(); // Error is only returned once we try to format.
    num = nf.formatDouble(2.4, status);
    if (!status.expectErrorAndReset(U_UNSUPPORTED_ERROR)) {
        errln(UnicodeString("Expected failure, got: \"") +
              nf.formatDouble(2.4, status).toString(status) + "\".");
        status.assertSuccess();
    }
    // As above, "square-second" is not a built-in type, however this time,
    // meter-per-square-second is a built-in type.
    assertFormatSingle(
            u"meter per square-second works as a composed unit",
            u"measure-unit/speed-meter-per-second per-measure-unit/duration-second",
            u"unit/meter-per-square-second",
            NumberFormatter::with().unit(METER).perUnit(SQUARE_SECOND),
            Locale("en-GB"),
            2.4,
            u"2.4 m/s\u00B2");
}

void NumberFormatterApiTest::unitArbitraryMeasureUnits() {
    IcuTestErrorCode status(*this, "unitArbitraryMeasureUnits()");

    // TODO: fix after data bug is resolved? See CLDR-14510.
//     assertFormatSingle(
//             u"Binary unit prefix: kibibyte",
//             u"unit/kibibyte",
//             u"unit/kibibyte",
//             NumberFormatter::with().unit(MeasureUnit::forIdentifier("kibibyte", status)),
//             Locale("en-GB"),
//             2.4,
//             u"2.4 KiB");

    assertFormatSingle(
            u"Binary unit prefix: kibibyte full-name",
            u"unit/kibibyte unit-width-full-name",
            u"unit/kibibyte unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("kibibyte", status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-GB"),
            2.4,
            u"2.4 kibibytes");

    assertFormatSingle(
            u"Binary unit prefix: kibibyte full-name",
            u"unit/kibibyte unit-width-full-name",
            u"unit/kibibyte unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("kibibyte", status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("de"),
            2.4,
            u"2,4 Kibibyte");

    assertFormatSingle(
            u"Binary prefix for non-digital units: kibimeter",
            u"unit/kibimeter",
            u"unit/kibimeter",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("kibimeter", status)),
            Locale("en-GB"),
            2.4,
            u"2.4 Kim");

    assertFormatSingle(
            u"Extra-large prefix: exabyte",
            u"unit/exabyte",
            u"unit/exabyte",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("exabyte", status)),
            Locale("en-GB"),
            2.4,
            u"2.4 Ebyte");

    assertFormatSingle(
            u"Extra-large prefix: exabyte (full-name)",
            u"unit/exabyte unit-width-full-name",
            u"unit/exabyte unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("exabyte", status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-GB"),
            2.4,
            u"2.4 exabytes");

    assertFormatSingle(
            u"SI prefix falling back to root: microohm",
            u"unit/microohm",
            u"unit/microohm",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("microohm", status)),
            Locale("de-CH"),
            2.4,
            u"2.4 μΩ");

    assertFormatSingle(
            u"de-CH fallback to de: microohm unit-width-full-name",
            u"unit/microohm unit-width-full-name",
            u"unit/microohm unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("microohm", status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("de-CH"),
            2.4,
            u"2.4\u00A0Mikroohm");

    assertFormatSingle(
            u"No prefixes, 'times' pattern: joule-furlong",
            u"unit/joule-furlong",
            u"unit/joule-furlong",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("joule-furlong", status)),
            Locale("en"),
            2.4,
            u"2.4 J⋅fur");

    assertFormatSingle(
            u"No numeratorUnitString: per-second",
            u"unit/per-second",
            u"unit/per-second",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("per-second", status)),
            Locale("de-CH"),
            2.4,
            u"2.4/s");

    assertFormatSingle(
            u"No numeratorUnitString: per-second unit-width-full-name",
            u"unit/per-second unit-width-full-name",
            u"unit/per-second unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("per-second", status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("de-CH"),
            2.4,
            u"2.4 pro Sekunde");

    assertFormatSingle(
            u"Prefix in the denominator: nanogram-per-picobarrel",
            u"unit/nanogram-per-picobarrel",
            u"unit/nanogram-per-picobarrel",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("nanogram-per-picobarrel", status)),
            Locale("en-ZA"),
            2.4,
            u"2.4 ng/pbbl");

    assertFormatSingle(
            u"Prefix in the denominator: nanogram-per-picobarrel unit-width-full-name",
            u"unit/nanogram-per-picobarrel unit-width-full-name",
            u"unit/nanogram-per-picobarrel unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("nanogram-per-picobarrel", status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-ZA"),
            2.4,
            u"2.4 nanograms per picobarrel");

    // Valid MeasureUnit, but unformattable, because we only have patterns for
    // pow2 and pow3 at this time:
    LocalizedNumberFormatter lnf = NumberFormatter::with()
              .unit(MeasureUnit::forIdentifier("pow4-mile", status))
              .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)
              .locale("en-ZA");
    lnf.operator=(lnf);  // self-assignment should be a no-op
    lnf.formatInt(1, status);
    status.expectErrorAndReset(U_INTERNAL_PROGRAM_ERROR);

    assertFormatSingle(
            u"kibijoule-foot-per-cubic-gigafurlong-square-second unit-width-full-name",
            u"unit/kibijoule-foot-per-cubic-gigafurlong-square-second unit-width-full-name",
            u"unit/kibijoule-foot-per-cubic-gigafurlong-square-second unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("kibijoule-foot-per-cubic-gigafurlong-square-second",
                                                 status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-ZA"),
            2.4,
            u"2.4 kibijoule-feet per cubic gigafurlong-square second");

    assertFormatSingle(
            u"kibijoule-foot-per-cubic-gigafurlong-square-second unit-width-full-name",
            u"unit/kibijoule-foot-per-cubic-gigafurlong-square-second unit-width-full-name",
            u"unit/kibijoule-foot-per-cubic-gigafurlong-square-second unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("kibijoule-foot-per-cubic-gigafurlong-square-second",
                                                 status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("de-CH"),
            2.4,
            u"2.4\u00A0Kibijoule⋅Fuss pro Kubikgigafurlong⋅Quadratsekunde");

    // TODO(ICU-21504): We want to be able to format this, but "100-kilometer"
    // is not yet supported when it's not part of liter-per-100-kilometer:
    // Actually now in CLDR 40 this is supported directly in data, so change test.
    assertFormatSingle(
            u"kilowatt-hour-per-100-kilometer unit-width-full-name",
            u"unit/kilowatt-hour-per-100-kilometer unit-width-full-name",
            u"unit/kilowatt-hour-per-100-kilometer unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("kilowatt-hour-per-100-kilometer",
                                                 status))
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-ZA"),
            2.4,
            u"2.4 kilowatt-hours per 100 kilometres");
}

// TODO: merge these tests into numbertest_skeletons.cpp instead of here:
void NumberFormatterApiTest::unitSkeletons() {
    const struct TestCase {
        const char *msg;
        const char16_t *inputSkeleton;
        const char16_t *normalizedSkeleton;
    } cases[] = {
        {"old-form built-in compound unit",      //
         u"measure-unit/speed-meter-per-second", //
         u"unit/meter-per-second"},

        {"old-form compound construction, converts to built-in",        //
         u"measure-unit/length-meter per-measure-unit/duration-second", //
         u"unit/meter-per-second"},

        {"old-form compound construction which does not simplify to a built-in", //
         u"measure-unit/energy-joule per-measure-unit/length-meter",             //
         u"unit/joule-per-meter"},

        {"old-form compound-compound ugliness resolves neatly",                   //
         u"measure-unit/speed-meter-per-second per-measure-unit/duration-second", //
         u"unit/meter-per-square-second"},

        {"short-form built-in units stick with the built-in", //
         u"unit/meter-per-second",                            //
         u"unit/meter-per-second"},

        {"short-form compound units stay as is", //
         u"unit/square-meter-per-square-meter",  //
         u"unit/square-meter-per-square-meter"},

        {"short-form compound units stay as is", //
         u"unit/joule-per-furlong",              //
         u"unit/joule-per-furlong"},

        {"short-form that doesn't consist of built-in units", //
         u"unit/hectometer-per-second",                       //
         u"unit/hectometer-per-second"},

        {"short-form that doesn't consist of built-in units", //
         u"unit/meter-per-hectosecond",                       //
         u"unit/meter-per-hectosecond"},

        {"percent compound skeletons handled correctly", //
         u"unit/percent-per-meter",                      //
         u"unit/percent-per-meter"},

        {"permille compound skeletons handled correctly",                 //
         u"measure-unit/concentr-permille per-measure-unit/length-meter", //
         u"unit/permille-per-meter"},

        {"percent simple unit is not actually considered a unit", //
         u"unit/percent",                                         //
         u"percent"},

        {"permille simple unit is not actually considered a unit", //
         u"measure-unit/concentr-permille",                        //
         u"permille"},

        {"Round-trip example from icu-units#35", //
         u"unit/kibijoule-per-furlong",          //
         u"unit/kibijoule-per-furlong"},
    };
    for (const auto& cas : cases) {
        IcuTestErrorCode status(*this, cas.msg);
        auto nf = NumberFormatter::forSkeleton(cas.inputSkeleton, status);
        if (status.errIfFailureAndReset("NumberFormatter::forSkeleton failed")) {
            continue;
        }
        assertEquals(                                                       //
            UnicodeString(true, cas.inputSkeleton, -1) + u" normalization", //
            cas.normalizedSkeleton,                                         //
            nf.toSkeleton(status));
        status.errIfFailureAndReset("NumberFormatter::toSkeleton failed");
    }

    const struct FailCase {
        const char *msg;
        const char16_t *inputSkeleton;
        UErrorCode expectedForSkelStatus;
        UErrorCode expectedToSkelStatus;
    } failCases[] = {
        {"Parsing measure-unit/* results in failure if not built-in unit",
         u"measure-unit/hectometer",     //
         U_NUMBER_SKELETON_SYNTAX_ERROR, //
         U_ZERO_ERROR},

        {"Parsing per-measure-unit/* results in failure if not built-in unit",
         u"measure-unit/meter per-measure-unit/hectosecond", //
         U_NUMBER_SKELETON_SYNTAX_ERROR,                     //
         U_ZERO_ERROR},

        {"\"currency/EUR measure-unit/length-meter\" fails, conflicting skeleton.",
         u"currency/EUR measure-unit/length-meter", //
         U_NUMBER_SKELETON_SYNTAX_ERROR,            //
         U_ZERO_ERROR},

        {"\"measure-unit/length-meter currency/EUR\" fails, conflicting skeleton.",
         u"measure-unit/length-meter currency/EUR", //
         U_NUMBER_SKELETON_SYNTAX_ERROR,            //
         U_ZERO_ERROR},

        {"\"currency/EUR per-measure-unit/meter\" fails, conflicting skeleton.",
         u"currency/EUR per-measure-unit/length-meter", //
         U_NUMBER_SKELETON_SYNTAX_ERROR,                //
         U_ZERO_ERROR},
    };
    for (const auto& cas : failCases) {
        IcuTestErrorCode status(*this, cas.msg);
        auto nf = NumberFormatter::forSkeleton(cas.inputSkeleton, status);
        if (status.expectErrorAndReset(cas.expectedForSkelStatus, cas.msg)) {
            continue;
        }
        nf.toSkeleton(status);
        status.expectErrorAndReset(cas.expectedToSkelStatus, cas.msg);
    }

    IcuTestErrorCode status(*this, "unitSkeletons");
    assertEquals(                                //
        ".unit(METER_PER_SECOND) normalization", //
        u"unit/meter-per-second",                //
        NumberFormatter::with().unit(METER_PER_SECOND).toSkeleton(status));
    assertEquals(                                     //
        ".unit(METER).perUnit(SECOND) normalization", //
        u"unit/meter-per-second",
        NumberFormatter::with().unit(METER).perUnit(SECOND).toSkeleton(status));
    assertEquals(                                                                  //
        ".unit(MeasureUnit::forIdentifier(\"hectometer\", status)) normalization", //
        u"unit/hectometer",
        NumberFormatter::with()
            .unit(MeasureUnit::forIdentifier("hectometer", status))
            .toSkeleton(status));
    assertEquals(                                                                  //
        ".unit(MeasureUnit::forIdentifier(\"hectometer\", status)) normalization", //
        u"unit/meter-per-hectosecond",
        NumberFormatter::with()
            .unit(METER)
            .perUnit(MeasureUnit::forIdentifier("hectosecond", status))
            .toSkeleton(status));

    status.assertSuccess();
    assertEquals(                                                //
        ".unit(CURRENCY) produces a currency/CURRENCY skeleton", //
        u"currency/GBP",                                         //
        NumberFormatter::with().unit(GBP).toSkeleton(status));
    status.assertSuccess();
    // .unit(CURRENCY).perUnit(ANYTHING) is not supported.
    NumberFormatter::with().unit(GBP).perUnit(METER).toSkeleton(status);
    status.expectErrorAndReset(U_UNSUPPORTED_ERROR);
}

void NumberFormatterApiTest::unitUsage() {
    IcuTestErrorCode status(*this, "unitUsage()");
    UnlocalizedNumberFormatter unloc_formatter;
    LocalizedNumberFormatter formatter;
    FormattedNumber formattedNum;
    UnicodeString uTestCase;

    status.assertSuccess();
    formattedNum =
        NumberFormatter::with().usage("road").locale(Locale::getEnglish()).formatInt(1, status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);

    unloc_formatter = NumberFormatter::with().usage("road").unit(MeasureUnit::getMeter());

    uTestCase = u"unitUsage() en-ZA road";
    formatter = unloc_formatter.locale("en-ZA");
    formattedNum = formatter.formatDouble(321, status);
    status.errIfFailureAndReset("unitUsage() en-ZA road formatDouble");
    assertTrue(
            uTestCase + u", got outputUnit: \"" + formattedNum.getOutputUnit(status).getIdentifier() + "\"",
            MeasureUnit::getMeter() == formattedNum.getOutputUnit(status));
    assertEquals(uTestCase, "300 m", formattedNum.toString(status));
    {
        static const UFieldPosition expectedFieldPositions[] = {
                {UNUM_INTEGER_FIELD, 0, 3},
                {UNUM_MEASURE_UNIT_FIELD, 4, 5}};
        assertNumberFieldPositions(
                (uTestCase + u" field positions").getTerminatedBuffer(),
                formattedNum,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }
    assertFormatDescendingBig(
            uTestCase.getTerminatedBuffer(),
            u"measure-unit/length-meter usage/road",
            u"unit/meter usage/road",
            unloc_formatter,
            Locale("en-ZA"),
            u"87,650 km",
            u"8,765 km",
            u"876 km", // 6.5 rounds down, 7.5 rounds up.
            u"88 km",
            u"8.8 km",
            u"900 m",
            u"90 m",
            u"9 m",
            u"0 m");

    uTestCase = u"unitUsage() en-GB road";
    formatter = unloc_formatter.locale("en-GB");
    formattedNum = formatter.formatDouble(321, status);
    status.errIfFailureAndReset("unitUsage() en-GB road, formatDouble(...)");
    assertTrue(
            uTestCase + u", got outputUnit: \"" + formattedNum.getOutputUnit(status).getIdentifier() + "\"",
            MeasureUnit::getYard() == formattedNum.getOutputUnit(status));
    status.errIfFailureAndReset("unitUsage() en-GB road, getOutputUnit(...)");
    assertEquals(uTestCase, "350 yd", formattedNum.toString(status));
    status.errIfFailureAndReset("unitUsage() en-GB road, toString(...)");
    {
        static const UFieldPosition expectedFieldPositions[] = {
                {UNUM_INTEGER_FIELD, 0, 3},
                {UNUM_MEASURE_UNIT_FIELD, 4, 6}};
        assertNumberFieldPositions(
                (uTestCase + u" field positions").getTerminatedBuffer(),
                formattedNum,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }
    assertFormatDescendingBig(
            uTestCase.getTerminatedBuffer(),
            u"measure-unit/length-meter usage/road",
            u"unit/meter usage/road",
            unloc_formatter,
            Locale("en-GB"),
            u"54,463 mi",
            u"5,446 mi",
            u"545 mi",
            u"54 mi",
            u"5.4 mi",
            u"0.54 mi",
            u"100 yd",
            u"10 yd",
            u"0 yd");

    uTestCase = u"unitUsage() en-US road";
    formatter = unloc_formatter.locale("en-US");
    formattedNum = formatter.formatDouble(321, status);
    status.errIfFailureAndReset("unitUsage() en-US road, formatDouble(...)");
    assertTrue(
            uTestCase + u", got outputUnit: \"" + formattedNum.getOutputUnit(status).getIdentifier() + "\"",
            MeasureUnit::getFoot() == formattedNum.getOutputUnit(status));
    status.errIfFailureAndReset("unitUsage() en-US road, getOutputUnit(...)");
    assertEquals(uTestCase, "1,050 ft", formattedNum.toString(status));
    status.errIfFailureAndReset("unitUsage() en-US road, toString(...)");
    {
        static const UFieldPosition expectedFieldPositions[] = {
                {UNUM_GROUPING_SEPARATOR_FIELD, 1, 2},
                {UNUM_INTEGER_FIELD, 0, 5},
                {UNUM_MEASURE_UNIT_FIELD, 6, 8}};
        assertNumberFieldPositions(
                (uTestCase + u" field positions").getTerminatedBuffer(),
                formattedNum,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }
    assertFormatDescendingBig(
            uTestCase.getTerminatedBuffer(),
            u"measure-unit/length-meter usage/road",
            u"unit/meter usage/road",
            unloc_formatter,
            Locale("en-US"),
            u"54,463 mi",
            u"5,446 mi",
            u"545 mi",
            u"54 mi",
            u"5.4 mi",
            u"0.54 mi",
            u"300 ft",
            u"30 ft",
            u"0 ft");

    unloc_formatter = NumberFormatter::with().usage("person").unit(MeasureUnit::getKilogram());
    uTestCase = u"unitUsage() en-GB person";
    formatter = unloc_formatter.locale("en-GB");
    formattedNum = formatter.formatDouble(80, status);
    status.errIfFailureAndReset("unitUsage() en-GB person formatDouble");
    assertTrue(
        uTestCase + ", got outputUnit: \"" + formattedNum.getOutputUnit(status).getIdentifier() + "\"",
        MeasureUnit::forIdentifier("stone-and-pound", status) == formattedNum.getOutputUnit(status));
    status.errIfFailureAndReset("unitUsage() en-GB person - formattedNum.getOutputUnit(status)");
    assertEquals(uTestCase, "12 st, 8.4 lb", formattedNum.toString(status));
    status.errIfFailureAndReset("unitUsage() en-GB person, toString(...)");
    {
        static const UFieldPosition expectedFieldPositions[] = {
                // // Desired output: TODO(icu-units#67)
                // {UNUM_INTEGER_FIELD, 0, 2},
                // {UNUM_MEASURE_UNIT_FIELD, 3, 5},
                // {ULISTFMT_LITERAL_FIELD, 5, 6},
                // {UNUM_INTEGER_FIELD, 7, 8},
                // {UNUM_DECIMAL_SEPARATOR_FIELD, 8, 9},
                // {UNUM_FRACTION_FIELD, 9, 10},
                // {UNUM_MEASURE_UNIT_FIELD, 11, 13}};

                // Current output: rather no fields than wrong fields
                {UNUM_INTEGER_FIELD, 7, 8},
                {UNUM_DECIMAL_SEPARATOR_FIELD, 8, 9},
                {UNUM_FRACTION_FIELD, 9, 10},
                };
        assertNumberFieldPositions(
                (uTestCase + u" field positions").getTerminatedBuffer(),
                formattedNum,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }
    assertFormatDescending(
            uTestCase.getTerminatedBuffer(),
            u"measure-unit/mass-kilogram usage/person",
            u"unit/kilogram usage/person",
            unloc_formatter,
            Locale("en-GB"),
            u"13,802 st, 7.2 lb",
            u"1,380 st, 3.5 lb",
            u"138 st, 0.35 lb",
            u"13 st, 11 lb",
            u"1 st, 5.3 lb",
            u"1 lb, 15 oz",
            u"0 lb, 3.1 oz",
            u"0 lb, 0.31 oz",
            u"0 lb, 0 oz");

   assertFormatDescending(
            uTestCase.getTerminatedBuffer(),
            u"usage/person unit-width-narrow measure-unit/mass-kilogram",
            u"usage/person unit-width-narrow unit/kilogram",
            unloc_formatter.unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale("en-GB"),
            u"13,802st 7.2lb",
            u"1,380st 3.5lb",
            u"138st 0.35lb",
            u"13st 11lb",
            u"1st 5.3lb",
            u"1lb 15oz",
            u"0lb 3.1oz",
            u"0lb 0.31oz",
            u"0lb 0oz");

   assertFormatDescending(
            uTestCase.getTerminatedBuffer(),
            u"usage/person unit-width-short measure-unit/mass-kilogram",
            u"usage/person unit-width-short unit/kilogram",
            unloc_formatter.unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("en-GB"),
            u"13,802 st, 7.2 lb",
            u"1,380 st, 3.5 lb",
            u"138 st, 0.35 lb",
            u"13 st, 11 lb",
            u"1 st, 5.3 lb",
            u"1 lb, 15 oz",
            u"0 lb, 3.1 oz",
            u"0 lb, 0.31 oz",
            u"0 lb, 0 oz");

   assertFormatDescending(
            uTestCase.getTerminatedBuffer(),
            u"usage/person unit-width-full-name measure-unit/mass-kilogram",
            u"usage/person unit-width-full-name unit/kilogram",
            unloc_formatter.unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-GB"),
            u"13,802 stone, 7.2 pounds",
            u"1,380 stone, 3.5 pounds",
            u"138 stone, 0.35 pounds",
            u"13 stone, 11 pounds",
            u"1 stone, 5.3 pounds",
            u"1 pound, 15 ounces",
            u"0 pounds, 3.1 ounces",
            u"0 pounds, 0.31 ounces",
            u"0 pounds, 0 ounces");

    assertFormatDescendingBig(
            u"Scientific notation with Usage: possible when using a reasonable Precision",
            u"scientific @### usage/default measure-unit/area-square-meter unit-width-full-name",
            u"scientific @### usage/default unit/square-meter unit-width-full-name",
            NumberFormatter::with()
                    .unit(SQUARE_METER)
                    .usage("default")
                    .notation(Notation::scientific())
                    .precision(Precision::minMaxSignificantDigits(1, 4))
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-ZA"),
            u"8.765E1 square kilometres",
            u"8.765E0 square kilometres",
            u"8.765E1 hectares",
            u"8.765E0 hectares",
            u"8.765E3 square metres",
            u"8.765E2 square metres",
            u"8.765E1 square metres",
            u"8.765E0 square metres",
            u"0E0 square centimetres");

    assertFormatSingle(
            u"Negative Infinity with Unit Preferences",
            u"measure-unit/area-acre usage/default",
            u"unit/acre usage/default",
            NumberFormatter::with().unit(MeasureUnit::getAcre()).usage("default"),
            Locale::getEnglish(),
            -uprv_getInfinity(),
            u"-∞ sq mi");

//     // TODO(icu-units#131): do we care about NaN?
//     // TODO: on some platforms with MSVC, "-NaN sec" is returned.
//     assertFormatSingle(
//             u"NaN with Unit Preferences",
//             u"measure-unit/area-acre usage/default",
//             u"unit/acre usage/default",
//             NumberFormatter::with().unit(MeasureUnit::getAcre()).usage("default"),
//             Locale::getEnglish(),
//             uprv_getNaN(),
//             u"NaN cm²");

    assertFormatSingle(
            u"Negative numbers: minute-and-second",
            u"measure-unit/duration-second usage/media",
            u"unit/second usage/media",
            NumberFormatter::with().unit(SECOND).usage("media"),
            Locale("nl-NL"),
            -77.7,
            u"-1 min, 18 sec");

    assertFormatSingle(
            u"Negative numbers: media seconds",
            u"measure-unit/duration-second usage/media",
            u"unit/second usage/media",
            NumberFormatter::with().unit(SECOND).usage("media"),
            Locale("nl-NL"),
            -2.7,
            u"-2,7 sec");

//     // TODO: on some platforms with MSVC, "-NaN sec" is returned.
//     assertFormatSingle(
//             u"NaN minute-and-second",
//             u"measure-unit/duration-second usage/media",
//             u"unit/second usage/media",
//             NumberFormatter::with().unit(SECOND).usage("media"),
//             Locale("nl-NL"),
//             uprv_getNaN(),
//             u"NaN sec");

    assertFormatSingle(
            u"NaN meter-and-centimeter",
            u"measure-unit/length-meter usage/person-height",
            u"unit/meter usage/person-height",
            NumberFormatter::with().unit(METER).usage("person-height"),
            Locale("sv-SE"),
            uprv_getNaN(),
            u"0 m, NaN cm");

    assertFormatSingle(
            u"Rounding Mode propagates: rounding down",
            u"usage/road measure-unit/length-centimeter rounding-mode-floor",
            u"usage/road unit/centimeter rounding-mode-floor",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("centimeter", status))
                .usage("road")
                .roundingMode(UNUM_ROUND_FLOOR),
            Locale("en-ZA"),
            34500,
            u"300 m");

    assertFormatSingle(
            u"Rounding Mode propagates: rounding up",
            u"usage/road measure-unit/length-centimeter rounding-mode-ceiling",
            u"usage/road unit/centimeter rounding-mode-ceiling",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("centimeter", status))
                .usage("road")
                .roundingMode(UNUM_ROUND_CEILING),
            Locale("en-ZA"),
            30500,
            u"350 m");

    assertFormatSingle(u"Fuel consumption: inverted units",                                     //
                       u"unit/liter-per-100-kilometer usage/vehicle-fuel",                      //
                       u"unit/liter-per-100-kilometer usage/vehicle-fuel",                      //
                       NumberFormatter::with()                                                  //
                           .unit(MeasureUnit::forIdentifier("liter-per-100-kilometer", status)) //
                           .usage("vehicle-fuel"),                                              //
                       Locale("en-US"),                                                         //
                       6.6,                                                                     //
                       "36 mpg");

    assertFormatSingle(u"Fuel consumption: inverted units, divide-by-zero, en-US",              //
                       u"unit/liter-per-100-kilometer usage/vehicle-fuel",                      //
                       u"unit/liter-per-100-kilometer usage/vehicle-fuel",                      //
                       NumberFormatter::with()                                                  //
                           .unit(MeasureUnit::forIdentifier("liter-per-100-kilometer", status)) //
                           .usage("vehicle-fuel"),                                              //
                       Locale("en-US"),                                                         //
                       0,                                                                       //
                       u"∞ mpg");

    assertFormatSingle(u"Fuel consumption: inverted units, divide-by-zero, en-ZA",      //
                       u"unit/mile-per-gallon usage/vehicle-fuel",                      //
                       u"unit/mile-per-gallon usage/vehicle-fuel",                      //
                       NumberFormatter::with()                                          //
                           .unit(MeasureUnit::forIdentifier("mile-per-gallon", status)) //
                           .usage("vehicle-fuel"),                                      //
                       Locale("en-ZA"),                                                 //
                       0,                                                               //
                       u"∞ l/100 km");

    assertFormatSingle(u"Fuel consumption: inverted units, divide-by-inf",              //
                       u"unit/mile-per-gallon usage/vehicle-fuel",                      //
                       u"unit/mile-per-gallon usage/vehicle-fuel",                      //
                       NumberFormatter::with()                                          //
                           .unit(MeasureUnit::forIdentifier("mile-per-gallon", status)) //
                           .usage("vehicle-fuel"),                                      //
                       Locale("de-CH"),                                                 //
                       uprv_getInfinity(),                                              //
                       u"0 L/100 km");

    // Test calling `.usage("")` should unset the existing usage.
    // First: without usage
    assertFormatSingle(u"Rounding Mode propagates: rounding up",
                       u"measure-unit/length-centimeter rounding-mode-ceiling",
                       u"unit/centimeter rounding-mode-ceiling",
                       NumberFormatter::with()
                           .unit(MeasureUnit::forIdentifier("centimeter", status))
                           .roundingMode(UNUM_ROUND_CEILING),
                       Locale("en-US"), //
                       3048,            //
                       u"3,048 cm");

    // Second: with "road" usage
    assertFormatSingle(u"Rounding Mode propagates: rounding up",
                       u"usage/road measure-unit/length-centimeter rounding-mode-ceiling",
                       u"usage/road unit/centimeter rounding-mode-ceiling",
                       NumberFormatter::with()
                           .unit(MeasureUnit::forIdentifier("centimeter", status))
                           .usage("road")
                           .roundingMode(UNUM_ROUND_CEILING),
                       Locale("en-US"), //
                       3048,            //
                       u"100 ft");

    // Third: with "road" usage, then the usage unsetted by calling .usage("")
    assertFormatSingle(u"Rounding Mode propagates: rounding up",
                       u"measure-unit/length-centimeter rounding-mode-ceiling",
                       u"unit/centimeter rounding-mode-ceiling",
                       NumberFormatter::with()
                           .unit(MeasureUnit::forIdentifier("centimeter", status))
                           .usage("road")
                           .roundingMode(UNUM_ROUND_CEILING)
                           .usage(""),  // unset
                       Locale("en-US"), //
                       3048,            //
                       u"3,048 cm");

    assertFormatSingle(u"kilometer-per-liter match the correct category",                   //
                       u"unit/kilometer-per-liter usage/default",                           //
                       u"unit/kilometer-per-liter usage/default",                           //
                       NumberFormatter::with()                                              //
                           .unit(MeasureUnit::forIdentifier("kilometer-per-liter", status)) //
                           .usage("default"),                                               //
                       Locale("en-US"),                                                     //
                       1,                                                                   //
                       u"100 L/100 km");

    assertFormatSingle(u"gallon-per-mile match the correct category",                   //
                       u"unit/gallon-per-mile usage/default",                           //
                       u"unit/gallon-per-mile usage/default",                           //
                       NumberFormatter::with()                                          //
                           .unit(MeasureUnit::forIdentifier("gallon-per-mile", status)) //
                           .usage("default"),                                           //
                       Locale("en-US"),                                                 //
                       1,                                                               //
                       u"235 L/100 km");

    assertFormatSingle(u"psi match the correct category",                          //
                       u"unit/megapascal usage/default",                           //
                       u"unit/megapascal usage/default",                           //
                       NumberFormatter::with()                                     //
                           .unit(MeasureUnit::forIdentifier("megapascal", status)) //
                           .usage("default"),                                      //
                       Locale("en-US"),                                            //
                       1,                                                          //
                       "145 psi");

    assertFormatSingle(u"millibar match the correct category",                   //
                       u"unit/millibar usage/default",                           //
                       u"unit/millibar usage/default",                           //
                       NumberFormatter::with()                                   //
                           .unit(MeasureUnit::forIdentifier("millibar", status)) //
                           .usage("default"),                                    //
                       Locale("en-US"),                                          //
                       1,                                                        //
                       "0.015 psi");

    assertFormatSingle(u"pound-force-per-square-inch match the correct category",                   //
                       u"unit/pound-force-per-square-inch usage/default",                           //
                       u"unit/pound-force-per-square-inch usage/default",                           //
                       NumberFormatter::with()                                                      //
                           .unit(MeasureUnit::forIdentifier("pound-force-per-square-inch", status)) //
                           .usage("default"),                                                       //
                       Locale("en-US"),                                                             //
                       1,                                                                           //
                       "1 psi");                                                                    //

    assertFormatSingle(u"inch-ofhg match the correct category",                   //
                       u"unit/inch-ofhg usage/default",                           //
                       u"unit/inch-ofhg usage/default",                           //
                       NumberFormatter::with()                                    //
                           .unit(MeasureUnit::forIdentifier("inch-ofhg", status)) //
                           .usage("default"),                                     //
                       Locale("en-US"),                                           //
                       1,                                                         //
                       "0.49 psi");

    assertFormatSingle(u"millimeter-ofhg match the correct category",                   //
                       u"unit/millimeter-ofhg usage/default",                           //
                       u"unit/millimeter-ofhg usage/default",                           //
                       NumberFormatter::with()                                          //
                           .unit(MeasureUnit::forIdentifier("millimeter-ofhg", status)) //
                           .usage("default"),                                           //
                       Locale("en-US"),                                                 //
                       1,                                                               //
                       "0.019 psi");

    assertFormatSingle(u"negative temperature conversion",                                 //
                       u"measure-unit/temperature-celsius unit-width-short usage/default", //
                       u"measure-unit/temperature-celsius unit-width-short usage/default", //
                       NumberFormatter::with()                                             //
                           .unit(MeasureUnit::forIdentifier("celsius", status))            //
                           .usage("default")                                               //
                           .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_SHORT),            //
                       Locale("en-US"),                                                    //
                       -1,                                                                 //
                       u"30°F");
}

void NumberFormatterApiTest::unitUsageErrorCodes() {
    IcuTestErrorCode status(*this, "unitUsageErrorCodes()");
    UnlocalizedNumberFormatter unloc_formatter;

    unloc_formatter = NumberFormatter::forSkeleton(u"unit/foobar", status);
    // This gives an error, because foobar is an invalid unit:
    status.expectErrorAndReset(U_NUMBER_SKELETON_SYNTAX_ERROR);

    unloc_formatter = NumberFormatter::forSkeleton(u"usage/foobar", status);
    // This does not give an error, because usage is not looked up yet.
    status.errIfFailureAndReset("Expected behaviour: no immediate error for invalid usage");
    unloc_formatter.locale("en-GB").formatInt(1, status);
    // Lacking a unit results in a failure. The skeleton is "incomplete", but we
    // support adding the unit via the fluent API, so it is not an error until
    // we build the formatting pipeline itself.
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    // Adding the unit as part of the fluent chain leads to success.
    unloc_formatter.unit(MeasureUnit::getMeter()).locale("en-GB").formatInt(1, status);
    status.assertSuccess();

    // Setting unit to the "base dimensionless unit" is like clearing unit.
    unloc_formatter = NumberFormatter::with().unit(MeasureUnit()).usage("default");
    // This does not give an error, because usage-vs-unit isn't resolved yet.
    status.errIfFailureAndReset("Expected behaviour: no immediate error for invalid unit");
    unloc_formatter.locale("en-GB").formatInt(1, status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
}

// Tests for the "skeletons" field in unitPreferenceData, as well as precision
// and notation overrides.
void NumberFormatterApiTest::unitUsageSkeletons() {
    IcuTestErrorCode status(*this, "unitUsageSkeletons()");

    assertFormatSingle(
            u"Default >300m road preference skeletons round to 50m",
            u"usage/road measure-unit/length-meter",
            u"usage/road unit/meter",
            NumberFormatter::with().unit(METER).usage("road"),
            Locale("en-ZA"),
            321,
            u"300 m");

    assertFormatSingle(
            u"Precision can be overridden: override takes precedence",
            u"usage/road measure-unit/length-meter @#",
            u"usage/road unit/meter @#",
            NumberFormatter::with()
                .unit(METER)
                .usage("road")
                .precision(Precision::maxSignificantDigits(2)),
            Locale("en-ZA"),
            321,
            u"320 m");

    assertFormatSingle(
            u"Compact notation with Usage: bizarre, but possible (short)",
            u"compact-short usage/road measure-unit/length-meter",
            u"compact-short usage/road unit/meter",
            NumberFormatter::with()
               .unit(METER)
               .usage("road")
               .notation(Notation::compactShort()),
            Locale("en-ZA"),
            987654321,
            u"988K km");

    assertFormatSingle(
            u"Compact notation with Usage: bizarre, but possible (short, precision override)",
            u"compact-short usage/road measure-unit/length-meter @#",
            u"compact-short usage/road unit/meter @#",
            NumberFormatter::with()
                .unit(METER)
                .usage("road")
                .notation(Notation::compactShort())
                .precision(Precision::maxSignificantDigits(2)),
            Locale("en-ZA"),
            987654321,
            u"990K km");

    assertFormatSingle(
            u"Compact notation with Usage: unusual but possible (long)",
            u"compact-long usage/road measure-unit/length-meter @#",
            u"compact-long usage/road unit/meter @#",
            NumberFormatter::with()
                .unit(METER)
                .usage("road")
                .notation(Notation::compactLong())
                .precision(Precision::maxSignificantDigits(2)),
            Locale("en-ZA"),
            987654321,
            u"990 thousand km");

    assertFormatSingle(
            u"Compact notation with Usage: unusual but possible (long, precision override)",
            u"compact-long usage/road measure-unit/length-meter @#",
            u"compact-long usage/road unit/meter @#",
            NumberFormatter::with()
                .unit(METER)
                .usage("road")
                .notation(Notation::compactLong())
                .precision(Precision::maxSignificantDigits(2)),
            Locale("en-ZA"),
            987654321,
            u"990 thousand km");

    assertFormatSingle(
            u"Scientific notation, not recommended, requires precision override for road",
            u"scientific usage/road measure-unit/length-meter",
            u"scientific usage/road unit/meter",
            NumberFormatter::with().unit(METER).usage("road").notation(Notation::scientific()),
            Locale("en-ZA"),
            321.45,
            // Rounding to the nearest "50" is not exponent-adjusted in scientific notation:
            u"0E2 m");

    assertFormatSingle(
            u"Scientific notation with Usage: possible when using a reasonable Precision",
            u"scientific usage/road measure-unit/length-meter @###",
            u"scientific usage/road unit/meter @###",
            NumberFormatter::with()
                .unit(METER)
                .usage("road")
                .notation(Notation::scientific())
                .precision(Precision::maxSignificantDigits(4)),
            Locale("en-ZA"),
            321.45, // 0.45 rounds down, 0.55 rounds up.
            u"3.214E2 m");

    assertFormatSingle(
            u"Scientific notation with Usage: possible when using a reasonable Precision",
            u"scientific usage/default measure-unit/length-astronomical-unit unit-width-full-name",
            u"scientific usage/default unit/astronomical-unit unit-width-full-name",
            NumberFormatter::with()
                .unit(MeasureUnit::forIdentifier("astronomical-unit", status))
                .usage("default")
                .notation(Notation::scientific())
                .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("en-ZA"),
            1e20,
            u"1.5E28 kilometres");

    status.assertSuccess();
}

void NumberFormatterApiTest::unitCurrency() {
    assertFormatDescending(
            u"Currency",
            u"currency/GBP",
            u"currency/GBP",
            NumberFormatter::with().unit(GBP),
            Locale::getEnglish(),
            u"£87,650.00",
            u"£8,765.00",
            u"£876.50",
            u"£87.65",
            u"£8.76",
            u"£0.88",
            u"£0.09",
            u"£0.01",
            u"£0.00");

    assertFormatDescending(
            u"Currency ISO",
            u"currency/GBP unit-width-iso-code",
            u"currency/GBP unit-width-iso-code",
            NumberFormatter::with().unit(GBP).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_ISO_CODE),
            Locale::getEnglish(),
            u"GBP 87,650.00",
            u"GBP 8,765.00",
            u"GBP 876.50",
            u"GBP 87.65",
            u"GBP 8.76",
            u"GBP 0.88",
            u"GBP 0.09",
            u"GBP 0.01",
            u"GBP 0.00");

    assertFormatDescending(
            u"Currency Long Name",
            u"currency/GBP unit-width-full-name",
            u"currency/GBP unit-width-full-name",
            NumberFormatter::with().unit(GBP).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getEnglish(),
            u"87,650.00 British pounds",
            u"8,765.00 British pounds",
            u"876.50 British pounds",
            u"87.65 British pounds",
            u"8.76 British pounds",
            u"0.88 British pounds",
            u"0.09 British pounds",
            u"0.01 British pounds",
            u"0.00 British pounds");

    assertFormatDescending(
            u"Currency Hidden",
            u"currency/GBP unit-width-hidden",
            u"currency/GBP unit-width-hidden",
            NumberFormatter::with().unit(GBP).unitWidth(UNUM_UNIT_WIDTH_HIDDEN),
            Locale::getEnglish(),
            u"87,650.00",
            u"8,765.00",
            u"876.50",
            u"87.65",
            u"8.76",
            u"0.88",
            u"0.09",
            u"0.01",
            u"0.00");

//    TODO: Implement Measure in C++
//    assertFormatSingleMeasure(
//            u"Currency with CurrencyAmount Input",
//            NumberFormatter::with(),
//            Locale::getEnglish(),
//            new CurrencyAmount(5.43, GBP),
//            u"£5.43");

//    TODO: Enable this test when DecimalFormat wrapper is done.
//    assertFormatSingle(
//            u"Currency Long Name from Pattern Syntax", NumberFormatter.fromDecimalFormat(
//                    PatternStringParser.parseToProperties("0 ¤¤¤"),
//                    DecimalFormatSymbols.getInstance(Locale::getEnglish()),
//                    null).unit(GBP), Locale::getEnglish(), 1234567.89, u"1234568 British pounds");

    assertFormatSingle(
            u"Currency with Negative Sign",
            u"currency/GBP",
            u"currency/GBP",
            NumberFormatter::with().unit(GBP),
            Locale::getEnglish(),
            -9876543.21,
            u"-£9,876,543.21");

    // The full currency symbol is not shown in NARROW format.
    // NOTE: This example is in the documentation.
    assertFormatSingle(
            u"Currency Difference between Narrow and Short (Narrow Version)",
            u"currency/USD unit-width-narrow",
            u"currency/USD unit-width-narrow",
            NumberFormatter::with().unit(USD).unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale("en-CA"),
            5.43,
            u"$5.43");

    assertFormatSingle(
            u"Currency Difference between Narrow and Short (Short Version)",
            u"currency/USD unit-width-short",
            u"currency/USD unit-width-short",
            NumberFormatter::with().unit(USD).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("en-CA"),
            5.43,
            u"US$5.43");

    assertFormatSingle(
            u"Currency Difference between Formal and Short (Formal Version)",
            u"currency/TWD unit-width-formal",
            u"currency/TWD unit-width-formal",
            NumberFormatter::with().unit(TWD).unitWidth(UNUM_UNIT_WIDTH_FORMAL),
            Locale("zh-TW"),
            5.43,
            u"NT$5.43");

    assertFormatSingle(
            u"Currency Difference between Formal and Short (Short Version)",
            u"currency/TWD unit-width-short",
            u"currency/TWD unit-width-short",
            NumberFormatter::with().unit(TWD).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("zh-TW"),
            5.43,
            u"$5.43");

    assertFormatSingle(
            u"Currency Difference between Variant and Short (Formal Version)",
            u"currency/TRY unit-width-variant",
            u"currency/TRY unit-width-variant",
            NumberFormatter::with().unit(TRY).unitWidth(UNUM_UNIT_WIDTH_VARIANT),
            Locale("tr-TR"),
            5.43,
            u"TL\u00A05,43");

    assertFormatSingle(
            u"Currency Difference between Variant and Short (Short Version)",
            u"currency/TRY unit-width-short",
            u"currency/TRY unit-width-short",
            NumberFormatter::with().unit(TRY).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("tr-TR"),
            5.43,
            u"₺5,43");

    assertFormatSingle(
            u"Currency-dependent format (Control)",
            u"currency/USD unit-width-short",
            u"currency/USD unit-width-short",
            NumberFormatter::with().unit(USD).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("ca"),
            444444.55,
            u"444.444,55 USD");

    assertFormatSingle(
            u"Currency-dependent format (Test)",
            u"currency/ESP unit-width-short",
            u"currency/ESP unit-width-short",
            NumberFormatter::with().unit(ESP).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("ca"),
            444444.55,
            u"₧ 444.445");

    assertFormatSingle(
            u"Currency-dependent symbols (Control)",
            u"currency/USD unit-width-short",
            u"currency/USD unit-width-short",
            NumberFormatter::with().unit(USD).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("pt-PT"),
            444444.55,
            u"444 444,55 US$");

    // NOTE: This is a bit of a hack on CLDR's part. They set the currency symbol to U+200B (zero-
    // width space), and they set the decimal separator to the $ symbol.
    assertFormatSingle(
            u"Currency-dependent symbols (Test Short)",
            u"currency/PTE unit-width-short",
            u"currency/PTE unit-width-short",
            NumberFormatter::with().unit(PTE).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("pt-PT"),
            444444.55,
            u"444,444$55 \u200B");

    assertFormatSingle(
            u"Currency-dependent symbols (Test Narrow)",
            u"currency/PTE unit-width-narrow",
            u"currency/PTE unit-width-narrow",
            NumberFormatter::with().unit(PTE).unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale("pt-PT"),
            444444.55,
            u"444,444$55 \u200B");

    assertFormatSingle(
            u"Currency-dependent symbols (Test ISO Code)",
            u"currency/PTE unit-width-iso-code",
            u"currency/PTE unit-width-iso-code",
            NumberFormatter::with().unit(PTE).unitWidth(UNUM_UNIT_WIDTH_ISO_CODE),
            Locale("pt-PT"),
            444444.55,
            u"444,444$55 PTE");

    assertFormatSingle(
            u"Plural form depending on visible digits (ICU-20499)",
            u"currency/RON unit-width-full-name",
            u"currency/RON unit-width-full-name",
            NumberFormatter::with().unit(RON).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("ro-RO"),
            24,
            u"24,00 lei românești");

    assertFormatSingle(
            u"Currency spacing in suffix (ICU-20954)",
            u"currency/CNY",
            u"currency/CNY",
            NumberFormatter::with().unit(CNY),
            Locale("lu"),
            123.12,
            u"123,12 CN¥");

    // de-CH has currency pattern "¤ #,##0.00;¤-#,##0.00"
    assertFormatSingle(
            u"Sign position on negative number with pattern spacing",
            u"currency/RON",
            u"currency/RON",
            NumberFormatter::with().unit(RON),
            Locale("de-CH"),
            -123.12,
            u"RON-123.12");

    // TODO(ICU-21420): Move the sign to the inside of the number
    assertFormatSingle(
            u"Sign position on negative number with currency spacing",
            u"currency/RON",
            u"currency/RON",
            NumberFormatter::with().unit(RON),
            Locale("en"),
            -123.12,
            u"-RON 123.12");
}

void NumberFormatterApiTest::runUnitInflectionsTestCases(UnlocalizedNumberFormatter unf,
                                                         UnicodeString skeleton,
                                                         const UnitInflectionTestCase *cases,
                                                         int32_t numCases,
                                                         IcuTestErrorCode &status) {
    for (int32_t i = 0; i < numCases; i++) {
        UnitInflectionTestCase t = cases[i];
        status.assertSuccess();
        MeasureUnit mu = MeasureUnit::forIdentifier(t.unitIdentifier, status);
        if (status.errIfFailureAndReset("MeasureUnit::forIdentifier(\"%s\", ...) failed",
                                        t.unitIdentifier)) {
            continue;
        };
        UnicodeString skelString = UnicodeString("unit/") + t.unitIdentifier + u" " + skeleton;
        const char16_t *skel;
        if (t.unitDisplayCase == nullptr || t.unitDisplayCase[0] == 0) {
            unf = unf.unit(mu).unitDisplayCase("");
            skel = skelString.getTerminatedBuffer();
        } else {
            unf = unf.unit(mu).unitDisplayCase(t.unitDisplayCase);
            // No skeleton support for unitDisplayCase yet.
            skel = nullptr;
        }
        assertFormatSingle((UnicodeString("Unit: \"") + t.unitIdentifier + ("\", \"") + skeleton +
                            u"\", locale=\"" + t.locale + u"\", case=\"" +
                            (t.unitDisplayCase ? t.unitDisplayCase : "") + u"\", value=" + t.value)
                               .getTerminatedBuffer(),
                           skel, skel, unf, Locale(t.locale), t.value, t.expected);
        status.assertSuccess();
    }

    for (int32_t i = 0; i < numCases; i++) {
        UnitInflectionTestCase t = cases[i];
        status.assertSuccess();
        MeasureUnit mu = MeasureUnit::forIdentifier(t.unitIdentifier, status);
        if (status.errIfFailureAndReset("MeasureUnit::forIdentifier(\"%s\", ...) failed",
                                        t.unitIdentifier)) {
            continue;
        };

        UnicodeString skelString = UnicodeString("unit/") + t.unitIdentifier + u" " + skeleton;
        const char16_t *skel;
        auto displayOptionsBuilder = DisplayOptions::builder();
        if (t.unitDisplayCase == nullptr || t.unitDisplayCase[0] == 0) {
            auto displayoptions = displayOptionsBuilder.build();
            unf = unf.unit(mu).displayOptions(displayoptions);
            skel = skelString.getTerminatedBuffer();
        } else {
            auto displayoptions =
                displayOptionsBuilder
                    .setGrammaticalCase(udispopt_fromGrammaticalCaseIdentifier(t.unitDisplayCase))
                    .build();
            unf = unf.unit(mu).displayOptions(displayoptions);
            // No skeleton support for unitDisplayCase yet.
            skel = nullptr;
        }
        assertFormatSingle((UnicodeString("Unit: \"") + t.unitIdentifier + ("\", \"") + skeleton +
                            u"\", locale=\"" + t.locale + u"\", case=\"" +
                            (t.unitDisplayCase ? t.unitDisplayCase : "") + u"\", value=" + t.value)
                               .getTerminatedBuffer(),
                           skel, skel, unf, Locale(t.locale), t.value, t.expected);
        status.assertSuccess();
    }
}

void NumberFormatterApiTest::unitInflections() {
    IcuTestErrorCode status(*this, "unitInflections");

    UnlocalizedNumberFormatter unf;
    const char16_t *skeleton;
    {
        // Simple inflected form test - test case based on the example in CLDR's
        // grammaticalFeatures.xml
        unf = NumberFormatter::with().unitWidth(UNUM_UNIT_WIDTH_FULL_NAME);
        skeleton = u"unit-width-full-name";
        const UnitInflectionTestCase percentCases[] = {
            {"percent", "ru", nullptr, 10, u"10 процентов"},    // many
            {"percent", "ru", "genitive", 10, u"10 процентов"}, // many
            {"percent", "ru", nullptr, 33, u"33 процента"},     // few
            {"percent", "ru", "genitive", 33, u"33 процентов"}, // few
            {"percent", "ru", nullptr, 1, u"1 процент"},        // one
            {"percent", "ru", "genitive", 1, u"1 процента"},    // one
        };
        runUnitInflectionsTestCases(unf, skeleton, percentCases, UPRV_LENGTHOF(percentCases), status);
    }
    {
        // General testing of inflection rules
        unf = NumberFormatter::with().unitWidth(UNUM_UNIT_WIDTH_FULL_NAME);
        skeleton = u"unit-width-full-name";
        const UnitInflectionTestCase testCases[] = {
            // Check up on the basic values that the compound patterns below are
            // derived from:
            {"meter", "de", nullptr, 1, u"1 Meter"},
            {"meter", "de", "genitive", 1, u"1 Meters"},
            {"meter", "de", nullptr, 2, u"2 Meter"},
            {"meter", "de", "dative", 2, u"2 Metern"},
            {"mile", "de", nullptr, 1, u"1 Meile"},
            {"mile", "de", nullptr, 2, u"2 Meilen"},
            {"day", "de", nullptr, 1, u"1 Tag"},
            {"day", "de", "genitive", 1, u"1 Tages"},
            {"day", "de", nullptr, 2, u"2 Tage"},
            {"day", "de", "dative", 2, u"2 Tagen"},
            {"decade", "de", nullptr, 1, u"1\u00A0Jahrzehnt"},
            {"decade", "de", nullptr, 2, u"2\u00A0Jahrzehnte"},

            // Testing de "per" rules:
            //   <deriveComponent feature="case" structure="per" value0="compound" value1="accusative"/>
            //   <deriveComponent feature="plural" structure="per" value0="compound" value1="one"/>
            // per-patterns use accusative, but since the accusative form
            // matches the nominative form, we're not effectively testing value1
            // in the "case & per" rule above.

            // We have a perUnitPattern for "day" in de, so "per" rules are not
            // applied for these:
            {"meter-per-day", "de", nullptr, 1, u"1 Meter pro Tag"},
            {"meter-per-day", "de", "genitive", 1, u"1 Meters pro Tag"},
            {"meter-per-day", "de", nullptr, 2, u"2 Meter pro Tag"},
            {"meter-per-day", "de", "dative", 2, u"2 Metern pro Tag"},

            // testing code path that falls back to "root" grammaticalFeatures
            // but does not inflect:
            {"meter-per-day", "af", nullptr, 1, u"1 meter per dag"},
            {"meter-per-day", "af", "dative", 1, u"1 meter per dag"},

            // Decade does not have a perUnitPattern at this time (CLDR 39 / ICU
            // 69), so we can use it to test for selection of correct plural form.
            // - Note: fragile test cases, these cases will break when
            //   whitespace is more consistently applied.
            {"parsec-per-decade", "de", nullptr, 1, u"1\u00A0Parsec pro Jahrzehnt"},
            {"parsec-per-decade", "de", "genitive", 1, u"1 Parsec pro Jahrzehnt"},
            {"parsec-per-decade", "de", nullptr, 2, u"2\u00A0Parsec pro Jahrzehnt"},
            {"parsec-per-decade", "de", "dative", 2, u"2 Parsec pro Jahrzehnt"},

            // Testing de "times", "power" and "prefix" rules:
            //
            //   <deriveComponent feature="plural" structure="times" value0="one"  value1="compound"/>
            //   <deriveComponent feature="case" structure="times" value0="nominative"  value1="compound"/>
            //
            //   <deriveComponent feature="plural" structure="prefix" value0="one"  value1="compound"/>
            //   <deriveComponent feature="case" structure="prefix" value0="nominative"  value1="compound"/>
            //
            // Prefixes in German don't change with plural or case, so these
            // tests can't test value0 of the following two rules:
            //   <deriveComponent feature="plural" structure="power" value0="one"  value1="compound"/>
            //   <deriveComponent feature="case" structure="power" value0="nominative"  value1="compound"/>
            {"square-decimeter-dekameter", "de", nullptr, 1, u"1 Dekameter⋅Quadratdezimeter"},
            {"square-decimeter-dekameter", "de", "genitive", 1, u"1 Dekameter⋅Quadratdezimeter"},
            {"square-decimeter-dekameter", "de", nullptr, 2, u"2 Dekameter⋅Quadratdezimeter"},
            {"square-decimeter-dekameter", "de", "dative", 2, u"2 Dekameter⋅Quadratdezimeter"},
            // Feminine "Meile" better demonstrates singular-vs-plural form:
            {"cubic-mile-dekamile", "de", nullptr, 1, u"1 Dekameile⋅Kubikmeile"},
            {"cubic-mile-dekamile", "de", nullptr, 2, u"2 Dekameile⋅Kubikmeilen"},

            // French handles plural "times" and "power" structures differently:
            // plural form impacts all "numerator" units (denominator remains
            // singular like German), and "pow2" prefixes have different forms
            //   <deriveComponent feature="plural" structure="times" value0="compound"  value1="compound"/>
            //   <deriveComponent feature="plural" structure="power" value0="compound"  value1="compound"/>
            {"square-decimeter-square-second", "fr", nullptr, 1, u"1\u00A0décimètre carré-seconde carrée"},
            {"square-decimeter-square-second", "fr", nullptr, 2, u"2\u00A0décimètres carrés-secondes carrées"},
        };
        runUnitInflectionsTestCases(unf, skeleton, testCases, UPRV_LENGTHOF(testCases), status);
    }
    {
        // Testing inflection of mixed units:
        unf = NumberFormatter::with().unitWidth(UNUM_UNIT_WIDTH_FULL_NAME);
        skeleton = u"unit-width-full-name";
        const UnitInflectionTestCase testCases[] = {
            {"meter", "de", nullptr, 1, u"1 Meter"},
            {"meter", "de", "genitive", 1, u"1 Meters"},
            {"meter", "de", "dative", 2, u"2 Metern"},
            {"centimeter", "de", nullptr, 1, u"1 Zentimeter"},
            {"centimeter", "de", "genitive", 1, u"1 Zentimeters"},
            {"centimeter", "de", "dative", 10, u"10 Zentimetern"},
            // TODO(CLDR-14582): check that these inflections are correct, and
            // whether CLDR needs any rules for them (presumably CLDR spec
            // should mention it, if it's a consistent rule):
            {"meter-and-centimeter", "de", nullptr, 1.01, u"1 Meter, 1 Zentimeter"},
            {"meter-and-centimeter", "de", "genitive", 1.01, u"1 Meters, 1 Zentimeters"},
            {"meter-and-centimeter", "de", "genitive", 1.1, u"1 Meters, 10 Zentimeter"},
            {"meter-and-centimeter", "de", "dative", 1.1, u"1 Meter, 10 Zentimetern"},
            {"meter-and-centimeter", "de", "dative", 2.1, u"2 Metern, 10 Zentimetern"},
        };
        runUnitInflectionsTestCases(unf, skeleton, testCases, UPRV_LENGTHOF(testCases),
                                    status);
    }
    // TODO: add a usage case that selects between preferences with different
    // genders (e.g. year, month, day, hour).
    // TODO: look at "↑↑↑" cases: check that inheritance is done right.
}

void NumberFormatterApiTest::unitNounClass() {
    IcuTestErrorCode status(*this, "unitNounClass");
    const struct TestCase {
        const char *locale;
        const char *unitIdentifier;
        const UDisplayOptionsNounClass expectedNounClass;
    } cases[] = {
        {"de", "inch", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"de", "yard", UDISPOPT_NOUN_CLASS_NEUTER},
        {"de", "meter", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"de", "liter", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"de", "second", UDISPOPT_NOUN_CLASS_FEMININE},
        {"de", "minute", UDISPOPT_NOUN_CLASS_FEMININE},
        {"de", "hour", UDISPOPT_NOUN_CLASS_FEMININE},
        {"de", "day", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"de", "year", UDISPOPT_NOUN_CLASS_NEUTER},
        {"de", "gram", UDISPOPT_NOUN_CLASS_NEUTER},
        {"de", "watt", UDISPOPT_NOUN_CLASS_NEUTER},
        {"de", "bit", UDISPOPT_NOUN_CLASS_NEUTER},
        {"de", "byte", UDISPOPT_NOUN_CLASS_NEUTER},

        {"fr", "inch", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"fr", "yard", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"fr", "meter", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"fr", "liter", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"fr", "second", UDISPOPT_NOUN_CLASS_FEMININE},
        {"fr", "minute", UDISPOPT_NOUN_CLASS_FEMININE},
        {"fr", "hour", UDISPOPT_NOUN_CLASS_FEMININE},
        {"fr", "day", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"fr", "year", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"fr", "gram", UDISPOPT_NOUN_CLASS_MASCULINE},

        // grammaticalFeatures deriveCompound "per" rule takes the gender of the
        // numerator unit:
        {"de", "meter-per-hour", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"fr", "meter-per-hour", UDISPOPT_NOUN_CLASS_MASCULINE},
        {"af", "meter-per-hour",
         UDISPOPT_NOUN_CLASS_UNDEFINED}, // ungendered language

        // French "times" takes gender from first value, German takes the
        // second. Prefix and power does not have impact on gender for these
        // languages:
        {"de", "square-decimeter-square-second", UDISPOPT_NOUN_CLASS_FEMININE},
        {"fr", "square-decimeter-square-second",
         UDISPOPT_NOUN_CLASS_MASCULINE},

        // TODO(icu-units#149): percent and permille bypasses LongNameHandler
        // when unitWidth is not FULL_NAME:
        // // Gender of per-second might be that of percent? TODO(icu-units#28)
        // {"de", "percent", UNounClass::UNOUN_CLASS_NEUTER},
        // {"fr", "percent", UNounClass::UNOUN_CLASS_MASCULINE},

        // Built-in units whose simple units lack gender in the CLDR data file
        {"de", "kilopascal", UDISPOPT_NOUN_CLASS_NEUTER},
        {"fr", "kilopascal", UDISPOPT_NOUN_CLASS_MASCULINE},
        // {"de", "pascal", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "pascal", UNounClass::UNOUN_CLASS_UNDEFINED},

        // Built-in units that lack gender in the CLDR data file
        // {"de", "revolution", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "radian", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "arc-minute", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "arc-second", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"de", "square-yard", UDISPOPT_NOUN_CLASS_NEUTER},    // POWER
        {"de", "square-inch", UDISPOPT_NOUN_CLASS_MASCULINE}, // POWER
        // {"de", "dunam", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"de", "karat", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"de", "milligram-ofglucose-per-deciliter", UNounClass::UNOUN_CLASS_UNDEFINED}, // COMPOUND,
        // ofglucose
        // {"de", "millimole-per-liter", UNounClass::UNOUN_CLASS_UNDEFINED},               // COMPOUND,
        // mole
        // {"de", "permillion", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "permille", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "permyriad", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "mole", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"de", "liter-per-kilometer",
         UDISPOPT_NOUN_CLASS_MASCULINE},                // COMPOUND
        {"de", "petabyte", UDISPOPT_NOUN_CLASS_NEUTER}, // PREFIX
        {"de", "terabit", UDISPOPT_NOUN_CLASS_NEUTER},  // PREFIX
        // {"de", "century", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "decade", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"de", "millisecond", UDISPOPT_NOUN_CLASS_FEMININE}, // PREFIX
        {"de", "microsecond", UDISPOPT_NOUN_CLASS_FEMININE}, // PREFIX
        {"de", "nanosecond", UDISPOPT_NOUN_CLASS_FEMININE},  // PREFIX
        // {"de", "ampere", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "milliampere", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, ampere
        // {"de", "ohm", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "calorie", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "kilojoule", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, joule
        // {"de", "joule", UNounClass::UNOUN_CLASS_ UNDEFINED},
        {"de", "kilowatt-hour", UDISPOPT_NOUN_CLASS_FEMININE}, // COMPOUND
        // {"de", "electronvolt", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "british-thermal-unit", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "therm-us", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "pound-force", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "newton", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "gigahertz", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, hertz
        // {"de", "megahertz", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, hertz
        // {"de", "kilohertz", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, hertz
        // {"de", "hertz", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"de", "em", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "pixel", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"de", "megapixel", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "pixel-per-centimeter", UNounClass::UNOUN_CLASS_UNDEFINED}, // COMPOUND, pixel
        // {"de", "pixel-per-inch", UNounClass::UNOUN_CLASS_UNDEFINED},       // COMPOUND, pixel
        // {"de", "dot-per-centimeter", UNounClass::UNOUN_CLASS_UNDEFINED},   // COMPOUND, dot
        // {"de", "dot-per-inch", UNounClass::UNOUN_CLASS_UNDEFINED},         // COMPOUND, dot
        // {"de", "dot", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "earth-radius", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"de", "decimeter", UDISPOPT_NOUN_CLASS_MASCULINE},  // PREFIX
        {"de", "micrometer", UDISPOPT_NOUN_CLASS_MASCULINE}, // PREFIX
        {"de", "nanometer", UDISPOPT_NOUN_CLASS_MASCULINE},  // PREFIX
        // {"de", "light-year", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "astronomical-unit", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "furlong", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "fathom", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "nautical-mile", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"de", "mile-scandinavian", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "point", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"de", "lux", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "candela", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "lumen", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"de", "tonne", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "microgram", UNounClass::UNOUN_CLASS_NEUTER}, // PREFIX
        // {"de", "ton", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "stone", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"de", "ounce-troy", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "carat", UNounClass::UNOUN_CLASS_ UNDEFINED},
        {"de", "gigawatt", UDISPOPT_NOUN_CLASS_NEUTER},  // PREFIX
        {"de", "milliwatt", UDISPOPT_NOUN_CLASS_NEUTER}, // PREFIX
        // {"de", "horsepower", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "millimeter-ofhg", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "pound-force-per-square-inch", UNounClass::UNOUN_CLASS_UNDEFINED}, // COMPOUND,
        // pound-force
        // {"de", "inch-ofhg", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "bar", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "millibar", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, bar
        // {"de", "atmosphere", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "pascal", UNounClass::UNOUN_CLASS_UNDEFINED},      // PREFIX, kilopascal? neuter?
        // {"de", "hectopascal", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, pascal, neuter?
        // {"de", "megapascal", UNounClass::UNOUN_CLASS_UNDEFINED},  // PREFIX, pascal, neuter?
        // {"de", "knot", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"de", "pound-force-foot", UDISPOPT_NOUN_CLASS_MASCULINE}, // COMPOUND
        {"de", "newton-meter", UDISPOPT_NOUN_CLASS_MASCULINE},     // COMPOUND
        {"de", "cubic-kilometer", UDISPOPT_NOUN_CLASS_MASCULINE},  // POWER
        {"de", "cubic-yard", UDISPOPT_NOUN_CLASS_NEUTER},          // POWER
        {"de", "cubic-inch", UDISPOPT_NOUN_CLASS_MASCULINE},       // POWER
        {"de", "megaliter", UDISPOPT_NOUN_CLASS_MASCULINE},        // PREFIX
        {"de", "hectoliter", UDISPOPT_NOUN_CLASS_MASCULINE},       // PREFIX
        // {"de", "pint-metric", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "cup-metric", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"de", "acre-foot", UDISPOPT_NOUN_CLASS_MASCULINE}, // COMPOUND
        // {"de", "bushel", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"de", "barrel", UNounClass::UNOUN_CLASS_UNDEFINED},
        // Units missing gender in German also misses gender in French:
        // {"fr", "revolution", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "radian", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "arc-minute", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "arc-second", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "square-yard", UDISPOPT_NOUN_CLASS_MASCULINE}, // POWER
        {"fr", "square-inch", UDISPOPT_NOUN_CLASS_MASCULINE}, // POWER
        // {"fr", "dunam", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"fr", "karat", UNounClass::UNOUN_CLASS_ UNDEFINED},
        {"fr", "milligram-ofglucose-per-deciliter",
         UDISPOPT_NOUN_CLASS_MASCULINE}, // COMPOUND
        // {"fr", "millimole-per-liter", UNounClass::UNOUN_CLASS_UNDEFINED},                        //
        // COMPOUND, mole
        // {"fr", "permillion", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "permille", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "permyriad", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "mole", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "liter-per-kilometer",
         UDISPOPT_NOUN_CLASS_MASCULINE}, // COMPOUND
        // {"fr", "petabyte", UNounClass::UNOUN_CLASS_UNDEFINED},                     // PREFIX
        // {"fr", "terabit", UNounClass::UNOUN_CLASS_UNDEFINED},                      // PREFIX
        // {"fr", "century", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "decade", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "millisecond", UDISPOPT_NOUN_CLASS_FEMININE}, // PREFIX
        {"fr", "microsecond", UDISPOPT_NOUN_CLASS_FEMININE}, // PREFIX
        {"fr", "nanosecond", UDISPOPT_NOUN_CLASS_FEMININE},  // PREFIX
        // {"fr", "ampere", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "milliampere", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, ampere
        // {"fr", "ohm", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "calorie", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "kilojoule", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, joule
        // {"fr", "joule", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "kilowatt-hour", UNounClass::UNOUN_CLASS_UNDEFINED}, // COMPOUND
        // {"fr", "electronvolt", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "british-thermal-unit", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "therm-us", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "pound-force", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "newton", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "gigahertz", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, hertz
        // {"fr", "megahertz", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, hertz
        // {"fr", "kilohertz", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, hertz
        // {"fr", "hertz", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"fr", "em", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "pixel", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"fr", "megapixel", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "pixel-per-centimeter", UNounClass::UNOUN_CLASS_UNDEFINED}, // COMPOUND, pixel
        // {"fr", "pixel-per-inch", UNounClass::UNOUN_CLASS_UNDEFINED},       // COMPOUND, pixel
        // {"fr", "dot-per-centimeter", UNounClass::UNOUN_CLASS_UNDEFINED},   // COMPOUND, dot
        // {"fr", "dot-per-inch", UNounClass::UNOUN_CLASS_UNDEFINED},         // COMPOUND, dot
        // {"fr", "dot", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "earth-radius", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "decimeter", UDISPOPT_NOUN_CLASS_MASCULINE},  // PREFIX
        {"fr", "micrometer", UDISPOPT_NOUN_CLASS_MASCULINE}, // PREFIX
        {"fr", "nanometer", UDISPOPT_NOUN_CLASS_MASCULINE},  // PREFIX
        // {"fr", "light-year", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "astronomical-unit", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "furlong", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "fathom", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "nautical-mile", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"fr", "mile-scandinavian", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "point", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"fr", "lux", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "candela", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "lumen", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"fr", "tonne", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "microgram", UNounClass::UNOUN_CLASS_MASCULINE}, // PREFIX
        // {"fr", "ton", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "stone", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"fr", "ounce-troy", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "carat", UNounClass::UNOUN_CLASS_ UNDEFINED},
        // {"fr", "gigawatt", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX
        // {"fr", "milliwatt", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "horsepower", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "millimeter-ofhg", UDISPOPT_NOUN_CLASS_MASCULINE},
        // {"fr", "pound-force-per-square-inch", UNounClass::UNOUN_CLASS_UNDEFINED}, // COMPOUND,
        // pound-force
        {"fr", "inch-ofhg", UDISPOPT_NOUN_CLASS_MASCULINE},
        // {"fr", "bar", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "millibar", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, bar
        // {"fr", "atmosphere", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "pascal", UNounClass::UNOUN_CLASS_UNDEFINED},      // PREFIX, kilopascal?
        // {"fr", "hectopascal", UNounClass::UNOUN_CLASS_UNDEFINED}, // PREFIX, pascal
        // {"fr", "megapascal", UNounClass::UNOUN_CLASS_UNDEFINED},  // PREFIX, pascal
        // {"fr", "knot", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "pound-force-foot", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "newton-meter", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "cubic-kilometer", UDISPOPT_NOUN_CLASS_MASCULINE}, // POWER
        {"fr", "cubic-yard", UDISPOPT_NOUN_CLASS_MASCULINE},      // POWER
        {"fr", "cubic-inch", UDISPOPT_NOUN_CLASS_MASCULINE},      // POWER
        {"fr", "megaliter", UDISPOPT_NOUN_CLASS_MASCULINE},       // PREFIX
        {"fr", "hectoliter", UDISPOPT_NOUN_CLASS_MASCULINE},      // PREFIX
        // {"fr", "pint-metric", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "cup-metric", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "acre-foot", UDISPOPT_NOUN_CLASS_FEMININE}, // COMPOUND
        // {"fr", "bushel", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "barrel", UNounClass::UNOUN_CLASS_UNDEFINED},
        // Some more French units missing gender:
        // {"fr", "degree", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "square-meter", UDISPOPT_NOUN_CLASS_MASCULINE}, // POWER
        // {"fr", "terabyte", UNounClass::UNOUN_CLASS_UNDEFINED},              // PREFIX, byte
        // {"fr", "gigabyte", UNounClass::UNOUN_CLASS_UNDEFINED},              // PREFIX, byte
        // {"fr", "gigabit", UNounClass::UNOUN_CLASS_UNDEFINED},               // PREFIX, bit
        // {"fr", "megabyte", UNounClass::UNOUN_CLASS_UNDEFINED},              // PREFIX, byte
        // {"fr", "megabit", UNounClass::UNOUN_CLASS_UNDEFINED},               // PREFIX, bit
        // {"fr", "kilobyte", UNounClass::UNOUN_CLASS_UNDEFINED},              // PREFIX, byte
        // {"fr", "kilobit", UNounClass::UNOUN_CLASS_UNDEFINED},               // PREFIX, bit
        // {"fr", "byte", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "bit", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "volt", UNounClass::UNOUN_CLASS_UNDEFINED},
        // {"fr", "watt", UNounClass::UNOUN_CLASS_UNDEFINED},
        {"fr", "cubic-meter", UDISPOPT_NOUN_CLASS_MASCULINE}, // POWER

        // gender-lacking builtins within compound units
        {"de", "newton-meter-per-second", UDISPOPT_NOUN_CLASS_MASCULINE},

        // TODO(ICU-21494): determine whether list genders behave as follows,
        // and implement proper getListGender support (covering more than just
        // two genders):
        // // gender rule for lists of people: de "neutral", fr "maleTaints"
        // {"de", "day-and-hour-and-minute", UNounClass::UNOUN_CLASS_NEUTER},
        // {"de", "hour-and-minute", UNounClass::UNOUN_CLASS_FEMININE},
        // {"fr", "day-and-hour-and-minute", UNounClass::UNOUN_CLASS_MASCULINE},
        // {"fr", "hour-and-minute", UNounClass::UNOUN_CLASS_FEMININE},
    };

    LocalizedNumberFormatter formatter;
    FormattedNumber fn;
    for (const TestCase &t : cases) {
        formatter = NumberFormatter::with()
                        .unit(MeasureUnit::forIdentifier(t.unitIdentifier, status))
                        .locale(Locale(t.locale));
        fn = formatter.formatDouble(1.1, status);
        assertEquals(UnicodeString("Testing NounClass with default width, unit: ") + t.unitIdentifier +
                         ", locale: " + t.locale,
                     t.expectedNounClass, fn.getNounClass(status));
        status.assertSuccess();

        formatter = NumberFormatter::with()
                        .unit(MeasureUnit::forIdentifier(t.unitIdentifier, status))
                        .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)
                        .locale(Locale(t.locale));
        fn = formatter.formatDouble(1.1, status);
        assertEquals(UnicodeString("Testing NounClass with UNUM_UNIT_WIDTH_FULL_NAME, unit: ") +
                         t.unitIdentifier + ", locale: " + t.locale,
                     t.expectedNounClass, fn.getNounClass(status));
        status.assertSuccess();
    }

    // Make sure getNounClass does not return garbage for languages without noun classes.
    formatter = NumberFormatter::with().locale(Locale::getEnglish());
    fn = formatter.formatDouble(1.1, status);
    status.assertSuccess();
    assertEquals("getNounClasses for a not supported language",
                 UDISPOPT_NOUN_CLASS_UNDEFINED, fn.getNounClass(status));
}

// The following test of getGender (removed in ICU 72) is replaced by the above
// parallel test unitNounClass using getNounClass (getGender replacement).
//void NumberFormatterApiTest::unitGender() {...}

void NumberFormatterApiTest::unitNotConvertible() {
    IcuTestErrorCode status(*this, "unitNotConvertible");
    const double randomNumber = 1234;

    NumberFormatter::with()
        .unit(MeasureUnit::forIdentifier("meter-and-liter", status))
        .locale("en_US")
        .formatDouble(randomNumber, status);
    assertEquals(u"error must be returned", status.errorName(), u"U_ARGUMENT_TYPE_MISMATCH");

    status.reset();
    NumberFormatter::with()
        .unit(MeasureUnit::forIdentifier("month-and-week", status))
        .locale("en_US")
        .formatDouble(randomNumber, status);
    assertEquals(u"error must be returned", status.errorName(), u"U_ARGUMENT_TYPE_MISMATCH");

    status.reset();
    NumberFormatter::with()
        .unit(MeasureUnit::forIdentifier("week-and-day", status))
        .locale("en_US")
        .formatDouble(randomNumber, status);
    assertTrue(u"no error", !U_FAILURE(status));
}

void NumberFormatterApiTest::unitPercent() {
    assertFormatDescending(
            u"Percent",
            u"percent",
            u"%",
            NumberFormatter::with().unit(NoUnit::percent()),
            Locale::getEnglish(),
            u"87,650%",
            u"8,765%",
            u"876.5%",
            u"87.65%",
            u"8.765%",
            u"0.8765%",
            u"0.08765%",
            u"0.008765%",
            u"0%");

    assertFormatDescending(
            u"Permille",
            u"permille",
            u"permille",
            NumberFormatter::with().unit(NoUnit::permille()),
            Locale::getEnglish(),
            u"87,650‰",
            u"8,765‰",
            u"876.5‰",
            u"87.65‰",
            u"8.765‰",
            u"0.8765‰",
            u"0.08765‰",
            u"0.008765‰",
            u"0‰");

    assertFormatSingle(
            u"NoUnit Base",
            u"base-unit",
            u"",
            NumberFormatter::with().unit(NoUnit::base()),
            Locale::getEnglish(),
            51423,
            u"51,423");

    assertFormatSingle(
            u"Percent with Negative Sign",
            u"percent",
            u"%",
            NumberFormatter::with().unit(NoUnit::percent()),
            Locale::getEnglish(),
            -98.7654321,
            u"-98.765432%");

    // ICU-20923
    assertFormatDescendingBig(
            u"Compact Percent",
            u"compact-short percent",
            u"K %",
            NumberFormatter::with()
                    .notation(Notation::compactShort())
                    .unit(NoUnit::percent()),
            Locale::getEnglish(),
            u"88M%",
            u"8.8M%",
            u"876K%",
            u"88K%",
            u"8.8K%",
            u"876%",
            u"88%",
            u"8.8%",
            u"0%");

    // ICU-20923
    assertFormatDescendingBig(
            u"Compact Percent with Scale",
            u"compact-short percent scale/100",
            u"K %x100",
            NumberFormatter::with()
                    .notation(Notation::compactShort())
                    .unit(NoUnit::percent())
                    .scale(Scale::powerOfTen(2)),
            Locale::getEnglish(),
            u"8.8B%",
            u"876M%",
            u"88M%",
            u"8.8M%",
            u"876K%",
            u"88K%",
            u"8.8K%",
            u"876%",
            u"0%");

    // ICU-20923
    assertFormatDescendingBig(
            u"Compact Percent Long Name",
            u"compact-short percent unit-width-full-name",
            u"K % unit-width-full-name",
            NumberFormatter::with()
                    .notation(Notation::compactShort())
                    .unit(NoUnit::percent())
                    .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getEnglish(),
            u"88M percent",
            u"8.8M percent",
            u"876K percent",
            u"88K percent",
            u"8.8K percent",
            u"876 percent",
            u"88 percent",
            u"8.8 percent",
            u"0 percent");

    assertFormatSingle(
            u"Per Percent",
            u"measure-unit/length-meter per-measure-unit/concentr-percent unit-width-full-name",
            u"measure-unit/length-meter per-measure-unit/concentr-percent unit-width-full-name",
            NumberFormatter::with()
                    .unit(MeasureUnit::getMeter())
                    .perUnit(MeasureUnit::getPercent())
                    .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getEnglish(),
            50,
            u"50 meters per percent");
}

void NumberFormatterApiTest::unitLocaleTags() {
    IcuTestErrorCode status(*this, "unitLocaleTags");

    const struct TestCase {
        const UnicodeString message;
        const char *locale;
        const char *inputUnit;
        const double inputValue;
        const char *usage;
        const char *expectedOutputUnit;
        const double expectedOutputValue;
        const UnicodeString expectedFormattedNumber;
    } cases[] = {
        // Test without any tag behaviour
        {u"Test the locale without any addition and without usage", "en-US", "celsius", 0, nullptr,
         "celsius", 0.0, u"0 degrees Celsius"},
        {u"Test the locale without any addition and usage", "en-US", "celsius", 0, "default",
         "fahrenheit", 32.0, u"32 degrees Fahrenheit"},

        // Test the behaviour of the `mu` tag.
        {u"Test the locale with mu = celsius and without usage", "en-US-u-mu-celsius", "fahrenheit", 0,
         nullptr, "fahrenheit", 0.0, u"0 degrees Fahrenheit"},
        {u"Test the locale with mu = celsius and with usage", "en-US-u-mu-celsius", "fahrenheit", 0,
         "default", "celsius", -18.0, u"-18 degrees Celsius"},
        {u"Test the locale with mu = calsius (wrong spelling) and with usage", "en-US-u-mu-calsius",
         "fahrenheit", 0, "default", "fahrenheit", 0.0, u"0 degrees Fahrenheit"},
        {u"Test the locale with mu = meter (only temprature units are supported) and with usage",
         "en-US-u-mu-meter", "foot", 0, "default", "inch", 0.0, u"0 inches"},

        // Test the behaviour of the `ms` tag
        {u"Test the locale with ms = metric and without usage", "en-US-u-ms-metric", "fahrenheit", 0,
         nullptr, "fahrenheit", 0.0, u"0 degrees Fahrenheit"},
        {u"Test the locale with ms = metric and with usage", "en-US-u-ms-metric", "fahrenheit", 0,
         "default", "celsius", -18, u"-18 degrees Celsius"},
        {u"Test the locale with ms = Matric (wrong spelling) and with usage", "en-US-u-ms-Matric",
         "fahrenheit", 0, "default", "fahrenheit", 0.0, u"0 degrees Fahrenheit"},

        // Test the behaviour of the `rg` tag
        {u"Test the locale with rg = UK and without usage", "en-US-u-rg-ukzzzz", "fahrenheit", 0,
         nullptr, "fahrenheit", 0.0, u"0 degrees Fahrenheit"},
        {u"Test the locale with rg = UK and with usage", "en-US-u-rg-ukzzzz", "fahrenheit", 0, "default",
         "celsius", -18, u"-18 degrees Celsius"},
        {"Test the locale with mu = fahrenheit and without usage", "en-US-u-mu-fahrenheit", "celsius", 0,
         nullptr, "celsius", 0.0, "0 degrees Celsius"},
        {"Test the locale with mu = fahrenheit and with usage", "en-US-u-mu-fahrenheit", "celsius", 0,
         "default", "fahrenheit", 32.0, "32 degrees Fahrenheit"},
        {u"Test the locale with rg = UKOI and with usage", "en-US-u-rg-ukoi", "fahrenheit", 0,
         "default", "celsius", -18.0, u"-18 degrees Celsius"},

        // Test the priorities
        {u"Test the locale with mu,ms,rg --> mu tag wins", "en-US-u-mu-celsius-ms-ussystem-rg-uszzzz",
         "celsius", 0, "default", "celsius", 0.0, u"0 degrees Celsius"},
        {u"Test the locale with ms,rg --> ms tag wins", "en-US-u-ms-metric-rg-uszzzz", "foot", 1,
         "default", "centimeter", 30.0, u"30 centimeters"},

        // Test the liklihood of the languages
        {"Test the region of `en` --> region should be US", "en", "celsius", 1, "default", "fahrenheit",
         34.0, u"34 degrees Fahrenheit"},
        {"Test the region of `de` --> region should be DE", "de", "celsius", 1, "default", "celsius",
         1.0, u"1 Grad Celsius"},
        {"Test the region of `ar` --> region should be EG", "ar", "celsius", 1, "default", "celsius",
         1.0, u"١ درجة مئوية"},
    };

    for (const auto &testCase : cases) {
        UnicodeString message = testCase.message;
        Locale locale(testCase.locale);
        auto inputUnit = MeasureUnit::forIdentifier(testCase.inputUnit, status);
        auto inputValue = testCase.inputValue;
        const auto* usage = testCase.usage;
        auto expectedOutputUnit = MeasureUnit::forIdentifier(testCase.expectedOutputUnit, status);
        UnicodeString expectedFormattedNumber = testCase.expectedFormattedNumber;

        auto nf = NumberFormatter::with()
                      .locale(locale)                        //
                      .unit(inputUnit)                       //
                      .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME); //
        if (usage != nullptr) {
            nf = nf.usage(usage);
        }
        auto fn = nf.formatDouble(inputValue, status);
        if (status.errIfFailureAndReset()) {
            continue;
        }

        assertEquals(message, fn.toString(status), expectedFormattedNumber);
        // TODO: ICU-22154
        // assertEquals(message, fn.getOutputUnit(status).getIdentifier(),
        //              expectedOutputUnit.getIdentifier());
    }
}

void NumberFormatterApiTest::percentParity() {
    IcuTestErrorCode status(*this, "percentParity");
    UnlocalizedNumberFormatter uNoUnitPercent = NumberFormatter::with().unit(NoUnit::percent());
    UnlocalizedNumberFormatter uNoUnitPermille = NumberFormatter::with().unit(NoUnit::permille());
    UnlocalizedNumberFormatter uMeasurePercent = NumberFormatter::with().unit(MeasureUnit::getPercent());
    UnlocalizedNumberFormatter uMeasurePermille = NumberFormatter::with().unit(MeasureUnit::getPermille());

    int32_t localeCount;
    const auto* locales = Locale::getAvailableLocales(localeCount);
    for (int32_t i=0; i<localeCount; i++) {
        const auto& locale = locales[i];
        UnicodeString sNoUnitPercent = uNoUnitPercent.locale(locale)
            .formatDouble(50, status).toString(status);
        UnicodeString sNoUnitPermille = uNoUnitPermille.locale(locale)
            .formatDouble(50, status).toString(status);
        UnicodeString sMeasurePercent = uMeasurePercent.locale(locale)
            .formatDouble(50, status).toString(status);
        UnicodeString sMeasurePermille = uMeasurePermille.locale(locale)
            .formatDouble(50, status).toString(status);

        assertEquals(u"Percent, locale " + UnicodeString(locale.getName()),
            sNoUnitPercent, sMeasurePercent);
        assertEquals(u"Permille, locale " + UnicodeString(locale.getName()),
            sNoUnitPermille, sMeasurePermille);
    }
}

void NumberFormatterApiTest::roundingFraction() {
    assertFormatDescending(
            u"Integer",
            u"precision-integer",
            u".",
            NumberFormatter::with().precision(Precision::integer()),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876",
            u"88",
            u"9",
            u"1",
            u"0",
            u"0",
            u"0");

    assertFormatDescending(
            u"Fixed Fraction",
            u".000",
            u".000",
            NumberFormatter::with().precision(Precision::fixedFraction(3)),
            Locale::getEnglish(),
            u"87,650.000",
            u"8,765.000",
            u"876.500",
            u"87.650",
            u"8.765",
            u"0.876",
            u"0.088",
            u"0.009",
            u"0.000");

    assertFormatDescending(
            u"Min Fraction",
            u".0*",
            u".0+",
            NumberFormatter::with().precision(Precision::minFraction(1)),
            Locale::getEnglish(),
            u"87,650.0",
            u"8,765.0",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0.0");

    assertFormatDescending(
            u"Max Fraction",
            u".#",
            u".#",
            NumberFormatter::with().precision(Precision::maxFraction(1)),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.6",
            u"8.8",
            u"0.9",
            u"0.1",
            u"0",
            u"0");

    assertFormatDescending(
            u"Min/Max Fraction",
            u".0##",
            u".0##",
            NumberFormatter::with().precision(Precision::minMaxFraction(1, 3)),
            Locale::getEnglish(),
            u"87,650.0",
            u"8,765.0",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.876",
            u"0.088",
            u"0.009",
            u"0.0");

    assertFormatSingle(
            u"Hide If Whole A",
            u".00/w",
            u".00/w",
            NumberFormatter::with().precision(Precision::fixedFraction(2)
                .trailingZeroDisplay(UNUM_TRAILING_ZERO_HIDE_IF_WHOLE)),
            Locale::getEnglish(),
            1.2,
            "1.20");

    assertFormatSingle(
            u"Hide If Whole B",
            u".00/w",
            u".00/w",
            NumberFormatter::with().precision(Precision::fixedFraction(2)
                .trailingZeroDisplay(UNUM_TRAILING_ZERO_HIDE_IF_WHOLE)),
            Locale::getEnglish(),
            1,
            "1");

    assertFormatSingle(
            u"Hide If Whole with Rounding Mode A (ICU-21881)",
            u".00/w rounding-mode-floor",
            u".00/w rounding-mode-floor",
            NumberFormatter::with().precision(Precision::fixedFraction(2)
                .trailingZeroDisplay(UNUM_TRAILING_ZERO_HIDE_IF_WHOLE))
                .roundingMode(UNUM_ROUND_FLOOR),
            Locale::getEnglish(),
            3.009,
            "3");

    assertFormatSingle(
            u"Hide If Whole with Rounding Mode B (ICU-21881)",
            u".00/w rounding-mode-half-up",
            u".00/w rounding-mode-half-up",
            NumberFormatter::with().precision(Precision::fixedFraction(2)
                .trailingZeroDisplay(UNUM_TRAILING_ZERO_HIDE_IF_WHOLE))
                .roundingMode(UNUM_ROUND_HALFUP),
            Locale::getEnglish(),
            3.001,
            "3");
}

void NumberFormatterApiTest::roundingFigures() {
    assertFormatSingle(
            u"Fixed Significant",
            u"@@@",
            u"@@@",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(3)),
            Locale::getEnglish(),
            -98,
            u"-98.0");

    assertFormatSingle(
            u"Fixed Significant Rounding",
            u"@@@",
            u"@@@",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(3)),
            Locale::getEnglish(),
            -98.7654321,
            u"-98.8");

    assertFormatSingle(
            u"Fixed Significant at rounding boundary",
            u"@@@",
            u"@@@",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(3)),
            Locale::getEnglish(),
            9.999,
            u"10.0");

    assertFormatSingle(
            u"Fixed Significant Zero",
            u"@@@",
            u"@@@",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(3)),
            Locale::getEnglish(),
            0,
            u"0.00");

    assertFormatSingle(
            u"Min Significant",
            u"@@*",
            u"@@+",
            NumberFormatter::with().precision(Precision::minSignificantDigits(2)),
            Locale::getEnglish(),
            -9,
            u"-9.0");

    assertFormatSingle(
            u"Max Significant",
            u"@###",
            u"@###",
            NumberFormatter::with().precision(Precision::maxSignificantDigits(4)),
            Locale::getEnglish(),
            98.7654321,
            u"98.77");

    assertFormatSingle(
            u"Min/Max Significant",
            u"@@@#",
            u"@@@#",
            NumberFormatter::with().precision(Precision::minMaxSignificantDigits(3, 4)),
            Locale::getEnglish(),
            9.99999,
            u"10.0");

    assertFormatSingle(
            u"Fixed Significant on zero with lots of integer width",
            u"@ integer-width/+000",
            u"@ 000",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(1))
                    .integerWidth(IntegerWidth::zeroFillTo(3)),
            Locale::getEnglish(),
            0,
            "000");

    assertFormatSingle(
            u"Fixed Significant on zero with zero integer width",
            u"@ integer-width/*",
            u"@ integer-width/+",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(1))
                    .integerWidth(IntegerWidth::zeroFillTo(0)),
            Locale::getEnglish(),
            0,
            "0");
}

void NumberFormatterApiTest::roundingFractionFigures() {
    assertFormatDescending(
            u"Basic Significant", // for comparison
            u"@#",
            u"@#",
            NumberFormatter::with().precision(Precision::maxSignificantDigits(2)),
            Locale::getEnglish(),
            u"88,000",
            u"8,800",
            u"880",
            u"88",
            u"8.8",
            u"0.88",
            u"0.088",
            u"0.0088",
            u"0");

    assertFormatDescending(
            u"FracSig minMaxFrac minSig",
            u".0#/@@@*",
            u".0#/@@@+",
            NumberFormatter::with().precision(Precision::minMaxFraction(1, 2).withMinDigits(3)),
            Locale::getEnglish(),
            u"87,650.0",
            u"8,765.0",
            u"876.5",
            u"87.65",
            u"8.76",
            u"0.876", // minSig beats maxFrac
            u"0.0876", // minSig beats maxFrac
            u"0.00876", // minSig beats maxFrac
            u"0.0");

    assertFormatDescending(
            u"FracSig minMaxFrac maxSig A",
            u".0##/@#",
            u".0##/@#",
            NumberFormatter::with().precision(Precision::minMaxFraction(1, 3).withMaxDigits(2)),
            Locale::getEnglish(),
            u"88,000.0", // maxSig beats maxFrac
            u"8,800.0", // maxSig beats maxFrac
            u"880.0", // maxSig beats maxFrac
            u"88.0", // maxSig beats maxFrac
            u"8.8", // maxSig beats maxFrac
            u"0.88", // maxSig beats maxFrac
            u"0.088",
            u"0.009",
            u"0.0");

    assertFormatDescending(
            u"FracSig minMaxFrac maxSig B",
            u".00/@#",
            u".00/@#",
            NumberFormatter::with().precision(Precision::fixedFraction(2).withMaxDigits(2)),
            Locale::getEnglish(),
            u"88,000.00", // maxSig beats maxFrac
            u"8,800.00", // maxSig beats maxFrac
            u"880.00", // maxSig beats maxFrac
            u"88.00", // maxSig beats maxFrac
            u"8.80", // maxSig beats maxFrac
            u"0.88",
            u"0.09",
            u"0.01",
            u"0.00");

    assertFormatSingle(
            u"FracSig with trailing zeros A",
            u".00/@@@*",
            u".00/@@@+",
            NumberFormatter::with().precision(Precision::fixedFraction(2).withMinDigits(3)),
            Locale::getEnglish(),
            0.1,
            u"0.10");

    assertFormatSingle(
            u"FracSig with trailing zeros B",
            u".00/@@@*",
            u".00/@@@+",
            NumberFormatter::with().precision(Precision::fixedFraction(2).withMinDigits(3)),
            Locale::getEnglish(),
            0.0999999,
            u"0.10");

    assertFormatDescending(
            u"FracSig withSignificantDigits RELAXED",
            u"precision-integer/@#r",
            u"./@#r",
            NumberFormatter::with().precision(Precision::maxFraction(0)
                .withSignificantDigits(1, 2, UNUM_ROUNDING_PRIORITY_RELAXED)),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876",
            u"88",
            u"8.8",
            u"0.88",
            u"0.088",
            u"0.0088",
            u"0");

    assertFormatDescending(
            u"FracSig withSignificantDigits STRICT",
            u"precision-integer/@#s",
            u"./@#s",
            NumberFormatter::with().precision(Precision::maxFraction(0)
                .withSignificantDigits(1, 2, UNUM_ROUNDING_PRIORITY_STRICT)),
            Locale::getEnglish(),
            u"88,000",
            u"8,800",
            u"880",
            u"88",
            u"9",
            u"1",
            u"0",
            u"0",
            u"0");

    assertFormatSingle(
            u"FracSig withSignificantDigits Trailing Zeros RELAXED",
            u".0/@@@r",
            u".0/@@@r",
            NumberFormatter::with().precision(Precision::fixedFraction(1)
                .withSignificantDigits(3, 3, UNUM_ROUNDING_PRIORITY_RELAXED)),
            Locale::getEnglish(),
            1,
            u"1.00");

    // Trailing zeros follow the strategy that was chosen:
    assertFormatSingle(
            u"FracSig withSignificantDigits Trailing Zeros STRICT",
            u".0/@@@s",
            u".0/@@@s",
            NumberFormatter::with().precision(Precision::fixedFraction(1)
                .withSignificantDigits(3, 3, UNUM_ROUNDING_PRIORITY_STRICT)),
            Locale::getEnglish(),
            1,
            u"1.0");

    assertFormatSingle(
            u"FracSig withSignificantDigits at rounding boundary",
            u"precision-integer/@@@s",
            u"./@@@s",
            NumberFormatter::with().precision(Precision::fixedFraction(0)
                    .withSignificantDigits(3, 3, UNUM_ROUNDING_PRIORITY_STRICT)),
            Locale::getEnglish(),
            9.99,
            u"10");

    assertFormatSingle(
            u"FracSig with Trailing Zero Display",
            u".00/@@@*/w",
            u".00/@@@+/w",
            NumberFormatter::with().precision(Precision::fixedFraction(2).withMinDigits(3)
                .trailingZeroDisplay(UNUM_TRAILING_ZERO_HIDE_IF_WHOLE)),
            Locale::getEnglish(),
            1,
            u"1");
}

void NumberFormatterApiTest::roundingOther() {
    assertFormatDescending(
            u"Rounding None",
            u"precision-unlimited",
            u".+",
            NumberFormatter::with().precision(Precision::unlimited()),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0");

    assertFormatDescending(
            u"Increment",
            u"precision-increment/0.5",
            u"precision-increment/0.5",
            NumberFormatter::with().precision(Precision::increment(0.5).withMinFraction(1)),
            Locale::getEnglish(),
            u"87,650.0",
            u"8,765.0",
            u"876.5",
            u"87.5",
            u"9.0",
            u"1.0",
            u"0.0",
            u"0.0",
            u"0.0");

    assertFormatDescending(
            u"Increment with Min Fraction",
            u"precision-increment/0.50",
            u"precision-increment/0.50",
            NumberFormatter::with().precision(Precision::increment(0.5).withMinFraction(2)),
            Locale::getEnglish(),
            u"87,650.00",
            u"8,765.00",
            u"876.50",
            u"87.50",
            u"9.00",
            u"1.00",
            u"0.00",
            u"0.00",
            u"0.00");

    assertFormatDescending(
            u"Strange Increment",
            u"precision-increment/3.140",
            u"precision-increment/3.140",
            NumberFormatter::with().precision(Precision::increment(3.14).withMinFraction(3)),
            Locale::getEnglish(),
            u"87,649.960",
            u"8,763.740",
            u"876.060",
            u"87.920",
            u"9.420",
            u"0.000",
            u"0.000",
            u"0.000",
            u"0.000");

    assertFormatDescending(
            u"Medium nickel increment with rounding mode ceiling (ICU-21668)",
            u"precision-increment/50 rounding-mode-ceiling",
            u"precision-increment/50 rounding-mode-ceiling",
            NumberFormatter::with()
                .precision(Precision::increment(50))
                .roundingMode(UNUM_ROUND_CEILING),
            Locale::getEnglish(),
            u"87,650",
            u"8,800",
            u"900",
            u"100",
            u"50",
            u"50",
            u"50",
            u"50",
            u"0");

    assertFormatDescending(
            u"Large nickel increment with rounding mode up (ICU-21668)",
            u"precision-increment/5000 rounding-mode-up",
            u"precision-increment/5000 rounding-mode-up",
            NumberFormatter::with()
                .precision(Precision::increment(5000))
                .roundingMode(UNUM_ROUND_UP),
            Locale::getEnglish(),
            u"90,000",
            u"10,000",
            u"5,000",
            u"5,000",
            u"5,000",
            u"5,000",
            u"5,000",
            u"5,000",
            u"0");

    assertFormatDescending(
            u"Large dime increment with rounding mode up (ICU-21668)",
            u"precision-increment/10000 rounding-mode-up",
            u"precision-increment/10000 rounding-mode-up",
            NumberFormatter::with()
                .precision(Precision::increment(10000))
                .roundingMode(UNUM_ROUND_UP),
            Locale::getEnglish(),
            u"90,000",
            u"10,000",
            u"10,000",
            u"10,000",
            u"10,000",
            u"10,000",
            u"10,000",
            u"10,000",
            u"0");

    assertFormatDescending(
            u"Large non-nickel increment with rounding mode up (ICU-21668)",
            u"precision-increment/15000 rounding-mode-up",
            u"precision-increment/15000 rounding-mode-up",
            NumberFormatter::with()
                .precision(Precision::increment(15000))
                .roundingMode(UNUM_ROUND_UP),
            Locale::getEnglish(),
            u"90,000",
            u"15,000",
            u"15,000",
            u"15,000",
            u"15,000",
            u"15,000",
            u"15,000",
            u"15,000",
            u"0");

    assertFormatDescending(
            u"Increment Resolving to Power of 10",
            u"precision-increment/0.010",
            u"precision-increment/0.010",
            NumberFormatter::with().precision(Precision::increment(0.01).withMinFraction(3)),
            Locale::getEnglish(),
            u"87,650.000",
            u"8,765.000",
            u"876.500",
            u"87.650",
            u"8.760",
            u"0.880",
            u"0.090",
            u"0.010",
            u"0.000");

    assertFormatDescending(
            u"Integer increment with trailing zeros (ICU-21654)",
            u"precision-increment/50",
            u"precision-increment/50",
            NumberFormatter::with().precision(Precision::increment(50)),
            Locale::getEnglish(),
            u"87,650",
            u"8,750",
            u"900",
            u"100",
            u"0",
            u"0",
            u"0",
            u"0",
            u"0");

    assertFormatDescending(
            u"Integer increment with minFraction (ICU-21654)",
            u"precision-increment/5.0",
            u"precision-increment/5.0",
            NumberFormatter::with().precision(Precision::increment(5).withMinFraction(1)),
            Locale::getEnglish(),
            u"87,650.0",
            u"8,765.0",
            u"875.0",
            u"90.0",
            u"10.0",
            u"0.0",
            u"0.0",
            u"0.0",
            u"0.0");

    assertFormatSingle(
            u"Large integer increment",
            u"precision-increment/24000000000000000000000",
            u"precision-increment/24000000000000000000000",
            NumberFormatter::with().precision(Precision::incrementExact(24, 21)),
            Locale::getEnglish(),
            3.1e22,
            u"24,000,000,000,000,000,000,000");

    assertFormatSingle(
            u"Quarter rounding",
            u"precision-increment/250",
            u"precision-increment/250",
            NumberFormatter::with().precision(Precision::incrementExact(250, 0)),
            Locale::getEnglish(),
            700,
            u"750");

    assertFormatSingle(
            u"ECMA-402 limit",
            u"precision-increment/.00000000000000000020",
            u"precision-increment/.00000000000000000020",
            NumberFormatter::with().precision(Precision::incrementExact(20, -20)),
            Locale::getEnglish(),
            333e-20,
            u"0.00000000000000000340");

    assertFormatSingle(
            u"ECMA-402 limit with increment = 1",
            u"precision-increment/.00000000000000000001",
            u"precision-increment/.00000000000000000001",
            NumberFormatter::with().precision(Precision::incrementExact(1, -20)),
            Locale::getEnglish(),
            4321e-21,
            u"0.00000000000000000432");

    assertFormatDescending(
            u"Currency Standard",
            u"currency/CZK precision-currency-standard",
            u"currency/CZK precision-currency-standard",
            NumberFormatter::with().precision(Precision::currency(UCurrencyUsage::UCURR_USAGE_STANDARD))
                    .unit(CZK),
            Locale::getEnglish(),
            u"CZK 87,650.00",
            u"CZK 8,765.00",
            u"CZK 876.50",
            u"CZK 87.65",
            u"CZK 8.76",
            u"CZK 0.88",
            u"CZK 0.09",
            u"CZK 0.01",
            u"CZK 0.00");

    assertFormatDescending(
            u"Currency Cash",
            u"currency/CZK precision-currency-cash",
            u"currency/CZK precision-currency-cash",
            NumberFormatter::with().precision(Precision::currency(UCurrencyUsage::UCURR_USAGE_CASH))
                    .unit(CZK),
            Locale::getEnglish(),
            u"CZK 87,650",
            u"CZK 8,765",
            u"CZK 876",
            u"CZK 88",
            u"CZK 9",
            u"CZK 1",
            u"CZK 0",
            u"CZK 0",
            u"CZK 0");

    assertFormatDescending(
            u"Currency Standard with Trailing Zero Display",
            u"currency/CZK precision-currency-standard/w",
            u"currency/CZK precision-currency-standard/w",
            NumberFormatter::with().precision(
                        Precision::currency(UCurrencyUsage::UCURR_USAGE_STANDARD)
                        .trailingZeroDisplay(UNUM_TRAILING_ZERO_HIDE_IF_WHOLE))
                    .unit(CZK),
            Locale::getEnglish(),
            u"CZK 87,650",
            u"CZK 8,765",
            u"CZK 876.50",
            u"CZK 87.65",
            u"CZK 8.76",
            u"CZK 0.88",
            u"CZK 0.09",
            u"CZK 0.01",
            u"CZK 0");

    assertFormatDescending(
            u"Currency Cash with Nickel Rounding",
            u"currency/CAD precision-currency-cash",
            u"currency/CAD precision-currency-cash",
            NumberFormatter::with().precision(Precision::currency(UCurrencyUsage::UCURR_USAGE_CASH))
                    .unit(CAD),
            Locale::getEnglish(),
            u"CA$87,650.00",
            u"CA$8,765.00",
            u"CA$876.50",
            u"CA$87.65",
            u"CA$8.75",
            u"CA$0.90",
            u"CA$0.10",
            u"CA$0.00",
            u"CA$0.00");

    assertFormatDescending(
            u"Currency not in top-level fluent chain",
            u"precision-integer", // calling .withCurrency() applies currency rounding rules immediately
            u".",
            NumberFormatter::with().precision(
                    Precision::currency(UCurrencyUsage::UCURR_USAGE_CASH).withCurrency(CZK)),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876",
            u"88",
            u"9",
            u"1",
            u"0",
            u"0",
            u"0");

    // NOTE: Other tests cover the behavior of the other rounding modes.
    assertFormatDescending(
            u"Rounding Mode CEILING",
            u"precision-integer rounding-mode-ceiling",
            u". rounding-mode-ceiling",
            NumberFormatter::with().precision(Precision::integer()).roundingMode(UNUM_ROUND_CEILING),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"877",
            u"88",
            u"9",
            u"1",
            u"1",
            u"1",
            u"0");

    assertFormatSingle(
            u"ICU-20974 Double.MIN_NORMAL",
            u"scientific",
            u"E0",
            NumberFormatter::with().notation(Notation::scientific()),
            Locale::getEnglish(),
            DBL_MIN,
            u"2.225074E-308");

#ifndef DBL_TRUE_MIN
#define DBL_TRUE_MIN 4.9E-324
#endif

    // Note: this behavior is intentionally different from Java; see
    // https://github.com/google/double-conversion/issues/126
    assertFormatSingle(
            u"ICU-20974 Double.MIN_VALUE",
            u"scientific",
            u"E0",
            NumberFormatter::with().notation(Notation::scientific()),
            Locale::getEnglish(),
            DBL_TRUE_MIN,
            u"5E-324");
}

/** Test for ICU-21654 and ICU-21668 */
void NumberFormatterApiTest::roundingIncrementRegressionTest() {
    IcuTestErrorCode status(*this, "roundingIncrementRegressionTest");
    Locale locale = Locale::getEnglish();

    for (int min_fraction_digits = 1; min_fraction_digits < 8; min_fraction_digits++) {
        // pattern is a snprintf pattern string like "precision-increment/%0.5f"
        char pattern[256];
        snprintf(pattern, 256, "precision-increment/%%0.%df", min_fraction_digits);
        double increment = 0.05;
        for (int i = 0; i < 8 ; i++, increment *= 10.0) {
            const UnlocalizedNumberFormatter f =
                NumberFormatter::with().precision(
                    Precision::increment(increment).withMinFraction(
                        min_fraction_digits));
            const LocalizedNumberFormatter l = f.locale(locale);

            std::string skeleton;
            f.toSkeleton(status).toUTF8String<std::string>(skeleton);

            char message[256];
            snprintf(message, 256,
                "ICU-21654: Precision::increment(%0.5f).withMinFraction(%d) '%s'\n",
                increment, min_fraction_digits,
                skeleton.c_str());

            if (increment == 0.05 && min_fraction_digits == 1) {
                // Special case when the number of fraction digits is too low:
                // Precision::increment(0.05000).withMinFraction(1) 'precision-increment/0.05'
                assertEquals(message, "precision-increment/0.05", skeleton.c_str());
            } else {
                // All other cases: use the snprintf pattern computed above:
                // Precision::increment(0.50000).withMinFraction(1) 'precision-increment/0.5'
                // Precision::increment(5.00000).withMinFraction(1) 'precision-increment/5.0'
                // Precision::increment(50.00000).withMinFraction(1) 'precision-increment/50.0'
                // ...
                // Precision::increment(0.05000).withMinFraction(2) 'precision-increment/0.05'
                // Precision::increment(0.50000).withMinFraction(2) 'precision-increment/0.50'
                // Precision::increment(5.00000).withMinFraction(2) 'precision-increment/5.00'
                // ...

                char expected[256];
                snprintf(expected, 256, pattern, increment);
                assertEquals(message, expected, skeleton.c_str());
            }
        }
    }

    auto increment = NumberFormatter::with()
        .precision(Precision::increment(5000).withMinFraction(0))
        .roundingMode(UNUM_ROUND_UP)
        .locale(Locale::getEnglish())
        .formatDouble(5.625, status)
        .toString(status);
    assertEquals("ICU-21668", u"5,000", increment);
}

void NumberFormatterApiTest::roundingPriorityCoverageTest() {
    IcuTestErrorCode status(*this, "roundingPriorityCoverageTest");
    struct TestCase {
        double input;
        const char16_t* expectedRelaxed0113;
        const char16_t* expectedStrict0113;
        const char16_t* expectedRelaxed1133;
        const char16_t* expectedStrict1133;
    } cases[] = {
        { 0.9999, u"1",      u"1",     u"1.00",    u"1.0" },
        { 9.9999, u"10",     u"10",    u"10.0",    u"10.0" },
        { 99.999, u"100",    u"100",   u"100.0",   u"100" },
        { 999.99, u"1000",   u"1000",  u"1000.0",  u"1000" },

        { 0, u"0", u"0", u"0.00", u"0.0" },

        { 9.876,  u"9.88",   u"9.9",   u"9.88",   u"9.9" },
        { 9.001,  u"9",      u"9",     u"9.00",   u"9.0" },
    };
    for (const auto& cas : cases) {
        auto precisionRelaxed0113 = Precision::minMaxFraction(0, 1)
            .withSignificantDigits(1, 3, UNUM_ROUNDING_PRIORITY_RELAXED);
        auto precisionStrict0113 = Precision::minMaxFraction(0, 1)
            .withSignificantDigits(1, 3, UNUM_ROUNDING_PRIORITY_STRICT);
        auto precisionRelaxed1133 = Precision::minMaxFraction(1, 1)
            .withSignificantDigits(3, 3, UNUM_ROUNDING_PRIORITY_RELAXED);
        auto precisionStrict1133 = Precision::minMaxFraction(1, 1)
            .withSignificantDigits(3, 3, UNUM_ROUNDING_PRIORITY_STRICT);

        auto messageBase = DoubleToUnicodeString(cas.input);

        auto check = [&](
            const char16_t* name,
            const UnicodeString& expected,
            const Precision& precision
        ) {
            assertEquals(
                messageBase + name,
                expected,
                NumberFormatter::withLocale(Locale::getEnglish())
                    .precision(precision)
                    .grouping(UNUM_GROUPING_OFF)
                    .formatDouble(cas.input, status)
                    .toString(status)
            );
        };

        check(u" Relaxed 0113", cas.expectedRelaxed0113, precisionRelaxed0113);
        if (status.errIfFailureAndReset()) continue;

        check(u" Strict 0113", cas.expectedStrict0113, precisionStrict0113);
        if (status.errIfFailureAndReset()) continue;

        check(u" Relaxed 1133", cas.expectedRelaxed1133, precisionRelaxed1133);
        if (status.errIfFailureAndReset()) continue;

        check(u" Strict 1133", cas.expectedStrict1133, precisionStrict1133);
        if (status.errIfFailureAndReset()) continue;
    }
}

void NumberFormatterApiTest::grouping() {
    assertFormatDescendingBig(
            u"Western Grouping",
            u"group-auto",
            u"",
            NumberFormatter::with().grouping(UNUM_GROUPING_AUTO),
            Locale::getEnglish(),
            u"87,650,000",
            u"8,765,000",
            u"876,500",
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0");

    assertFormatDescendingBig(
            u"Indic Grouping",
            u"group-auto",
            u"",
            NumberFormatter::with().grouping(UNUM_GROUPING_AUTO),
            Locale("en-IN"),
            u"8,76,50,000",
            u"87,65,000",
            u"8,76,500",
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0");

    assertFormatDescendingBig(
            u"Western Grouping, Min 2",
            u"group-min2",
            u",?",
            NumberFormatter::with().grouping(UNUM_GROUPING_MIN2),
            Locale::getEnglish(),
            u"87,650,000",
            u"8,765,000",
            u"876,500",
            u"87,650",
            u"8765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0");

    assertFormatDescendingBig(
            u"Indic Grouping, Min 2",
            u"group-min2",
            u",?",
            NumberFormatter::with().grouping(UNUM_GROUPING_MIN2),
            Locale("en-IN"),
            u"8,76,50,000",
            u"87,65,000",
            u"8,76,500",
            u"87,650",
            u"8765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0");

    assertFormatDescendingBig(
            u"No Grouping",
            u"group-off",
            u",_",
            NumberFormatter::with().grouping(UNUM_GROUPING_OFF),
            Locale("en-IN"),
            u"87650000",
            u"8765000",
            u"876500",
            u"87650",
            u"8765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0");

    assertFormatDescendingBig(
            u"Indic locale with THOUSANDS grouping",
            u"group-thousands",
            u"group-thousands",
            NumberFormatter::with().grouping(UNUM_GROUPING_THOUSANDS),
            Locale("en-IN"),
            u"87,650,000",
            u"8,765,000",
            u"876,500",
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0");

    // NOTE: Polish is interesting because it has minimumGroupingDigits=2 in locale data
    // (Most locales have either 1 or 2)
    // If this test breaks due to data changes, find another locale that has minimumGroupingDigits.
    assertFormatDescendingBig(
            u"Polish Grouping",
            u"group-auto",
            u"",
            NumberFormatter::with().grouping(UNUM_GROUPING_AUTO),
            Locale("pl"),
            u"87 650 000",
            u"8 765 000",
            u"876 500",
            u"87 650",
            u"8765",
            u"876,5",
            u"87,65",
            u"8,765",
            u"0");

    assertFormatDescendingBig(
            u"Polish Grouping, Min 2",
            u"group-min2",
            u",?",
            NumberFormatter::with().grouping(UNUM_GROUPING_MIN2),
            Locale("pl"),
            u"87 650 000",
            u"8 765 000",
            u"876 500",
            u"87 650",
            u"8765",
            u"876,5",
            u"87,65",
            u"8,765",
            u"0");

    assertFormatDescendingBig(
            u"Polish Grouping, Always",
            u"group-on-aligned",
            u",!",
            NumberFormatter::with().grouping(UNUM_GROUPING_ON_ALIGNED),
            Locale("pl"),
            u"87 650 000",
            u"8 765 000",
            u"876 500",
            u"87 650",
            u"8 765",
            u"876,5",
            u"87,65",
            u"8,765",
            u"0");

    // NOTE: en_US_POSIX is interesting because it has no grouping in the default currency format.
    // If this test breaks due to data changes, find another locale that has no default grouping.
    assertFormatDescendingBig(
            u"en_US_POSIX Currency Grouping",
            u"currency/USD group-auto",
            u"currency/USD",
            NumberFormatter::with().grouping(UNUM_GROUPING_AUTO).unit(USD),
            Locale("en_US_POSIX"),
            u"$ 87650000.00",
            u"$ 8765000.00",
            u"$ 876500.00",
            u"$ 87650.00",
            u"$ 8765.00",
            u"$ 876.50",
            u"$ 87.65",
            u"$ 8.76",
            u"$ 0.00");

    assertFormatDescendingBig(
            u"en_US_POSIX Currency Grouping, Always",
            u"currency/USD group-on-aligned",
            u"currency/USD ,!",
            NumberFormatter::with().grouping(UNUM_GROUPING_ON_ALIGNED).unit(USD),
            Locale("en_US_POSIX"),
            u"$ 87,650,000.00",
            u"$ 8,765,000.00",
            u"$ 876,500.00",
            u"$ 87,650.00",
            u"$ 8,765.00",
            u"$ 876.50",
            u"$ 87.65",
            u"$ 8.76",
            u"$ 0.00");

    MacroProps macros;
    macros.grouper = Grouper(4, 1, 3, UNUM_GROUPING_COUNT);
    assertFormatDescendingBig(
            u"Custom Grouping via Internal API",
            nullptr,
            nullptr,
            NumberFormatter::with().macros(macros),
            Locale::getEnglish(),
            u"8,7,6,5,0000",
            u"8,7,6,5000",
            u"876500",
            u"87650",
            u"8765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0");
}

void NumberFormatterApiTest::padding() {
    assertFormatDescending(
            u"Padding",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(Padder::none()),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0");

    assertFormatDescending(
            u"Padding",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                    Padder::codePoints(
                            '*', 8, PadPosition::UNUM_PAD_AFTER_PREFIX)),
            Locale::getEnglish(),
            u"**87,650",
            u"***8,765",
            u"***876.5",
            u"***87.65",
            u"***8.765",
            u"**0.8765",
            u"*0.08765",
            u"0.008765",
            u"*******0");

    assertFormatDescending(
            u"Padding with code points",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                    Padder::codePoints(
                            0x101E4, 8, PadPosition::UNUM_PAD_AFTER_PREFIX)),
            Locale::getEnglish(),
            u"𐇤𐇤87,650",
            u"𐇤𐇤𐇤8,765",
            u"𐇤𐇤𐇤876.5",
            u"𐇤𐇤𐇤87.65",
            u"𐇤𐇤𐇤8.765",
            u"𐇤𐇤0.8765",
            u"𐇤0.08765",
            u"0.008765",
            u"𐇤𐇤𐇤𐇤𐇤𐇤𐇤0");

    assertFormatDescending(
            u"Padding with wide digits",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                            Padder::codePoints(
                                    '*', 8, PadPosition::UNUM_PAD_AFTER_PREFIX))
                    .adoptSymbols(new NumberingSystem(MATHSANB)),
            Locale::getEnglish(),
            u"**𝟴𝟳,𝟲𝟱𝟬",
            u"***𝟴,𝟳𝟲𝟱",
            u"***𝟴𝟳𝟲.𝟱",
            u"***𝟴𝟳.𝟲𝟱",
            u"***𝟴.𝟳𝟲𝟱",
            u"**𝟬.𝟴𝟳𝟲𝟱",
            u"*𝟬.𝟬𝟴𝟳𝟲𝟱",
            u"𝟬.𝟬𝟬𝟴𝟳𝟲𝟱",
            u"*******𝟬");

    assertFormatDescending(
            u"Padding with currency spacing",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                            Padder::codePoints(
                                    '*', 10, PadPosition::UNUM_PAD_AFTER_PREFIX))
                    .unit(GBP)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_ISO_CODE),
            Locale::getEnglish(),
            u"GBP 87,650.00",
            u"GBP 8,765.00",
            u"GBP*876.50",
            u"GBP**87.65",
            u"GBP***8.76",
            u"GBP***0.88",
            u"GBP***0.09",
            u"GBP***0.01",
            u"GBP***0.00");

    assertFormatSingle(
            u"Pad Before Prefix",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                    Padder::codePoints(
                            '*', 8, PadPosition::UNUM_PAD_BEFORE_PREFIX)),
            Locale::getEnglish(),
            -88.88,
            u"**-88.88");

    assertFormatSingle(
            u"Pad After Prefix",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                    Padder::codePoints(
                            '*', 8, PadPosition::UNUM_PAD_AFTER_PREFIX)),
            Locale::getEnglish(),
            -88.88,
            u"-**88.88");

    assertFormatSingle(
            u"Pad Before Suffix",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                    Padder::codePoints(
                            '*', 8, PadPosition::UNUM_PAD_BEFORE_SUFFIX)).unit(NoUnit::percent()),
            Locale::getEnglish(),
            88.88,
            u"88.88**%");

    assertFormatSingle(
            u"Pad After Suffix",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                    Padder::codePoints(
                            '*', 8, PadPosition::UNUM_PAD_AFTER_SUFFIX)).unit(NoUnit::percent()),
            Locale::getEnglish(),
            88.88,
            u"88.88%**");

    assertFormatSingle(
            u"Currency Spacing with Zero Digit Padding Broken",
            nullptr,
            nullptr,
            NumberFormatter::with().padding(
                            Padder::codePoints(
                                    '0', 12, PadPosition::UNUM_PAD_AFTER_PREFIX))
                    .unit(GBP)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_ISO_CODE),
            Locale::getEnglish(),
            514.23,
            u"GBP 000514.23"); // TODO: This is broken; it renders too wide (13 instead of 12).
}

void NumberFormatterApiTest::integerWidth() {
    assertFormatDescending(
            u"Integer Width Default",
            u"integer-width/+0",
            u"0",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(1)),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0");

    assertFormatDescending(
            u"Integer Width Zero Fill 0",
            u"integer-width/*",
            u"integer-width/+",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(0)),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u".8765",
            u".08765",
            u".008765",
            u"0");  // see ICU-20844

    assertFormatDescending(
            u"Integer Width Zero Fill 3",
            u"integer-width/+000",
            u"000",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(3)),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"087.65",
            u"008.765",
            u"000.8765",
            u"000.08765",
            u"000.008765",
            u"000");

    assertFormatDescending(
            u"Integer Width Max 3",
            u"integer-width/##0",
            u"integer-width/##0",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(1).truncateAt(3)),
            Locale::getEnglish(),
            u"650",
            u"765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0");

    assertFormatDescending(
            u"Integer Width Fixed 2",
            u"integer-width/00",
            u"integer-width/00",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(2).truncateAt(2)),
            Locale::getEnglish(),
            u"50",
            u"65",
            u"76.5",
            u"87.65",
            u"08.765",
            u"00.8765",
            u"00.08765",
            u"00.008765",
            u"00");

    assertFormatDescending(
            u"Integer Width Compact",
            u"compact-short integer-width/000",
            u"compact-short integer-width/000",
            NumberFormatter::with()
                .notation(Notation::compactShort())
                .integerWidth(IntegerWidth::zeroFillTo(3).truncateAt(3)),
            Locale::getEnglish(),
            u"088K",
            u"008.8K",
            u"876",
            u"088",
            u"008.8",
            u"000.88",
            u"000.088",
            u"000.0088",
            u"000");

    assertFormatDescending(
            u"Integer Width Scientific",
            u"scientific integer-width/000",
            u"scientific integer-width/000",
            NumberFormatter::with()
                .notation(Notation::scientific())
                .integerWidth(IntegerWidth::zeroFillTo(3).truncateAt(3)),
            Locale::getEnglish(),
            u"008.765E4",
            u"008.765E3",
            u"008.765E2",
            u"008.765E1",
            u"008.765E0",
            u"008.765E-1",
            u"008.765E-2",
            u"008.765E-3",
            u"000E0");

    assertFormatDescending(
            u"Integer Width Engineering",
            u"engineering integer-width/000",
            u"engineering integer-width/000",
            NumberFormatter::with()
                .notation(Notation::engineering())
                .integerWidth(IntegerWidth::zeroFillTo(3).truncateAt(3)),
            Locale::getEnglish(),
            u"087.65E3",
            u"008.765E3",
            u"876.5E0",
            u"087.65E0",
            u"008.765E0",
            u"876.5E-3",
            u"087.65E-3",
            u"008.765E-3",
            u"000E0");

    assertFormatSingle(
            u"Integer Width Remove All A",
            u"integer-width/00",
            u"integer-width/00",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(2).truncateAt(2)),
            "en",
            2500,
            u"00");

    assertFormatSingle(
            u"Integer Width Remove All B",
            u"integer-width/00",
            u"integer-width/00",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(2).truncateAt(2)),
            "en",
            25000,
            u"00");

    assertFormatSingle(
            u"Integer Width Remove All B, Bytes Mode",
            u"integer-width/00",
            u"integer-width/00",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(2).truncateAt(2)),
            "en",
            // Note: this double produces all 17 significant digits
            10000000000000002000.0,
            u"00");

    assertFormatDescending(
            u"Integer Width Double Zero (ICU-21590)",
            u"integer-width-trunc",
            u"integer-width-trunc",
            NumberFormatter::with()
                .integerWidth(IntegerWidth::zeroFillTo(0).truncateAt(0)),
            Locale::getEnglish(),
            u"0",
            u"0",
            u".5",
            u".65",
            u".765",
            u".8765",
            u".08765",
            u".008765",
            u"0");

    assertFormatDescending(
            u"Integer Width Double Zero with minFraction (ICU-21590)",
            u"integer-width-trunc .0*",
            u"integer-width-trunc .0*",
            NumberFormatter::with()
                .integerWidth(IntegerWidth::zeroFillTo(0).truncateAt(0))
                .precision(Precision::minFraction(1)),
            Locale::getEnglish(),
            u".0",
            u".0",
            u".5",
            u".65",
            u".765",
            u".8765",
            u".08765",
            u".008765",
            u".0");
}

void NumberFormatterApiTest::symbols() {
    assertFormatDescending(
            u"French Symbols with Japanese Data 1",
            nullptr,
            nullptr,
            NumberFormatter::with().symbols(FRENCH_SYMBOLS),
            Locale::getJapan(),
            u"87\u202F650",
            u"8\u202F765",
            u"876,5",
            u"87,65",
            u"8,765",
            u"0,8765",
            u"0,08765",
            u"0,008765",
            u"0");

    assertFormatSingle(
            u"French Symbols with Japanese Data 2",
            nullptr,
            nullptr,
            NumberFormatter::with().notation(Notation::compactShort()).symbols(FRENCH_SYMBOLS),
            Locale::getJapan(),
            12345,
            u"1,2\u4E07");

    assertFormatDescending(
            u"Latin Numbering System with Arabic Data",
            u"currency/USD latin",
            u"currency/USD latin",
            NumberFormatter::with().adoptSymbols(new NumberingSystem(LATN)).unit(USD),
            Locale("ar"),
            u"\u200F87,650.00 US$",
            u"\u200F8,765.00 US$",
            u"\u200F876.50 US$",
            u"\u200F87.65 US$",
            u"\u200F8.76 US$",
            u"\u200F0.88 US$",
            u"\u200F0.09 US$",
            u"\u200F0.01 US$",
            u"\u200F0.00 US$");

    assertFormatDescending(
            u"Math Numbering System with French Data",
            u"numbering-system/mathsanb",
            u"numbering-system/mathsanb",
            NumberFormatter::with().adoptSymbols(new NumberingSystem(MATHSANB)),
            Locale::getFrench(),
            u"𝟴𝟳\u202F𝟲𝟱𝟬",
            u"𝟴\u202F𝟳𝟲𝟱",
            u"𝟴𝟳𝟲,𝟱",
            u"𝟴𝟳,𝟲𝟱",
            u"𝟴,𝟳𝟲𝟱",
            u"𝟬,𝟴𝟳𝟲𝟱",
            u"𝟬,𝟬𝟴𝟳𝟲𝟱",
            u"𝟬,𝟬𝟬𝟴𝟳𝟲𝟱",
            u"𝟬");

    assertFormatSingle(
            u"Swiss Symbols (used in documentation)",
            nullptr,
            nullptr,
            NumberFormatter::with().symbols(SWISS_SYMBOLS),
            Locale::getEnglish(),
            12345.67,
            u"12’345.67");

    assertFormatSingle(
            u"Myanmar Symbols (used in documentation)",
            nullptr,
            nullptr,
            NumberFormatter::with().symbols(MYANMAR_SYMBOLS),
            Locale::getEnglish(),
            12345.67,
            u"\u1041\u1042,\u1043\u1044\u1045.\u1046\u1047");

    // NOTE: Locale ar puts ¤ after the number in NS arab but before the number in NS latn.

    assertFormatSingle(
            u"Currency symbol should follow number in ar with NS latn",
            u"currency/USD latin",
            u"currency/USD latin",
            NumberFormatter::with().adoptSymbols(new NumberingSystem(LATN)).unit(USD),
            Locale("ar"),
            12345.67,
            u"\u200F12,345.67 US$");

    assertFormatSingle(
            u"Currency symbol should follow number in ar@numbers=latn",
            u"currency/USD",
            u"currency/USD",
            NumberFormatter::with().unit(USD),
            Locale("ar@numbers=latn"),
            12345.67,
            u"\u200F12,345.67 US$");

    assertFormatSingle(
            u"Currency symbol should follow number in ar-EG with NS arab",
            u"currency/USD",
            u"currency/USD",
            NumberFormatter::with().unit(USD),
            Locale("ar-EG"),
            12345.67,
            u"\u200F١٢٬٣٤٥٫٦٧ US$");

    assertFormatSingle(
            u"Currency symbol should follow number in ar@numbers=arab",
            u"currency/USD",
            u"currency/USD",
            NumberFormatter::with().unit(USD),
            Locale("ar@numbers=arab"),
            12345.67,
            u"\u200F١٢٬٣٤٥٫٦٧ US$");

    assertFormatSingle(
            u"NumberingSystem in API should win over @numbers keyword",
            u"currency/USD latin",
            u"currency/USD latin",
            NumberFormatter::with().adoptSymbols(new NumberingSystem(LATN)).unit(USD),
            Locale("ar@numbers=arab"),
            12345.67,
            u"\u200F12,345.67 US$");

    UErrorCode status = U_ZERO_ERROR;
    assertEquals(
            "NumberingSystem in API should win over @numbers keyword in reverse order",
            u"\u200F12,345.67 US$",
            NumberFormatter::withLocale(Locale("ar@numbers=arab")).adoptSymbols(new NumberingSystem(LATN))
                    .unit(USD)
                    .formatDouble(12345.67, status)
                    .toString(status));

    DecimalFormatSymbols symbols = SWISS_SYMBOLS;
    UnlocalizedNumberFormatter f = NumberFormatter::with().symbols(symbols);
    symbols.setSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kGroupingSeparatorSymbol, u"!", status);
    assertFormatSingle(
            u"Symbols object should be copied",
            nullptr,
            nullptr,
            f,
            Locale::getEnglish(),
            12345.67,
            u"12’345.67");

    assertFormatSingle(
            u"The last symbols setter wins",
            u"latin",
            u"latin",
            NumberFormatter::with().symbols(symbols).adoptSymbols(new NumberingSystem(LATN)),
            Locale::getEnglish(),
            12345.67,
            u"12,345.67");

    assertFormatSingle(
            u"The last symbols setter wins",
            nullptr,
            nullptr,
            NumberFormatter::with().adoptSymbols(new NumberingSystem(LATN)).symbols(symbols),
            Locale::getEnglish(),
            12345.67,
            u"12!345.67");
}

// TODO: Enable if/when currency symbol override is added.
//void NumberFormatterTest::symbolsOverride() {
//    DecimalFormatSymbols dfs = DecimalFormatSymbols.getInstance(Locale::getEnglish());
//    dfs.setCurrencySymbol("@");
//    dfs.setInternationalCurrencySymbol("foo");
//    assertFormatSingle(
//            u"Custom Short Currency Symbol",
//            NumberFormatter::with().unit(Currency.getInstance("XXX")).symbols(dfs),
//            Locale::getEnglish(),
//            12.3,
//            u"@ 12.30");
//}

void NumberFormatterApiTest::sign() {
    assertFormatSingle(
            u"Sign Auto Positive",
            u"sign-auto",
            u"",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_AUTO),
            Locale::getEnglish(),
            444444,
            u"444,444");

    assertFormatSingle(
            u"Sign Auto Negative",
            u"sign-auto",
            u"",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_AUTO),
            Locale::getEnglish(),
            -444444,
            u"-444,444");

    assertFormatSingle(
            u"Sign Auto Zero",
            u"sign-auto",
            u"",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_AUTO),
            Locale::getEnglish(),
            0,
            u"0");

    assertFormatSingle(
            u"Sign Always Positive",
            u"sign-always",
            u"+!",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ALWAYS),
            Locale::getEnglish(),
            444444,
            u"+444,444");

    assertFormatSingle(
            u"Sign Always Negative",
            u"sign-always",
            u"+!",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ALWAYS),
            Locale::getEnglish(),
            -444444,
            u"-444,444");

    assertFormatSingle(
            u"Sign Always Zero",
            u"sign-always",
            u"+!",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ALWAYS),
            Locale::getEnglish(),
            0,
            u"+0");

    assertFormatSingle(
            u"Sign Never Positive",
            u"sign-never",
            u"+_",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEVER),
            Locale::getEnglish(),
            444444,
            u"444,444");

    assertFormatSingle(
            u"Sign Never Negative",
            u"sign-never",
            u"+_",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEVER),
            Locale::getEnglish(),
            -444444,
            u"444,444");

    assertFormatSingle(
            u"Sign Never Zero",
            u"sign-never",
            u"+_",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEVER),
            Locale::getEnglish(),
            0,
            u"0");

    assertFormatSingle(
            u"Sign Accounting Positive",
            u"currency/USD sign-accounting",
            u"currency/USD ()",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING).unit(USD),
            Locale::getEnglish(),
            444444,
            u"$444,444.00");

    assertFormatSingle(
            u"Sign Accounting Negative",
            u"currency/USD sign-accounting",
            u"currency/USD ()",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING).unit(USD),
            Locale::getEnglish(),
            -444444,
            u"($444,444.00)");

    assertFormatSingle(
            u"Sign Accounting Zero",
            u"currency/USD sign-accounting",
            u"currency/USD ()",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING).unit(USD),
            Locale::getEnglish(),
            0,
            u"$0.00");

    assertFormatSingle(
            u"Sign Accounting-Always Positive",
            u"currency/USD sign-accounting-always",
            u"currency/USD ()!",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_ALWAYS).unit(USD),
            Locale::getEnglish(),
            444444,
            u"+$444,444.00");

    assertFormatSingle(
            u"Sign Accounting-Always Negative",
            u"currency/USD sign-accounting-always",
            u"currency/USD ()!",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_ALWAYS).unit(USD),
            Locale::getEnglish(),
            -444444,
            u"($444,444.00)");

    assertFormatSingle(
            u"Sign Accounting-Always Zero",
            u"currency/USD sign-accounting-always",
            u"currency/USD ()!",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_ALWAYS).unit(USD),
            Locale::getEnglish(),
            0,
            u"+$0.00");

    assertFormatSingle(
            u"Sign Except-Zero Positive",
            u"sign-except-zero",
            u"+?",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_EXCEPT_ZERO),
            Locale::getEnglish(),
            444444,
            u"+444,444");

    assertFormatSingle(
            u"Sign Except-Zero Negative",
            u"sign-except-zero",
            u"+?",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_EXCEPT_ZERO),
            Locale::getEnglish(),
            -444444,
            u"-444,444");

    assertFormatSingle(
            u"Sign Except-Zero Zero",
            u"sign-except-zero",
            u"+?",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_EXCEPT_ZERO),
            Locale::getEnglish(),
            0,
            u"0");

    assertFormatSingle(
            u"Sign Accounting-Except-Zero Positive",
            u"currency/USD sign-accounting-except-zero",
            u"currency/USD ()?",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO).unit(USD),
            Locale::getEnglish(),
            444444,
            u"+$444,444.00");

    assertFormatSingle(
            u"Sign Accounting-Except-Zero Negative",
            u"currency/USD sign-accounting-except-zero",
            u"currency/USD ()?",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO).unit(USD),
            Locale::getEnglish(),
            -444444,
            u"($444,444.00)");

    assertFormatSingle(
            u"Sign Accounting-Except-Zero Zero",
            u"currency/USD sign-accounting-except-zero",
            u"currency/USD ()?",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO).unit(USD),
            Locale::getEnglish(),
            0,
            u"$0.00");

    assertFormatSingle(
            u"Sign Negative Positive",
            u"sign-negative",
            u"+-",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEGATIVE),
            Locale::getEnglish(),
            444444,
            u"444,444");

    assertFormatSingle(
            u"Sign Negative Negative",
            u"sign-negative",
            u"+-",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEGATIVE),
            Locale::getEnglish(),
            -444444,
            u"-444,444");

    assertFormatSingle(
            u"Sign Negative Negative Zero",
            u"sign-negative",
            u"+-",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEGATIVE),
            Locale::getEnglish(),
            -0.0000001,
            u"0");

    assertFormatSingle(
            u"Sign Accounting-Negative Positive",
            u"currency/USD sign-accounting-negative",
            u"currency/USD ()-",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_NEGATIVE).unit(USD),
            Locale::getEnglish(),
            444444,
            u"$444,444.00");
        
    assertFormatSingle(
            u"Sign Accounting-Negative Negative",
            u"currency/USD sign-accounting-negative",
            u"currency/USD ()-",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_NEGATIVE).unit(USD),
            Locale::getEnglish(),
            -444444,
            "($444,444.00)");

    assertFormatSingle(
            u"Sign Accounting-Negative Negative Zero",
            u"currency/USD sign-accounting-negative",
            u"currency/USD ()-",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_NEGATIVE).unit(USD),
            Locale::getEnglish(),
            -0.0000001,
            u"$0.00");

    assertFormatSingle(
            u"Sign Accounting Negative Hidden",
            u"currency/USD unit-width-hidden sign-accounting",
            u"currency/USD unit-width-hidden ()",
            NumberFormatter::with()
                    .sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING)
                    .unit(USD)
                    .unitWidth(UNUM_UNIT_WIDTH_HIDDEN),
            Locale::getEnglish(),
            -444444,
            u"(444,444.00)");

    assertFormatSingle(
            u"Sign Accounting Negative Narrow",
            u"currency/USD unit-width-narrow sign-accounting",
            u"currency/USD unit-width-narrow ()",
            NumberFormatter::with()
                .sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING)
                .unit(USD)
                .unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale::getCanada(),
            -444444,
            u"($444,444.00)");

    assertFormatSingle(
            u"Sign Accounting Negative Short",
            u"currency/USD sign-accounting",
            u"currency/USD ()",
            NumberFormatter::with()
                .sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING)
                .unit(USD)
                .unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale::getCanada(),
            -444444,
            u"(US$444,444.00)");

    assertFormatSingle(
            u"Sign Accounting Negative Iso Code",
            u"currency/USD unit-width-iso-code sign-accounting",
            u"currency/USD unit-width-iso-code ()",
            NumberFormatter::with()
                .sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING)
                .unit(USD)
                .unitWidth(UNUM_UNIT_WIDTH_ISO_CODE),
            Locale::getCanada(),
            -444444,
            u"(USD 444,444.00)");

    // Note: CLDR does not provide an accounting pattern for long name currency.
    // We fall back to normal currency format. This may change in the future.
    assertFormatSingle(
            u"Sign Accounting Negative Full Name",
            u"currency/USD unit-width-full-name sign-accounting",
            u"currency/USD unit-width-full-name ()",
            NumberFormatter::with()
                .sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING)
                .unit(USD)
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getCanada(),
            -444444,
            u"-444,444.00 US dollars");
}

void NumberFormatterApiTest::signNearZero() {
    // https://unicode-org.atlassian.net/browse/ICU-20709
    IcuTestErrorCode status(*this, "signNearZero");
    const struct TestCase {
        UNumberSignDisplay sign;
        double input;
        const char16_t* expected;
    } cases[] = {
        { UNUM_SIGN_AUTO,  1.1, u"1" },
        { UNUM_SIGN_AUTO,  0.9, u"1" },
        { UNUM_SIGN_AUTO,  0.1, u"0" },
        { UNUM_SIGN_AUTO, -0.1, u"-0" }, // interesting case
        { UNUM_SIGN_AUTO, -0.9, u"-1" },
        { UNUM_SIGN_AUTO, -1.1, u"-1" },
        { UNUM_SIGN_ALWAYS,  1.1, u"+1" },
        { UNUM_SIGN_ALWAYS,  0.9, u"+1" },
        { UNUM_SIGN_ALWAYS,  0.1, u"+0" },
        { UNUM_SIGN_ALWAYS, -0.1, u"-0" },
        { UNUM_SIGN_ALWAYS, -0.9, u"-1" },
        { UNUM_SIGN_ALWAYS, -1.1, u"-1" },
        { UNUM_SIGN_EXCEPT_ZERO,  1.1, u"+1" },
        { UNUM_SIGN_EXCEPT_ZERO,  0.9, u"+1" },
        { UNUM_SIGN_EXCEPT_ZERO,  0.1, u"0" }, // interesting case
        { UNUM_SIGN_EXCEPT_ZERO, -0.1, u"0" }, // interesting case
        { UNUM_SIGN_EXCEPT_ZERO, -0.9, u"-1" },
        { UNUM_SIGN_EXCEPT_ZERO, -1.1, u"-1" },
        { UNUM_SIGN_NEGATIVE,  1.1, u"1" },
        { UNUM_SIGN_NEGATIVE,  0.9, u"1" },
        { UNUM_SIGN_NEGATIVE,  0.1, u"0" },
        { UNUM_SIGN_NEGATIVE, -0.1, u"0" }, // interesting case
        { UNUM_SIGN_NEGATIVE, -0.9, u"-1" },
        { UNUM_SIGN_NEGATIVE, -1.1, u"-1" },
    };
    for (const auto& cas : cases) {
        auto sign = cas.sign;
        auto input = cas.input;
        const auto* expected = cas.expected;
        auto actual = NumberFormatter::with()
            .sign(sign)
            .precision(Precision::integer())
            .locale(Locale::getUS())
            .formatDouble(input, status)
            .toString(status);
        assertEquals(
            DoubleToUnicodeString(input) + " @ SignDisplay " + Int64ToUnicodeString(sign),
            expected, actual);
    }
}

void NumberFormatterApiTest::signCoverage() {
    // https://unicode-org.atlassian.net/browse/ICU-20708
    IcuTestErrorCode status(*this, "signCoverage");
    const struct TestCase {
        UNumberSignDisplay sign;
        const char16_t* expectedStrings[8];
    } cases[] = {
        { UNUM_SIGN_AUTO, {        u"-∞", u"-1", u"-0",  u"0",  u"1",  u"∞",  u"NaN", u"-NaN" } },
        { UNUM_SIGN_ALWAYS, {      u"-∞", u"-1", u"-0", u"+0", u"+1", u"+∞", u"+NaN", u"-NaN" } },
        { UNUM_SIGN_NEVER, {        u"∞",  u"1",  u"0",  u"0",  u"1",  u"∞",  u"NaN",  u"NaN" } },
        { UNUM_SIGN_EXCEPT_ZERO, { u"-∞", u"-1",  u"0",  u"0", u"+1", u"+∞",  u"NaN",  u"NaN" } },
    };
    double negNaN = std::copysign(uprv_getNaN(), -0.0);
    const double inputs[] = {
        -uprv_getInfinity(), -1, -0.0, 0, 1, uprv_getInfinity(), uprv_getNaN(), negNaN
    };
    for (const auto& cas : cases) {
        auto sign = cas.sign;
        for (int32_t i = 0; i < UPRV_LENGTHOF(inputs); i++) {
            auto input = inputs[i];
            const auto* expected = cas.expectedStrings[i];
            auto actual = NumberFormatter::with()
                .sign(sign)
                .locale(Locale::getUS())
                .formatDouble(input, status)
                .toString(status);
            assertEquals(
                DoubleToUnicodeString(input) + " " + Int64ToUnicodeString(sign),
                expected, actual);
        }
    }
}

void NumberFormatterApiTest::decimal() {
    assertFormatDescending(
            u"Decimal Default",
            u"decimal-auto",
            u"",
            NumberFormatter::with().decimal(UNumberDecimalSeparatorDisplay::UNUM_DECIMAL_SEPARATOR_AUTO),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0");

    assertFormatDescending(
            u"Decimal Always Shown",
            u"decimal-always",
            u"decimal-always",
            NumberFormatter::with().decimal(UNumberDecimalSeparatorDisplay::UNUM_DECIMAL_SEPARATOR_ALWAYS),
            Locale::getEnglish(),
            u"87,650.",
            u"8,765.",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0.");
}

void NumberFormatterApiTest::scale() {
    assertFormatDescending(
            u"Multiplier None",
            u"scale/1",
            u"",
            NumberFormatter::with().scale(Scale::none()),
            Locale::getEnglish(),
            u"87,650",
            u"8,765",
            u"876.5",
            u"87.65",
            u"8.765",
            u"0.8765",
            u"0.08765",
            u"0.008765",
            u"0");

    assertFormatDescending(
            u"Multiplier Power of Ten",
            u"scale/1000000",
            u"scale/1E6",
            NumberFormatter::with().scale(Scale::powerOfTen(6)),
            Locale::getEnglish(),
            u"87,650,000,000",
            u"8,765,000,000",
            u"876,500,000",
            u"87,650,000",
            u"8,765,000",
            u"876,500",
            u"87,650",
            u"8,765",
            u"0");

    assertFormatDescending(
            u"Multiplier Arbitrary Double",
            u"scale/5.2",
            u"scale/5.2",
            NumberFormatter::with().scale(Scale::byDouble(5.2)),
            Locale::getEnglish(),
            u"455,780",
            u"45,578",
            u"4,557.8",
            u"455.78",
            u"45.578",
            u"4.5578",
            u"0.45578",
            u"0.045578",
            u"0");

    assertFormatDescending(
            u"Multiplier Arbitrary BigDecimal",
            u"scale/5.2",
            u"scale/5.2",
            NumberFormatter::with().scale(Scale::byDecimal({"5.2", -1})),
            Locale::getEnglish(),
            u"455,780",
            u"45,578",
            u"4,557.8",
            u"455.78",
            u"45.578",
            u"4.5578",
            u"0.45578",
            u"0.045578",
            u"0");

    assertFormatDescending(
            u"Multiplier Arbitrary Double And Power Of Ten",
            u"scale/5200",
            u"scale/5200",
            NumberFormatter::with().scale(Scale::byDoubleAndPowerOfTen(5.2, 3)),
            Locale::getEnglish(),
            u"455,780,000",
            u"45,578,000",
            u"4,557,800",
            u"455,780",
            u"45,578",
            u"4,557.8",
            u"455.78",
            u"45.578",
            u"0");

    assertFormatDescending(
            u"Multiplier Zero",
            u"scale/0",
            u"scale/0",
            NumberFormatter::with().scale(Scale::byDouble(0)),
            Locale::getEnglish(),
            u"0",
            u"0",
            u"0",
            u"0",
            u"0",
            u"0",
            u"0",
            u"0",
            u"0");

    assertFormatSingle(
            u"Multiplier Skeleton Scientific Notation and Percent",
            u"percent scale/1E2",
            u"%x100",
            NumberFormatter::with().unit(NoUnit::percent()).scale(Scale::powerOfTen(2)),
            Locale::getEnglish(),
            0.5,
            u"50%");

    assertFormatSingle(
            u"Negative Multiplier",
            u"scale/-5.2",
            u"scale/-5.2",
            NumberFormatter::with().scale(Scale::byDouble(-5.2)),
            Locale::getEnglish(),
            2,
            u"-10.4");

    assertFormatSingle(
            u"Negative One Multiplier",
            u"scale/-1",
            u"scale/-1",
            NumberFormatter::with().scale(Scale::byDouble(-1)),
            Locale::getEnglish(),
            444444,
            u"-444,444");

    assertFormatSingle(
            u"Two-Type Multiplier with Overlap",
            u"scale/10000",
            u"scale/1E4",
            NumberFormatter::with().scale(Scale::byDoubleAndPowerOfTen(100, 2)),
            Locale::getEnglish(),
            2,
            u"20,000");
}

void NumberFormatterApiTest::locale() {
    // Coverage for the locale setters.
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString actual = NumberFormatter::withLocale(Locale::getFrench()).formatInt(1234, status)
            .toString(status);
    assertEquals("Locale withLocale()", u"1\u202f234", actual);

    LocalizedNumberFormatter lnf1 = NumberFormatter::withLocale("en").unitWidth(UNUM_UNIT_WIDTH_FULL_NAME)
            .scale(Scale::powerOfTen(2));
    LocalizedNumberFormatter lnf2 = NumberFormatter::with()
            .notation(Notation::compactLong()).locale("fr").unitWidth(UNUM_UNIT_WIDTH_FULL_NAME);
    UnlocalizedNumberFormatter unf1 = lnf1.withoutLocale();
    UnlocalizedNumberFormatter unf2 = std::move(lnf2).withoutLocale();

    assertFormatSingle(
            u"Formatter after withoutLocale A",
            u"unit/meter unit-width-full-name scale/100",
            u"unit/meter unit-width-full-name scale/100",
            unf1.unit(METER),
            "it-IT",
            2,
            u"200 metri");

    assertFormatSingle(
            u"Formatter after withoutLocale B",
            u"compact-long unit/meter unit-width-full-name",
            u"compact-long unit/meter unit-width-full-name",
            unf2.unit(METER),
            "ja-JP",
            2,
            u"2 メートル");
}

void NumberFormatterApiTest::skeletonUserGuideExamples() {
    IcuTestErrorCode status(*this, "skeletonUserGuideExamples");

    // Test the skeleton examples in userguide/format_parse/numbers/skeletons.md
    struct TestCase {
        const char16_t* skeleton;
        const char16_t* conciseSkeleton;
        double input;
        const char16_t* expected;
    } cases[] = {
        {u"percent", u"%", 25, u"25%"},
        {u".00", u".00", 25, u"25.00"},
        {u"percent .00", u"% .00", 25, u"25.00%"},
        {u"scale/100", u"scale/100", 0.3, u"30"},
        {u"percent scale/100", u"%x100", 0.3, u"30%"},
        {u"measure-unit/length-meter", u"unit/meter", 5, u"5 m"},
        {u"measure-unit/length-meter unit-width-full-name", u"unit/meter unit-width-full-name", 5, u"5 meters"},
        {u"currency/CAD", u"currency/CAD", 10, u"CA$10.00"},
        {u"currency/CAD unit-width-narrow", u"currency/CAD unit-width-narrow", 10, u"$10.00"},
        {u"compact-short", u"K", 5000, u"5K"},
        {u"compact-long", u"KK", 5000, u"5 thousand"},
        {u"compact-short currency/CAD", u"K currency/CAD", 5000, u"CA$5K"},
        {u"", u"", 5000, u"5,000"},
        {u"group-min2", u",?", 5000, u"5000"},
        {u"group-min2", u",?", 15000, u"15,000"},
        {u"sign-always", u"+!", 60, u"+60"},
        {u"sign-always", u"+!", 0, u"+0"},
        {u"sign-except-zero", u"+?", 60, u"+60"},
        {u"sign-except-zero", u"+?", 0, u"0"},
        {u"sign-accounting currency/CAD", u"() currency/CAD", -40, u"(CA$40.00)"}
    };

    for (const auto& cas : cases) {
        status.setScope(cas.skeleton);
        FormattedNumber actual = NumberFormatter::forSkeleton(cas.skeleton, status)
            .locale("en-US")
            .formatDouble(cas.input, status);
        assertEquals(cas.skeleton, cas.expected, actual.toTempString(status));
        status.errIfFailureAndReset();
        FormattedNumber actualConcise = NumberFormatter::forSkeleton(cas.conciseSkeleton, status)
            .locale("en-US")
            .formatDouble(cas.input, status);
        assertEquals(cas.conciseSkeleton, cas.expected, actualConcise.toTempString(status));
        status.errIfFailureAndReset();
    }
}

void NumberFormatterApiTest::formatTypes() {
    UErrorCode status = U_ZERO_ERROR;
    LocalizedNumberFormatter formatter = NumberFormatter::withLocale(Locale::getEnglish());

    // Double
    assertEquals("Format double", "514.23", formatter.formatDouble(514.23, status).toString(status));

    // Int64
    assertEquals("Format int64", "51,423", formatter.formatDouble(51423L, status).toString(status));

    // decNumber
    UnicodeString actual = formatter.formatDecimal("98765432123456789E1", status).toString(status);
    assertEquals("Format decNumber", u"987,654,321,234,567,890", actual);

    // Also test proper DecimalQuantity bytes storage when all digits are in the fraction.
    // The number needs to have exactly 40 digits, which is the size of the default buffer.
    // (issue discovered by the address sanitizer in C++)
    static const char* str = "0.009876543210987654321098765432109876543211";
    actual = formatter.precision(Precision::unlimited()).formatDecimal(str, status).toString(status);
    assertEquals("Format decNumber to 40 digits", str, actual);
}

void NumberFormatterApiTest::fieldPositionLogic() {
    IcuTestErrorCode status(*this, "fieldPositionLogic");

    const char16_t* message = u"Field position logic test";

    FormattedNumber fmtd = assertFormatSingle(
            message,
            u"",
            u"",
            NumberFormatter::with(),
            Locale::getEnglish(),
            -9876543210.12,
            u"-9,876,543,210.12");

    static const UFieldPosition expectedFieldPositions[] = {
            // field, begin index, end index
            {UNUM_SIGN_FIELD, 0, 1},
            {UNUM_GROUPING_SEPARATOR_FIELD, 2, 3},
            {UNUM_GROUPING_SEPARATOR_FIELD, 6, 7},
            {UNUM_GROUPING_SEPARATOR_FIELD, 10, 11},
            {UNUM_INTEGER_FIELD, 1, 14},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 14, 15},
            {UNUM_FRACTION_FIELD, 15, 17}};

    assertNumberFieldPositions(
            message,
            fmtd,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));

    // Test the iteration functionality of nextFieldPosition
    ConstrainedFieldPosition actual;
    actual.constrainField(UFIELD_CATEGORY_NUMBER, UNUM_GROUPING_SEPARATOR_FIELD);
    int32_t i = 1;
    while (fmtd.nextPosition(actual, status)) {
        UFieldPosition expected = expectedFieldPositions[i++];
        assertEquals(
                UnicodeString(u"Next for grouping, field, case #") + Int64ToUnicodeString(i),
                expected.field,
                actual.getField());
        assertEquals(
                UnicodeString(u"Next for grouping, begin index, case #") + Int64ToUnicodeString(i),
                expected.beginIndex,
                actual.getStart());
        assertEquals(
                UnicodeString(u"Next for grouping, end index, case #") + Int64ToUnicodeString(i),
                expected.endIndex,
                actual.getLimit());
    }
    assertEquals(u"Should have seen all grouping separators", 4, i);

    // Make sure strings without fraction do not contain fraction field
    actual.reset();
    actual.constrainField(UFIELD_CATEGORY_NUMBER, UNUM_FRACTION_FIELD);
    fmtd = NumberFormatter::withLocale("en").formatInt(5, status);
    assertFalse(u"No fraction part in an integer", fmtd.nextPosition(actual, status));
}

void NumberFormatterApiTest::fieldPositionCoverage() {
    IcuTestErrorCode status(*this, "fieldPositionCoverage");

    {
        const char16_t* message = u"Measure unit field position basic";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/temperature-fahrenheit",
                u"unit/fahrenheit",
                NumberFormatter::with().unit(FAHRENHEIT),
                Locale::getEnglish(),
                68,
                u"68°F");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 2},
                {UNUM_MEASURE_UNIT_FIELD, 2, 4}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Measure unit field position with compound unit";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/temperature-fahrenheit per-measure-unit/duration-day",
                u"unit/fahrenheit-per-day",
                NumberFormatter::with().unit(FAHRENHEIT).perUnit(DAY),
                Locale::getEnglish(),
                68,
                u"68°F/d");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 2},
                // coverage for old enum:
                {DecimalFormat::kMeasureUnitField, 2, 6}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Measure unit field position with spaces";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/length-meter unit-width-full-name",
                u"unit/meter unit-width-full-name",
                NumberFormatter::with().unit(METER).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
                Locale::getEnglish(),
                68,
                u"68 meters");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 2},
                // note: field starts after the space
                {UNUM_MEASURE_UNIT_FIELD, 3, 9}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Measure unit field position with prefix and suffix, composed m/s";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/length-meter per-measure-unit/duration-second unit-width-full-name",
                u"measure-unit/length-meter per-measure-unit/duration-second unit-width-full-name",
                NumberFormatter::with().unit(METER).perUnit(SECOND).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
                "ky", // locale with the interesting data
                68,
                u"секундасына 68 метр");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_MEASURE_UNIT_FIELD, 0, 11},
                {UNUM_INTEGER_FIELD, 12, 14},
                {UNUM_MEASURE_UNIT_FIELD, 15, 19}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Measure unit field position with prefix and suffix, built-in m/s";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/speed-meter-per-second unit-width-full-name",
                u"unit/meter-per-second unit-width-full-name",
                NumberFormatter::with().unit(METER_PER_SECOND).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
                "ky", // locale with the interesting data
                68,
                u"секундасына 68 метр");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_MEASURE_UNIT_FIELD, 0, 11},
                {UNUM_INTEGER_FIELD, 12, 14},
                {UNUM_MEASURE_UNIT_FIELD, 15, 19}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Measure unit field position with inner spaces";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/temperature-fahrenheit unit-width-full-name",
                u"unit/fahrenheit unit-width-full-name",
                NumberFormatter::with().unit(FAHRENHEIT).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
                "vi", // locale with the interesting data
                68,
                u"68 độ F");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 2},
                // Should trim leading/trailing spaces, but not inner spaces:
                {UNUM_MEASURE_UNIT_FIELD, 3, 7}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        // Data: other{"‎{0} K"} == "\u200E{0} K"
        // If that data changes, try to find another example of a non-empty unit prefix/suffix
        // that is also all ignorables (whitespace and bidi control marks).
        const char16_t* message = u"Measure unit field position with fully ignorable prefix";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/temperature-kelvin",
                u"unit/kelvin",
                NumberFormatter::with().unit(KELVIN),
                "fa", // locale with the interesting data
                68,
                u"‎۶۸ K");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 1, 3},
                {UNUM_MEASURE_UNIT_FIELD, 4, 5}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Compact field basic";
        FormattedNumber result = assertFormatSingle(
                message,
                u"compact-short",
                u"K",
                NumberFormatter::with().notation(Notation::compactShort()),
                Locale::getUS(),
                65000,
                u"65K");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 2},
                {UNUM_COMPACT_FIELD, 2, 3}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Compact field with spaces";
        FormattedNumber result = assertFormatSingle(
                message,
                u"compact-long",
                u"KK",
                NumberFormatter::with().notation(Notation::compactLong()),
                Locale::getUS(),
                65000,
                u"65 thousand");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 2},
                {UNUM_COMPACT_FIELD, 3, 11}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Compact field with inner space";
        FormattedNumber result = assertFormatSingle(
                message,
                u"compact-long",
                u"KK",
                NumberFormatter::with().notation(Notation::compactLong()),
                "fil",  // locale with interesting data
                6000,
                u"6 na libo");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 1},
                {UNUM_COMPACT_FIELD, 2, 9}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Compact field with bidi mark";
        FormattedNumber result = assertFormatSingle(
                message,
                u"compact-long",
                u"KK",
                NumberFormatter::with().notation(Notation::compactLong()),
                "he",  // locale with interesting data
                6000,
                u"\u200F6 אלף");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 1, 2},
                {UNUM_COMPACT_FIELD, 3, 6}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Compact with currency fields";
        FormattedNumber result = assertFormatSingle(
                message,
                u"compact-short currency/USD",
                u"K currency/USD",
                NumberFormatter::with().notation(Notation::compactShort()).unit(USD),
                "sr_Latn",  // locale with interesting data
                65000,
                u"65 hilj. US$");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 2},
                {UNUM_COMPACT_FIELD, 3, 8},
                {UNUM_CURRENCY_FIELD, 9, 12}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Currency long name fields";
        FormattedNumber result = assertFormatSingle(
                message,
                u"currency/USD unit-width-full-name",
                u"currency/USD unit-width-full-name",
                NumberFormatter::with().unit(USD)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
                "en",
                12345,
                u"12,345.00 US dollars");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_GROUPING_SEPARATOR_FIELD, 2, 3},
                {UNUM_INTEGER_FIELD, 0, 6},
                {UNUM_DECIMAL_SEPARATOR_FIELD, 6, 7},
                {UNUM_FRACTION_FIELD, 7, 9},
                {UNUM_CURRENCY_FIELD, 10, 20}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        const char16_t* message = u"Compact with measure unit fields";
        FormattedNumber result = assertFormatSingle(
                message,
                u"compact-long measure-unit/length-meter unit-width-full-name",
                u"KK unit/meter unit-width-full-name",
                NumberFormatter::with().notation(Notation::compactLong())
                    .unit(METER)
                    .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
                Locale::getUS(),
                65000,
                u"65 thousand meters");
        static const UFieldPosition expectedFieldPositions[] = {
                // field, begin index, end index
                {UNUM_INTEGER_FIELD, 0, 2},
                {UNUM_COMPACT_FIELD, 3, 11},
                {UNUM_MEASURE_UNIT_FIELD, 12, 18}};
        assertNumberFieldPositions(
                message,
                result,
                expectedFieldPositions,
                UPRV_LENGTHOF(expectedFieldPositions));
    }
}

void NumberFormatterApiTest::toFormat() {
    IcuTestErrorCode status(*this, "icuFormat");
    LocalizedNumberFormatter lnf = NumberFormatter::withLocale("fr")
            .precision(Precision::fixedFraction(3));
    LocalPointer<Format> format(lnf.toFormat(status), status);
    FieldPosition fpos(UNUM_DECIMAL_SEPARATOR_FIELD);
    UnicodeString sb;
    format->format(514.23, sb, fpos, status);
    assertEquals("Should correctly format number", u"514,230", sb);
    assertEquals("Should find decimal separator", 3, fpos.getBeginIndex());
    assertEquals("Should find end of decimal separator", 4, fpos.getEndIndex());
    assertEquals(
            "ICU Format should round-trip",
            lnf.toSkeleton(status),
            dynamic_cast<LocalizedNumberFormatterAsFormat*>(format.getAlias())->getNumberFormatter()
                    .toSkeleton(status));

    UFormattedNumberData result;
    result.quantity.setToDouble(514.23);
    lnf.formatImpl(&result, status);
    FieldPositionIterator fpi1;
    {
        FieldPositionIteratorHandler fpih(&fpi1, status);
        result.getAllFieldPositions(fpih, status);
    }

    FieldPositionIterator fpi2;
    format->format(514.23, sb.remove(), &fpi2, status);

    assertTrue("Should produce same field position iterator", fpi1 == fpi2);
}

void NumberFormatterApiTest::errors() {
    LocalizedNumberFormatter lnf = NumberFormatter::withLocale(Locale::getEnglish()).precision(
            Precision::fixedFraction(
                    -1));

    // formatInt
    UErrorCode status = U_ZERO_ERROR;
    FormattedNumber fn = lnf.formatInt(1, status);
    assertEquals(
            "Should fail in formatInt method with error code for rounding",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);

    // formatDouble
    status = U_ZERO_ERROR;
    fn = lnf.formatDouble(1.0, status);
    assertEquals(
            "Should fail in formatDouble method with error code for rounding",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);

    // formatDecimal (decimal error)
    status = U_ZERO_ERROR;
    fn = NumberFormatter::withLocale("en").formatDecimal("1x2", status);
    assertEquals(
            "Should fail in formatDecimal method with error code for decimal number syntax",
            U_DECIMAL_NUMBER_SYNTAX_ERROR,
            status);

    // formatDecimal (setting error)
    status = U_ZERO_ERROR;
    fn = lnf.formatDecimal("1.0", status);
    assertEquals(
            "Should fail in formatDecimal method with error code for rounding",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);

    // Skeleton string
    status = U_ZERO_ERROR;
    UnicodeString output = lnf.toSkeleton(status);
    assertEquals(
            "Should fail on toSkeleton terminal method with correct error code",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);
    assertTrue(
            "Terminal toSkeleton on error object should be bogus",
            output.isBogus());

    // FieldPosition (constrained category)
    status = U_ZERO_ERROR;
    ConstrainedFieldPosition fp;
    fp.constrainCategory(UFIELD_CATEGORY_NUMBER);
    fn.nextPosition(fp, status);
    assertEquals(
            "Should fail on FieldPosition terminal method with correct error code",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);

    // FieldPositionIterator (no constraints)
    status = U_ZERO_ERROR;
    fp.reset();
    fn.nextPosition(fp, status);
    assertEquals(
            "Should fail on FieldPositoinIterator terminal method with correct error code",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);

    // Appendable
    status = U_ZERO_ERROR;
    UnicodeStringAppendable appendable(output.remove());
    fn.appendTo(appendable, status);
    assertEquals(
            "Should fail on Appendable terminal method with correct error code",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);

    // UnicodeString
    status = U_ZERO_ERROR;
    output = fn.toString(status);
    assertEquals(
            "Should fail on UnicodeString terminal method with correct error code",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);
    assertTrue(
            "Terminal UnicodeString on error object should be bogus",
            output.isBogus());

    // CopyErrorTo
    status = U_ZERO_ERROR;
    lnf.copyErrorTo(status);
    assertEquals(
            "Should fail since rounder is not legal with correct error code",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);
}

void NumberFormatterApiTest::validRanges() {

#define EXPECTED_MAX_INT_FRAC_SIG 999

#define VALID_RANGE_ASSERT(status, method, lowerBound, argument) UPRV_BLOCK_MACRO_BEGIN { \
    UErrorCode expectedStatus = ((lowerBound <= argument) && (argument <= EXPECTED_MAX_INT_FRAC_SIG)) \
        ? U_ZERO_ERROR \
        : U_NUMBER_ARG_OUTOFBOUNDS_ERROR; \
    assertEquals( \
        UnicodeString(u"Incorrect status for " #method " on input ") \
            + Int64ToUnicodeString(argument), \
        expectedStatus, \
        status); \
} UPRV_BLOCK_MACRO_END

#define VALID_RANGE_ONEARG(setting, method, lowerBound) UPRV_BLOCK_MACRO_BEGIN { \
    for (int32_t argument = -2; argument <= EXPECTED_MAX_INT_FRAC_SIG + 2; argument++) { \
        UErrorCode status = U_ZERO_ERROR; \
        NumberFormatter::with().setting(method(argument)).copyErrorTo(status); \
        VALID_RANGE_ASSERT(status, method, lowerBound, argument); \
    } \
} UPRV_BLOCK_MACRO_END

#define VALID_RANGE_TWOARGS(setting, method, lowerBound) UPRV_BLOCK_MACRO_BEGIN { \
    for (int32_t argument = -2; argument <= EXPECTED_MAX_INT_FRAC_SIG + 2; argument++) { \
        UErrorCode status = U_ZERO_ERROR; \
        /* Pass EXPECTED_MAX_INT_FRAC_SIG as the second argument so arg1 <= arg2 in expected cases */ \
        NumberFormatter::with().setting(method(argument, EXPECTED_MAX_INT_FRAC_SIG)).copyErrorTo(status); \
        VALID_RANGE_ASSERT(status, method, lowerBound, argument); \
        status = U_ZERO_ERROR; \
        /* Pass lowerBound as the first argument so arg1 <= arg2 in expected cases */ \
        NumberFormatter::with().setting(method(lowerBound, argument)).copyErrorTo(status); \
        VALID_RANGE_ASSERT(status, method, lowerBound, argument); \
        /* Check that first argument must be less than or equal to second argument */ \
        NumberFormatter::with().setting(method(argument, argument - 1)).copyErrorTo(status); \
        assertEquals("Incorrect status for " #method " on max < min input", \
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR, \
            status); \
    } \
} UPRV_BLOCK_MACRO_END

    VALID_RANGE_ONEARG(precision, Precision::fixedFraction, 0);
    VALID_RANGE_ONEARG(precision, Precision::minFraction, 0);
    VALID_RANGE_ONEARG(precision, Precision::maxFraction, 0);
    VALID_RANGE_TWOARGS(precision, Precision::minMaxFraction, 0);
    VALID_RANGE_ONEARG(precision, Precision::fixedSignificantDigits, 1);
    VALID_RANGE_ONEARG(precision, Precision::minSignificantDigits, 1);
    VALID_RANGE_ONEARG(precision, Precision::maxSignificantDigits, 1);
    VALID_RANGE_TWOARGS(precision, Precision::minMaxSignificantDigits, 1);
    VALID_RANGE_ONEARG(precision, Precision::fixedFraction(1).withMinDigits, 1);
    VALID_RANGE_ONEARG(precision, Precision::fixedFraction(1).withMaxDigits, 1);
    VALID_RANGE_ONEARG(notation, Notation::scientific().withMinExponentDigits, 1);
    VALID_RANGE_ONEARG(integerWidth, IntegerWidth::zeroFillTo, 0);
    VALID_RANGE_ONEARG(integerWidth, IntegerWidth::zeroFillTo(0).truncateAt, -1);
}

void NumberFormatterApiTest::copyMove() {
    IcuTestErrorCode status(*this, "copyMove");

    // Default constructors
    LocalizedNumberFormatter l1;
    assertEquals("Initial behavior", u"10", l1.formatInt(10, status).toString(status), true);
    if (status.errDataIfFailureAndReset()) { return; }
    assertEquals("Initial call count", 1, l1.getCallCount());
    assertTrue("Initial compiled", l1.getCompiled() == nullptr);

    // Setup
    l1 = NumberFormatter::withLocale("en").unit(NoUnit::percent()).threshold(3);
    assertEquals("Initial behavior", u"10%", l1.formatInt(10, status).toString(status));
    assertEquals("Initial call count", 1, l1.getCallCount());
    assertTrue("Initial compiled", l1.getCompiled() == nullptr);
    l1.formatInt(123, status);
    assertEquals("Still not compiled", 2, l1.getCallCount());
    assertTrue("Still not compiled", l1.getCompiled() == nullptr);
    l1.formatInt(123, status);
    assertEquals("Compiled", u"10%", l1.formatInt(10, status).toString(status));
    assertEquals("Compiled", INT32_MIN, l1.getCallCount());
    assertTrue("Compiled", l1.getCompiled() != nullptr);

    // Copy constructor
    LocalizedNumberFormatter l2 = l1;
    assertEquals("[constructor] Copy behavior", u"10%", l2.formatInt(10, status).toString(status));
    assertEquals("[constructor] Copy should not have compiled state", 1, l2.getCallCount());
    assertTrue("[constructor] Copy should not have compiled state", l2.getCompiled() == nullptr);

    // Move constructor
    LocalizedNumberFormatter l3 = std::move(l1);
    assertEquals("[constructor] Move behavior", u"10%", l3.formatInt(10, status).toString(status));
    assertEquals("[constructor] Move *should* have compiled state", INT32_MIN, l3.getCallCount());
    assertTrue("[constructor] Move *should* have compiled state", l3.getCompiled() != nullptr);
    assertEquals("[constructor] Source should be reset after move", 0, l1.getCallCount());
    assertTrue("[constructor] Source should be reset after move", l1.getCompiled() == nullptr);

    // Reset l1 and l2 to check for macro-props copying for behavior testing
    // Make the test more interesting: also warm them up with a compiled formatter.
    l1 = NumberFormatter::withLocale("en");
    l1.formatInt(1, status);
    l1.formatInt(1, status);
    l1.formatInt(1, status);
    l2 = NumberFormatter::withLocale("en");
    l2.formatInt(1, status);
    l2.formatInt(1, status);
    l2.formatInt(1, status);

    // Copy assignment
    l1 = l3;
    assertEquals("[assignment] Copy behavior", u"10%", l1.formatInt(10, status).toString(status));
    assertEquals("[assignment] Copy should not have compiled state", 1, l1.getCallCount());
    assertTrue("[assignment] Copy should not have compiled state", l1.getCompiled() == nullptr);

    // Move assignment
    l2 = std::move(l3);
    assertEquals("[assignment] Move behavior", u"10%", l2.formatInt(10, status).toString(status));
    assertEquals("[assignment] Move *should* have compiled state", INT32_MIN, l2.getCallCount());
    assertTrue("[assignment] Move *should* have compiled state", l2.getCompiled() != nullptr);
    assertEquals("[assignment] Source should be reset after move", 0, l3.getCallCount());
    assertTrue("[assignment] Source should be reset after move", l3.getCompiled() == nullptr);

    // Coverage tests for UnlocalizedNumberFormatter
    UnlocalizedNumberFormatter u1;
    assertEquals("Default behavior", u"10", u1.locale("en").formatInt(10, status).toString(status));
    u1 = u1.unit(NoUnit::percent());
    assertEquals("Copy assignment", u"10%", u1.locale("en").formatInt(10, status).toString(status));
    UnlocalizedNumberFormatter u2 = u1;
    assertEquals("Copy constructor", u"10%", u2.locale("en").formatInt(10, status).toString(status));
    UnlocalizedNumberFormatter u3 = std::move(u1);
    assertEquals("Move constructor", u"10%", u3.locale("en").formatInt(10, status).toString(status));
    u1 = NumberFormatter::with();
    u1 = std::move(u2);
    assertEquals("Move assignment", u"10%", u1.locale("en").formatInt(10, status).toString(status));

    // FormattedNumber move operators
    FormattedNumber result = l1.formatInt(10, status);
    assertEquals("FormattedNumber move constructor", u"10%", result.toString(status));
    result = l1.formatInt(20, status);
    assertEquals("FormattedNumber move assignment", u"20%", result.toString(status));
}

void NumberFormatterApiTest::localPointerCAPI() {
    // NOTE: This is also the sample code in unumberformatter.h
    UErrorCode ec = U_ZERO_ERROR;

    // Setup:
    LocalUNumberFormatterPointer uformatter(unumf_openForSkeletonAndLocale(u"percent", -1, "en", &ec));
    LocalUFormattedNumberPointer uresult(unumf_openResult(&ec));
    if (!assertSuccess("", ec, true, __FILE__, __LINE__)) { return; }

    // Format a decimal number:
    unumf_formatDecimal(uformatter.getAlias(), "9.87E-3", -1, uresult.getAlias(), &ec);
    if (!assertSuccess("", ec, true, __FILE__, __LINE__)) { return; }

    // Get the location of the percent sign:
    UFieldPosition ufpos = {UNUM_PERCENT_FIELD, 0, 0};
    unumf_resultNextFieldPosition(uresult.getAlias(), &ufpos, &ec);
    assertEquals("Percent sign location within '0.00987%'", 7, ufpos.beginIndex);
    assertEquals("Percent sign location within '0.00987%'", 8, ufpos.endIndex);

    // No need to do any cleanup since we are using LocalPointer.
}

void NumberFormatterApiTest::toObject() {
    IcuTestErrorCode status(*this, "toObject");

    // const lvalue version
    {
        LocalizedNumberFormatter lnf = NumberFormatter::withLocale("en")
                .precision(Precision::fixedFraction(2));
        LocalPointer<LocalizedNumberFormatter> lnf2(lnf.clone());
        assertFalse("should create successfully, const lvalue", lnf2.isNull());
        assertEquals("object API test, const lvalue", u"1,000.00",
            lnf2->formatDouble(1000, status).toString(status));
    }

    // rvalue reference version
    {
        LocalPointer<LocalizedNumberFormatter> lnf(
            NumberFormatter::withLocale("en")
                .precision(Precision::fixedFraction(2))
                .clone());
        assertFalse("should create successfully, rvalue reference", lnf.isNull());
        assertEquals("object API test, rvalue reference", u"1,000.00",
            lnf->formatDouble(1000, status).toString(status));
    }

    // to std::unique_ptr via constructor
    {
        std::unique_ptr<LocalizedNumberFormatter> lnf(
            NumberFormatter::withLocale("en")
                .precision(Precision::fixedFraction(2))
                .clone());
        assertTrue("should create successfully, unique_ptr", static_cast<bool>(lnf));
        assertEquals("object API test, unique_ptr", u"1,000.00",
            lnf->formatDouble(1000, status).toString(status));
    }

    // to std::unique_ptr via assignment
    {
        std::unique_ptr<LocalizedNumberFormatter> lnf =
            NumberFormatter::withLocale("en")
                .precision(Precision::fixedFraction(2))
                .clone();
        assertTrue("should create successfully, unique_ptr B", static_cast<bool>(lnf));
        assertEquals("object API test, unique_ptr B", u"1,000.00",
            lnf->formatDouble(1000, status).toString(status));
    }

    // to LocalPointer via assignment
    {
        LocalPointer<UnlocalizedNumberFormatter> f =
            NumberFormatter::with().clone();
    }

    // make sure no memory leaks
    {
        NumberFormatter::with().clone();
    }
}

void NumberFormatterApiTest::toDecimalNumber() {
    IcuTestErrorCode status(*this, "toDecimalNumber");
    FormattedNumber fn = NumberFormatter::withLocale("bn-BD")
        .scale(Scale::powerOfTen(2))
        .precision(Precision::maxSignificantDigits(5))
        .formatDouble(9.87654321e12, status);
    assertEquals("Should have expected localized string result",
        u"৯৮,৭৬,৫০,০০,০০,০০,০০০", fn.toString(status));
    assertEquals(u"Should have expected toDecimalNumber string result",
        "9.8765E+14", fn.toDecimalNumber<std::string>(status).c_str());

    fn = NumberFormatter::withLocale("bn-BD").formatDouble(0, status);
    assertEquals("Should have expected localized string result",
        u"০", fn.toString(status));
    assertEquals(u"Should have expected toDecimalNumber string result",
        "0", fn.toDecimalNumber<std::string>(status).c_str());
}

void NumberFormatterApiTest::microPropsInternals() {
    // Verify copy construction and assignment operators.
    int64_t testValues[2] = {4, 61};

    MicroProps mp;
    assertEquals("capacity", 2, mp.mixedMeasures.getCapacity());
    mp.mixedMeasures[0] = testValues[0];
    mp.mixedMeasures[1] = testValues[1];
    MicroProps copyConstructed(mp);
    MicroProps copyAssigned;
    int64_t *resizeResult = mp.mixedMeasures.resize(4, 4);
    assertTrue("Resize success", resizeResult != nullptr);
    copyAssigned = mp;

    assertTrue("MicroProps success status", U_SUCCESS(mp.mixedMeasures.status));
    assertTrue("Copy Constructed success status", U_SUCCESS(copyConstructed.mixedMeasures.status));
    assertTrue("Copy Assigned success status", U_SUCCESS(copyAssigned.mixedMeasures.status));
    assertEquals("Original values[0]", testValues[0], mp.mixedMeasures[0]);
    assertEquals("Original values[1]", testValues[1], mp.mixedMeasures[1]);
    assertEquals("Copy Constructed[0]", testValues[0], copyConstructed.mixedMeasures[0]);
    assertEquals("Copy Constructed[1]", testValues[1], copyConstructed.mixedMeasures[1]);
    assertEquals("Copy Assigned[0]", testValues[0], copyAssigned.mixedMeasures[0]);
    assertEquals("Copy Assigned[1]", testValues[1], copyAssigned.mixedMeasures[1]);
    assertEquals("Original capacity", 4, mp.mixedMeasures.getCapacity());
    assertEquals("Copy Constructed capacity", 2, copyConstructed.mixedMeasures.getCapacity());
    assertEquals("Copy Assigned capacity", 4, copyAssigned.mixedMeasures.getCapacity());
}

void NumberFormatterApiTest::formatUnitsAliases() {
    IcuTestErrorCode status(*this, "formatUnitsAliases");

    struct TestCase {
        const MeasureUnit measureUnit;
        const UnicodeString expectedFormat;
    } testCases[]{
        // Aliases
        {MeasureUnit::getMilligramPerDeciliter(), u"2 milligrams per deciliter"},
        {MeasureUnit::getLiterPer100Kilometers(), u"2 liters per 100 kilometers"},
        {MeasureUnit::getPartPerMillion(), u"2 parts per million"},
        {MeasureUnit::getMillimeterOfMercury(), u"2 millimeters of mercury"},

        // Replacements
        {MeasureUnit::getMilligramOfglucosePerDeciliter(), u"2 milligrams per deciliter"},
        {MeasureUnit::forIdentifier("millimeter-ofhg", status), u"2 millimeters of mercury"},
        {MeasureUnit::forIdentifier("liter-per-100-kilometer", status), u"2 liters per 100 kilometers"},
        {MeasureUnit::forIdentifier("permillion", status), u"2 parts per million"},
    };

    for (const auto &testCase : testCases) {
        UnicodeString actualFormat = NumberFormatter::withLocale(icu::Locale::getEnglish())
                                         .unit(testCase.measureUnit)
                                         .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME)
                                         .formatDouble(2.0, status)
                                         .toString(status);

        assertEquals("test unit aliases", testCase.expectedFormat, actualFormat);
    }
}

void NumberFormatterApiTest::testIssue22378() {
    IcuTestErrorCode status(*this, "testIssue22378");

    // I checked the results before the fix and everything works the same except
    // "fr-FR-u-mu-fahrenhe" and "fr_FR@mu=fahrenhe"
    struct TestCase {
        const std::string localeId;
        const UnicodeString expectedFormat;
    } testCases[]{
        {"en-US", u"73\u00B0F"},
        {"en-US-u-mu-fahrenhe", u"73\u00B0F"},
        // Unlike ULocale, forLanguageTag fails wih U_ILLEGAL_ARGUMENT_ERROR
        // because fahrenheit is not valid value for -u-mu-
        // {"en-US-u-mu-fahrenheit", u"73\u00B0F"},
        {"en-US-u-mu-celsius", u"23\u00B0C"},
        {"en-US-u-mu-badvalue", u"73\u00B0F"},
        {"en_US@mu=fahrenhe", u"73\u00B0F"},
        {"en_US@mu=fahrenheit", u"73\u00B0F"},
        {"en_US@mu=celsius", u"23\u00B0C"},
        {"en_US@mu=badvalue", u"73\u00B0F"},

        {"fr-FR", u"23\u202F\u00B0C"},
        {"fr-FR-u-mu-fahrenhe", u"73\u202F\u00B0F"},
        // Unlike ULocale, forLanguageTag fails wih U_ILLEGAL_ARGUMENT_ERROR
        // because fahrenheit is not valid value for -u-mu-
        // {"fr-FR-u-mu-fahrenheit", u"23\u202F\u00B0C"},
        {"fr-FR-u-mu-celsius", u"23\u202F\u00B0C"},
        {"fr-FR-u-mu-badvalue", u"23\u202F\u00B0C"},
        {"fr_FR@mu=fahrenhe", u"73\u202F\u00B0F"},
        {"fr_FR@mu=fahrenheit", u"73\u202F\u00B0F"},
        {"fr_FR@mu=celsius", u"23\u202F\u00B0C"},
        {"fr_FR@mu=badvalue", u"23\u202F\u00B0C"},
    };

    UnlocalizedNumberFormatter formatter = NumberFormatter::with()
            .usage("weather")
            .unit(MeasureUnit::getCelsius());
    double value = 23.0;

    for (const auto &testCase : testCases) {
        std::string localeId = testCase.localeId;
        const Locale locale = (localeId.find("@") != std::string::npos)
                ? Locale(localeId.c_str())
                : Locale::forLanguageTag(localeId, status);
        UnicodeString actualFormat = formatter.locale(locale)
                .formatDouble(value, status)
                .toString(status);
        assertEquals(u"-u-mu- honored (" + UnicodeString(localeId.c_str()) + u")",
                testCase.expectedFormat, actualFormat);
    }

    UnicodeString result = formatter.locale("en-US").formatDouble(value, status).getOutputUnit(status).getIdentifier();
    assertEquals("Testing default -u-mu- for en-US", MeasureUnit::getFahrenheit().getIdentifier(), result);
    result = formatter.locale("fr-FR").formatDouble(value, status).getOutputUnit(status).getIdentifier();
    assertEquals("Testing default -u-mu- for fr-FR", MeasureUnit::getCelsius().getIdentifier(), result);
}

/* For skeleton comparisons: this checks the toSkeleton output for `f` and for
 * `conciseSkeleton` against the normalized version of `uskeleton` - this does
 * not round-trip uskeleton itself.
 *
 * If `conciseSkeleton` starts with a "~", its round-trip check is skipped.
 *
 * If `uskeleton` is nullptr, toSkeleton is expected to return an
 * U_UNSUPPORTED_ERROR.
 */
void NumberFormatterApiTest::assertFormatDescending(
        const char16_t* umessage,
        const char16_t* uskeleton,
        const char16_t* conciseSkeleton,
        const UnlocalizedNumberFormatter& f,
        Locale locale,
        ...) {
    va_list args;
    va_start(args, locale);
    UnicodeString message(true, umessage, -1);
    static double inputs[] = {87650, 8765, 876.5, 87.65, 8.765, 0.8765, 0.08765, 0.008765, 0};
    const LocalizedNumberFormatter l1 = f.threshold(0).locale(locale); // no self-regulation
    const LocalizedNumberFormatter l2 = f.threshold(1).locale(locale); // all self-regulation
    IcuTestErrorCode status(*this, "assertFormatDescending");
    status.setScope(message);
    UnicodeString expecteds[10];
    for (int16_t i = 0; i < 9; i++) {
        char16_t caseNumber = u'0' + i;
        double d = inputs[i];
        UnicodeString expected = va_arg(args, const char16_t*);
        expecteds[i] = expected;
        UnicodeString actual1 = l1.formatDouble(d, status).toString(status);
        assertSuccess(message + u": Unsafe Path: " + caseNumber, status);
        assertEquals(message + u": Unsafe Path: " + caseNumber, expected, actual1);
        UnicodeString actual2 = l2.formatDouble(d, status).toString(status);
        assertSuccess(message + u": Safe Path: " + caseNumber, status);
        assertEquals(message + u": Safe Path: " + caseNumber, expected, actual2);
    }
    if (uskeleton != nullptr) { // if null, skeleton is declared as undefined.
        UnicodeString skeleton(true, uskeleton, -1);
        // Only compare normalized skeletons: the tests need not provide the normalized forms.
        // Use the normalized form to construct the testing formatter to guarantee no loss of info.
        UnicodeString normalized = NumberFormatter::forSkeleton(skeleton, status).toSkeleton(status);
        assertEquals(message + ": Skeleton:", normalized, f.toSkeleton(status));
        LocalizedNumberFormatter l3 = NumberFormatter::forSkeleton(normalized, status).locale(locale);
        for (int32_t i = 0; i < 9; i++) {
            double d = inputs[i];
            UnicodeString actual3 = l3.formatDouble(d, status).toString(status);
            assertEquals(message + ": Skeleton Path: '" + normalized + "': " + d, expecteds[i], actual3);
        }
        // Concise skeletons should have same output, and usually round-trip to the normalized skeleton.
        // If the concise skeleton starts with '~', disable the round-trip check.
        bool shouldRoundTrip = true;
        if (conciseSkeleton[0] == u'~') {
            conciseSkeleton++;
            shouldRoundTrip = false;
        }
        LocalizedNumberFormatter l4 = NumberFormatter::forSkeleton(conciseSkeleton, status).locale(locale);
        if (shouldRoundTrip) {
            assertEquals(message + ": Concise Skeleton:", normalized, l4.toSkeleton(status));
        }
        for (int32_t i = 0; i < 9; i++) {
            double d = inputs[i];
            UnicodeString actual4 = l4.formatDouble(d, status).toString(status);
            assertEquals(message + ": Concise Skeleton Path: '" + normalized + "': " + d, expecteds[i], actual4);
        }
    } else {
        assertUndefinedSkeleton(f);
    }
}

/* For skeleton comparisons: this checks the toSkeleton output for `f` and for
 * `conciseSkeleton` against the normalized version of `uskeleton` - this does
 * not round-trip uskeleton itself.
 *
 * If `conciseSkeleton` starts with a "~", its round-trip check is skipped.
 *
 * If `uskeleton` is nullptr, toSkeleton is expected to return an
 * U_UNSUPPORTED_ERROR.
 */
void NumberFormatterApiTest::assertFormatDescendingBig(
        const char16_t* umessage,
        const char16_t* uskeleton,
        const char16_t* conciseSkeleton,
        const UnlocalizedNumberFormatter& f,
        Locale locale,
        ...) {
    va_list args;
    va_start(args, locale);
    UnicodeString message(true, umessage, -1);
    static double inputs[] = {87650000, 8765000, 876500, 87650, 8765, 876.5, 87.65, 8.765, 0};
    const LocalizedNumberFormatter l1 = f.threshold(0).locale(locale); // no self-regulation
    const LocalizedNumberFormatter l2 = f.threshold(1).locale(locale); // all self-regulation
    IcuTestErrorCode status(*this, "assertFormatDescendingBig");
    status.setScope(message);
    UnicodeString expecteds[10];
    for (int16_t i = 0; i < 9; i++) {
        char16_t caseNumber = u'0' + i;
        double d = inputs[i];
        UnicodeString expected = va_arg(args, const char16_t*);
        expecteds[i] = expected;
        UnicodeString actual1 = l1.formatDouble(d, status).toString(status);
        assertSuccess(message + u": Unsafe Path: " + caseNumber, status);
        assertEquals(message + u": Unsafe Path: " + caseNumber, expected, actual1);
        UnicodeString actual2 = l2.formatDouble(d, status).toString(status);
        assertSuccess(message + u": Safe Path: " + caseNumber, status);
        assertEquals(message + u": Safe Path: " + caseNumber, expected, actual2);
    }
    if (uskeleton != nullptr) { // if null, skeleton is declared as undefined.
        UnicodeString skeleton(true, uskeleton, -1);
        // Only compare normalized skeletons: the tests need not provide the normalized forms.
        // Use the normalized form to construct the testing formatter to guarantee no loss of info.
        UnicodeString normalized = NumberFormatter::forSkeleton(skeleton, status).toSkeleton(status);
        assertEquals(message + ": Skeleton:", normalized, f.toSkeleton(status));
        LocalizedNumberFormatter l3 = NumberFormatter::forSkeleton(normalized, status).locale(locale);
        for (int32_t i = 0; i < 9; i++) {
            double d = inputs[i];
            UnicodeString actual3 = l3.formatDouble(d, status).toString(status);
            assertEquals(message + ": Skeleton Path: '" + normalized + "': " + d, expecteds[i], actual3);
        }
        // Concise skeletons should have same output, and usually round-trip to the normalized skeleton.
        // If the concise skeleton starts with '~', disable the round-trip check.
        bool shouldRoundTrip = true;
        if (conciseSkeleton[0] == u'~') {
            conciseSkeleton++;
            shouldRoundTrip = false;
        }
        LocalizedNumberFormatter l4 = NumberFormatter::forSkeleton(conciseSkeleton, status).locale(locale);
        if (shouldRoundTrip) {
            assertEquals(message + ": Concise Skeleton:", normalized, l4.toSkeleton(status));
        }
        for (int32_t i = 0; i < 9; i++) {
            double d = inputs[i];
            UnicodeString actual4 = l4.formatDouble(d, status).toString(status);
            assertEquals(message + ": Concise Skeleton Path: '" + normalized + "': " + d, expecteds[i], actual4);
        }
    } else {
        assertUndefinedSkeleton(f);
    }
}

/* For skeleton comparisons: this checks the toSkeleton output for `f` and for
 * `conciseSkeleton` against the normalized version of `uskeleton` - this does
 * not round-trip uskeleton itself.
 *
 * If `conciseSkeleton` starts with a "~", its round-trip check is skipped.
 *
 * If `uskeleton` is nullptr, toSkeleton is expected to return an
 * U_UNSUPPORTED_ERROR.
 */
FormattedNumber
NumberFormatterApiTest::assertFormatSingle(
        const char16_t* umessage,
        const char16_t* uskeleton,
        const char16_t* conciseSkeleton,
        const UnlocalizedNumberFormatter& f,
        Locale locale,
        double input,
        const UnicodeString& expected) {
    UnicodeString message(true, umessage, -1);
    const LocalizedNumberFormatter l1 = f.threshold(0).locale(locale); // no self-regulation
    const LocalizedNumberFormatter l2 = f.threshold(1).locale(locale); // all self-regulation
    IcuTestErrorCode status(*this, "assertFormatSingle");
    status.setScope(message);
    FormattedNumber result1 = l1.formatDouble(input, status);
    UnicodeString actual1 = result1.toString(status);
    assertSuccess(message + u": Unsafe Path", status);
    assertEquals(message + u": Unsafe Path", expected, actual1);
    UnicodeString actual2 = l2.formatDouble(input, status).toString(status);
    assertSuccess(message + u": Safe Path", status);
    assertEquals(message + u": Safe Path", expected, actual2);
    if (uskeleton != nullptr) { // if null, skeleton is declared as undefined.
        UnicodeString skeleton(true, uskeleton, -1);
        // Only compare normalized skeletons: the tests need not provide the normalized forms.
        // Use the normalized form to construct the testing formatter to ensure no loss of info.
        UnicodeString normalized = NumberFormatter::forSkeleton(skeleton, status).toSkeleton(status);
        assertEquals(message + ": Skeleton", normalized, f.toSkeleton(status));
        LocalizedNumberFormatter l3 = NumberFormatter::forSkeleton(normalized, status).locale(locale);
        UnicodeString actual3 = l3.formatDouble(input, status).toString(status);
        assertEquals(message + ": Skeleton Path: '" + normalized + "': " + input, expected, actual3);
        // Concise skeletons should have same output, and usually round-trip to the normalized skeleton.
        // If the concise skeleton starts with '~', disable the round-trip check.
        bool shouldRoundTrip = true;
        if (conciseSkeleton[0] == u'~') {
            conciseSkeleton++;
            shouldRoundTrip = false;
        }
        LocalizedNumberFormatter l4 = NumberFormatter::forSkeleton(conciseSkeleton, status).locale(locale);
        if (shouldRoundTrip) {
            assertEquals(message + ": Concise Skeleton:", normalized, l4.toSkeleton(status));
        }
        UnicodeString actual4 = l4.formatDouble(input, status).toString(status);
        assertEquals(message + ": Concise Skeleton Path: '" + normalized + "': " + input, expected, actual4);
    } else {
        assertUndefinedSkeleton(f);
    }
    return result1;
}

void NumberFormatterApiTest::assertUndefinedSkeleton(const UnlocalizedNumberFormatter& f) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString skeleton = f.toSkeleton(status);
    assertEquals(
            u"Expect toSkeleton to fail, but passed, producing: " + skeleton,
            U_UNSUPPORTED_ERROR,
            status);
}

void NumberFormatterApiTest::assertNumberFieldPositions(
        const char16_t* message,
        const FormattedNumber& formattedNumber,
        const UFieldPosition* expectedFieldPositions,
        int32_t length) {
    IcuTestErrorCode status(*this, "assertNumberFieldPositions");

    // Check FormattedValue functions
    checkFormattedValue(
        message,
        static_cast<const FormattedValue&>(formattedNumber),
        formattedNumber.toString(status),
        UFIELD_CATEGORY_NUMBER,
        expectedFieldPositions,
        length);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
