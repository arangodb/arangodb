// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include <stdbool.h>
#include <stdio.h>
#include "unicode/unumberformatter.h"
#include "unicode/usimplenumberformatter.h"
#include "unicode/umisc.h"
#include "unicode/unum.h"
#include "unicode/ustring.h"
#include "cformtst.h"
#include "cintltst.h"
#include "cmemory.h"

static void TestSkeletonFormatToString(void);

static void TestSkeletonFormatToFields(void);

static void TestExampleCode(void);

static void TestSimpleNumberFormatterExample(void);

static void TestSimpleNumberFormatterFull(void);

static void TestFormattedValue(void);

static void TestSkeletonParseError(void);

static void TestToDecimalNumber(void);

static void TestPerUnitInArabic(void);

static void Test21674_State(void);

static void TestNegativeDegrees(void);

void addUNumberFormatterTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/unumberformatter/" #x)

void addUNumberFormatterTest(TestNode** root) {
    TESTCASE(TestSkeletonFormatToString);
    TESTCASE(TestSkeletonFormatToFields);
    TESTCASE(TestExampleCode);
    TESTCASE(TestSimpleNumberFormatterExample);
    TESTCASE(TestSimpleNumberFormatterFull);
    TESTCASE(TestFormattedValue);
    TESTCASE(TestSkeletonParseError);
    TESTCASE(TestToDecimalNumber);
    TESTCASE(TestPerUnitInArabic);
    TESTCASE(Test21674_State);
    TESTCASE(TestNegativeDegrees);
}


#define CAPACITY 30

static void TestSkeletonFormatToString(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UChar buffer[CAPACITY];
    UFormattedNumber* result = NULL;

    // setup:
    UNumberFormatter* f = unumf_openForSkeletonAndLocale(
                              u"precision-integer currency/USD sign-accounting", -1, "en", &ec);
    assertSuccessCheck("Should create without error", &ec, true);
    result = unumf_openResult(&ec);
    assertSuccess("Should create result without error", &ec);

    // int64 test:
    unumf_formatInt(f, -444444, result, &ec);
    // Missing data will give a U_MISSING_RESOURCE_ERROR here.
    if (assertSuccessCheck("Should format integer without error", &ec, true)) {
        unumf_resultToString(result, buffer, CAPACITY, &ec);
        assertSuccess("Should print string to buffer without error", &ec);
        assertUEquals("Should produce expected string result", u"($444,444)", buffer);

        // double test:
        unumf_formatDouble(f, -5142.3, result, &ec);
        assertSuccess("Should format double without error", &ec);
        unumf_resultToString(result, buffer, CAPACITY, &ec);
        assertSuccess("Should print string to buffer without error", &ec);
        assertUEquals("Should produce expected string result", u"($5,142)", buffer);

        // decnumber test:
        unumf_formatDecimal(f, "9.876E2", -1, result, &ec);
        assertSuccess("Should format decimal without error", &ec);
        unumf_resultToString(result, buffer, CAPACITY, &ec);
        assertSuccess("Should print string to buffer without error", &ec);
        assertUEquals("Should produce expected string result", u"$988", buffer);
    }

    // cleanup:
    unumf_closeResult(result);
    unumf_close(f);
}


