// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "charstr.h"
#include <cstdarg>
#include <cmath>
#include <memory>
#include "unicode/unum.h"
#include "unicode/numberformatter.h"
#include "number_asformat.h"
#include "number_types.h"
#include "number_utils.h"
#include "numbertest.h"
#include "unicode/utypes.h"

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

    DAY = *LocalPointer<MeasureUnit>(MeasureUnit::createDay(status));
    SQUARE_METER = *LocalPointer<MeasureUnit>(MeasureUnit::createSquareMeter(status));
    FAHRENHEIT = *LocalPointer<MeasureUnit>(MeasureUnit::createFahrenheit(status));
    SECOND = *LocalPointer<MeasureUnit>(MeasureUnit::createSecond(status));
    POUND = *LocalPointer<MeasureUnit>(MeasureUnit::createPound(status));
    SQUARE_MILE = *LocalPointer<MeasureUnit>(MeasureUnit::createSquareMile(status));
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
        TESTCASE_AUTO(unitCurrency);
        TESTCASE_AUTO(unitPercent);
        if (!quick) {
            // Slow test: run in exhaustive mode only
            TESTCASE_AUTO(percentParity);
        }
        TESTCASE_AUTO(roundingFraction);
        TESTCASE_AUTO(roundingFigures);
        TESTCASE_AUTO(roundingFractionFigures);
        TESTCASE_AUTO(roundingOther);
        TESTCASE_AUTO(grouping);
        TESTCASE_AUTO(padding);
        TESTCASE_AUTO(integerWidth);
        TESTCASE_AUTO(symbols);
        // TODO: Add this method if currency symbols override support is added.
        //TESTCASE_AUTO(symbolsOverride);
        TESTCASE_AUTO(sign);
        TESTCASE_AUTO(decimal);
        TESTCASE_AUTO(scale);
        TESTCASE_AUTO(locale);
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
    TESTCASE_AUTO_END;
}

void NumberFormatterApiTest::notationSimple() {
    assertFormatDescending(
            u"Basic",
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
            NumberFormatter::with(),
            Locale::getEnglish(),
            -9876543.21,
            u"-9,876,543.21");
}


void NumberFormatterApiTest::notationScientific() {
    assertFormatDescending(
            u"Scientific",
            u"scientific",
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
            u"scientific/+ee",
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
            NumberFormatter::with().notation(Notation::scientific()),
            Locale::getEnglish(),
            -1000000,
            u"-1E6");
}

void NumberFormatterApiTest::notationCompact() {
    assertFormatDescending(
            u"Compact Short",
            u"compact-short",
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
            NumberFormatter::with().notation(Notation::compactLong()),
            Locale::createFromName("es"),
            1000000,
            u"1 millón");

    assertFormatSingle(
            u"Compact Plural Other",
            u"compact-long",
            NumberFormatter::with().notation(Notation::compactLong()),
            Locale::createFromName("es"),
            2000000,
            u"2 millones");

    assertFormatSingle(
            u"Compact with Negative Sign",
            u"compact-short",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            -9876543.21,
            u"-9.9M");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            990000,
            u"990K");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            999000,
            u"999K");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            999900,
            u"1M");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            9900000,
            u"9.9M");

    assertFormatSingle(
            u"Compact Rounding",
            u"compact-short",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale::getEnglish(),
            9990000,
            u"10M");

    assertFormatSingle(
            u"Compact in zh-Hant-HK",
            u"compact-short",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale("zh-Hant-HK"),
            1e7,
            u"10M");

    assertFormatSingle(
            u"Compact in zh-Hant",
            u"compact-short",
            NumberFormatter::with().notation(Notation::compactShort()),
            Locale("zh-Hant"),
            1e7,
            u"1000\u842C");

    // NOTE: There is no API for compact custom data in C++
    // and thus no "Compact Somali No Figure" test
}

