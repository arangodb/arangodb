// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <cmath>
#include <iostream>

#include "charstr.h"
#include "cmemory.h"
#include "filestrm.h"
#include "intltest.h"
#include "number_decimalquantity.h"
#include "putilimp.h"
#include "unicode/ctest.h"
#include "unicode/measunit.h"
#include "unicode/measure.h"
#include "unicode/unistr.h"
#include "unicode/unum.h"
#include "unicode/ures.h"
#include "units_complexconverter.h"
#include "units_converter.h"
#include "units_data.h"
#include "units_router.h"
#include "uparse.h"
#include "uresimp.h"

struct UnitConversionTestCase {
    const StringPiece source;
    const StringPiece target;
    const double inputValue;
    const double expectedValue;
};

using ::icu::number::impl::DecimalQuantity;
using namespace ::icu::units;

class UnitsTest : public IntlTest {
  public:
    UnitsTest() {}

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par = nullptr) override;

    void testUnitConstantFreshness();
    void testExtractConvertibility();
    void testConversionInfo();
    void testConverterWithCLDRTests();
    void testComplexUnitsConverter();
    void testComplexUnitsConverterSorting();
    void testUnitPreferencesWithCLDRTests();
    void testConverter();
};

extern IntlTest *createUnitsTest() { return new UnitsTest(); }

void UnitsTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if (exec) {
        logln("TestSuite UnitsTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testUnitConstantFreshness);
    TESTCASE_AUTO(testExtractConvertibility);
    TESTCASE_AUTO(testConversionInfo);
    TESTCASE_AUTO(testConverterWithCLDRTests);
    TESTCASE_AUTO(testComplexUnitsConverter);
    TESTCASE_AUTO(testComplexUnitsConverterSorting);
    TESTCASE_AUTO(testUnitPreferencesWithCLDRTests);
    TESTCASE_AUTO(testConverter);
    TESTCASE_AUTO_END;
}

// Tests the hard-coded constants in the code against constants that appear in
// units.txt.
void UnitsTest::testUnitConstantFreshness() {
    IcuTestErrorCode status(*this, "testUnitConstantFreshness");
    LocalUResourceBundlePointer unitsBundle(ures_openDirect(nullptr, "units", status));
    LocalUResourceBundlePointer unitConstants(
        ures_getByKey(unitsBundle.getAlias(), "unitConstants", nullptr, status));

    while (ures_hasNext(unitConstants.getAlias())) {
        int32_t len;
        const char *constant = nullptr;
        ures_getNextString(unitConstants.getAlias(), &len, &constant, status);

        Factor factor;
        addSingleFactorConstant(constant, 1, POSITIVE, factor, status);
        if (status.errDataIfFailureAndReset(
                "addSingleFactorConstant(<%s>, ...).\n\n"
                "If U_INVALID_FORMAT_ERROR, please check that \"icu4c/source/i18n/units_converter.cpp\" "
                "has all constants? Is \"%s\" a new constant?\n"
                "See docs/processes/release/tasks/updating-measure-unit.md for more information.\n",
                constant, constant)) {
            continue;
        }

        // Check the values of constants that have a simple numeric value
        factor.substituteConstants();
        int32_t uLen;
        UnicodeString uVal = ures_getStringByKey(unitConstants.getAlias(), constant, &uLen, status);
        CharString val;
        val.appendInvariantChars(uVal, status);
        if (status.errDataIfFailureAndReset("Failed to get constant value for %s.", constant)) {
            continue;
        }
        DecimalQuantity dqVal;
        UErrorCode parseStatus = U_ZERO_ERROR;
        // TODO(units): unify with strToDouble() in units_converter.cpp
        dqVal.setToDecNumber(val.toStringPiece(), parseStatus);
        if (!U_SUCCESS(parseStatus)) {
            // Not simple to parse, skip validating this constant's value. (We
            // leave catching mistakes to the data-driven integration tests.)
            continue;
        }
        double expectedNumerator = dqVal.toDouble();
        assertEquals(UnicodeString("Constant ") + constant + u" numerator", expectedNumerator,
                     factor.factorNum);
        assertEquals(UnicodeString("Constant ") + constant + u" denominator", 1.0, factor.factorDen);
    }
}

void UnitsTest::testExtractConvertibility() {
    IcuTestErrorCode status(*this, "UnitsTest::testExtractConvertibility");

    struct TestCase {
        const char *const source;
        const char *const target;
        const Convertibility expectedState;
    } testCases[]{
        {"meter", "foot", CONVERTIBLE},                                              //
        {"kilometer", "foot", CONVERTIBLE},                                          //
        {"hectare", "square-foot", CONVERTIBLE},                                     //
        {"kilometer-per-second", "second-per-meter", RECIPROCAL},                    //
        {"square-meter", "square-foot", CONVERTIBLE},                                //
        {"kilometer-per-second", "foot-per-second", CONVERTIBLE},                    //
        {"square-hectare", "pow4-foot", CONVERTIBLE},                                //
        {"square-kilometer-per-second", "second-per-square-meter", RECIPROCAL},      //
        {"cubic-kilometer-per-second-meter", "second-per-square-meter", RECIPROCAL}, //
        {"square-meter-per-square-hour", "hectare-per-square-second", CONVERTIBLE},  //
        {"hertz", "revolution-per-second", CONVERTIBLE},                             //
        {"millimeter", "meter", CONVERTIBLE},                                        //
        {"yard", "meter", CONVERTIBLE},                                              //
        {"ounce-troy", "kilogram", CONVERTIBLE},                                     //
        {"percent", "portion", CONVERTIBLE},                                         //
        {"ofhg", "kilogram-per-square-meter-square-second", CONVERTIBLE},            //
        {"second-per-meter", "meter-per-second", RECIPROCAL},                        //
        {"mile-per-hour", "meter-per-second", CONVERTIBLE},                        //
        {"knot", "meter-per-second", CONVERTIBLE},                        //
        {"beaufort", "meter-per-second", CONVERTIBLE},                        //
    };

    for (const auto &testCase : testCases) {
        MeasureUnitImpl source = MeasureUnitImpl::forIdentifier(testCase.source, status);
        if (status.errIfFailureAndReset("source MeasureUnitImpl::forIdentifier(\"%s\", ...)",
                                        testCase.source)) {
            continue;
        }
        MeasureUnitImpl target = MeasureUnitImpl::forIdentifier(testCase.target, status);
        if (status.errIfFailureAndReset("target MeasureUnitImpl::forIdentifier(\"%s\", ...)",
                                        testCase.target)) {
            continue;
        }

        ConversionRates conversionRates(status);
        if (status.errIfFailureAndReset("conversionRates(status)")) {
            continue;
        }
        auto convertibility = extractConvertibility(source, target, conversionRates, status);
        if (status.errIfFailureAndReset("extractConvertibility(<%s>, <%s>, ...)", testCase.source,
                                        testCase.target)) {
            continue;
        }

        assertEquals(UnicodeString("Conversion Capability: ") + testCase.source + " to " +
                         testCase.target,
                     testCase.expectedState, convertibility);
    }
}

void UnitsTest::testConversionInfo() {
    IcuTestErrorCode status(*this, "UnitsTest::testExtractConvertibility");
    // Test Cases
    struct TestCase {
        const char *source;
        const char *target;
        const ConversionInfo expectedConversionInfo;
    } testCases[]{
        {
            "meter",
            "meter",
            {1.0, 0, false},
        },
        {
            "meter",
            "foot",
            {3.28084, 0, false},
        },
        {
            "foot",
            "meter",
            {0.3048, 0, false},
        },
        {
            "celsius",
            "kelvin",
            {1, 273.15, false},
        },
        {
            "fahrenheit",
            "kelvin",
            {5.0 / 9.0, 255.372, false},
        },
        {
            "fahrenheit",
            "celsius",
            {5.0 / 9.0, -17.7777777778, false},
        },
        {
            "celsius",
            "fahrenheit",
            {9.0 / 5.0, 32, false},
        },
        {
            "fahrenheit",
            "fahrenheit",
            {1.0, 0, false},
        },
        {
            "mile-per-gallon",
            "liter-per-100-kilometer",
            {0.00425143707, 0, true},
        },
    };

    ConversionRates rates(status);
    for (const auto &testCase : testCases) {
        auto sourceImpl = MeasureUnitImpl::forIdentifier(testCase.source, status);
        auto targetImpl = MeasureUnitImpl::forIdentifier(testCase.target, status);
        UnitsConverter unitsConverter(sourceImpl, targetImpl, rates, status);

        if (status.errIfFailureAndReset()) {
            continue;
        }

        ConversionInfo actualConversionInfo = unitsConverter.getConversionInfo();
        UnicodeString message =
            UnicodeString("testConverter: ") + testCase.source + " to " + testCase.target;

        double maxDelta = 1e-6 * uprv_fabs(testCase.expectedConversionInfo.conversionRate);
        if (testCase.expectedConversionInfo.conversionRate == 0) {
            maxDelta = 1e-12;
        }
        assertEqualsNear(message + ", conversion rate: ", testCase.expectedConversionInfo.conversionRate,
                         actualConversionInfo.conversionRate, maxDelta);

        maxDelta = 1e-6 * uprv_fabs(testCase.expectedConversionInfo.offset);
        if (testCase.expectedConversionInfo.offset == 0) {
            maxDelta = 1e-12;
        }
        assertEqualsNear(message + ", offset: ", testCase.expectedConversionInfo.offset, actualConversionInfo.offset,
                         maxDelta);

        assertEquals(message + ", reciprocal: ", testCase.expectedConversionInfo.reciprocal,
                     actualConversionInfo.reciprocal);
    }
}

void UnitsTest::testConverter() {
    IcuTestErrorCode status(*this, "UnitsTest::testConverter");

    // Test Cases
    struct TestCase {
        const char *source;
        const char *target;
        const double inputValue;
        const double expectedValue;
    } testCases[]{
        // SI Prefixes
        {"gram", "kilogram", 1.0, 0.001},
        {"milligram", "kilogram", 1.0, 0.000001},
        {"microgram", "kilogram", 1.0, 0.000000001},
        {"megagram", "gram", 1.0, 1000000},
        {"megagram", "kilogram", 1.0, 1000},
        {"gigabyte", "byte", 1.0, 1000000000},
        {"megawatt", "watt", 1.0, 1000000},
        {"megawatt", "kilowatt", 1.0, 1000},
        // Binary Prefixes
        {"kilobyte", "byte", 1, 1000},
        {"kibibyte", "byte", 1, 1024},
        {"mebibyte", "byte", 1, 1048576},
        {"gibibyte", "kibibyte", 1, 1048576},
        {"pebibyte", "tebibyte", 4, 4096},
        {"zebibyte", "pebibyte", 1.0 / 16, 65536.0},
        {"yobibyte", "exbibyte", 1, 1048576},
        // Mass
        {"gram", "kilogram", 1.0, 0.001},
        {"pound", "kilogram", 1.0, 0.453592},
        {"pound", "kilogram", 2.0, 0.907185},
        {"ounce", "pound", 16.0, 1.0},
        {"ounce", "kilogram", 16.0, 0.453592},
        {"ton", "pound", 1.0, 2000},
        {"stone", "pound", 1.0, 14},
        {"stone", "kilogram", 1.0, 6.35029},
        // Speed
        {"mile-per-hour", "meter-per-second", 1.0, 0.44704},
        {"knot", "meter-per-second", 1.0, 0.514444},
        {"beaufort", "meter-per-second", 1.0, 0.95},
        {"beaufort", "meter-per-second", 4.0, 6.75},
        {"beaufort", "meter-per-second", 7.0, 15.55},
        {"beaufort", "meter-per-second", 10.0, 26.5},
        {"beaufort", "meter-per-second", 13.0, 39.15},
        {"beaufort", "mile-per-hour", 1.0, 2.12509},
        {"beaufort", "mile-per-hour", 4.0, 15.099319971367215},
        {"beaufort", "mile-per-hour", 7.0, 34.784359341445956},
        {"beaufort", "mile-per-hour", 10.0, 59.2788},
        {"beaufort", "mile-per-hour", 13.0, 87.5761},
        // Temperature
        {"celsius", "fahrenheit", 0.0, 32.0},
        {"celsius", "fahrenheit", 10.0, 50.0},
        {"celsius", "fahrenheit", 1000, 1832},
        {"fahrenheit", "celsius", 32.0, 0.0},
        {"fahrenheit", "celsius", 89.6, 32},
        {"fahrenheit", "fahrenheit", 1000, 1000},
        {"kelvin", "fahrenheit", 0.0, -459.67},
        {"kelvin", "fahrenheit", 300, 80.33},
        {"kelvin", "celsius", 0.0, -273.15},
        {"kelvin", "celsius", 300.0, 26.85},
        // Area
        {"square-meter", "square-yard", 10.0, 11.9599},
        {"hectare", "square-yard", 1.0, 11959.9},
        {"square-mile", "square-foot", 0.0001, 2787.84},
        {"hectare", "square-yard", 1.0, 11959.9},
        {"hectare", "square-meter", 1.0, 10000},
        {"hectare", "square-meter", 0.0, 0.0},
        {"square-mile", "square-foot", 0.0001, 2787.84},
        {"square-yard", "square-foot", 10, 90},
        {"square-yard", "square-foot", 0, 0},
        {"square-yard", "square-foot", 0.000001, 0.000009},
        {"square-mile", "square-foot", 0.0, 0.0},
        // Fuel Consumption
        {"cubic-meter-per-meter", "mile-per-gallon", 2.1383143939394E-6, 1.1},
        {"cubic-meter-per-meter", "mile-per-gallon", 2.6134953703704E-6, 0.9},
        {"liter-per-100-kilometer", "mile-per-gallon", 6.6, 35.6386},
        {"liter-per-100-kilometer", "mile-per-gallon", 0, uprv_getInfinity()},
        {"mile-per-gallon", "liter-per-100-kilometer", 0, uprv_getInfinity()},
        {"mile-per-gallon", "liter-per-100-kilometer", uprv_getInfinity(), 0},
        // We skip testing -Inf, because the inverse conversion loses the sign:
        // {"mile-per-gallon", "liter-per-100-kilometer", -uprv_getInfinity(), 0},

        // Test Aliases
        // Alias is just another name to the same unit. Therefore, converting
        // between them should be the same.
        {"foodcalorie", "kilocalorie", 1.0, 1.0},
        {"dot-per-centimeter", "pixel-per-centimeter", 1.0, 1.0},
        {"dot-per-inch", "pixel-per-inch", 1.0, 1.0},
        {"dot", "pixel", 1.0, 1.0},

    };

    for (const auto &testCase : testCases) {
        MeasureUnitImpl source = MeasureUnitImpl::forIdentifier(testCase.source, status);
        if (status.errIfFailureAndReset("source MeasureUnitImpl::forIdentifier(\"%s\", ...)",
                                        testCase.source)) {
            continue;
        }
        MeasureUnitImpl target = MeasureUnitImpl::forIdentifier(testCase.target, status);
        if (status.errIfFailureAndReset("target MeasureUnitImpl::forIdentifier(\"%s\", ...)",
                                        testCase.target)) {
            continue;
        }

        double maxDelta = 1e-6 * uprv_fabs(testCase.expectedValue);
        if (testCase.expectedValue == 0) {
            maxDelta = 1e-12;
        }
        double inverseMaxDelta = 1e-6 * uprv_fabs(testCase.inputValue);
        if (testCase.inputValue == 0) {
            inverseMaxDelta = 1e-12;
        }

        ConversionRates conversionRates(status);
        if (status.errIfFailureAndReset("conversionRates(status)")) {
            continue;
        }

        UnitsConverter converter(source, target, conversionRates, status);
        if (status.errIfFailureAndReset("UnitsConverter(<%s>, <%s>, ...)", testCase.source,
                                        testCase.target)) {
            continue;
        }
        assertEqualsNear(UnicodeString("testConverter: ") + testCase.source + " to " + testCase.target,
                         testCase.expectedValue, converter.convert(testCase.inputValue), maxDelta);
        assertEqualsNear(
            UnicodeString("testConverter inverse: ") + testCase.target + " back to " + testCase.source,
            testCase.inputValue, converter.convertInverse(testCase.expectedValue), inverseMaxDelta);

        // Test UnitsConverter created by CLDR unit identifiers
        UnitsConverter converter2(testCase.source, testCase.target, status);
        if (status.errIfFailureAndReset("UnitsConverter(<%s>, <%s>, ...)", testCase.source,
                                        testCase.target)) {
            continue;
        }
        assertEqualsNear(UnicodeString("testConverter2: ") + testCase.source + " to " + testCase.target,
                         testCase.expectedValue, converter2.convert(testCase.inputValue), maxDelta);
        assertEqualsNear(
            UnicodeString("testConverter2 inverse: ") + testCase.target + " back to " + testCase.source,
            testCase.inputValue, converter2.convertInverse(testCase.expectedValue), inverseMaxDelta);
    }
}

/**
 * Trims whitespace off of the specified string.
 * @param field is two pointers pointing at the start and end of the string.
 * @return A StringPiece with initial and final space characters trimmed off.
 */
StringPiece trimField(char *(&field)[2]) {
    const char *start = field[0];
    start = u_skipWhitespace(start);
    if (start >= field[1]) {
        start = field[1];
    }
    const char *end = field[1];
    while ((start < end) && U_IS_INV_WHITESPACE(*(end - 1))) {
        end--;
    }
    int32_t length = (int32_t)(end - start);
    return StringPiece(start, length);
}

// Used for passing context to unitsTestDataLineFn via u_parseDelimitedFile.
struct UnitsTestContext {
    // Provides access to UnitsTest methods like logln.
    UnitsTest *unitsTest;
    // Conversion rates: does not take ownership.
    ConversionRates *conversionRates;
};

/**
 * Deals with a single data-driven unit test for unit conversions.
 *
 * This is a UParseLineFn as required by u_parseDelimitedFile, intended for
 * parsing unitsTest.txt.
 *
 * @param context Must point at a UnitsTestContext struct.
 * @param fields A list of pointer-pairs, each pair pointing at the start and
 * end of each field. End pointers are important because these are *not*
 * null-terminated strings. (Interpreted as a null-terminated string,
 * fields[0][0] points at the whole line.)
 * @param fieldCount The number of fields (pointer pairs) passed to the fields
 * parameter.
 * @param pErrorCode Receives status.
 */
void unitsTestDataLineFn(void *context, char *fields[][2], int32_t fieldCount, UErrorCode *pErrorCode) {
    if (U_FAILURE(*pErrorCode)) {
        return;
    }
    UnitsTestContext *ctx = static_cast<UnitsTestContext *>(context);
    UnitsTest *unitsTest = ctx->unitsTest;
    (void)fieldCount; // unused UParseLineFn variable
    IcuTestErrorCode status(*unitsTest, "unitsTestDatalineFn");

    StringPiece quantity = trimField(fields[0]);
    StringPiece x = trimField(fields[1]);
    StringPiece y = trimField(fields[2]);
    StringPiece commentConversionFormula = trimField(fields[3]);
    StringPiece utf8Expected = trimField(fields[4]);

    UNumberFormat *nf = unum_open(UNUM_DEFAULT, nullptr, -1, "en_US", nullptr, status);
    if (status.errIfFailureAndReset("unum_open failed")) {
        return;
    }
    UnicodeString uExpected = UnicodeString::fromUTF8(utf8Expected);
    double expected = unum_parseDouble(nf, uExpected.getBuffer(), uExpected.length(), nullptr, status);
    unum_close(nf);
    if (status.errIfFailureAndReset("unum_parseDouble(\"%s\") failed", utf8Expected)) {
        return;
    }

    CharString sourceIdent(x, status);
    MeasureUnitImpl sourceUnit = MeasureUnitImpl::forIdentifier(x, status);
    if (status.errIfFailureAndReset("forIdentifier(\"%.*s\")", x.length(), x.data())) {
        return;
    }

    CharString targetIdent(y, status);
    MeasureUnitImpl targetUnit = MeasureUnitImpl::forIdentifier(y, status);
    if (status.errIfFailureAndReset("forIdentifier(\"%.*s\")", y.length(), y.data())) {
        return;
    }

    unitsTest->logln("Quantity (Category): \"%.*s\", "
                     "Expected value of \"1000 %.*s in %.*s\": %f, "
                     "commentConversionFormula: \"%.*s\", ",
                     quantity.length(), quantity.data(), x.length(), x.data(), y.length(), y.data(),
                     expected, commentConversionFormula.length(), commentConversionFormula.data());

    // Convertibility:
    auto convertibility = extractConvertibility(sourceUnit, targetUnit, *ctx->conversionRates, status);
    if (status.errIfFailureAndReset("extractConvertibility(<%s>, <%s>, ...)",
                                    sourceIdent.data(), targetIdent.data())) {
        return;
    }
    CharString msg;
    msg.append("convertible: ", status)
        .append(sourceIdent.data(), status)
        .append(" -> ", status)
        .append(targetIdent.data(), status);
    if (status.errIfFailureAndReset("msg construction")) {
        return;
    }
    unitsTest->assertNotEquals(msg.data(), UNCONVERTIBLE, convertibility);

    // Conversion:
    UnitsConverter converter(sourceUnit, targetUnit, *ctx->conversionRates, status);
    if (status.errIfFailureAndReset("UnitsConverter(<%s>, <%s>, ...)", sourceIdent.data(),
                                    targetIdent.data())) {
        return;
    }
    double got = converter.convert(1000);
    msg.clear();
    msg.append("Converting 1000 ", status).append(x, status).append(" to ", status).append(y, status);
    unitsTest->assertEqualsNear(msg.data(), expected, got, 0.0001 * expected);
    double inverted = converter.convertInverse(got);
    msg.clear();
    msg.append("Converting back to ", status).append(x, status).append(" from ", status).append(y, status);
    if (strncmp(x.data(), "beaufort", 8)
    		&& log_knownIssue("CLDR-17454", "unitTest.txt for beaufort doesn't scale correctly") ) {
		unitsTest->assertEqualsNear(msg.data(), 1000, inverted, 0.0001);
    }
}

/**
 * Runs data-driven unit tests for unit conversion. It looks for the test cases
 * in source/test/testdata/cldr/units/unitsTest.txt, which originates in CLDR.
 */
void UnitsTest::testConverterWithCLDRTests() {
    const char *filename = "unitsTest.txt";
    const int32_t kNumFields = 5;
    char *fields[kNumFields][2];

    IcuTestErrorCode errorCode(*this, "UnitsTest::testConverterWithCLDRTests");
    const char *sourceTestDataPath = getSourceTestData(errorCode);
    if (errorCode.errIfFailureAndReset("unable to find the source/test/testdata "
                                       "folder (getSourceTestData())")) {
        return;
    }

    CharString path(sourceTestDataPath, errorCode);
    path.appendPathPart("cldr/units", errorCode);
    path.appendPathPart(filename, errorCode);

    ConversionRates rates(errorCode);
    UnitsTestContext ctx = {this, &rates};
    u_parseDelimitedFile(path.data(), ';', fields, kNumFields, unitsTestDataLineFn, &ctx, errorCode);
    if (errorCode.errIfFailureAndReset("error parsing %s: %s\n", path.data(), u_errorName(errorCode))) {
        return;
    }
}

void UnitsTest::testComplexUnitsConverter() {
    IcuTestErrorCode status(*this, "UnitsTest::testComplexUnitsConverter");

    // DBL_EPSILON is approximately 2.22E-16, and is the precision of double for
    // values in the range [1.0, 2.0), but half the precision of double for
    // [2.0, 4.0).
    U_ASSERT(1.0 + DBL_EPSILON > 1.0);
    U_ASSERT(2.0 - DBL_EPSILON < 2.0);
    U_ASSERT(2.0 + DBL_EPSILON == 2.0);

    struct TestCase {
        const char* msg;
        const char* input;
        const char* output;
        double value;
        Measure expected[2];
        int32_t expectedCount;
        // For mixed units, accuracy of the smallest unit
        double accuracy;
    } testCases[]{
        // Significantly less than 2.0.
        {"1.9999",
         "foot",
         "foot-and-inch",
         1.9999,
         {Measure(1, MeasureUnit::createFoot(status), status),
          Measure(11.9988, MeasureUnit::createInch(status), status)},
         2,
         0},

        // A minimal nudge under 2.0, rounding up to 2.0 ft, 0 in.
        {"2-eps",
         "foot",
         "foot-and-inch",
         2.0 - DBL_EPSILON,
         {Measure(2, MeasureUnit::createFoot(status), status),
          Measure(0, MeasureUnit::createInch(status), status)},
         2,
         0},

        // A slightly bigger nudge under 2.0, *not* rounding up to 2.0 ft!
        {"2-3eps",
         "foot",
         "foot-and-inch",
         2.0 - 3 * DBL_EPSILON,
         {Measure(1, MeasureUnit::createFoot(status), status),
          // We expect 12*3*DBL_EPSILON inches (7.92e-15) less than 12.
          Measure(12 - 36 * DBL_EPSILON, MeasureUnit::createInch(status), status)},
         2,
         // Might accuracy be lacking with some compilers or on some systems? In
         // case it is somehow lacking, we'll allow a delta of 12 * DBL_EPSILON.
         12 * DBL_EPSILON},

        // Testing precision with meter and light-year.
        //
        // DBL_EPSILON light-years, ~2.22E-16 light-years, is ~2.1 meters
        // (maximum precision when exponent is 0).
        //
        // 1e-16 light years is 0.946073 meters.

        // A 2.1 meter nudge under 2.0 light years, rounding up to 2.0 ly, 0 m.
        {"2-eps",
         "light-year",
         "light-year-and-meter",
         2.0 - DBL_EPSILON,
         {Measure(2, MeasureUnit::createLightYear(status), status),
          Measure(0, MeasureUnit::createMeter(status), status)},
         2,
         0},

        // A 2.1 meter nudge under 1.0 light years, rounding up to 1.0 ly, 0 m.
        {"1-eps",
         "light-year",
         "light-year-and-meter",
         1.0 - DBL_EPSILON,
         {Measure(1, MeasureUnit::createLightYear(status), status),
          Measure(0, MeasureUnit::createMeter(status), status)},
         2,
         0},

        // 1e-15 light years is 9.46073 meters (calculated using "bc" and the
        // CLDR conversion factor). With double-precision maths in C++, we get
        // 10.5. In this case, we're off by a bit more than 1 meter. With Java
        // BigDecimal, we get accurate results.
        {"1 + 1e-15",
         "light-year",
         "light-year-and-meter",
         1.0 + 1e-15,
         {Measure(1, MeasureUnit::createLightYear(status), status),
          Measure(9.46073, MeasureUnit::createMeter(status), status)},
         2,
         1.5 /* meters, precision */},

        // 2.1 meters more than 1 light year is not rounded away.
        {"1 + eps",
         "light-year",
         "light-year-and-meter",
         1.0 + DBL_EPSILON,
         {Measure(1, MeasureUnit::createLightYear(status), status),
          Measure(2.1, MeasureUnit::createMeter(status), status)},
         2,
         0.001},

        // Negative numbers
        {"Negative number conversion",
         "yard",
         "mile-and-yard",
         -1800,
         {Measure(-1, MeasureUnit::createMile(status), status),
          Measure(-40, MeasureUnit::createYard(status), status)},
         2,
         1e-10},
    };
    status.assertSuccess();

    ConversionRates rates(status);
    MeasureUnit input, output;
    MeasureUnitImpl tempInput, tempOutput;
    MaybeStackVector<Measure> measures;
    auto testATestCase = [&](const ComplexUnitsConverter& converter ,StringPiece initMsg , const TestCase &testCase) {
        measures = converter.convert(testCase.value, nullptr, status);

        CharString msg(initMsg, status);
        msg.append(testCase.msg, status);
        msg.append(" ", status);
        msg.append(testCase.input, status);
        msg.append(" -> ", status);
        msg.append(testCase.output, status);

        CharString msgCount(msg, status);
        msgCount.append(", measures.length()", status);
        assertEquals(msgCount.data(), testCase.expectedCount, measures.length());
        for (int i = 0; i < measures.length() && i < testCase.expectedCount; i++) {
            if (i == testCase.expectedCount-1) {
                assertEqualsNear(msg.data(), testCase.expected[i].getNumber().getDouble(status),
                                 measures[i]->getNumber().getDouble(status), testCase.accuracy);
            } else {
                assertEquals(msg.data(), testCase.expected[i].getNumber().getDouble(status),
                             measures[i]->getNumber().getDouble(status));
            }
            assertEquals(msg.data(), testCase.expected[i].getUnit().getIdentifier(),
                         measures[i]->getUnit().getIdentifier());
        }
    };

    for (const auto &testCase : testCases)
    {
        input = MeasureUnit::forIdentifier(testCase.input, status);
        output = MeasureUnit::forIdentifier(testCase.output, status);
        const MeasureUnitImpl& inputImpl = MeasureUnitImpl::forMeasureUnit(input, tempInput, status);
        const MeasureUnitImpl& outputImpl = MeasureUnitImpl::forMeasureUnit(output, tempOutput, status);

        ComplexUnitsConverter converter1(inputImpl, outputImpl, rates, status);
        testATestCase(converter1, "ComplexUnitsConverter #1 " , testCase);

        // Test ComplexUnitsConverter created with CLDR units identifiers.
        ComplexUnitsConverter converter2( testCase.input, testCase.output, status);
        testATestCase(converter2, "ComplexUnitsConverter #1 " , testCase);
    }
    status.assertSuccess();
}

void UnitsTest::testComplexUnitsConverterSorting() {
    IcuTestErrorCode status(*this, "UnitsTest::testComplexUnitsConverterSorting");
    ConversionRates conversionRates(status);

    status.assertSuccess();

    struct TestCase {
        const char *msg;
        const char *input;
        const char *output;
        double inputValue;
        Measure expected[3];
        int32_t expectedCount;
        // For mixed units, accuracy of the smallest unit
        double accuracy;
    } testCases[]{{"inch-and-foot",
                   "meter",
                   "inch-and-foot",
                   10.0,
                   {
                       Measure(9.70079, MeasureUnit::createInch(status), status),
                       Measure(32, MeasureUnit::createFoot(status), status),
                       Measure(0, MeasureUnit::createBit(status), status),
                   },
                   2,
                   0.00001},
                  {"inch-and-yard-and-foot",
                   "meter",
                   "inch-and-yard-and-foot",
                   100.0,
                   {
                       Measure(1.0079, MeasureUnit::createInch(status), status),
                       Measure(109, MeasureUnit::createYard(status), status),
                       Measure(1, MeasureUnit::createFoot(status), status),
                   },
                   3,
                   0.0001}};

    for (const auto &testCase : testCases) {
        MeasureUnitImpl inputImpl = MeasureUnitImpl::forIdentifier(testCase.input, status);
        if (status.errIfFailureAndReset()) {
            continue;
        }
        MeasureUnitImpl outputImpl = MeasureUnitImpl::forIdentifier(testCase.output, status);
        if (status.errIfFailureAndReset()) {
            continue;
        }
        ComplexUnitsConverter converter(inputImpl, outputImpl, conversionRates, status);
        if (status.errIfFailureAndReset()) {
            continue;
        }

        auto actual = converter.convert(testCase.inputValue, nullptr, status);
        if (status.errIfFailureAndReset()) {
            continue;
        }
        if (actual.length() < testCase.expectedCount) {
            errln("converter.convert(...) returned too few Measures");
            continue;
        }

        for (int i = 0; i < testCase.expectedCount; i++) {
            assertEquals(testCase.msg, testCase.expected[i].getUnit().getIdentifier(),
                         actual[i]->getUnit().getIdentifier());

            if (testCase.expected[i].getNumber().getType() == Formattable::Type::kInt64) {
                assertEquals(testCase.msg, testCase.expected[i].getNumber().getInt64(),
                             actual[i]->getNumber().getInt64());
            } else {
                assertEqualsNear(testCase.msg, testCase.expected[i].getNumber().getDouble(),
                                 actual[i]->getNumber().getDouble(), testCase.accuracy);
            }
        }
    }
}

/**
 * This class represents the output fields from unitPreferencesTest.txt. Please
 * see the documentation at the top of that file for details.
 *
 * For "mixed units" output, there are more (repeated) output fields. The last
 * output unit has the expected output specified as both a rational fraction and
 * a decimal fraction. This class ignores rational fractions, and expects to
 * find a decimal fraction for each output unit.
 */
class ExpectedOutput {
  public:
    // Counts number of units in the output. When this is more than one, we have
    // "mixed units" in the expected output.
    int _compoundCount = 0;

    // Counts how many fields were skipped: we expect to skip only one per
    // output unit type (the rational fraction).
    int _skippedFields = 0;

    // The expected output units: more than one for "mixed units".
    MeasureUnit _measureUnits[3];

    // The amounts of each of the output units.
    double _amounts[3];

    /**
     * Parse an expected output field from the test data file.
     *
     * @param output may be a string representation of an integer, a rational
     * fraction, a decimal fraction, or it may be a unit identifier. Whitespace
     * should already be trimmed. This function ignores rational fractions,
     * saving only decimal fractions and their unit identifiers.
     * @return true if the field was successfully parsed, false if parsing
     * failed.
     */
    void parseOutputField(StringPiece output, UErrorCode &errorCode) {
        if (U_FAILURE(errorCode)) return;
        DecimalQuantity dqOutputD;

        dqOutputD.setToDecNumber(output, errorCode);
        if (U_SUCCESS(errorCode)) {
            _amounts[_compoundCount] = dqOutputD.toDouble();
            return;
        } else if (errorCode == U_DECIMAL_NUMBER_SYNTAX_ERROR) {
            // Not a decimal fraction, it might be a rational fraction or a unit
            // identifier: continue.
            errorCode = U_ZERO_ERROR;
        } else {
            // Unexpected error, so we propagate it.
            return;
        }

        _measureUnits[_compoundCount] = MeasureUnit::forIdentifier(output, errorCode);
        if (U_SUCCESS(errorCode)) {
            _compoundCount++;
            _skippedFields = 0;
            return;
        }
        _skippedFields++;
        if (_skippedFields < 2) {
            // We are happy skipping one field per output unit: we want to skip
            // rational fraction fields like "11 / 10".
            errorCode = U_ZERO_ERROR;
            return;
        } else {
            // Propagate the error.
            return;
        }
    }

    /**
     * Produces an output string for debug purposes.
     */
    std::string toDebugString() {
        std::string result;
        for (int i = 0; i < _compoundCount; i++) {
            result += std::to_string(_amounts[i]);
            result += " ";
            result += _measureUnits[i].getIdentifier();
            result += " ";
        }
        return result;
    }
};

// Checks a vector of Measure instances against ExpectedOutput.
void checkOutput(UnitsTest *unitsTest, const char *msg, ExpectedOutput expected,
                 const MaybeStackVector<Measure> &actual, double precision) {
    IcuTestErrorCode status(*unitsTest, "checkOutput");

    CharString testMessage("Test case \"", status);
    testMessage.append(msg, status);
    testMessage.append("\": expected output: ", status);
    testMessage.append(expected.toDebugString().c_str(), status);
    testMessage.append(", obtained output:", status);
    for (int i = 0; i < actual.length(); i++) {
        testMessage.append(" ", status);
        testMessage.append(std::to_string(actual[i]->getNumber().getDouble(status)), status);
        testMessage.append(" ", status);
        testMessage.appendInvariantChars(actual[i]->getUnit().getIdentifier(), status);
    }
    if (!unitsTest->assertEquals(testMessage.data(), expected._compoundCount, actual.length())) {
        return;
    };
    for (int i = 0; i < actual.length(); i++) {
        double permittedDiff = precision * expected._amounts[i];
        if (permittedDiff == 0) {
            // If 0 is expected, still permit a small delta.
            // TODO: revisit this experimentally chosen value:
            permittedDiff = 0.00000001;
        }
        unitsTest->assertEqualsNear(testMessage.data(), expected._amounts[i],
                                    actual[i]->getNumber().getDouble(status), permittedDiff);
    }
}

/**
 * Runs a single data-driven unit test for unit preferences.
 *
 * This is a UParseLineFn as required by u_parseDelimitedFile, intended for
 * parsing unitPreferencesTest.txt.
 */
void unitPreferencesTestDataLineFn(void *context, char *fields[][2], int32_t fieldCount,
                                   UErrorCode *pErrorCode) {
    if (U_FAILURE(*pErrorCode)) return;
    UnitsTest *unitsTest = static_cast<UnitsTest *>(context);
    IcuTestErrorCode status(*unitsTest, "unitPreferencesTestDatalineFn");

    if (!unitsTest->assertTrue(u"unitPreferencesTestDataLineFn expects 9 fields for simple and 11 "
                               u"fields for compound. Other field counts not yet supported. ",
                               fieldCount == 9 || fieldCount == 11)) {
        return;
    }

    StringPiece quantity = trimField(fields[0]);
    StringPiece usage = trimField(fields[1]);
    StringPiece region = trimField(fields[2]);
    // Unused // StringPiece inputR = trimField(fields[3]);
    StringPiece inputD = trimField(fields[4]);
    StringPiece inputUnit = trimField(fields[5]);
    ExpectedOutput expected;
    for (int i = 6; i < fieldCount; i++) {
        expected.parseOutputField(trimField(fields[i]), status);
    }
    if (status.errIfFailureAndReset("parsing unitPreferencesTestData.txt test case: %s", fields[0][0])) {
        return;
    }

    DecimalQuantity dqInputD;
    dqInputD.setToDecNumber(inputD, status);
    if (status.errIfFailureAndReset("parsing decimal quantity: \"%.*s\"", inputD.length(),
                                    inputD.data())) {
        return;
    }
    double inputAmount = dqInputD.toDouble();

    MeasureUnit inputMeasureUnit = MeasureUnit::forIdentifier(inputUnit, status);
    if (status.errIfFailureAndReset("forIdentifier(\"%.*s\")", inputUnit.length(), inputUnit.data())) {
        return;
    }

    unitsTest->logln("Quantity (Category): \"%.*s\", Usage: \"%.*s\", Region: \"%.*s\", "
                     "Input: \"%f %s\", Expected Output: %s",
                     quantity.length(), quantity.data(), usage.length(), usage.data(), region.length(),
                     region.data(), inputAmount, inputMeasureUnit.getIdentifier(),
                     expected.toDebugString().c_str());

    if (U_FAILURE(status)) {
        return;
    }

    CharString localeID;
    localeID.append("und-", status); // append undefined language.
    localeID.append(region, status);
    Locale locale(localeID.data());
    UnitsRouter router(inputMeasureUnit, locale, usage, status);
    if (status.errIfFailureAndReset("UnitsRouter(<%s>, \"%.*s\", \"%.*s\", status)",
                                    inputMeasureUnit.getIdentifier(), region.length(), region.data(),
                                    usage.length(), usage.data())) {
        return;
    }

    CharString msg(quantity, status);
    msg.append(" ", status);
    msg.append(usage, status);
    msg.append(" ", status);
    msg.append(region, status);
    msg.append(" ", status);
    msg.append(inputD, status);
    msg.append(" ", status);
    msg.append(inputMeasureUnit.getIdentifier(), status);
    if (status.errIfFailureAndReset("Failure before router.route")) {
        return;
    }
    RouteResult routeResult = router.route(inputAmount, nullptr, status);
    if (status.errIfFailureAndReset("router.route(inputAmount, ...)")) {
        return;
    }
    // TODO: revisit this experimentally chosen precision:
    checkOutput(unitsTest, msg.data(), expected, routeResult.measures, 0.0000000001);

    // Test UnitsRouter created with CLDR units identifiers.
    CharString inputUnitIdentifier(inputUnit, status);
    UnitsRouter router2(inputUnitIdentifier.data(), locale, usage, status);
    if (status.errIfFailureAndReset("UnitsRouter2(<%s>, \"%.*s\", \"%.*s\", status)",
                                    inputUnitIdentifier.data(), region.length(), region.data(),
                                    usage.length(), usage.data())) {
        return;
    }

    CharString msg2(quantity, status);
    msg2.append(" ", status);
    msg2.append(usage, status);
    msg2.append(" ", status);
    msg2.append(region, status);
    msg2.append(" ", status);
    msg2.append(inputD, status);
    msg2.append(" ", status);
    msg2.append(inputUnitIdentifier.data(), status);
    if (status.errIfFailureAndReset("Failure before router2.route")) {
        return;
    }

    RouteResult routeResult2 = router2.route(inputAmount, nullptr, status);
    if (status.errIfFailureAndReset("router2.route(inputAmount, ...)")) {
        return;
    }
    // TODO: revisit this experimentally chosen precision:
    checkOutput(unitsTest, msg2.data(), expected, routeResult.measures, 0.0000000001);
}

/**
 * Parses the format used by unitPreferencesTest.txt, calling lineFn for each
 * line.
 *
 * This is a modified version of u_parseDelimitedFile, customized for
 * unitPreferencesTest.txt, due to it having a variable number of fields per
 * line.
 */
void parsePreferencesTests(const char *filename, char delimiter, char *fields[][2],
                           int32_t maxFieldCount, UParseLineFn *lineFn, void *context,
                           UErrorCode *pErrorCode) {
    FileStream *file;
    char line[10000];
    char *start, *limit;
    int32_t i;

    if (U_FAILURE(*pErrorCode)) {
        return;
    }

    if (fields == nullptr || lineFn == nullptr || maxFieldCount <= 0) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (filename == nullptr || *filename == 0 || (*filename == '-' && filename[1] == 0)) {
        filename = nullptr;
        file = T_FileStream_stdin();
    } else {
        file = T_FileStream_open(filename, "r");
    }
    if (file == nullptr) {
        *pErrorCode = U_FILE_ACCESS_ERROR;
        return;
    }

    while (T_FileStream_readLine(file, line, sizeof(line)) != nullptr) {
        /* remove trailing newline characters */
        u_rtrim(line);

        start = line;
        *pErrorCode = U_ZERO_ERROR;

        /* skip this line if it is empty or a comment */
        if (*start == 0 || *start == '#') {
            continue;
        }

        /* remove in-line comments */
        limit = uprv_strchr(start, '#');
        if (limit != nullptr) {
            /* get white space before the pound sign */
            while (limit > start && U_IS_INV_WHITESPACE(*(limit - 1))) {
                --limit;
            }

            /* truncate the line */
            *limit = 0;
        }

        /* skip lines with only whitespace */
        if (u_skipWhitespace(start)[0] == 0) {
            continue;
        }

        /* for each field, call the corresponding field function */
        for (i = 0; i < maxFieldCount; ++i) {
            /* set the limit pointer of this field */
            limit = start;
            while (*limit != delimiter && *limit != 0) {
                ++limit;
            }

            /* set the field start and limit in the fields array */
            fields[i][0] = start;
            fields[i][1] = limit;

            /* set start to the beginning of the next field, if any */
            start = limit;
            if (*start != 0) {
                ++start;
            } else {
                break;
            }
        }
        if (i == maxFieldCount) {
            *pErrorCode = U_PARSE_ERROR;
        }
        int fieldCount = i + 1;

        /* call the field function */
        lineFn(context, fields, fieldCount, pErrorCode);
        if (U_FAILURE(*pErrorCode)) {
            break;
        }
    }

    if (filename != nullptr) {
        T_FileStream_close(file);
    }
}

/**
 * Runs data-driven unit tests for unit preferences. It looks for the test cases
 * in source/test/testdata/cldr/units/unitPreferencesTest.txt, which originates
 * in CLDR.
 */
void UnitsTest::testUnitPreferencesWithCLDRTests() {
    const char *filename = "unitPreferencesTest.txt";
    const int32_t maxFields = 11;
    char *fields[maxFields][2];

    IcuTestErrorCode errorCode(*this, "UnitsTest::testUnitPreferencesWithCLDRTests");
    const char *sourceTestDataPath = getSourceTestData(errorCode);
    if (errorCode.errIfFailureAndReset("unable to find the source/test/testdata "
                                       "folder (getSourceTestData())")) {
        return;
    }

    CharString path(sourceTestDataPath, errorCode);
    path.appendPathPart("cldr/units", errorCode);
    path.appendPathPart(filename, errorCode);

    parsePreferencesTests(path.data(), ';', fields, maxFields, unitPreferencesTestDataLineFn, this,
                          errorCode);
    if (errorCode.errIfFailureAndReset("error parsing %s: %s\n", path.data(), u_errorName(errorCode))) {
        return;
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