static void TestSkeletonFormatToFields(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UFieldPositionIterator* ufpositer = NULL;

    // setup:
    UNumberFormatter* uformatter = unumf_openForSkeletonAndLocale(
            u".00 measure-unit/length-meter sign-always", -1, "en", &ec);
    assertSuccessCheck("Should create without error", &ec, true);
    UFormattedNumber* uresult = unumf_openResult(&ec);
    assertSuccess("Should create result without error", &ec);
    unumf_formatInt(uformatter, 9876543210L, uresult, &ec); // "+9,876,543,210.00 m"
    if (assertSuccessCheck("unumf_formatInt() failed", &ec, true)) {

        // field position test:
        UFieldPosition ufpos = {UNUM_DECIMAL_SEPARATOR_FIELD, 0, 0};
        unumf_resultNextFieldPosition(uresult, &ufpos, &ec);
        assertIntEquals("Field position should be correct", 14, ufpos.beginIndex);
        assertIntEquals("Field position should be correct", 15, ufpos.endIndex);

        // field position iterator test:
        ufpositer = ufieldpositer_open(&ec);
        if (assertSuccessCheck("Should create iterator without error", &ec, true)) {

            unumf_resultGetAllFieldPositions(uresult, ufpositer, &ec);
            static const UFieldPosition expectedFields[] = {
                // Field, begin index, end index
                {UNUM_SIGN_FIELD, 0, 1},
                {UNUM_GROUPING_SEPARATOR_FIELD, 2, 3},
                {UNUM_GROUPING_SEPARATOR_FIELD, 6, 7},
                {UNUM_GROUPING_SEPARATOR_FIELD, 10, 11},
                {UNUM_INTEGER_FIELD, 1, 14},
                {UNUM_DECIMAL_SEPARATOR_FIELD, 14, 15},
                {UNUM_FRACTION_FIELD, 15, 17},
                {UNUM_MEASURE_UNIT_FIELD, 18, 19}
            };
            UFieldPosition actual;
            for (int32_t i = 0; i < (int32_t)(sizeof(expectedFields) / sizeof(*expectedFields)); i++) {
                // Iterate using the UFieldPosition to hold state...
                UFieldPosition expected = expectedFields[i];
                actual.field = ufieldpositer_next(ufpositer, &actual.beginIndex, &actual.endIndex);
                assertTrue("Should not return a negative index yet", actual.field >= 0);
                if (expected.field != actual.field) {
                    log_err(
                        "FAIL: iteration %d; expected field %d; got %d\n", i, expected.field, actual.field);
                }
                if (expected.beginIndex != actual.beginIndex) {
                    log_err(
                        "FAIL: iteration %d; expected beginIndex %d; got %d\n",
                        i,
                        expected.beginIndex,
                        actual.beginIndex);
                }
                if (expected.endIndex != actual.endIndex) {
                    log_err(
                        "FAIL: iteration %d; expected endIndex %d; got %d\n",
                        i,
                        expected.endIndex,
                        actual.endIndex);
                }
            }
            actual.field = ufieldpositer_next(ufpositer, &actual.beginIndex, &actual.endIndex);
            assertTrue("No more fields; should return a negative index", actual.field < 0);

            // next field iteration:
            actual.field = UNUM_GROUPING_SEPARATOR_FIELD;
            actual.beginIndex = 0;
            actual.endIndex = 0;
            int32_t i = 1;
            while (unumf_resultNextFieldPosition(uresult, &actual, &ec)) {
                UFieldPosition expected = expectedFields[i++];
                assertIntEquals("Grouping separator begin index", expected.beginIndex, actual.beginIndex);
                assertIntEquals("Grouping separator end index", expected.endIndex, actual.endIndex);
            }
            assertIntEquals("Should have seen all grouping separators", 4, i);
        }
    }

    // cleanup:
    unumf_closeResult(uresult);
    unumf_close(uformatter);
    ufieldpositer_close(ufpositer);
}


static void TestExampleCode(void) {
    // This is the example code given in unumberformatter.h.

    // Setup:
    UErrorCode ec = U_ZERO_ERROR;
    UNumberFormatter* uformatter = unumf_openForSkeletonAndLocale(u"precision-integer", -1, "en", &ec);
    UFormattedNumber* uresult = unumf_openResult(&ec);
    UChar* buffer = NULL;
    assertSuccessCheck("There should not be a failure in the example code", &ec, true);

    // Format a double:
    unumf_formatDouble(uformatter, 5142.3, uresult, &ec);
    if (assertSuccessCheck("There should not be a failure in the example code", &ec, true)) {

        // Export the string to a malloc'd buffer:
        int32_t len = unumf_resultToString(uresult, NULL, 0, &ec);
        assertTrue("No buffer yet", ec == U_BUFFER_OVERFLOW_ERROR);
        ec = U_ZERO_ERROR;
        buffer = (UChar*) uprv_malloc((len+1)*sizeof(UChar));
        unumf_resultToString(uresult, buffer, len+1, &ec);
        assertSuccess("There should not be a failure in the example code", &ec);
        assertUEquals("Should produce expected string result", u"5,142", buffer);
    }

    // Cleanup:
    unumf_close(uformatter);
    unumf_closeResult(uresult);
    uprv_free(buffer);
}