void NumberFormatterApiTest::unitMeasure() {
    assertFormatDescending(
            u"Meters Short and unit() method",
            u"measure-unit/length-meter",
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
            NumberFormatter::with().unit(METER),
            Locale::getEnglish(),
            -9876543.21,
            u"-9,876,543.21 m");

    // The locale string "सान" appears only in brx.txt:
    assertFormatSingle(
            u"Interesting Data Fallback 1",
            u"measure-unit/duration-day unit-width-full-name",
            NumberFormatter::with().unit(DAY).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::createFromName("brx"),
            5.43,
            u"5.43 सान");

    // Requires following the alias from unitsNarrow to unitsShort:
    assertFormatSingle(
            u"Interesting Data Fallback 2",
            u"measure-unit/duration-day unit-width-narrow",
            NumberFormatter::with().unit(DAY).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW),
            Locale::createFromName("brx"),
            5.43,
            u"5.43 d");

    // en_001.txt has a unitsNarrow/area/square-meter table, but table does not contain the OTHER unit,
    // requiring fallback to the root.
    assertFormatSingle(
            u"Interesting Data Fallback 3",
            u"measure-unit/area-square-meter unit-width-narrow",
            NumberFormatter::with().unit(SQUARE_METER).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW),
            Locale::createFromName("en-GB"),
            5.43,
            u"5.43 m²");

    // Try accessing a narrow unit directly from root.
    assertFormatSingle(
            u"Interesting Data Fallback 4",
            u"measure-unit/area-square-meter unit-width-narrow",
            NumberFormatter::with().unit(SQUARE_METER).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW),
            Locale::createFromName("root"),
            5.43,
            u"5.43 m²");

    // es_US has "{0}°" for unitsNarrow/temperature/FAHRENHEIT.
    // NOTE: This example is in the documentation.
    assertFormatSingle(
            u"Difference between Narrow and Short (Narrow Version)",
            u"measure-unit/temperature-fahrenheit unit-width-narrow",
            NumberFormatter::with().unit(FAHRENHEIT).unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale("es-US"),
            5.43,
            u"5.43°");

    assertFormatSingle(
            u"Difference between Narrow and Short (Short Version)",
            u"measure-unit/temperature-fahrenheit unit-width-short",
            NumberFormatter::with().unit(FAHRENHEIT).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("es-US"),
            5.43,
            u"5.43 °F");

    assertFormatSingle(
            u"MeasureUnit form without {0} in CLDR pattern",
            u"measure-unit/temperature-kelvin unit-width-full-name",
            NumberFormatter::with().unit(KELVIN).unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("es-MX"),
            1,
            u"kelvin");

    assertFormatSingle(
            u"MeasureUnit form without {0} in CLDR pattern and wide base form",
            u"measure-unit/temperature-kelvin .00000000000000000000 unit-width-full-name",
            NumberFormatter::with().precision(Precision::fixedFraction(20))
                    .unit(KELVIN)
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("es-MX"),
            1,
            u"kelvin");

    assertFormatSingle(
            u"Person unit not in short form",
            u"measure-unit/duration-year-person unit-width-full-name",
            NumberFormatter::with().unit(MeasureUnit::getYearPerson())
                    .unitWidth(UNumberUnitWidth::UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("es-MX"),
            5,
            u"5 a\u00F1os");
}

void NumberFormatterApiTest::unitCompoundMeasure() {
    assertFormatDescending(
            u"Meters Per Second Short (unit that simplifies) and perUnit method",
            u"measure-unit/length-meter per-measure-unit/duration-second",
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
            u"Pounds Per Square Mile Short (secondary unit has per-format) and adoptPerUnit method",
            u"measure-unit/mass-pound per-measure-unit/area-square-mile",
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
}

void NumberFormatterApiTest::unitCurrency() {
    assertFormatDescending(
            u"Currency",
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
            NumberFormatter::with().unit(GBP),
            Locale::getEnglish(),
            -9876543.21,
            u"-£9,876,543.21");

    // The full currency symbol is not shown in NARROW format.
    // NOTE: This example is in the documentation.
    assertFormatSingle(
            u"Currency Difference between Narrow and Short (Narrow Version)",
            u"currency/USD unit-width-narrow",
            NumberFormatter::with().unit(USD).unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale("en-CA"),
            5.43,
            u"$5.43");

    assertFormatSingle(
            u"Currency Difference between Narrow and Short (Short Version)",
            u"currency/USD unit-width-short",
            NumberFormatter::with().unit(USD).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("en-CA"),
            5.43,
            u"US$5.43");

    assertFormatSingle(
            u"Currency-dependent format (Control)",
            u"currency/USD unit-width-short",
            NumberFormatter::with().unit(USD).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("ca"),
            444444.55,
            u"444.444,55 USD");

    assertFormatSingle(
            u"Currency-dependent format (Test)",
            u"currency/ESP unit-width-short",
            NumberFormatter::with().unit(ESP).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("ca"),
            444444.55,
            u"₧ 444.445");

    assertFormatSingle(
            u"Currency-dependent symbols (Control)",
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
            NumberFormatter::with().unit(PTE).unitWidth(UNUM_UNIT_WIDTH_SHORT),
            Locale("pt-PT"),
            444444.55,
            u"444,444$55 \u200B");

    assertFormatSingle(
            u"Currency-dependent symbols (Test Narrow)",
            u"currency/PTE unit-width-narrow",
            NumberFormatter::with().unit(PTE).unitWidth(UNUM_UNIT_WIDTH_NARROW),
            Locale("pt-PT"),
            444444.55,
            u"444,444$55 \u200B");

    assertFormatSingle(
            u"Currency-dependent symbols (Test ISO Code)",
            u"currency/PTE unit-width-iso-code",
            NumberFormatter::with().unit(PTE).unitWidth(UNUM_UNIT_WIDTH_ISO_CODE),
            Locale("pt-PT"),
            444444.55,
            u"444,444$55 PTE");

    assertFormatSingle(
            u"Plural form depending on visible digits (ICU-20499)",
            u"currency/RON unit-width-full-name",
            NumberFormatter::with().unit(RON).unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale("ro-RO"),
            24,
            u"24,00 lei românești");
}

void NumberFormatterApiTest::unitPercent() {
    assertFormatDescending(
            u"Percent",
            u"percent",
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
            NumberFormatter::with().unit(NoUnit::base()),
            Locale::getEnglish(),
            51423,
            u"51,423");

    assertFormatSingle(
            u"Percent with Negative Sign",
            u"percent",
            NumberFormatter::with().unit(NoUnit::percent()),
            Locale::getEnglish(),
            -98.7654321,
            u"-98.765432%");
}

void NumberFormatterApiTest::percentParity() {
    IcuTestErrorCode status(*this, "percentParity");
    UnlocalizedNumberFormatter uNoUnitPercent = NumberFormatter::with().unit(NoUnit::percent());
    UnlocalizedNumberFormatter uNoUnitPermille = NumberFormatter::with().unit(NoUnit::permille());
    UnlocalizedNumberFormatter uMeasurePercent = NumberFormatter::with().unit(MeasureUnit::getPercent());
    UnlocalizedNumberFormatter uMeasurePermille = NumberFormatter::with().unit(MeasureUnit::getPermille());

    int32_t localeCount;
    auto locales = Locale::getAvailableLocales(localeCount);
    for (int32_t i=0; i<localeCount; i++) {
        auto& locale = locales[i];
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
}

void NumberFormatterApiTest::roundingFigures() {
    assertFormatSingle(
            u"Fixed Significant",
            u"@@@",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(3)),
            Locale::getEnglish(),
            -98,
            u"-98.0");

    assertFormatSingle(
            u"Fixed Significant Rounding",
            u"@@@",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(3)),
            Locale::getEnglish(),
            -98.7654321,
            u"-98.8");

    assertFormatSingle(
            u"Fixed Significant Zero",
            u"@@@",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(3)),
            Locale::getEnglish(),
            0,
            u"0.00");

    assertFormatSingle(
            u"Min Significant",
            u"@@+",
            NumberFormatter::with().precision(Precision::minSignificantDigits(2)),
            Locale::getEnglish(),
            -9,
            u"-9.0");

    assertFormatSingle(
            u"Max Significant",
            u"@###",
            NumberFormatter::with().precision(Precision::maxSignificantDigits(4)),
            Locale::getEnglish(),
            98.7654321,
            u"98.77");

    assertFormatSingle(
            u"Min/Max Significant",
            u"@@@#",
            NumberFormatter::with().precision(Precision::minMaxSignificantDigits(3, 4)),
            Locale::getEnglish(),
            9.99999,
            u"10.0");

    assertFormatSingle(
            u"Fixed Significant on zero with lots of integer width",
            u"@ integer-width/+000",
            NumberFormatter::with().precision(Precision::fixedSignificantDigits(1))
                    .integerWidth(IntegerWidth::zeroFillTo(3)),
            Locale::getEnglish(),
            0,
            "000");

    assertFormatSingle(
            u"Fixed Significant on zero with zero integer width",
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
            u".00/@@@+",
            NumberFormatter::with().precision(Precision::fixedFraction(2).withMinDigits(3)),
            Locale::getEnglish(),
            0.1,
            u"0.10");

    assertFormatSingle(
            u"FracSig with trailing zeros B",
            u".00/@@@+",
            NumberFormatter::with().precision(Precision::fixedFraction(2).withMinDigits(3)),
            Locale::getEnglish(),
            0.0999999,
            u"0.10");
}