static void TestSimpleNumberFormatterExample(void) {
    // This is the example in usimplenumberformatter.h
    UErrorCode ec = U_ZERO_ERROR;
    USimpleNumberFormatter* uformatter = usnumf_openForLocale("bn", &ec);
    USimpleNumber* unumber = usnum_openForInt64(1000007, &ec);
    UFormattedNumber* uresult = unumf_openResult(&ec);
    usnumf_format(uformatter, unumber, uresult, &ec);
    int32_t len;
    const UChar* str = ufmtval_getString(unumf_resultAsValue(uresult, &ec), &len, &ec);
    if (assertSuccess("Formatting end-to-end 1", &ec)) {
        assertUEquals("Should produce a result in Bangla digits", u"১০,০০,০০৭", str);
    }

    // Cleanup:
    unumf_closeResult(uresult);
    usnum_close(unumber);
    usnumf_close(uformatter);
}


static void TestSimpleNumberFormatterFull(void) {
    UErrorCode ec = U_ZERO_ERROR;
    USimpleNumberFormatter* uformatter = usnumf_openForLocaleAndGroupingStrategy("de-CH", UNUM_GROUPING_ON_ALIGNED, &ec);
    UFormattedNumber* uresult = unumf_openResult(&ec);

    usnumf_formatInt64(uformatter, 4321, uresult, &ec);
    int32_t len;
    const UChar* str = str = ufmtval_getString(unumf_resultAsValue(uresult, &ec), &len, &ec);
    if (assertSuccess("Formatting end-to-end 2", &ec)) {
        assertUEquals("Should produce a result with Swiss symbols", u"4’321", str);
    }

    USimpleNumber* unumber = usnum_openForInt64(1000007, &ec);
    usnum_setToInt64(unumber, 98765, &ec);
    usnum_multiplyByPowerOfTen(unumber, -2, &ec);
    usnum_roundTo(unumber, -1, UNUM_ROUND_HALFDOWN, &ec);
    usnum_setMaximumIntegerDigits(unumber, 1, &ec);
    usnum_setMinimumIntegerDigits(unumber, 4, &ec);
    usnum_setMinimumFractionDigits(unumber, 3, &ec);
    usnum_setSign(unumber, UNUM_SIMPLE_NUMBER_PLUS_SIGN, &ec);

    usnumf_format(uformatter, unumber, uresult, &ec);
    str = ufmtval_getString(unumf_resultAsValue(uresult, &ec), &len, &ec);
    if (assertSuccess("Formatting end-to-end 3", &ec)) {
        assertUEquals("Should produce a result with mutated number", u"+0’007.600", str);
    }

    // Cleanup:
    unumf_closeResult(uresult);
    usnum_close(unumber);
    usnumf_close(uformatter);
}