void NumberFormatterApiTest::roundingOther() {
    assertFormatDescending(
            u"Rounding None",
            u"precision-unlimited",
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
            u"Increment Resolving to Power of 10",
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
            u"Currency Standard",
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
            u"Currency Cash with Nickel Rounding",
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
}

void NumberFormatterApiTest::grouping() {
    assertFormatDescendingBig(
            u"Western Grouping",
            u"group-auto",
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

    // NOTE: Hungarian is interesting because it has minimumGroupingDigits=4 in locale data
    // If this test breaks due to data changes, find another locale that has minimumGroupingDigits.
    assertFormatDescendingBig(
            u"Hungarian Grouping",
            u"group-auto",
            NumberFormatter::with().grouping(UNUM_GROUPING_AUTO),
            Locale("hu"),
            u"87 650 000",
            u"8 765 000",
            u"876500",
            u"87650",
            u"8765",
            u"876,5",
            u"87,65",
            u"8,765",
            u"0");

    assertFormatDescendingBig(
            u"Hungarian Grouping, Min 2",
            u"group-min2",
            NumberFormatter::with().grouping(UNUM_GROUPING_MIN2),
            Locale("hu"),
            u"87 650 000",
            u"8 765 000",
            u"876500",
            u"87650",
            u"8765",
            u"876,5",
            u"87,65",
            u"8,765",
            u"0");

    assertFormatDescendingBig(
            u"Hungarian Grouping, Always",
            u"group-on-aligned",
            NumberFormatter::with().grouping(UNUM_GROUPING_ON_ALIGNED),
            Locale("hu"),
            u"87 650 000",
            u"8 765 000",
            u"876 500",
            u"87 650",
            u"8 765",
            u"876,5",
            u"87,65",
            u"8,765",
            u"0");

    // NOTE: Bulgarian is interesting because it has no grouping in the default currency format.
    // If this test breaks due to data changes, find another locale that has no default grouping.
    assertFormatDescendingBig(
            u"Bulgarian Currency Grouping",
            u"currency/USD group-auto",
            NumberFormatter::with().grouping(UNUM_GROUPING_AUTO).unit(USD),
            Locale("bg"),
            u"87650000,00 щ.д.",
            u"8765000,00 щ.д.",
            u"876500,00 щ.д.",
            u"87650,00 щ.д.",
            u"8765,00 щ.д.",
            u"876,50 щ.д.",
            u"87,65 щ.д.",
            u"8,76 щ.д.",
            u"0,00 щ.д.");

    assertFormatDescendingBig(
            u"Bulgarian Currency Grouping, Always",
            u"currency/USD group-on-aligned",
            NumberFormatter::with().grouping(UNUM_GROUPING_ON_ALIGNED).unit(USD),
            Locale("bg"),
            u"87 650 000,00 щ.д.",
            u"8 765 000,00 щ.д.",
            u"876 500,00 щ.д.",
            u"87 650,00 щ.д.",
            u"8 765,00 щ.д.",
            u"876,50 щ.д.",
            u"87,65 щ.д.",
            u"8,76 щ.д.",
            u"0,00 щ.д.");

    MacroProps macros;
    macros.grouper = Grouper(4, 1, 3, UNUM_GROUPING_COUNT);
    assertFormatDescendingBig(
            u"Custom Grouping via Internal API",
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
            NumberFormatter::with().padding(
                    Padder::codePoints(
                            '*', 8, PadPosition::UNUM_PAD_BEFORE_PREFIX)),
            Locale::getEnglish(),
            -88.88,
            u"**-88.88");

    assertFormatSingle(
            u"Pad After Prefix",
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
            NumberFormatter::with().padding(
                    Padder::codePoints(
                            '*', 8, PadPosition::UNUM_PAD_BEFORE_SUFFIX)).unit(NoUnit::percent()),
            Locale::getEnglish(),
            88.88,
            u"88.88**%");

    assertFormatSingle(
            u"Pad After Suffix",
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
            u""); // TODO: Avoid the empty string here?

    assertFormatDescending(
            u"Integer Width Zero Fill 3",
            u"integer-width/+000",
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

    assertFormatSingle(
            u"Integer Width Remove All A",
            u"integer-width/00",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(2).truncateAt(2)),
            "en",
            2500,
            u"00");

    assertFormatSingle(
            u"Integer Width Remove All B",
            u"integer-width/00",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(2).truncateAt(2)),
            "en",
            25000,
            u"00");

    assertFormatSingle(
            u"Integer Width Remove All B, Bytes Mode",
            u"integer-width/00",
            NumberFormatter::with().integerWidth(IntegerWidth::zeroFillTo(2).truncateAt(2)),
            "en",
            // Note: this double produces all 17 significant digits
            10000000000000002000.0,
            u"00");
}

void NumberFormatterApiTest::symbols() {
    assertFormatDescending(
            u"French Symbols with Japanese Data 1",
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
            NumberFormatter::with().notation(Notation::compactShort()).symbols(FRENCH_SYMBOLS),
            Locale::getJapan(),
            12345,
            u"1,2\u4E07");

    assertFormatDescending(
            u"Latin Numbering System with Arabic Data",
            u"currency/USD latin",
            NumberFormatter::with().adoptSymbols(new NumberingSystem(LATN)).unit(USD),
            Locale("ar"),
            u"US$ 87,650.00",
            u"US$ 8,765.00",
            u"US$ 876.50",
            u"US$ 87.65",
            u"US$ 8.76",
            u"US$ 0.88",
            u"US$ 0.09",
            u"US$ 0.01",
            u"US$ 0.00");

    assertFormatDescending(
            u"Math Numbering System with French Data",
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
            NumberFormatter::with().symbols(SWISS_SYMBOLS),
            Locale::getEnglish(),
            12345.67,
            u"12’345.67");

    assertFormatSingle(
            u"Myanmar Symbols (used in documentation)",
            nullptr,
            NumberFormatter::with().symbols(MYANMAR_SYMBOLS),
            Locale::getEnglish(),
            12345.67,
            u"\u1041\u1042,\u1043\u1044\u1045.\u1046\u1047");

    // NOTE: Locale ar puts ¤ after the number in NS arab but before the number in NS latn.

    assertFormatSingle(
            u"Currency symbol should precede number in ar with NS latn",
            u"currency/USD latin",
            NumberFormatter::with().adoptSymbols(new NumberingSystem(LATN)).unit(USD),
            Locale("ar"),
            12345.67,
            u"US$ 12,345.67");

    assertFormatSingle(
            u"Currency symbol should precede number in ar@numbers=latn",
            u"currency/USD",
            NumberFormatter::with().unit(USD),
            Locale("ar@numbers=latn"),
            12345.67,
            u"US$ 12,345.67");

    assertFormatSingle(
            u"Currency symbol should follow number in ar-EG with NS arab",
            u"currency/USD",
            NumberFormatter::with().unit(USD),
            Locale("ar-EG"),
            12345.67,
            u"١٢٬٣٤٥٫٦٧ US$");

    assertFormatSingle(
            u"Currency symbol should follow number in ar@numbers=arab",
            u"currency/USD",
            NumberFormatter::with().unit(USD),
            Locale("ar@numbers=arab"),
            12345.67,
            u"١٢٬٣٤٥٫٦٧ US$");

    assertFormatSingle(
            u"NumberingSystem in API should win over @numbers keyword",
            u"currency/USD latin",
            NumberFormatter::with().adoptSymbols(new NumberingSystem(LATN)).unit(USD),
            Locale("ar@numbers=arab"),
            12345.67,
            u"US$ 12,345.67");

    UErrorCode status = U_ZERO_ERROR;
    assertEquals(
            "NumberingSystem in API should win over @numbers keyword in reverse order",
            u"US$ 12,345.67",
            NumberFormatter::withLocale(Locale("ar@numbers=arab")).adoptSymbols(new NumberingSystem(LATN))
                    .unit(USD)
                    .formatDouble(12345.67, status)
                    .toString(status));

    DecimalFormatSymbols symbols = SWISS_SYMBOLS;
    UnlocalizedNumberFormatter f = NumberFormatter::with().symbols(symbols);
    symbols.setSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kGroupingSeparatorSymbol, u"!", status);
    assertFormatSingle(
            u"Symbols object should be copied", nullptr, f, Locale::getEnglish(), 12345.67, u"12’345.67");

    assertFormatSingle(
            u"The last symbols setter wins",
            u"latin",
            NumberFormatter::with().symbols(symbols).adoptSymbols(new NumberingSystem(LATN)),
            Locale::getEnglish(),
            12345.67,
            u"12,345.67");

    assertFormatSingle(
            u"The last symbols setter wins",
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
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_AUTO),
            Locale::getEnglish(),
            444444,
            u"444,444");

    assertFormatSingle(
            u"Sign Auto Negative",
            u"sign-auto",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_AUTO),
            Locale::getEnglish(),
            -444444,
            u"-444,444");

    assertFormatSingle(
            u"Sign Auto Zero",
            u"sign-auto",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_AUTO),
            Locale::getEnglish(),
            0,
            u"0");

    assertFormatSingle(
            u"Sign Always Positive",
            u"sign-always",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ALWAYS),
            Locale::getEnglish(),
            444444,
            u"+444,444");

    assertFormatSingle(
            u"Sign Always Negative",
            u"sign-always",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ALWAYS),
            Locale::getEnglish(),
            -444444,
            u"-444,444");

    assertFormatSingle(
            u"Sign Always Zero",
            u"sign-always",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ALWAYS),
            Locale::getEnglish(),
            0,
            u"+0");

    assertFormatSingle(
            u"Sign Never Positive",
            u"sign-never",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEVER),
            Locale::getEnglish(),
            444444,
            u"444,444");

    assertFormatSingle(
            u"Sign Never Negative",
            u"sign-never",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEVER),
            Locale::getEnglish(),
            -444444,
            u"444,444");

    assertFormatSingle(
            u"Sign Never Zero",
            u"sign-never",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_NEVER),
            Locale::getEnglish(),
            0,
            u"0");

    assertFormatSingle(
            u"Sign Accounting Positive",
            u"currency/USD sign-accounting",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING).unit(USD),
            Locale::getEnglish(),
            444444,
            u"$444,444.00");

    assertFormatSingle(
            u"Sign Accounting Negative",
            u"currency/USD sign-accounting",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING).unit(USD),
            Locale::getEnglish(),
            -444444,
            u"($444,444.00)");

    assertFormatSingle(
            u"Sign Accounting Zero",
            u"currency/USD sign-accounting",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING).unit(USD),
            Locale::getEnglish(),
            0,
            u"$0.00");

    assertFormatSingle(
            u"Sign Accounting-Always Positive",
            u"currency/USD sign-accounting-always",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_ALWAYS).unit(USD),
            Locale::getEnglish(),
            444444,
            u"+$444,444.00");

    assertFormatSingle(
            u"Sign Accounting-Always Negative",
            u"currency/USD sign-accounting-always",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_ALWAYS).unit(USD),
            Locale::getEnglish(),
            -444444,
            u"($444,444.00)");

    assertFormatSingle(
            u"Sign Accounting-Always Zero",
            u"currency/USD sign-accounting-always",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_ALWAYS).unit(USD),
            Locale::getEnglish(),
            0,
            u"+$0.00");

    assertFormatSingle(
            u"Sign Except-Zero Positive",
            u"sign-except-zero",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_EXCEPT_ZERO),
            Locale::getEnglish(),
            444444,
            u"+444,444");

    assertFormatSingle(
            u"Sign Except-Zero Negative",
            u"sign-except-zero",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_EXCEPT_ZERO),
            Locale::getEnglish(),
            -444444,
            u"-444,444");

    assertFormatSingle(
            u"Sign Except-Zero Zero",
            u"sign-except-zero",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_EXCEPT_ZERO),
            Locale::getEnglish(),
            0,
            u"0");

    assertFormatSingle(
            u"Sign Accounting-Except-Zero Positive",
            u"currency/USD sign-accounting-except-zero",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO).unit(USD),
            Locale::getEnglish(),
            444444,
            u"+$444,444.00");

    assertFormatSingle(
            u"Sign Accounting-Except-Zero Negative",
            u"currency/USD sign-accounting-except-zero",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO).unit(USD),
            Locale::getEnglish(),
            -444444,
            u"($444,444.00)");

    assertFormatSingle(
            u"Sign Accounting-Except-Zero Zero",
            u"currency/USD sign-accounting-except-zero",
            NumberFormatter::with().sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO).unit(USD),
            Locale::getEnglish(),
            0,
            u"$0.00");

    assertFormatSingle(
            u"Sign Accounting Negative Hidden",
            u"currency/USD unit-width-hidden sign-accounting",
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
            NumberFormatter::with()
                .sign(UNumberSignDisplay::UNUM_SIGN_ACCOUNTING)
                .unit(USD)
                .unitWidth(UNUM_UNIT_WIDTH_FULL_NAME),
            Locale::getCanada(),
            -444444,
            u"-444,444.00 US dollars");
}