static void TestFormattedValue(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UNumberFormatter* uformatter = unumf_openForSkeletonAndLocale(
            u".00 compact-short", -1, "en", &ec);
    assertSuccessCheck("Should create without error", &ec, true);
    UFormattedNumber* uresult = unumf_openResult(&ec);
    assertSuccess("Should create result without error", &ec);

    unumf_formatInt(uformatter, 55000, uresult, &ec); // "55.00 K"
    if (assertSuccessCheck("Should format without error", &ec, true)) {
        const UFormattedValue* fv = unumf_resultAsValue(uresult, &ec);
        assertSuccess("Should convert without error", &ec);
        static const UFieldPosition expectedFieldPositions[] = {
            // field, begin index, end index
            {UNUM_INTEGER_FIELD, 0, 2},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 2, 3},
            {UNUM_FRACTION_FIELD, 3, 5},
            {UNUM_COMPACT_FIELD, 5, 6}};
        checkFormattedValue(
            "FormattedNumber as FormattedValue",
            fv,
            u"55.00K",
            UFIELD_CATEGORY_NUMBER,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    // cleanup:
    unumf_closeResult(uresult);
    unumf_close(uformatter);
}


static void TestSkeletonParseError(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UNumberFormatter* uformatter;
    UParseError perror;

    // The UParseError can be null. The following should not segfault.
    uformatter = unumf_openForSkeletonAndLocaleWithError(
            u".00 measure-unit/typo", -1, "en", NULL, &ec);
    unumf_close(uformatter);

    // Now test the behavior.
    ec = U_ZERO_ERROR;
    uformatter = unumf_openForSkeletonAndLocaleWithError(
            u".00 measure-unit/typo", -1, "en", &perror, &ec);

    assertIntEquals("Should have set error code", U_NUMBER_SKELETON_SYNTAX_ERROR, ec);
    assertIntEquals("Should have correct skeleton error offset", 17, perror.offset);
    assertUEquals("Should have correct pre context", u"0 measure-unit/", perror.preContext);
    assertUEquals("Should have correct post context", u"typo", perror.postContext);

    // cleanup:
    unumf_close(uformatter);
}


static void TestToDecimalNumber(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UNumberFormatter* uformatter = unumf_openForSkeletonAndLocale(
        u"currency/USD",
        -1,
        "en-US",
        &ec);
    assertSuccessCheck("Should create without error", &ec, true);
    UFormattedNumber* uresult = unumf_openResult(&ec);
    assertSuccess("Should create result without error", &ec);

    unumf_formatDouble(uformatter, 3.0, uresult, &ec);
    const UChar* str = ufmtval_getString(unumf_resultAsValue(uresult, &ec), NULL, &ec);
    assertSuccessCheck("Formatting should succeed", &ec, true);
    assertUEquals("Should produce expected string result", u"$3.00", str);

    char buffer[CAPACITY];

    int32_t len = unumf_resultToDecimalNumber(uresult, buffer, CAPACITY, &ec);
    assertIntEquals("Length should be as expected", strlen(buffer), len);
    assertEquals("Decimal should be as expected", "3", buffer);

    // cleanup:
    unumf_closeResult(uresult);
    unumf_close(uformatter);
}


static void TestPerUnitInArabic(void) {
    const char* simpleMeasureUnits[] = {
        "area-acre",
        "digital-bit",
        "digital-byte",
        "temperature-celsius",
        "length-centimeter",
        "duration-day",
        "angle-degree",
        "temperature-fahrenheit",
        "volume-fluid-ounce",
        "length-foot",
        "volume-gallon",
        "digital-gigabit",
        "digital-gigabyte",
        "mass-gram",
        "area-hectare",
        "duration-hour",
        "length-inch",
        "digital-kilobit",
        "digital-kilobyte",
        "mass-kilogram",
        "length-kilometer",
        "volume-liter",
        "digital-megabit",
        "digital-megabyte",
        "length-meter",
        "length-mile",
        "length-mile-scandinavian",
        "volume-milliliter",
        "length-millimeter",
        "duration-millisecond",
        "duration-minute",
        "duration-month",
        "mass-ounce",
        "concentr-percent",
        "digital-petabyte",
        "mass-pound",
        "duration-second",
        "mass-stone",
        "digital-terabit",
        "digital-terabyte",
        "duration-week",
        "length-yard",
        "duration-year"
    };
#define BUFFER_LEN 256
    char buffer[BUFFER_LEN];
    UChar ubuffer[BUFFER_LEN];
    const char* locale = "ar";
    UErrorCode status = U_ZERO_ERROR;
    UFormattedNumber* formatted = unumf_openResult(&status);
    if (U_FAILURE(status)) {
        log_err("FAIL: unumf_openResult failed");
        return;
    }
    for(int32_t i=0; i < UPRV_LENGTHOF(simpleMeasureUnits); ++i) {
        for(int32_t j=0; j < UPRV_LENGTHOF(simpleMeasureUnits); ++j) {
            status = U_ZERO_ERROR;
            snprintf(buffer, sizeof(buffer), "measure-unit/%s per-measure-unit/%s",
                    simpleMeasureUnits[i], simpleMeasureUnits[j]);
            int32_t outputlen = 0;
            u_strFromUTF8(ubuffer, BUFFER_LEN, &outputlen, buffer, (int32_t)strlen(buffer), &status);
            if (U_FAILURE(status)) {
                log_err("FAIL u_strFromUTF8: %s = %s ( %s )\n", locale, buffer,
                        u_errorName(status));
            }
            UNumberFormatter* nf = unumf_openForSkeletonAndLocale(
                ubuffer, outputlen, locale, &status);
            if (U_FAILURE(status)) {
                log_err("FAIL unumf_openForSkeletonAndLocale: %s = %s ( %s )\n",
                        locale, buffer, u_errorName(status));
            } else {
                unumf_formatDouble(nf, 1, formatted, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL unumf_formatDouble: %s = %s ( %s )\n",
                            locale, buffer, u_errorName(status));
                }
            }
            unumf_close(nf);
        }
    }
    unumf_closeResult(formatted);
}


static void Test21674_State(void) {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormatter* nf = NULL;
    UFormattedNumber* result = NULL;

    nf = unumf_openForSkeletonAndLocale(u"precision-increment/0.05/w", -1, "en", &status);
    if (!assertSuccess("unumf_openForSkeletonAndLocale", &status)) { goto cleanup; }

    result = unumf_openResult(&status);
    if (!assertSuccess("unumf_openResult", &status)) { goto cleanup; }

    typedef struct TestCase {
        double num;
        const UChar* expected;
    } TestCase;
    TestCase cases[] = {
        { 1.975, u"2" },
        { 1.97, u"1.95" },
        { 1.975, u"2" },
    };
    for (int i=0; i<3; i++) {
        unumf_formatDouble(nf, cases[i].num, result, &status);
        if (!assertSuccess("unumf_formatDouble", &status)) { goto cleanup; }

        const UFormattedValue* formattedValue = unumf_resultAsValue(result, &status);
        if (!assertSuccess("unumf_resultAsValue", &status)) { goto cleanup; }

        int32_t length;
        const UChar* str = ufmtval_getString(formattedValue, &length, &status);
        if (!assertSuccess("ufmtval_getString", &status)) { goto cleanup; }

        char message[] = {i + '0', '\0'};
        assertUEquals(message, cases[i].expected, str);
    }

cleanup:
    unumf_close(nf);
    unumf_closeResult(result);
}

// Test for ICU-22105
static void TestNegativeDegrees(void) {
    typedef struct {
        const UChar* skeleton;
        double value;
        const UChar* expectedResult;
    } TestCase;
    
    TestCase testCases[] = {
        { u"measure-unit/temperature-celsius unit-width-short",               0,  u"0°C" },
        { u"measure-unit/temperature-celsius unit-width-short usage/default", 0,  u"32°F" },
        { u"measure-unit/temperature-celsius unit-width-short usage/weather", 0,  u"32°F" },

        { u"measure-unit/temperature-celsius unit-width-short",               -1, u"-1°C" },
        { u"measure-unit/temperature-celsius unit-width-short usage/default", -1, u"30°F" },
        { u"measure-unit/temperature-celsius unit-width-short usage/weather", -1, u"30°F" }
    };
    
    for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        UErrorCode err = U_ZERO_ERROR;
        UNumberFormatter* nf = unumf_openForSkeletonAndLocale(testCases[i].skeleton, -1, "en_US", &err);
        UFormattedNumber* fn = unumf_openResult(&err);
        
        if (assertSuccess("Failed to create formatter or result", &err)) {
            UChar result[200];
            unumf_formatDouble(nf, testCases[i].value, fn, &err);
            unumf_resultToString(fn, result, 200, &err);
            
            if (assertSuccess("Formatting number failed", &err)) {
                assertUEquals("Got wrong result", testCases[i].expectedResult, result);
            }
        }
        
        unumf_closeResult(fn);
        unumf_close(nf);
    }
}


#endif /* #if !UCONFIG_NO_FORMATTING */