void NumberFormatterApiTest::decimal() {
    assertFormatDescending(
            u"Decimal Default",
            u"decimal-auto",
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
            NumberFormatter::with().unit(NoUnit::percent()).scale(Scale::powerOfTen(2)),
            Locale::getEnglish(),
            0.5,
            u"50%");

    assertFormatSingle(
            u"Negative Multiplier",
            u"scale/-5.2",
            NumberFormatter::with().scale(Scale::byDouble(-5.2)),
            Locale::getEnglish(),
            2,
            u"-10.4");

    assertFormatSingle(
            u"Negative One Multiplier",
            u"scale/-1",
            NumberFormatter::with().scale(Scale::byDouble(-1)),
            Locale::getEnglish(),
            444444,
            u"-444,444");

    assertFormatSingle(
            u"Two-Type Multiplier with Overlap",
            u"scale/10000",
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
    FieldPosition actual = {UNUM_GROUPING_SEPARATOR_FIELD};
    int32_t i = 1;
    while (fmtd.nextFieldPosition(actual, status)) {
        UFieldPosition expected = expectedFieldPositions[i++];
        assertEquals(
                UnicodeString(u"Next for grouping, field, case #") + Int64ToUnicodeString(i),
                expected.field,
                actual.getField());
        assertEquals(
                UnicodeString(u"Next for grouping, begin index, case #") + Int64ToUnicodeString(i),
                expected.beginIndex,
                actual.getBeginIndex());
        assertEquals(
                UnicodeString(u"Next for grouping, end index, case #") + Int64ToUnicodeString(i),
                expected.endIndex,
                actual.getEndIndex());
    }
    assertEquals(u"Should have seen all grouping separators", 4, i);

    // Make sure strings without fraction do not contain fraction field
    actual = {UNUM_FRACTION_FIELD};
    fmtd = NumberFormatter::withLocale("en").formatInt(5, status);
    assertFalse(u"No fraction part in an integer", fmtd.nextFieldPosition(actual, status));
}

void NumberFormatterApiTest::fieldPositionCoverage() {
    IcuTestErrorCode status(*this, "fieldPositionCoverage");

    {
        const char16_t* message = u"Measure unit field position basic";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/temperature-fahrenheit",
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
        const char16_t* message = u"Measure unit field position with prefix and suffix";
        FormattedNumber result = assertFormatSingle(
                message,
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
        const char16_t* message = u"Measure unit field position with inner spaces";
        FormattedNumber result = assertFormatSingle(
                message,
                u"measure-unit/temperature-fahrenheit unit-width-full-name",
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

    FieldPositionIterator fpi1;
    lnf.formatDouble(514.23, status).getAllFieldPositions(fpi1, status);
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

    // FieldPosition
    status = U_ZERO_ERROR;
    FieldPosition fp;
    fn.nextFieldPosition(fp, status);
    assertEquals(
            "Should fail on FieldPosition terminal method with correct error code",
            U_NUMBER_ARG_OUTOFBOUNDS_ERROR,
            status);

    // FieldPositionIterator
    status = U_ZERO_ERROR;
    FieldPositionIterator fpi;
    fn.getAllFieldPositions(fpi, status);
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

#define VALID_RANGE_ASSERT(status, method, lowerBound, argument) { \
    UErrorCode expectedStatus = ((lowerBound <= argument) && (argument <= EXPECTED_MAX_INT_FRAC_SIG)) \
        ? U_ZERO_ERROR \
        : U_NUMBER_ARG_OUTOFBOUNDS_ERROR; \
    assertEquals( \
        UnicodeString(u"Incorrect status for " #method " on input ") \
            + Int64ToUnicodeString(argument), \
        expectedStatus, \
        status); \
}

#define VALID_RANGE_ONEARG(setting, method, lowerBound) { \
    for (int32_t argument = -2; argument <= EXPECTED_MAX_INT_FRAC_SIG + 2; argument++) { \
        UErrorCode status = U_ZERO_ERROR; \
        NumberFormatter::with().setting(method(argument)).copyErrorTo(status); \
        VALID_RANGE_ASSERT(status, method, lowerBound, argument); \
    } \
}

#define VALID_RANGE_TWOARGS(setting, method, lowerBound) { \
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
}

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


void NumberFormatterApiTest::assertFormatDescending(const char16_t* umessage, const char16_t* uskeleton,
                                                    const UnlocalizedNumberFormatter& f, Locale locale,
                                                    ...) {
    va_list args;
    va_start(args, locale);
    UnicodeString message(TRUE, umessage, -1);
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
        UnicodeString skeleton(TRUE, uskeleton, -1);
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
    } else {
        assertUndefinedSkeleton(f);
    }
}

void NumberFormatterApiTest::assertFormatDescendingBig(const char16_t* umessage, const char16_t* uskeleton,
                                                       const UnlocalizedNumberFormatter& f, Locale locale,
                                                       ...) {
    va_list args;
    va_start(args, locale);
    UnicodeString message(TRUE, umessage, -1);
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
        UnicodeString skeleton(TRUE, uskeleton, -1);
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
    } else {
        assertUndefinedSkeleton(f);
    }
}

FormattedNumber
NumberFormatterApiTest::assertFormatSingle(const char16_t* umessage, const char16_t* uskeleton,
                                           const UnlocalizedNumberFormatter& f, Locale locale,
                                           double input, const UnicodeString& expected) {
    UnicodeString message(TRUE, umessage, -1);
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
        UnicodeString skeleton(TRUE, uskeleton, -1);
        // Only compare normalized skeletons: the tests need not provide the normalized forms.
        // Use the normalized form to construct the testing formatter to ensure no loss of info.
        UnicodeString normalized = NumberFormatter::forSkeleton(skeleton, status).toSkeleton(status);
        assertEquals(message + ": Skeleton:", normalized, f.toSkeleton(status));
        LocalizedNumberFormatter l3 = NumberFormatter::forSkeleton(normalized, status).locale(locale);
        UnicodeString actual3 = l3.formatDouble(input, status).toString(status);
        assertEquals(message + ": Skeleton Path: '" + normalized + "': " + input, expected, actual3);
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
        const char16_t* message, const FormattedNumber& formattedNumber,
        const UFieldPosition* expectedFieldPositions, int32_t length) {
    IcuTestErrorCode status(*this, "assertNumberFieldPositions");

    // Check FormattedValue functions
    checkFormattedValue(
        message,
        static_cast<const FormattedValue&>(formattedNumber),
        formattedNumber.toString(status),
        UFIELD_CATEGORY_NUMBER,
        expectedFieldPositions,
        length);

    // Check FormattedNumber-specific functions
    UnicodeString baseMessage = UnicodeString(message) + u": " + formattedNumber.toString(status) + u": ";
    FieldPositionIterator fpi;
    formattedNumber.getAllFieldPositions(fpi, status);
    int32_t i = 0;
    FieldPosition actual;
    while (fpi.next(actual)) {
        UFieldPosition expected = expectedFieldPositions[i++];
        assertEquals(
                baseMessage + UnicodeString(u"Field, case #") + Int64ToUnicodeString(i),
                expected.field,
                actual.getField());
        assertEquals(
                baseMessage + UnicodeString(u"Iterator, begin, case #") + Int64ToUnicodeString(i),
                expected.beginIndex,
                actual.getBeginIndex());
        assertEquals(
                baseMessage + UnicodeString(u"Iterator, end, case #") + Int64ToUnicodeString(i),
                expected.endIndex,
                actual.getEndIndex());

        // Check for the first location of the field
        FieldPosition actual2(expected.field);
        // Fast-forward the field to skip previous occurrences of the field:
        actual2.setBeginIndex(expected.beginIndex);
        actual2.setEndIndex(expected.beginIndex);
        UBool found = formattedNumber.nextFieldPosition(actual2, status);
        assertEquals(
                baseMessage + UnicodeString(u"Next, found first, case #") + Int64ToUnicodeString(i),
                (UBool) TRUE,
                found);
        assertEquals(
                baseMessage + UnicodeString(u"Next, begin, case #") + Int64ToUnicodeString(i),
                expected.beginIndex,
                actual2.getBeginIndex());
        assertEquals(
                baseMessage + UnicodeString(u"Next, end, case #") + Int64ToUnicodeString(i),
                expected.endIndex,
                actual2.getEndIndex());

        // The next position should be empty unless the field occurs again
        UBool occursAgain = false;
        for (int32_t j=i; j<length; j++) {
            if (expectedFieldPositions[j].field == expected.field) {
                occursAgain = true;
                break;
            }
        }
        if (!occursAgain) {
            found = formattedNumber.nextFieldPosition(actual2, status);
            assertEquals(
                    baseMessage + UnicodeString(u"Next, found second, case #") + Int64ToUnicodeString(i),
                    (UBool) FALSE,
                    found);
        }
    }
    assertEquals(baseMessage + u"Should have seen every field position", length, i);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
