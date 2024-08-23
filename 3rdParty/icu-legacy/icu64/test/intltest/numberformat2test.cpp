// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File NUMBERFORMAT2TEST.CPP
*
*******************************************************************************
*/
#include "unicode/utypes.h"

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/localpointer.h"
#include "unicode/plurrule.h"

#include "affixpatternparser.h"
#include "charstr.h"
#include "datadrivennumberformattestsuite.h"
#include "decimalformatpattern.h"
#include "digitaffixesandpadding.h"
#include "digitformatter.h"
#include "digitgrouping.h"
#include "digitinterval.h"
#include "digitlst.h"
#include "fphdlimp.h"
#include "plurrule_impl.h"
#include "precision.h"
#include "significantdigitinterval.h"
#include "smallintformatter.h"
#include "uassert.h"
#include "valueformatter.h"
#include "visibledigits.h"

struct NumberFormat2Test_Attributes {
    int32_t id;
    int32_t spos;
    int32_t epos;
};

class NumberFormat2Test_FieldPositionHandler : public FieldPositionHandler {
public:
NumberFormat2Test_Attributes attributes[100];
int32_t count;
UBool bRecording;



NumberFormat2Test_FieldPositionHandler() : count(0), bRecording(TRUE) { attributes[0].spos = -1; }
NumberFormat2Test_FieldPositionHandler(UBool recording) : count(0), bRecording(recording) { attributes[0].spos = -1; }
virtual ~NumberFormat2Test_FieldPositionHandler();
virtual void addAttribute(int32_t id, int32_t start, int32_t limit);
virtual void shiftLast(int32_t delta);
virtual UBool isRecording(void) const;
};
 
NumberFormat2Test_FieldPositionHandler::~NumberFormat2Test_FieldPositionHandler() {
}

void NumberFormat2Test_FieldPositionHandler::addAttribute(
        int32_t id, int32_t start, int32_t limit) {
    if (count == UPRV_LENGTHOF(attributes) - 1) {
        return;
    }
    attributes[count].id = id;
    attributes[count].spos = start;
    attributes[count].epos = limit;
    ++count;
    attributes[count].spos = -1;
}

void NumberFormat2Test_FieldPositionHandler::shiftLast(int32_t /* delta */) {
}

UBool NumberFormat2Test_FieldPositionHandler::isRecording() const {
    return bRecording;
}


class NumberFormat2Test : public IntlTest {
public:
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestQuantize();
    void TestConvertScientificNotation();
    void TestLowerUpperExponent();
    void TestRounding();
    void TestRoundingIncrement();
    void TestDigitInterval();
    void TestGroupingUsed();
    void TestBenchmark();
    void TestBenchmark2();
    void TestSmallIntFormatter();
    void TestPositiveIntDigitFormatter();
    void TestDigitListInterval();
    void TestLargeIntValue();
    void TestIntInitVisibleDigits();
    void TestIntInitVisibleDigitsToDigitList();
    void TestDoubleInitVisibleDigits();
    void TestDoubleInitVisibleDigitsToDigitList();
    void TestDigitListInitVisibleDigits();
    void TestSpecialInitVisibleDigits();
    void TestVisibleDigitsWithExponent();
    void TestDigitAffixesAndPadding();
    void TestPluralsAndRounding();
    void TestPluralsAndRoundingScientific();
    void TestValueFormatterIsFastFormattable();
    void TestCurrencyAffixInfo();
    void TestAffixPattern();
    void TestAffixPatternAppend();
    void TestAffixPatternAppendAjoiningLiterals();
    void TestAffixPatternDoubleQuote();
    void TestAffixPatternParser();
    void TestPluralAffix();
    void TestDigitAffix();
    void TestDigitFormatterDefaultCtor();
    void TestDigitFormatterMonetary();
    void TestDigitFormatter();
    void TestSciFormatterDefaultCtor();
    void TestSciFormatter();
    void TestToPatternScientific11648();
    void verifyInterval(const DigitInterval &, int32_t minInclusive, int32_t maxExclusive);
    void verifyAffix(
            const UnicodeString &expected,
            const DigitAffix &affix,
            const NumberFormat2Test_Attributes *expectedAttributes);
    void verifyAffixesAndPadding(
            const UnicodeString &expected,
            const DigitAffixesAndPadding &aaf,
            DigitList &digits,
            const ValueFormatter &vf,
            const PluralRules *optPluralRules,
            const NumberFormat2Test_Attributes *expectedAttributes);
    void verifyAffixesAndPaddingInt32(
            const UnicodeString &expected,
            const DigitAffixesAndPadding &aaf,
            int32_t value,
            const ValueFormatter &vf,
            const PluralRules *optPluralRules,
            const NumberFormat2Test_Attributes *expectedAttributes);
    void verifyDigitList(
        const UnicodeString &expected,
        const DigitList &digits);
    void verifyVisibleDigits(
        const UnicodeString &expected,
        UBool bNegative,
        const VisibleDigits &digits);
    void verifyVisibleDigitsWithExponent(
        const UnicodeString &expected,
        UBool bNegative,
        const VisibleDigitsWithExponent &digits);
    void verifyDigitFormatter(
            const UnicodeString &expected,
            const DigitFormatter &formatter,
            const VisibleDigits &digits,
            const DigitGrouping &grouping,
            const DigitFormatterOptions &options,
            const NumberFormat2Test_Attributes *expectedAttributes);
    void verifySciFormatter(
            const UnicodeString &expected,
            const DigitFormatter &formatter,
            const VisibleDigitsWithExponent &digits,
            const SciFormatterOptions &options,
            const NumberFormat2Test_Attributes *expectedAttributes);
    void verifySmallIntFormatter(
            const UnicodeString &expected,
            int32_t positiveValue,
            int32_t minDigits,
            int32_t maxDigits);
    void verifyPositiveIntDigitFormatter(
            const UnicodeString &expected,
            const DigitFormatter &formatter,
            int32_t value,
            int32_t minDigits,
            int32_t maxDigits,
            const NumberFormat2Test_Attributes *expectedAttributes);
    void verifyAttributes(
            const NumberFormat2Test_Attributes *expected,
            const NumberFormat2Test_Attributes *actual);
    void verifyIntValue(
            int64_t expected, const VisibleDigits &digits);
    void verifySource(
            double expected, const VisibleDigits &digits);
};

void NumberFormat2Test::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite ScientificNumberFormatterTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestQuantize);
    TESTCASE_AUTO(TestConvertScientificNotation);
    TESTCASE_AUTO(TestLowerUpperExponent);
    TESTCASE_AUTO(TestRounding);
    TESTCASE_AUTO(TestRoundingIncrement);
    TESTCASE_AUTO(TestDigitInterval);
    TESTCASE_AUTO(TestGroupingUsed);
    TESTCASE_AUTO(TestDigitListInterval);
    TESTCASE_AUTO(TestDigitFormatterDefaultCtor);
    TESTCASE_AUTO(TestDigitFormatterMonetary);
    TESTCASE_AUTO(TestDigitFormatter);
    TESTCASE_AUTO(TestSciFormatterDefaultCtor);
    TESTCASE_AUTO(TestSciFormatter);
    TESTCASE_AUTO(TestBenchmark);
    TESTCASE_AUTO(TestBenchmark2);
    TESTCASE_AUTO(TestSmallIntFormatter);
    TESTCASE_AUTO(TestPositiveIntDigitFormatter);
    TESTCASE_AUTO(TestCurrencyAffixInfo);
    TESTCASE_AUTO(TestAffixPattern);
    TESTCASE_AUTO(TestAffixPatternAppend);
    TESTCASE_AUTO(TestAffixPatternAppendAjoiningLiterals);
    TESTCASE_AUTO(TestAffixPatternDoubleQuote);
    TESTCASE_AUTO(TestAffixPatternParser);
    TESTCASE_AUTO(TestPluralAffix);
    TESTCASE_AUTO(TestDigitAffix);
    TESTCASE_AUTO(TestValueFormatterIsFastFormattable);
    TESTCASE_AUTO(TestLargeIntValue);
    TESTCASE_AUTO(TestIntInitVisibleDigits);
    TESTCASE_AUTO(TestIntInitVisibleDigitsToDigitList);
    TESTCASE_AUTO(TestDoubleInitVisibleDigits);
    TESTCASE_AUTO(TestDoubleInitVisibleDigitsToDigitList);
    TESTCASE_AUTO(TestDigitListInitVisibleDigits);
    TESTCASE_AUTO(TestSpecialInitVisibleDigits);
    TESTCASE_AUTO(TestVisibleDigitsWithExponent);
    TESTCASE_AUTO(TestDigitAffixesAndPadding);
    TESTCASE_AUTO(TestPluralsAndRounding);
    TESTCASE_AUTO(TestPluralsAndRoundingScientific);
    TESTCASE_AUTO(TestToPatternScientific11648);
 
    TESTCASE_AUTO_END;
}

void NumberFormat2Test::TestDigitInterval() {
    DigitInterval all;
    DigitInterval threeInts;
    DigitInterval fourFrac;
    threeInts.setIntDigitCount(3);
    fourFrac.setFracDigitCount(4);
    verifyInterval(all, INT32_MIN, INT32_MAX);
    verifyInterval(threeInts, INT32_MIN, 3);
    verifyInterval(fourFrac, -4, INT32_MAX);
    {
        DigitInterval result(threeInts);
        result.shrinkToFitWithin(fourFrac);
        verifyInterval(result, -4, 3);
        assertEquals("", 7, result.length());
    }
    {
        DigitInterval result(threeInts);
        result.expandToContain(fourFrac);
        verifyInterval(result, INT32_MIN, INT32_MAX);
    }
    {
        DigitInterval result(threeInts);
        result.setIntDigitCount(0);
        verifyInterval(result, INT32_MIN, 0);
        result.setIntDigitCount(-1);
        verifyInterval(result, INT32_MIN, INT32_MAX);
    }
    {
        DigitInterval result(fourFrac);
        result.setFracDigitCount(0);
        verifyInterval(result, 0, INT32_MAX);
        result.setFracDigitCount(-1);
        verifyInterval(result, INT32_MIN, INT32_MAX);
    }
    {
        DigitInterval result;
        result.setIntDigitCount(3);
        result.setFracDigitCount(1);
        result.expandToContainDigit(0);
        result.expandToContainDigit(-1);
        result.expandToContainDigit(2);
        verifyInterval(result, -1, 3);
        result.expandToContainDigit(3);
        verifyInterval(result, -1, 4);
        result.expandToContainDigit(-2);
        verifyInterval(result, -2, 4);
        result.expandToContainDigit(15);
        result.expandToContainDigit(-15);
        verifyInterval(result, -15, 16);
    }
    {
        DigitInterval result;
        result.setIntDigitCount(3);
        result.setFracDigitCount(1);
        assertTrue("", result.contains(2));
        assertTrue("", result.contains(-1));
        assertFalse("", result.contains(3));
        assertFalse("", result.contains(-2));
    }
}

void NumberFormat2Test::verifyInterval(
        const DigitInterval &interval,
        int32_t minInclusive, int32_t maxExclusive) {
    assertEquals("", minInclusive, interval.getLeastSignificantInclusive());
    assertEquals("", maxExclusive, interval.getMostSignificantExclusive());
    assertEquals("", maxExclusive, interval.getIntDigitCount());
}

void NumberFormat2Test::TestGroupingUsed() {
    {
        DigitGrouping grouping;
        assertFalse("", grouping.isGroupingUsed());
    }
    {
        DigitGrouping grouping;
        grouping.fGrouping = 2;
        assertTrue("", grouping.isGroupingUsed());
    }
}

void NumberFormat2Test::TestDigitListInterval() {
    DigitInterval result;
    DigitList digitList;
    {
        digitList.set((int32_t)12345);
        verifyInterval(digitList.getSmallestInterval(result), 0, 5);
    }
    {
        digitList.set(1000.00);
        verifyInterval(digitList.getSmallestInterval(result), 0, 4);
    }
    {
        digitList.set(43.125);
        verifyInterval(digitList.getSmallestInterval(result), -3, 2);
    }
    {
        digitList.set(.0078125);
        verifyInterval(digitList.getSmallestInterval(result), -7, 0);
    }
    {
        digitList.set(1000.00);
        digitList.getSmallestInterval(result);
        result.expandToContainDigit(3);
        verifyInterval(result, 0, 4);
    }
    {
        digitList.set(1000.00);
        digitList.getSmallestInterval(result);
        result.expandToContainDigit(4);
        verifyInterval(result, 0, 5);
    }
    {
        digitList.set(1000.00);
        digitList.getSmallestInterval(result);
        result.expandToContainDigit(0);
        verifyInterval(result, 0, 4);
    }
    {
        digitList.set(1000.00);
        digitList.getSmallestInterval(result);
        result.expandToContainDigit(-1);
        verifyInterval(result, -1, 4);
    }
    {
        digitList.set(43.125);
        digitList.getSmallestInterval(result);
        result.expandToContainDigit(1);
        verifyInterval(result, -3, 2);
    }
    {
        digitList.set(43.125);
        digitList.getSmallestInterval(result);
        result.expandToContainDigit(2);
        verifyInterval(result, -3, 3);
    }
    {
        digitList.set(43.125);
        digitList.getSmallestInterval(result);
        result.expandToContainDigit(-3);
        verifyInterval(result, -3, 2);
    }
    {
        digitList.set(43.125);
        digitList.getSmallestInterval(result);
        result.expandToContainDigit(-4);
        verifyInterval(result, -4, 2);
    }
}

void NumberFormat2Test::TestQuantize() {
    DigitList quantity;
    quantity.set(0.00168);
    quantity.roundAtExponent(-5);
    DigitList digits;
    UErrorCode status = U_ZERO_ERROR;
    {
        digits.set((int32_t)1);
        digits.quantize(quantity, status);
        verifyDigitList(".9996", digits);
    }
    {
        // round half even up
        digits.set(1.00044);
        digits.roundAtExponent(-5);
        digits.quantize(quantity, status);
        verifyDigitList("1.00128", digits);
    }
    {
        // round half down
        digits.set(0.99876);
        digits.roundAtExponent(-5);
        digits.quantize(quantity, status);
        verifyDigitList(".99792", digits);
    }
    assertSuccess("", status);
}

void NumberFormat2Test::TestConvertScientificNotation() {
    DigitList digits;
    {
        digits.set((int32_t)186283);
        assertEquals("", 5, digits.toScientific(1, 1));
        verifyDigitList(
                "1.86283",
                digits);
    }
    {
        digits.set((int32_t)186283);
        assertEquals("", 0, digits.toScientific(6, 1));
        verifyDigitList(
                "186283",
                digits);
    }
    {
        digits.set((int32_t)186283);
        assertEquals("", -2, digits.toScientific(8, 1));
        verifyDigitList(
                "18628300",
                digits);
    }
    {
        digits.set((int32_t)43561);
        assertEquals("", 6, digits.toScientific(-1, 3));
        verifyDigitList(
                ".043561",
                digits);
    }
    {
        digits.set((int32_t)43561);
        assertEquals("", 3, digits.toScientific(0, 3));
        verifyDigitList(
                "43.561",
                digits);
    }
    {
        digits.set((int32_t)43561);
        assertEquals("", 3, digits.toScientific(2, 3));
        verifyDigitList(
                "43.561",
                digits);
    }
    {
        digits.set((int32_t)43561);
        assertEquals("", 0, digits.toScientific(3, 3));
        verifyDigitList(
                "43561",
                digits);
    }
    {
        digits.set((int32_t)43561);
        assertEquals("", 0, digits.toScientific(5, 3));
        verifyDigitList(
                "43561",
                digits);
    }
    {
        digits.set((int32_t)43561);
        assertEquals("", -3, digits.toScientific(6, 3));
        verifyDigitList(
                "43561000",
                digits);
    }
    {
        digits.set((int32_t)43561);
        assertEquals("", -3, digits.toScientific(8, 3));
        verifyDigitList(
                "43561000",
                digits);
    }
    {
        digits.set((int32_t)43561);
        assertEquals("", -6, digits.toScientific(9, 3));
        verifyDigitList(
                "43561000000",
                digits);
    }
}

void NumberFormat2Test::TestLowerUpperExponent() {
    DigitList digits;

    digits.set(98.7);
    assertEquals("", -1, digits.getLowerExponent());
    assertEquals("", 2, digits.getUpperExponent());
}

void NumberFormat2Test::TestRounding() {
    DigitList digits;
    uprv_decContextSetRounding(&digits.fContext, DEC_ROUND_CEILING);
    {
        // Round at very large exponent
        digits.set(789.123);
        digits.roundAtExponent(100);
        verifyDigitList(
                "10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", // 100 0's after 1
                digits);
    }
    {
        // Round at very large exponent
        digits.set(789.123);
        digits.roundAtExponent(1);
        verifyDigitList(
                "790", // 100 0's after 1
                digits);
    }
    {
        // Round at positive exponent
        digits.set(789.123);
        digits.roundAtExponent(1);
        verifyDigitList("790", digits);
    }
    {
        // Round at zero exponent
        digits.set(788.123);
        digits.roundAtExponent(0);
        verifyDigitList("789", digits);
    }
    {
        // Round at negative exponent
        digits.set(789.123);
        digits.roundAtExponent(-2);
        verifyDigitList("789.13", digits);
    }
    {
        // Round to exponent of digits.
        digits.set(789.123);
        digits.roundAtExponent(-3);
        verifyDigitList("789.123", digits);
    }
    {
        // Round at large negative exponent
        digits.set(789.123);
        digits.roundAtExponent(-100);
        verifyDigitList("789.123", digits);
    }
    {
        // Round negative
        digits.set(-789.123);
        digits.roundAtExponent(-2);
        digits.setPositive(TRUE);
        verifyDigitList("789.12", digits);
    }
    {
        // Round to 1 significant digit
        digits.set(789.123);
        digits.roundAtExponent(INT32_MIN, 1);
        verifyDigitList("800", digits);
    }
    {
        // Round to 5 significant digit
        digits.set(789.123);
        digits.roundAtExponent(INT32_MIN, 5);
        verifyDigitList("789.13", digits);
    }
    {
        // Round to 6 significant digit
        digits.set(789.123);
        digits.roundAtExponent(INT32_MIN, 6);
        verifyDigitList("789.123", digits);
    }
    {
        // no-op
        digits.set(789.123);
        digits.roundAtExponent(INT32_MIN, INT32_MAX);
        verifyDigitList("789.123", digits);
    }
    {
        // Rounding at -1 produces fewer than 5 significant digits
        digits.set(789.123);
        digits.roundAtExponent(-1, 5);
        verifyDigitList("789.2", digits);
    }
    {
        // Rounding at -1 produces exactly 4 significant digits
        digits.set(789.123);
        digits.roundAtExponent(-1, 4);
        verifyDigitList("789.2", digits);
    }
    {
        // Rounding at -1 produces more than 3 significant digits
        digits.set(788.123);
        digits.roundAtExponent(-1, 3);
        verifyDigitList("789", digits);
    }
    {
        digits.set(123.456);
        digits.round(INT32_MAX);
        verifyDigitList("123.456", digits);
    }
    {
        digits.set(123.456);
        digits.round(1);
        verifyDigitList("200", digits);
    }
}
void NumberFormat2Test::TestBenchmark() {
/*
    UErrorCode status = U_ZERO_ERROR;
    Locale en("en");
    DecimalFormatSymbols *sym = new DecimalFormatSymbols(en, status);
    DecimalFormat2 fmt(en, "0.0000000", status);
    FieldPosition fpos(FieldPostion::DONT_CARE);
    clock_t start = clock();
    for (int32_t i = 0; i < 100000; ++i) {
       UParseError perror;
       DecimalFormat2 fmt2("0.0000000", new DecimalFormatSymbols(*sym), perror, status);
//       UnicodeString append;
//       fmt.format(4.6692016, append, fpos, status);
    }
    errln("Took %f", (double) (clock() - start) / CLOCKS_PER_SEC);
    assertSuccess("", status);
*/
}

void NumberFormat2Test::TestBenchmark2() {
/*
    UErrorCode status = U_ZERO_ERROR;
    Locale en("en");
    DecimalFormatSymbols *sym = new DecimalFormatSymbols(en, status);
    DecimalFormat fmt("0.0000000", sym, status);
    FieldPosition fpos(FieldPostion::DONT_CARE);
    clock_t start = clock();
    for (int32_t i = 0; i < 100000; ++i) {
      UParseError perror;
      DecimalFormat fmt("0.0000000", new DecimalFormatSymbols(*sym), perror, status);
//        UnicodeString append;
//        fmt.format(4.6692016, append, fpos, status);
    }
    errln("Took %f", (double) (clock() - start) / CLOCKS_PER_SEC);
    assertSuccess("", status);
*/
}

void NumberFormat2Test::TestSmallIntFormatter() {
    verifySmallIntFormatter("0", 7, 0, -2);
    verifySmallIntFormatter("7", 7, 1, -2);
    verifySmallIntFormatter("07", 7, 2, -2);
    verifySmallIntFormatter("07", 7, 2, 2);
    verifySmallIntFormatter("007", 7, 3, 4);
    verifySmallIntFormatter("7", 7, -1, 3);
    verifySmallIntFormatter("0", 0, -1, 3);
    verifySmallIntFormatter("057", 57, 3, 7);
    verifySmallIntFormatter("0057", 57, 4, 7);
    // too many digits for small int
    verifySmallIntFormatter("", 57, 5, 7);
    // too many digits for small int
    verifySmallIntFormatter("", 57, 5, 4);
    verifySmallIntFormatter("03", 3, 2, 3);
    verifySmallIntFormatter("32", 32, 2, 3);
    verifySmallIntFormatter("321", 321, 2, 3);
    verifySmallIntFormatter("219", 3219, 2, 3);
    verifySmallIntFormatter("4095", 4095, 2, 4);
    verifySmallIntFormatter("4095", 4095, 2, 5);
    verifySmallIntFormatter("", 4096, 2, 5);
}

void NumberFormat2Test::TestPositiveIntDigitFormatter() {
    DigitFormatter formatter;
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 4},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "0057",
                formatter,
                57,
                4,
                INT32_MAX,
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 5},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "00057",
                formatter,
                57,
                5,
                INT32_MAX,
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 5},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "01000",
                formatter,
                1000,
                5,
                INT32_MAX,
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 3},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "100",
                formatter,
                100,
                0,
                INT32_MAX,
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 10},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "2147483647",
                formatter,
                2147483647,
                5,
                INT32_MAX,
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 12},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "002147483647",
                formatter,
                2147483647,
                12,
                INT32_MAX,
                expectedAttributes);
    }
    {
        // Test long digit string where we have to append one
        // character at a time.
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 40},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "0000000000000000000000000000002147483647",
                formatter,
                2147483647,
                40,
                INT32_MAX,
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 4},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "6283",
                formatter,
                186283,
                2,
                4,
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "0",
                formatter,
                186283,
                0,
                0,
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {0, -1, 0}};
        verifyPositiveIntDigitFormatter(
                "3",
                formatter,
                186283,
                1,
                1,
                expectedAttributes);
    }
}


void NumberFormat2Test::TestDigitFormatterDefaultCtor() {
    DigitFormatter formatter;
    VisibleDigits digits;
    FixedPrecision precision;
    UErrorCode status = U_ZERO_ERROR;
    precision.initVisibleDigits(246.801, digits, status);
    assertSuccess("", status);
    DigitGrouping grouping;
    DigitFormatterOptions options;
    verifyDigitFormatter(
            "246.801",
            formatter,
            digits,
            grouping,
            options,
            NULL);
}

void NumberFormat2Test::TestDigitFormatterMonetary() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en", status);
    if (!assertSuccess("", status)) {
        return;
    }
    symbols.setSymbol(
            DecimalFormatSymbols::kMonetarySeparatorSymbol,
            "decimal separator");
    symbols.setSymbol(
            DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol,
            "grouping separator");
    DigitFormatter formatter(symbols);
    VisibleDigits visibleDigits;
    DigitGrouping grouping;
    FixedPrecision precision;
    precision.initVisibleDigits(43560.02, visibleDigits, status);
    if (!assertSuccess("", status)) {
        return;
    }
    DigitFormatterOptions options;
    grouping.fGrouping = 3;
    {
        verifyDigitFormatter(
                "43,560.02",
                formatter,
                visibleDigits,
                grouping,
                options,
                NULL);
        formatter.setDecimalFormatSymbolsForMonetary(symbols);
        verifyDigitFormatter(
                "43grouping separator560decimal separator02",
                formatter,
                visibleDigits,
                grouping,
                options,
                NULL);
    }
}

void NumberFormat2Test::TestDigitFormatter() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en", status);
    if (!assertSuccess("", status)) {
        return;
    }
    DigitFormatter formatter(symbols);
    DigitInterval interval;
    {
        VisibleDigits visibleDigits;
        DigitGrouping grouping;
        FixedPrecision precision;
        precision.initVisibleDigits((int64_t) 8192, visibleDigits, status);
        if (!assertSuccess("", status)) {
            return;
        }
        DigitFormatterOptions options;
        verifyDigitFormatter(
                "8192",
                formatter,
                visibleDigits,
                grouping,
                options,
                NULL);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 4},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 4, 5},
            {0, -1, 0}};
        options.fAlwaysShowDecimal = TRUE;
        verifyDigitFormatter(
                "8192.",
                formatter,
                visibleDigits,
                grouping,
                options,
                expectedAttributes);

        // Turn on grouping
        grouping.fGrouping = 3;
        options.fAlwaysShowDecimal = FALSE;
        verifyDigitFormatter(
                "8,192",
                formatter,
                visibleDigits,
                grouping,
                options,
                NULL);

        // turn on min grouping which will suppress grouping
        grouping.fMinGrouping = 2;
        verifyDigitFormatter(
                "8192",
                formatter,
                visibleDigits,
                grouping,
                options,
                NULL);

        // adding one more digit will enable grouping once again.
        precision.initVisibleDigits((int64_t) 43560, visibleDigits, status);
        if (!assertSuccess("", status)) {
            return;
        }
        verifyDigitFormatter(
                "43,560",
                formatter,
                visibleDigits,
                grouping,
                options,
                NULL);
    }
    {
        DigitGrouping grouping;
        FixedPrecision precision;
        VisibleDigits visibleDigits;
        precision.initVisibleDigits(
                31415926.0078125, visibleDigits, status);
        if (!assertSuccess("", status)) {
            return;
        }
        DigitFormatterOptions options;
        verifyDigitFormatter(
                "31415926.0078125",
                formatter,
                visibleDigits,
                grouping,
                options,
                NULL);

        // Turn on grouping with secondary.
        grouping.fGrouping = 2;
        grouping.fGrouping2 = 3;
        verifyDigitFormatter(
                "314,159,26.0078125",
                formatter,
                visibleDigits,
                grouping,
                options,
                NULL);

        // Pad with zeros by widening interval.
        precision.fMin.setIntDigitCount(9);
        precision.fMin.setFracDigitCount(10);
        precision.initVisibleDigits(
                31415926.0078125, visibleDigits, status);
        if (!assertSuccess("", status)) {
            return;
        }
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_GROUPING_SEPARATOR_FIELD, 1, 2},
            {UNUM_GROUPING_SEPARATOR_FIELD, 5, 6},
            {UNUM_GROUPING_SEPARATOR_FIELD, 9, 10},
            {UNUM_INTEGER_FIELD, 0, 12},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 12, 13},
            {UNUM_FRACTION_FIELD, 13, 23},
            {0, -1, 0}};
        verifyDigitFormatter(
                "0,314,159,26.0078125000",
                formatter,
                visibleDigits,
                grouping,
                options,
                expectedAttributes);
    }
    {
        DigitGrouping grouping;
        FixedPrecision precision;
        VisibleDigits visibleDigits;
        DigitFormatterOptions options;
        precision.fMax.setIntDigitCount(0);
        precision.fMax.setFracDigitCount(0);
        precision.initVisibleDigits(
                3125.0, visibleDigits, status);
        if (!assertSuccess("", status)) {
            return;
        }
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {0, -1, 0}};
        verifyDigitFormatter(
                "0",
                formatter,
                visibleDigits,
                grouping,
                options,
                expectedAttributes);
        NumberFormat2Test_Attributes expectedAttributesWithDecimal[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 1, 2},
            {0, -1, 0}};
        options.fAlwaysShowDecimal = TRUE;
        verifyDigitFormatter(
                "0.",
                formatter,
                visibleDigits,
                grouping,
                options,
                expectedAttributesWithDecimal);
    }
    {
        DigitGrouping grouping;
        FixedPrecision precision;
        VisibleDigits visibleDigits;
        DigitFormatterOptions options;
        precision.fMax.setIntDigitCount(1);
        precision.fMin.setFracDigitCount(1);
        precision.initVisibleDigits(
                3125.0, visibleDigits, status);
        if (!assertSuccess("", status)) {
            return;
        }
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 1, 2},
            {UNUM_FRACTION_FIELD, 2, 3},
            {0, -1, 0}};
        options.fAlwaysShowDecimal = TRUE;
        verifyDigitFormatter(
                "5.0",
                formatter,
                visibleDigits,
                grouping,
                options,
                expectedAttributes);
    }
}

void NumberFormat2Test::TestSciFormatterDefaultCtor() {
    DigitFormatter formatter;
    ScientificPrecision precision;
    VisibleDigitsWithExponent visibleDigits;
    UErrorCode status = U_ZERO_ERROR;
    precision.initVisibleDigitsWithExponent(
            6.02E23, visibleDigits, status);
    if (!assertSuccess("", status)) {
        return;
    }
    SciFormatterOptions options;
    verifySciFormatter(
            "6.02E23",
            formatter,
            visibleDigits,
            options,
            NULL);
    precision.initVisibleDigitsWithExponent(
            6.62E-34, visibleDigits, status);
    if (!assertSuccess("", status)) {
        return;
    }
    verifySciFormatter(
            "6.62E-34",
            formatter,
            visibleDigits,
            options,
            NULL);
}

void NumberFormat2Test::TestSciFormatter() {
    DigitFormatter formatter;
    ScientificPrecision precision;
    precision.fMantissa.fMin.setIntDigitCount(4);
    precision.fMantissa.fMax.setIntDigitCount(4);
    precision.fMantissa.fMin.setFracDigitCount(0);
    precision.fMantissa.fMax.setFracDigitCount(0);
    precision.fMinExponentDigits = 3;
    VisibleDigitsWithExponent visibleDigits;
    UErrorCode status = U_ZERO_ERROR;
    precision.initVisibleDigitsWithExponent(
            1.248E26, visibleDigits, status);
    if (!assertSuccess("", status)) {
        return;
    }
    SciFormatterOptions options;

    {
        options.fExponent.fAlwaysShowSign = TRUE;
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 4},
            {UNUM_EXPONENT_SYMBOL_FIELD, 4, 5},
            {UNUM_EXPONENT_SIGN_FIELD, 5, 6},
            {UNUM_EXPONENT_FIELD, 6, 9},
            {0, -1, 0}};
        verifySciFormatter(
                "1248E+023",
                formatter,
                visibleDigits,
                options,
                expectedAttributes);
    }
    {
        options.fMantissa.fAlwaysShowDecimal = TRUE;
        options.fExponent.fAlwaysShowSign = FALSE;
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 4},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 4, 5},
            {UNUM_EXPONENT_SYMBOL_FIELD, 5, 6},
            {UNUM_EXPONENT_FIELD, 6, 9},
            {0, -1, 0}};
        verifySciFormatter(
                "1248.E023",
                formatter,
                visibleDigits,
                options,
                expectedAttributes);
    }
}

void NumberFormat2Test::TestValueFormatterIsFastFormattable() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en", status);
    if (!assertSuccess("", status)) {
        return;
    }
    DigitFormatter formatter(symbols);
    DigitGrouping grouping;
    FixedPrecision precision;
    DigitFormatterOptions options;
    ValueFormatter vf;
    vf.prepareFixedDecimalFormatting(
            formatter, grouping, precision, options);
    assertTrue("", vf.isFastFormattable(0));
    assertTrue("", vf.isFastFormattable(35));
    assertTrue("", vf.isFastFormattable(-48));
    assertTrue("", vf.isFastFormattable(2147483647));
    assertTrue("", vf.isFastFormattable(-2147483647));
    assertFalse("", vf.isFastFormattable(-2147483648L));
    {
        DigitGrouping grouping;
        grouping.fGrouping = 3;
        ValueFormatter vf;
        vf.prepareFixedDecimalFormatting(
                formatter, grouping, precision, options);
        assertTrue("0", vf.isFastFormattable(0));
        assertTrue("62", vf.isFastFormattable(62));
        assertTrue("999", vf.isFastFormattable(999));
        assertFalse("1000", vf.isFastFormattable(1000));
        assertTrue("-1", vf.isFastFormattable(-1));
        assertTrue("-38", vf.isFastFormattable(-38));
        assertTrue("-999", vf.isFastFormattable(-999));
        assertFalse("-1000", vf.isFastFormattable(-1000));
        grouping.fMinGrouping = 2;
        assertTrue("-1000", vf.isFastFormattable(-1000));
        assertTrue("-4095", vf.isFastFormattable(-4095));
        assertTrue("4095", vf.isFastFormattable(4095));
        // We give up on acounting digits at 4096
        assertFalse("-4096", vf.isFastFormattable(-4096));
        assertFalse("4096", vf.isFastFormattable(4096));
    }
    {
        // grouping on but with max integer digits set.
        DigitGrouping grouping;
        grouping.fGrouping = 4;
        FixedPrecision precision;
        precision.fMax.setIntDigitCount(4);
        ValueFormatter vf;
        vf.prepareFixedDecimalFormatting(
                formatter, grouping, precision, options);
        assertTrue("-4096", vf.isFastFormattable(-4096));
        assertTrue("4096", vf.isFastFormattable(4096));
        assertTrue("-10000", vf.isFastFormattable(-10000));
        assertTrue("10000", vf.isFastFormattable(10000));
        assertTrue("-2147483647", vf.isFastFormattable(-2147483647));
        assertTrue("2147483647", vf.isFastFormattable(2147483647));

        precision.fMax.setIntDigitCount(5);
        assertFalse("-4096", vf.isFastFormattable(-4096));
        assertFalse("4096", vf.isFastFormattable(4096));

    }
    {
        // grouping on but with min integer digits set.
        DigitGrouping grouping;
        grouping.fGrouping = 3;
        FixedPrecision precision;
        precision.fMin.setIntDigitCount(3);
        ValueFormatter vf;
        vf.prepareFixedDecimalFormatting(
                formatter, grouping, precision, options);
        assertTrue("-999", vf.isFastFormattable(-999));
        assertTrue("999", vf.isFastFormattable(999));
        assertFalse("-1000", vf.isFastFormattable(-1000));
        assertFalse("1000", vf.isFastFormattable(1000));

        precision.fMin.setIntDigitCount(4);
        assertFalse("-999", vf.isFastFormattable(-999));
        assertFalse("999", vf.isFastFormattable(999));
        assertFalse("-2147483647", vf.isFastFormattable(-2147483647));
        assertFalse("2147483647", vf.isFastFormattable(2147483647));
    }
    {
        // options set.
        DigitFormatterOptions options;
        ValueFormatter vf;
        vf.prepareFixedDecimalFormatting(
                formatter, grouping, precision, options);
        assertTrue("5125", vf.isFastFormattable(5125));
        options.fAlwaysShowDecimal = TRUE;
        assertFalse("5125", vf.isFastFormattable(5125));
        options.fAlwaysShowDecimal = FALSE;
        assertTrue("5125", vf.isFastFormattable(5125));
    }
    {
        // test fraction digits
        FixedPrecision precision;
        ValueFormatter vf;
        vf.prepareFixedDecimalFormatting(
                formatter, grouping, precision, options);
        assertTrue("7127", vf.isFastFormattable(7127));
        precision.fMin.setFracDigitCount(1);
        assertFalse("7127", vf.isFastFormattable(7127));
    }
    {
        // test presence of significant digits
        FixedPrecision precision;
        ValueFormatter vf;
        vf.prepareFixedDecimalFormatting(
                formatter, grouping, precision, options);
        assertTrue("1049", vf.isFastFormattable(1049));
        precision.fSignificant.setMin(1);
        assertFalse("1049", vf.isFastFormattable(1049));
    }
    {
        // test presence of rounding increment
        FixedPrecision precision;
        ValueFormatter vf;
        vf.prepareFixedDecimalFormatting(
                formatter, grouping, precision, options);
        assertTrue("1099", vf.isFastFormattable(1099));
        precision.fRoundingIncrement.set(2.3);
        assertFalse("1099", vf.isFastFormattable(1099));
    }
    {
        // test scientific notation
        ScientificPrecision precision;
        SciFormatterOptions options;
        ValueFormatter vf;
        vf.prepareScientificFormatting(
                formatter, precision, options);
        assertFalse("1081", vf.isFastFormattable(1081));
    }
}

void NumberFormat2Test::TestDigitAffix() {
    DigitAffix affix;
    {
        affix.append("foo");
        affix.append("--", UNUM_SIGN_FIELD);
        affix.append("%", UNUM_PERCENT_FIELD);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 3, 5},
            {UNUM_PERCENT_FIELD, 5, 6},
            {0, -1, 0}};
        verifyAffix("foo--%", affix, expectedAttributes);
    }
    {
        affix.remove();
        affix.append("USD", UNUM_CURRENCY_FIELD);
        affix.append(" ");
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_CURRENCY_FIELD, 0, 3},
            {0, -1, 0}};
        verifyAffix("USD ", affix, expectedAttributes);
    }
    {
        affix.setTo("%%", UNUM_PERCENT_FIELD);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_PERCENT_FIELD, 0, 2},
            {0, -1, 0}};
        verifyAffix("%%", affix, expectedAttributes);
    }
}

void NumberFormat2Test::TestPluralAffix() {
    UErrorCode status = U_ZERO_ERROR;
    PluralAffix part;
    part.setVariant("one", "Dollar", status);
    part.setVariant("few", "DollarFew", status);
    part.setVariant("other", "Dollars", status);
    PluralAffix dollar(part);
    PluralAffix percent(part);
    part.remove();
    part.setVariant("one", "Percent", status);
    part.setVariant("many", "PercentMany", status);
    part.setVariant("other", "Percents", status);
    percent = part;
    part.remove();
    part.setVariant("one", "foo", status);

    PluralAffix pa;
    assertEquals("", "", pa.getOtherVariant().toString());
    pa.append(dollar, UNUM_CURRENCY_FIELD, status);
    pa.append(" and ");
    pa.append(percent, UNUM_PERCENT_FIELD, status);
    pa.append("-", UNUM_SIGN_FIELD);

    {
        // other
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_CURRENCY_FIELD, 0, 7},
            {UNUM_PERCENT_FIELD, 12, 20},
            {UNUM_SIGN_FIELD, 20, 21},
            {0, -1, 0}};
        verifyAffix(
                "Dollars and Percents-",
                pa.getByCategory("other"),
                expectedAttributes);
    }
    {
        // two which is same as other
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_CURRENCY_FIELD, 0, 7},
            {UNUM_PERCENT_FIELD, 12, 20},
            {UNUM_SIGN_FIELD, 20, 21},
            {0, -1, 0}};
        verifyAffix(
                "Dollars and Percents-",
                pa.getByCategory("two"),
                expectedAttributes);
    }
    {
        // bad which is same as other
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_CURRENCY_FIELD, 0, 7},
            {UNUM_PERCENT_FIELD, 12, 20},
            {UNUM_SIGN_FIELD, 20, 21},
            {0, -1, 0}};
        verifyAffix(
                "Dollars and Percents-",
                pa.getByCategory("bad"),
                expectedAttributes);
    }
    {
        // one
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_CURRENCY_FIELD, 0, 6},
            {UNUM_PERCENT_FIELD, 11, 18},
            {UNUM_SIGN_FIELD, 18, 19},
            {0, -1, 0}};
        verifyAffix(
                "Dollar and Percent-",
                pa.getByCategory("one"),
                expectedAttributes);
    }
    {
        // few
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_CURRENCY_FIELD, 0, 9},
            {UNUM_PERCENT_FIELD, 14, 22},
            {UNUM_SIGN_FIELD, 22, 23},
            {0, -1, 0}};
        verifyAffix(
                "DollarFew and Percents-",
                pa.getByCategory("few"),
                expectedAttributes);
    }
    {
        // many
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_CURRENCY_FIELD, 0, 7},
            {UNUM_PERCENT_FIELD, 12, 23},
            {UNUM_SIGN_FIELD, 23, 24},
            {0, -1, 0}};
        verifyAffix(
                "Dollars and PercentMany-",
                pa.getByCategory("many"),
                expectedAttributes);
    }
    assertTrue("", pa.hasMultipleVariants());
    pa.remove();
    pa.append("$$$", UNUM_CURRENCY_FIELD);
    assertFalse("", pa.hasMultipleVariants());
    
}

void NumberFormat2Test::TestCurrencyAffixInfo() {
    CurrencyAffixInfo info;
    assertTrue("", info.isDefault());
    UnicodeString expectedSymbol("\\u00a4");
    UnicodeString expectedSymbolIso("\\u00a4\\u00a4");
    UnicodeString expectedSymbols("\\u00a4\\u00a4\\u00a4");
    assertEquals("", expectedSymbol.unescape(), info.getSymbol());
    assertEquals("", expectedSymbolIso.unescape(), info.getISO());
    assertEquals("", expectedSymbols.unescape(), info.getLong().getByCategory("one").toString());
    assertEquals("", expectedSymbols.unescape(), info.getLong().getByCategory("other").toString());
    assertEquals("", expectedSymbols.unescape(), info.getLong().getByCategory("two").toString());
    UErrorCode status = U_ZERO_ERROR;
    static UChar USD[] = {0x55, 0x53, 0x44, 0x0};
    LocalPointer<PluralRules> rules(PluralRules::forLocale("en", status));
    if (!assertSuccess("", status)) {
        return;
    }
    info.set("en", rules.getAlias(), USD, status);
    assertEquals("", "$", info.getSymbol(), TRUE);
    assertEquals("", "USD", info.getISO(), TRUE);
    assertEquals("", "US dollar", info.getLong().getByCategory("one").toString(), TRUE);
    assertEquals("", "US dollars", info.getLong().getByCategory("other").toString(), TRUE);
    assertEquals("", "US dollars", info.getLong().getByCategory("two").toString(), TRUE);
    assertFalse("", info.isDefault());
    info.set(NULL, NULL, NULL, status);
    assertTrue("", info.isDefault());
    assertEquals("", expectedSymbol.unescape(), info.getSymbol());
    assertEquals("", expectedSymbolIso.unescape(), info.getISO());
    assertEquals("", expectedSymbols.unescape(), info.getLong().getByCategory("one").toString());
    assertEquals("", expectedSymbols.unescape(), info.getLong().getByCategory("other").toString());
    assertEquals("", expectedSymbols.unescape(), info.getLong().getByCategory("two").toString());
    info.setSymbol("$");
    assertFalse("", info.isDefault());
    info.set(NULL, NULL, NULL, status);
    assertTrue("", info.isDefault());
    info.setISO("USD");
    assertFalse("", info.isDefault());
    assertSuccess("", status);
}

void NumberFormat2Test::TestAffixPattern() {
    static UChar chars[500];
    for (int32_t i = 0; i < UPRV_LENGTHOF(chars); ++i) {
        chars[i] = (UChar) (i + 1);
    }
    AffixPattern first;
    first.add(AffixPattern::kPercent);
    first.addLiteral(chars, 0, 200);
    first.addLiteral(chars, 200, 300);
    first.addCurrency(2);
    first.addLiteral(chars, 0, 256);
    AffixPattern second;
    second.add(AffixPattern::kPercent);
    second.addLiteral(chars, 0, 300);
    second.addLiteral(chars, 300, 200);
    second.addCurrency(2);
    second.addLiteral(chars, 0, 150);
    second.addLiteral(chars, 150, 106);
    assertTrue("", first.equals(second));
    AffixPatternIterator iter;
    second.remove();
    assertFalse("", second.iterator(iter).nextToken());
    assertTrue("", first.iterator(iter).nextToken());
    assertEquals("", (int32_t)AffixPattern::kPercent, iter.getTokenType());
    assertEquals("", 1, iter.getTokenLength());
    assertTrue("", iter.nextToken());
    UnicodeString str;
    assertEquals("", 500, iter.getLiteral(str).length());
    assertEquals("", (int32_t)AffixPattern::kLiteral, iter.getTokenType());
    assertEquals("", 500, iter.getTokenLength());
    assertTrue("", iter.nextToken());
    assertEquals("", (int32_t)AffixPattern::kCurrency, iter.getTokenType());
    assertEquals("", 2, iter.getTokenLength());
    assertTrue("", iter.nextToken());
    assertEquals("", 256, iter.getLiteral(str).length());
    assertEquals("", (int32_t)AffixPattern::kLiteral, iter.getTokenType());
    assertEquals("", 256, iter.getTokenLength());
    assertFalse("", iter.nextToken());
}

void NumberFormat2Test::TestAffixPatternDoubleQuote() {
    UnicodeString str("'Don''t'");
    AffixPattern expected;
    // Don't
    static UChar chars[] = {0x44, 0x6F, 0x6E, 0x27, 0x74};
    expected.addLiteral(chars, 0, UPRV_LENGTHOF(chars));
    AffixPattern actual;
    UErrorCode status = U_ZERO_ERROR;
    AffixPattern::parseUserAffixString(str, actual, status);
    assertTrue("", expected.equals(actual));
    UnicodeString formattedString;
    assertEquals("", "Don''t", actual.toUserString(formattedString));
    assertSuccess("", status);
}

void NumberFormat2Test::TestAffixPatternParser() {
    UErrorCode status = U_ZERO_ERROR;
    static UChar USD[] = {0x55, 0x53, 0x44, 0x0};
    LocalPointer<PluralRules> rules(PluralRules::forLocale("en", status));
    DecimalFormatSymbols symbols("en", status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating DecimalFormatSymbols - %s", u_errorName(status));
        return;
    }
    AffixPatternParser parser(symbols);
    CurrencyAffixInfo currencyAffixInfo;
    currencyAffixInfo.set("en", rules.getAlias(), USD, status);
    PluralAffix affix;
    UnicodeString str("'--y'''dz'%'\\u00a4\\u00a4\\u00a4\\u00a4 y '\\u00a4\\u00a4\\u00a4 or '\\u00a4\\u00a4 but '\\u00a4");
    str = str.unescape();
    assertSuccess("", status);
    AffixPattern affixPattern;
    parser.parse(
            AffixPattern::parseAffixString(str, affixPattern, status),
            currencyAffixInfo,
            affix,
            status);
    UnicodeString formattedStr;
    affixPattern.toString(formattedStr);
    UnicodeString expectedFormattedStr("'--y''dz'%'\\u00a4\\u00a4\\u00a4\\u00a4 y '\\u00a4\\u00a4\\u00a4 or '\\u00a4\\u00a4 but '\\u00a4");
    expectedFormattedStr = expectedFormattedStr.unescape();
    assertEquals("1", expectedFormattedStr, formattedStr);
    AffixPattern userAffixPattern;
    UnicodeString userStr("-'-'y'''d'z%\\u00a4\\u00a4\\u00a4'\\u00a4' y \\u00a4\\u00a4\\u00a4 or \\u00a4\\u00a4 but \\u00a4");
    userStr = userStr.unescape();
    AffixPattern::parseUserAffixString(userStr, userAffixPattern, status),
    assertTrue("", affixPattern.equals(userAffixPattern));
    AffixPattern userAffixPattern2;
    UnicodeString formattedUserStr;
    AffixPattern::parseUserAffixString(
            userAffixPattern.toUserString(formattedUserStr),
            userAffixPattern2,
            status);
    UnicodeString expectedFormattedUserStr(
            "-'-'y''dz%\\u00a4\\u00a4\\u00a4'\\u00a4' y \\u00a4\\u00a4\\u00a4 or \\u00a4\\u00a4 but \\u00a4");
    assertEquals("2", expectedFormattedUserStr.unescape(), formattedUserStr);
    assertTrue("", userAffixPattern2.equals(userAffixPattern));
    assertSuccess("", status);
    assertTrue("", affixPattern.usesCurrency());
    assertTrue("", affixPattern.usesPercent());
    assertFalse("", affixPattern.usesPermill());
    assertTrue("", affix.hasMultipleVariants());
    {
        // other
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 1},
            {UNUM_PERCENT_FIELD, 6, 7},
            {UNUM_CURRENCY_FIELD, 7, 17},
            {UNUM_CURRENCY_FIELD, 21, 31},
            {UNUM_CURRENCY_FIELD, 35, 38},
            {UNUM_CURRENCY_FIELD, 43, 44},
            {0, -1, 0}};
        verifyAffix(
                "--y'dz%US dollars\\u00a4 y US dollars or USD but $",
                affix.getByCategory("other"),
                expectedAttributes);
    }
    {
        // one
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 1},
            {UNUM_PERCENT_FIELD, 6, 7},
            {UNUM_CURRENCY_FIELD, 7, 16},
            {UNUM_CURRENCY_FIELD, 20, 29},
            {UNUM_CURRENCY_FIELD, 33, 36},
            {UNUM_CURRENCY_FIELD, 41, 42},
            {0, -1, 0}};
        verifyAffix(
                "--y'dz%US dollar\\u00a4 y US dollar or USD but $",
                affix.getByCategory("one"),
                expectedAttributes);
    }
    affix.remove();
    str = "%'-";
    affixPattern.remove();
    parser.parse(
            AffixPattern::parseAffixString(str, affixPattern, status),
            currencyAffixInfo,
            affix,
            status);
    assertSuccess("", status);
    assertFalse("", affixPattern.usesCurrency());
    assertFalse("", affixPattern.usesPercent());
    assertFalse("", affixPattern.usesPermill());
    assertFalse("", affix.hasMultipleVariants());
    {
        // other
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 1, 2},
            {0, -1, 0}};
        verifyAffix(
                "%-",
                affix.getByCategory("other"),
                expectedAttributes);
    }
    UnicodeString a4("\\u00a4");
    AffixPattern scratchPattern;
    AffixPattern::parseAffixString(a4.unescape(), scratchPattern, status);
    assertFalse("", scratchPattern.usesCurrency());

    // Test really long string > 256 chars.
    str = "'\\u2030012345678901234567890123456789012345678901234567890123456789"
          "012345678901234567890123456789012345678901234567890123456789"
          "012345678901234567890123456789012345678901234567890123456789"
          "012345678901234567890123456789012345678901234567890123456789"
          "012345678901234567890123456789012345678901234567890123456789";
    str = str.unescape();
    affixPattern.remove();
    affix.remove();
    parser.parse(
            AffixPattern::parseAffixString(str, affixPattern, status),
            currencyAffixInfo,
            affix,
            status);
    assertSuccess("", status);
    assertFalse("", affixPattern.usesCurrency());
    assertFalse("", affixPattern.usesPercent());
    assertTrue("", affixPattern.usesPermill());
    assertFalse("", affix.hasMultipleVariants());
    {
       UnicodeString expected =
           "\\u2030012345678901234567890123456789012345678901234567890123456789"
           "012345678901234567890123456789012345678901234567890123456789"
           "012345678901234567890123456789012345678901234567890123456789"
           "012345678901234567890123456789012345678901234567890123456789"
           "012345678901234567890123456789012345678901234567890123456789";
        expected = expected.unescape();
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_PERMILL_FIELD, 0, 1},
            {0, -1, 0}};
        verifyAffix(
                expected,
                affix.getOtherVariant(),
                expectedAttributes);
    }
}

void NumberFormat2Test::TestAffixPatternAppend() {
  AffixPattern pattern;
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString patternStr("%\\u2030");
  AffixPattern::parseUserAffixString(
          patternStr.unescape(), pattern, status);

  AffixPattern appendPattern;
  UnicodeString appendPatternStr("-\\u00a4\\u00a4*");
  AffixPattern::parseUserAffixString(
          appendPatternStr.unescape(), appendPattern, status);

  AffixPattern expectedPattern;
  UnicodeString expectedPatternStr("%\\u2030-\\u00a4\\u00a4*");
  AffixPattern::parseUserAffixString(
          expectedPatternStr.unescape(), expectedPattern, status);
  
  assertTrue("", pattern.append(appendPattern).equals(expectedPattern));
  assertSuccess("", status);
}

void NumberFormat2Test::TestAffixPatternAppendAjoiningLiterals() {
  AffixPattern pattern;
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString patternStr("%baaa");
  AffixPattern::parseUserAffixString(
          patternStr, pattern, status);

  AffixPattern appendPattern;
  UnicodeString appendPatternStr("caa%");
  AffixPattern::parseUserAffixString(
          appendPatternStr, appendPattern, status);

  AffixPattern expectedPattern;
  UnicodeString expectedPatternStr("%baaacaa%");
  AffixPattern::parseUserAffixString(
          expectedPatternStr, expectedPattern, status);
  
  assertTrue("", pattern.append(appendPattern).equals(expectedPattern));
  assertSuccess("", status);
}

void NumberFormat2Test::TestLargeIntValue() {
    VisibleDigits digits;
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;

        // Last 18 digits for int values.
        verifyIntValue(
                223372036854775807LL, 
                precision.initVisibleDigits(U_INT64_MAX, digits, status));
        assertSuccess("U_INT64_MAX", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setIntDigitCount(5);

        // Last 18 digits for int values.
        verifyIntValue(
                75807LL, 
                precision.initVisibleDigits(U_INT64_MAX, digits, status));
        verifySource(75807.0, digits);
        assertSuccess("75807", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;

        // Last 18 digits for int values.
        verifyIntValue(
                223372036854775808LL, 
                precision.initVisibleDigits(U_INT64_MIN, digits, status));
        assertSuccess("U_INT64_MIN", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setIntDigitCount(5);

        // Last 18 digits for int values.
        verifyIntValue(
                75808LL, 
                precision.initVisibleDigits(U_INT64_MIN, digits, status));
        verifySource(75808.0, digits);
        assertSuccess("75808", status);
    }
        
}

void NumberFormat2Test::TestIntInitVisibleDigits() {
    VisibleDigits digits;
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        verifyVisibleDigits(
                "13",
                FALSE,
                precision.initVisibleDigits((int64_t) 13LL, digits, status));
        assertSuccess("13", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        verifyVisibleDigits(
                "17",
                TRUE,
                precision.initVisibleDigits((int64_t) -17LL, digits, status));
        assertSuccess("-17", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        verifyVisibleDigits(
                "9223372036854775808",
                TRUE,
                precision.initVisibleDigits(U_INT64_MIN, digits, status));
        assertSuccess("-9223372036854775808", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        verifyVisibleDigits(
                "9223372036854775807",
                FALSE,
                precision.initVisibleDigits(U_INT64_MAX, digits, status));
        assertSuccess("9223372036854775807", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        verifyVisibleDigits(
                "31536000",
                TRUE,
                precision.initVisibleDigits((int64_t) -31536000LL, digits, status));
        assertSuccess("-31536000", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        verifyVisibleDigits(
                "0",
                FALSE,
                precision.initVisibleDigits((int64_t) 0LL, digits, status));
        assertSuccess("0", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMin.setIntDigitCount(4);
        precision.fMin.setFracDigitCount(2);
        verifyVisibleDigits(
                "0000.00",
                FALSE,
                precision.initVisibleDigits((int64_t) 0LL, digits, status));
        assertSuccess("0", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMin.setIntDigitCount(4);
        precision.fMin.setFracDigitCount(2);
        verifyVisibleDigits(
                "0057.00",
                FALSE,
                precision.initVisibleDigits((int64_t) 57LL, digits, status));
        assertSuccess("57", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMin.setIntDigitCount(4);
        precision.fMin.setFracDigitCount(2);
        verifyVisibleDigits(
                "0057.00",
                TRUE,
                precision.initVisibleDigits((int64_t) -57LL, digits, status));
        assertSuccess("-57", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setIntDigitCount(2);
        precision.fMin.setFracDigitCount(1);
        verifyVisibleDigits(
                "35.0",
                FALSE,
                precision.initVisibleDigits((int64_t) 235LL, digits, status));
        assertSuccess("235", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setIntDigitCount(2);
        precision.fMin.setFracDigitCount(1);
        precision.fFailIfOverMax = TRUE;
        precision.initVisibleDigits((int64_t) 239LL, digits, status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("239: Expected U_ILLEGAL_ARGUMENT_ERROR");
        }
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMin(5);
        verifyVisibleDigits(
                "153.00",
                FALSE,
                precision.initVisibleDigits((int64_t) 153LL, digits, status));
        assertSuccess("153", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(2);
        precision.fExactOnly = TRUE;
        precision.initVisibleDigits((int64_t) 154LL, digits, status);
        if (status != U_FORMAT_INEXACT_ERROR) {
            errln("154: Expected U_FORMAT_INEXACT_ERROR");
        }
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(5);
        verifyVisibleDigits(
                "150",
                FALSE,
                precision.initVisibleDigits((int64_t) 150LL, digits, status));
        assertSuccess("150", status);
    }
}

void NumberFormat2Test::TestIntInitVisibleDigitsToDigitList() {
    VisibleDigits digits;
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fRoundingIncrement.set(7.3);
        verifyVisibleDigits(
                "29.2",
                TRUE,
                precision.initVisibleDigits((int64_t) -30LL, digits, status));
        assertSuccess("-29.2", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fRoundingIncrement.set(7.3);
        precision.fRoundingMode = DecimalFormat::kRoundFloor;
        verifyVisibleDigits(
                "36.5",
                TRUE,
                precision.initVisibleDigits((int64_t) -30LL, digits, status));
        assertSuccess("-36.5", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(3);
        precision.fRoundingMode = DecimalFormat::kRoundCeiling;
        verifyVisibleDigits(
                "1390",
                FALSE,
                precision.initVisibleDigits((int64_t) 1381LL, digits, status));
        assertSuccess("1390", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(1);
        precision.fRoundingMode = DecimalFormat::kRoundFloor;
        verifyVisibleDigits(
                "2000",
                TRUE,
                precision.initVisibleDigits((int64_t) -1381LL, digits, status));
        assertSuccess("-2000", status);
    }
}

void NumberFormat2Test::TestDoubleInitVisibleDigits() {
    VisibleDigits digits;
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        verifyVisibleDigits(
                "2.05",
                FALSE,
                precision.initVisibleDigits(2.05, digits, status));
        assertSuccess("2.05", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        verifyVisibleDigits(
                "3547",
                FALSE,
                precision.initVisibleDigits(3547.0, digits, status));
        assertSuccess("3547", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setFracDigitCount(2);
        precision.fMax.setIntDigitCount(1);
        precision.fFailIfOverMax = TRUE;
        precision.fExactOnly = TRUE;
        verifyVisibleDigits(
                "2.05",
                TRUE,
                precision.initVisibleDigits(-2.05, digits, status));
        assertSuccess("-2.05", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setFracDigitCount(1);
        precision.fMax.setIntDigitCount(1);
        precision.fFailIfOverMax = TRUE;
        precision.fExactOnly = TRUE;
        precision.initVisibleDigits(-2.05, digits, status);
        if (status != U_FORMAT_INEXACT_ERROR) {
            errln("6245.3: Expected U_FORMAT_INEXACT_ERROR");
        }
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setFracDigitCount(2);
        precision.fMax.setIntDigitCount(0);
        precision.fFailIfOverMax = TRUE;
        precision.fExactOnly = TRUE;
        precision.initVisibleDigits(-2.05, digits, status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("-2.05: Expected U_ILLEGAL_ARGUMENT_ERROR");
        }
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMin.setIntDigitCount(5);
        precision.fMin.setFracDigitCount(2);
        precision.fExactOnly = TRUE;
        verifyVisibleDigits(
                "06245.30",
                FALSE,
                precision.initVisibleDigits(6245.3, digits, status));
        assertSuccess("06245.30", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(5);
        precision.fExactOnly = TRUE;
        verifyVisibleDigits(
                "6245.3",
                FALSE,
                precision.initVisibleDigits(6245.3, digits, status));
        assertSuccess("6245.3", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(4);
        precision.fExactOnly = TRUE;
        precision.initVisibleDigits(6245.3, digits, status);
        if (status != U_FORMAT_INEXACT_ERROR) {
            errln("6245.3: Expected U_FORMAT_INEXACT_ERROR");
        }
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setIntDigitCount(3);
        precision.fMin.setFracDigitCount(2);
        verifyVisibleDigits(
                "384.90",
                FALSE,
                precision.initVisibleDigits(2384.9, digits, status));
        assertSuccess("380.00", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setIntDigitCount(3);
        precision.fMin.setFracDigitCount(2);
        precision.fFailIfOverMax = TRUE;
        precision.initVisibleDigits(2384.9, digits, status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("2384.9: Expected U_ILLEGAL_ARGUMENT_ERROR");
        }
    }
}

void NumberFormat2Test::TestDoubleInitVisibleDigitsToDigitList() {
    VisibleDigits digits;
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        // 2.01 produces round off error when multiplied by powers of
        // 10 forcing the use of DigitList.
        verifyVisibleDigits(
                "2.01",
                TRUE,
                precision.initVisibleDigits(-2.01, digits, status));
        assertSuccess("-2.01", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(3);
        precision.fMin.setFracDigitCount(2);
        verifyVisibleDigits(
                "2380.00",
                FALSE,
                precision.initVisibleDigits(2385.0, digits, status));
        assertSuccess("2380.00", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setFracDigitCount(2);
        verifyVisibleDigits(
                "45.83",
                TRUE,
                precision.initVisibleDigits(-45.8251, digits, status));
        assertSuccess("45.83", status);
    }
}

void NumberFormat2Test::TestDigitListInitVisibleDigits() {
    VisibleDigits digits;
    DigitList dlist;
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fMax.setIntDigitCount(3);
        precision.fMin.setFracDigitCount(2);
        precision.fFailIfOverMax = TRUE;
        dlist.set(2384.9);
        precision.initVisibleDigits(dlist, digits, status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("2384.9: Expected U_ILLEGAL_ARGUMENT_ERROR");
        }
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(4);
        precision.fExactOnly = TRUE;
        dlist.set(6245.3);
        precision.initVisibleDigits(dlist, digits, status);
        if (status != U_FORMAT_INEXACT_ERROR) {
            errln("6245.3: Expected U_FORMAT_INEXACT_ERROR");
        }
    }
}

void NumberFormat2Test::TestSpecialInitVisibleDigits() {
    VisibleDigits digits;
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.fSignificant.setMax(3);
        precision.fMin.setFracDigitCount(2);
        precision.initVisibleDigits(-uprv_getInfinity(), digits, status);
        assertFalse("", digits.isNaN());
        assertTrue("", digits.isInfinite());
        assertTrue("", digits.isNegative());
        assertSuccess("-Inf", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.initVisibleDigits(uprv_getInfinity(), digits, status);
        assertFalse("", digits.isNaN());
        assertTrue("", digits.isInfinite());
        assertFalse("", digits.isNegative());
        assertSuccess("Inf", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        FixedPrecision precision;
        precision.initVisibleDigits(uprv_getNaN(), digits, status);
        assertTrue("", digits.isNaN());
        assertSuccess("Inf", status);
    }
}

void NumberFormat2Test::TestVisibleDigitsWithExponent() {
    VisibleDigitsWithExponent digits;
    {
        UErrorCode status = U_ZERO_ERROR;
        ScientificPrecision precision;
        precision.initVisibleDigitsWithExponent(389.256, digits, status);
        verifyVisibleDigitsWithExponent(
                "3.89256E2", FALSE, digits);
        assertSuccess("3.89256E2", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        ScientificPrecision precision;
        precision.initVisibleDigitsWithExponent(-389.256, digits, status);
        verifyVisibleDigitsWithExponent(
                "3.89256E2", TRUE, digits);
        assertSuccess("-3.89256E2", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        ScientificPrecision precision;
        precision.fMinExponentDigits = 3;
        precision.fMantissa.fMin.setIntDigitCount(1);
        precision.fMantissa.fMax.setIntDigitCount(3);
        precision.initVisibleDigitsWithExponent(12345.67, digits, status);
        verifyVisibleDigitsWithExponent(
                "12.34567E003", FALSE, digits);
        assertSuccess("12.34567E003", status);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        ScientificPrecision precision;
        precision.fMantissa.fRoundingIncrement.set(0.073);
        precision.fMantissa.fMin.setIntDigitCount(2);
        precision.fMantissa.fMax.setIntDigitCount(2);
        precision.initVisibleDigitsWithExponent(999.74, digits, status);
        verifyVisibleDigitsWithExponent(
                "10.001E2", FALSE, digits);
        assertSuccess("10.001E2", status);
    }
}

void NumberFormat2Test::TestDigitAffixesAndPadding() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en", status);
    if (!assertSuccess("", status)) {
        return;
    }
    DigitFormatter formatter(symbols);
    DigitGrouping grouping;
    grouping.fGrouping = 3;
    FixedPrecision precision;
    DigitFormatterOptions options;
    options.fAlwaysShowDecimal = TRUE;
    ValueFormatter vf;
    vf.prepareFixedDecimalFormatting(
            formatter,
            grouping,
            precision,
            options);
    DigitAffixesAndPadding aap;
    aap.fPositivePrefix.append("(+", UNUM_SIGN_FIELD);
    aap.fPositiveSuffix.append("+)", UNUM_SIGN_FIELD);
    aap.fNegativePrefix.append("(-", UNUM_SIGN_FIELD);
    aap.fNegativeSuffix.append("-)", UNUM_SIGN_FIELD);
    aap.fWidth = 10;
    aap.fPadPosition = DigitAffixesAndPadding::kPadBeforePrefix;
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 4, 6},
            {UNUM_INTEGER_FIELD, 6, 7},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 7, 8},
            {UNUM_SIGN_FIELD, 8, 10},
            {0, -1, 0}};
        verifyAffixesAndPaddingInt32(
                "****(+3.+)",
                aap,
                3,
                vf,
                NULL,
                expectedAttributes);
    }
    aap.fPadPosition = DigitAffixesAndPadding::kPadAfterPrefix;
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 2},
            {UNUM_INTEGER_FIELD, 6, 7},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 7, 8},
            {UNUM_SIGN_FIELD, 8, 10},
            {0, -1, 0}};
        verifyAffixesAndPaddingInt32(
                "(+****3.+)",
                aap,
                3,
                vf,
                NULL,
                expectedAttributes);
    }
    aap.fPadPosition = DigitAffixesAndPadding::kPadBeforeSuffix;
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 2},
            {UNUM_INTEGER_FIELD, 2, 3},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 3, 4},
            {UNUM_SIGN_FIELD, 8, 10},
            {0, -1, 0}};
        verifyAffixesAndPaddingInt32(
                "(+3.****+)",
                aap,
                3,
                vf,
                NULL,
                expectedAttributes);
    }
    aap.fPadPosition = DigitAffixesAndPadding::kPadAfterSuffix;
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 2},
            {UNUM_INTEGER_FIELD, 2, 3},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 3, 4},
            {UNUM_SIGN_FIELD, 4, 6},
            {0, -1, 0}};
        verifyAffixesAndPaddingInt32(
                "(+3.+)****",
                aap,
                3,
                vf,
                NULL,
                expectedAttributes);
    }
    aap.fPadPosition = DigitAffixesAndPadding::kPadAfterSuffix;
    {
        DigitList digits;
        digits.set(-1234.5);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 2},
            {UNUM_GROUPING_SEPARATOR_FIELD, 3, 4},
            {UNUM_INTEGER_FIELD, 2, 7},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 7, 8},
            {UNUM_FRACTION_FIELD, 8, 9},
            {UNUM_SIGN_FIELD, 9, 11},
            {0, -1, 0}};
        verifyAffixesAndPadding(
                "(-1,234.5-)",
                aap,
                digits,
                vf,
                NULL,
                expectedAttributes);
    }
    assertFalse("", aap.needsPluralRules());

    aap.fWidth = 0;
    aap.fPositivePrefix.remove();
    aap.fPositiveSuffix.remove();
    aap.fNegativePrefix.remove();
    aap.fNegativeSuffix.remove();
    
    // Set up for plural currencies.
    aap.fNegativePrefix.append("-", UNUM_SIGN_FIELD);
    {
        PluralAffix part;
        part.setVariant("one", " Dollar", status);
        part.setVariant("other", " Dollars", status);
        aap.fPositiveSuffix.append(part, UNUM_CURRENCY_FIELD, status);
    }
    aap.fNegativeSuffix = aap.fPositiveSuffix;
    
    LocalPointer<PluralRules> rules(PluralRules::forLocale("en", status));
    if (!assertSuccess("", status)) {
        return;
    }

    // Exercise the fastrack path
    {
        options.fAlwaysShowDecimal = FALSE;
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 1},
            {UNUM_INTEGER_FIELD, 1, 3},
            {UNUM_CURRENCY_FIELD, 3, 11},
            {0, -1, 0}};
        verifyAffixesAndPaddingInt32(
                "-45 Dollars",
                aap,
                -45,
                vf,
                NULL,
                expectedAttributes);
        options.fAlwaysShowDecimal = TRUE;
    }
    
    // Now test plurals
    assertTrue("", aap.needsPluralRules());
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 1, 2},
            {UNUM_CURRENCY_FIELD, 2, 9},
            {0, -1, 0}};
        verifyAffixesAndPaddingInt32(
                "1. Dollar",
                aap,
                1,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 1},
            {UNUM_INTEGER_FIELD, 1, 2},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 2, 3},
            {UNUM_CURRENCY_FIELD, 3, 10},
            {0, -1, 0}};
        verifyAffixesAndPaddingInt32(
                "-1. Dollar",
                aap,
                -1,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    precision.fMin.setFracDigitCount(2);
    {
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 1, 2},
            {UNUM_FRACTION_FIELD, 2, 4},
            {UNUM_CURRENCY_FIELD, 4, 12},
            {0, -1, 0}};
        verifyAffixesAndPaddingInt32(
                "1.00 Dollars",
                aap,
                1,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
}

void NumberFormat2Test::TestPluralsAndRounding() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en", status);
    if (!assertSuccess("", status)) {
        return;
    }
    DigitFormatter formatter(symbols);
    DigitGrouping grouping;
    FixedPrecision precision;
    precision.fSignificant.setMax(3);
    DigitFormatterOptions options;
    ValueFormatter vf;
    vf.prepareFixedDecimalFormatting(
            formatter,
            grouping,
            precision,
            options);
    DigitList digits;
    DigitAffixesAndPadding aap;
    // Set up for plural currencies.
    aap.fNegativePrefix.append("-", UNUM_SIGN_FIELD);
    {
        PluralAffix part;
        part.setVariant("one", " Dollar", status);
        part.setVariant("other", " Dollars", status);
        aap.fPositiveSuffix.append(part, UNUM_CURRENCY_FIELD, status);
    }
    aap.fNegativeSuffix = aap.fPositiveSuffix;
    aap.fWidth = 14;
    LocalPointer<PluralRules> rules(PluralRules::forLocale("en", status));
    if (!assertSuccess("", status)) {
        return;
    }
    {
        digits.set(0.999);
        verifyAffixesAndPadding(
                "*0.999 Dollars",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    {
        digits.set(0.9996);
        verifyAffixesAndPadding(
                "******1 Dollar",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    {
        digits.set(1.004);
        verifyAffixesAndPadding(
                "******1 Dollar",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    precision.fSignificant.setMin(2);
    {
        digits.set(0.9996);
        verifyAffixesAndPadding(
                "***1.0 Dollars",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    {
        digits.set(1.004);
        verifyAffixesAndPadding(
                "***1.0 Dollars",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    precision.fSignificant.setMin(0);
    {
        digits.set(-79.214);
        verifyAffixesAndPadding(
                "*-79.2 Dollars",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    // No more sig digits just max fractions
    precision.fSignificant.setMax(0); 
    precision.fMax.setFracDigitCount(4);
    {
        digits.set(79.213562);
        verifyAffixesAndPadding(
                "79.2136 Dollars",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }

}


void NumberFormat2Test::TestPluralsAndRoundingScientific() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en", status);
    if (!assertSuccess("", status)) {
        return;
    }
    DigitFormatter formatter(symbols);
    ScientificPrecision precision;
    precision.fMantissa.fSignificant.setMax(4);
    SciFormatterOptions options;
    ValueFormatter vf;
    vf.prepareScientificFormatting(
            formatter,
            precision,
            options);
    DigitList digits;
    DigitAffixesAndPadding aap;
    aap.fNegativePrefix.append("-", UNUM_SIGN_FIELD);
    {
        PluralAffix part;
        part.setVariant("one", " Meter", status);
        part.setVariant("other", " Meters", status);
        aap.fPositiveSuffix.append(part, UNUM_FIELD_COUNT, status);
    }
    aap.fNegativeSuffix = aap.fPositiveSuffix;
    LocalPointer<PluralRules> rules(PluralRules::forLocale("en", status));
    if (!assertSuccess("", status)) {
        return;
    }
    {
        digits.set(0.99996);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_EXPONENT_SYMBOL_FIELD, 1, 2},
            {UNUM_EXPONENT_FIELD, 2, 3},
            {0, -1, 0}};
        verifyAffixesAndPadding(
                "1E0 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    options.fMantissa.fAlwaysShowDecimal = TRUE;
    {
        digits.set(0.99996);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 1, 2},
            {UNUM_EXPONENT_SYMBOL_FIELD, 2, 3},
            {UNUM_EXPONENT_FIELD, 3, 4},
            {0, -1, 0}};
        verifyAffixesAndPadding(
                "1.E0 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    {
        digits.set(-299792458.0);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_SIGN_FIELD, 0, 1},
            {UNUM_INTEGER_FIELD, 1, 2},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 2, 3},
            {UNUM_FRACTION_FIELD, 3, 6},
            {UNUM_EXPONENT_SYMBOL_FIELD, 6, 7},
            {UNUM_EXPONENT_FIELD, 7, 8},
            {0, -1, 0}};
        verifyAffixesAndPadding(
                "-2.998E8 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    precision.fMantissa.fSignificant.setMin(4);
    options.fExponent.fAlwaysShowSign = TRUE;
    precision.fMinExponentDigits = 3;
    {
        digits.set(3.0);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 1, 2},
            {UNUM_FRACTION_FIELD, 2, 5},
            {UNUM_EXPONENT_SYMBOL_FIELD, 5, 6},
            {UNUM_EXPONENT_SIGN_FIELD, 6, 7},
            {UNUM_EXPONENT_FIELD, 7, 10},
            {0, -1, 0}};
        verifyAffixesAndPadding(
                "3.000E+000 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    precision.fMantissa.fMax.setIntDigitCount(3);
    {
        digits.set(0.00025001);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 3},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 3, 4},
            {UNUM_FRACTION_FIELD, 4, 5},
            {UNUM_EXPONENT_SYMBOL_FIELD, 5, 6},
            {UNUM_EXPONENT_SIGN_FIELD, 6, 7},
            {UNUM_EXPONENT_FIELD, 7, 10},
            {0, -1, 0}};
        verifyAffixesAndPadding(
                "250.0E-006 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    {
        digits.set(0.0000025001);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 1, 2},
            {UNUM_FRACTION_FIELD, 2, 5},
            {UNUM_EXPONENT_SYMBOL_FIELD, 5, 6},
            {UNUM_EXPONENT_SIGN_FIELD, 6, 7},
            {UNUM_EXPONENT_FIELD, 7, 10},
            {0, -1, 0}};
        verifyAffixesAndPadding(
                "2.500E-006 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    precision.fMantissa.fMax.setFracDigitCount(1);
    {
        digits.set(0.0000025499);
        NumberFormat2Test_Attributes expectedAttributes[] = {
            {UNUM_INTEGER_FIELD, 0, 1},
            {UNUM_DECIMAL_SEPARATOR_FIELD, 1, 2},
            {UNUM_FRACTION_FIELD, 2, 3},
            {UNUM_EXPONENT_SYMBOL_FIELD, 3, 4},
            {UNUM_EXPONENT_SIGN_FIELD, 4, 5},
            {UNUM_EXPONENT_FIELD, 5, 8},
            {0, -1, 0}};
        verifyAffixesAndPadding(
                "2.5E-006 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                expectedAttributes);
    }
    precision.fMantissa.fMax.setIntDigitCount(1);
    precision.fMantissa.fMax.setFracDigitCount(2);
    {
        digits.set((int32_t)299792458);
        verifyAffixesAndPadding(
                "3.00E+008 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    // clear significant digits
    precision.fMantissa.fSignificant.setMin(0);
    precision.fMantissa.fSignificant.setMax(0);

    // set int and fraction digits
    precision.fMantissa.fMin.setFracDigitCount(2);
    precision.fMantissa.fMax.setFracDigitCount(4);
    precision.fMantissa.fMin.setIntDigitCount(2);
    precision.fMantissa.fMax.setIntDigitCount(3);
    {
        digits.set(-0.0000025300001);
        verifyAffixesAndPadding(
                "-253.00E-008 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    {
        digits.set(-0.0000025300006);
        verifyAffixesAndPadding(
                "-253.0001E-008 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
    {
        digits.set(-0.000025300006);
        verifyAffixesAndPadding(
                "-25.30E-006 Meters",
                aap,
                digits,
                vf,
                rules.getAlias(),
                NULL);
    }
}


void NumberFormat2Test::TestRoundingIncrement() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en", status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating DecimalFormatSymbols - %s", u_errorName(status));
        return;
    }
    DigitFormatter formatter(symbols);
    ScientificPrecision precision;
    SciFormatterOptions options;
    precision.fMantissa.fRoundingIncrement.set(0.25);
    precision.fMantissa.fSignificant.setMax(4);
    DigitGrouping grouping;
    ValueFormatter vf;

    // fixed
    vf.prepareFixedDecimalFormatting(
            formatter,
            grouping,
            precision.fMantissa,
            options.fMantissa);
    DigitList digits;
    DigitAffixesAndPadding aap;
    aap.fNegativePrefix.append("-", UNUM_SIGN_FIELD);
    {
        digits.set(3.7);
        verifyAffixesAndPadding(
                "3.75",
                aap,
                digits,
                vf,
                NULL, NULL);
    }
    {
        digits.set(-7.4);
        verifyAffixesAndPadding(
                "-7.5",
                aap,
                digits,
                vf,
                NULL, NULL);
    }
    {
        digits.set(99.8);
        verifyAffixesAndPadding(
                "99.75",
                aap,
                digits,
                vf,
                NULL, NULL);
    }
    precision.fMantissa.fMin.setFracDigitCount(2);
    {
        digits.set(99.1);
        verifyAffixesAndPadding(
                "99.00",
                aap,
                digits,
                vf,
                NULL, NULL);
    }
    {
        digits.set(-639.65);
        verifyAffixesAndPadding(
                "-639.80",
                aap,
                digits,
                vf,
                NULL, NULL);
    }

    precision.fMantissa.fMin.setIntDigitCount(2);
    // Scientific notation
    vf.prepareScientificFormatting(
            formatter,
            precision,
            options);
    {
        digits.set(-6396.5);
        verifyAffixesAndPadding(
                "-64.00E2",
                aap,
                digits,
                vf,
                NULL, NULL);
    }
    {
        digits.set(-0.00092374);
        verifyAffixesAndPadding(
                "-92.25E-5",
                aap,
                digits,
                vf,
                NULL, NULL);
    }
    precision.fMantissa.fMax.setIntDigitCount(3);
    {
        digits.set(-0.00092374);
        verifyAffixesAndPadding(
                "-923.80E-6",
                aap,
                digits,
                vf,
                NULL, NULL);
    }
}

void NumberFormat2Test::TestToPatternScientific11648() {
/*
    UErrorCode status = U_ZERO_ERROR;
    Locale en("en");
    DecimalFormat2 fmt(en, "0.00", status);
    fmt.setScientificNotation(TRUE);
    UnicodeString pattern;
    // Fails, produces "0.00E"
    assertEquals("", "0.00E0", fmt.toPattern(pattern));
    DecimalFormat fmt2(pattern, status);
    // Fails, bad pattern.
    assertSuccess("", status);
*/
}

void NumberFormat2Test::verifyAffixesAndPadding(
        const UnicodeString &expected,
        const DigitAffixesAndPadding &aaf,
        DigitList &digits,
        const ValueFormatter &vf,
        const PluralRules *optPluralRules,
        const NumberFormat2Test_Attributes *expectedAttributes) {
    UnicodeString appendTo;
    NumberFormat2Test_FieldPositionHandler handler;
    UErrorCode status = U_ZERO_ERROR;
    assertEquals(
            "",
            expected,
            aaf.format(
                    digits,
                    vf,
                    handler,
                    optPluralRules,
                    appendTo,
                    status));
    if (!assertSuccess("", status)) {
        return;
    }
    if (expectedAttributes != NULL) {
        verifyAttributes(expectedAttributes, handler.attributes);
    }
}

void NumberFormat2Test::verifyAffixesAndPaddingInt32(
        const UnicodeString &expected,
        const DigitAffixesAndPadding &aaf,
        int32_t value,
        const ValueFormatter &vf,
        const PluralRules *optPluralRules,
        const NumberFormat2Test_Attributes *expectedAttributes) {
    UnicodeString appendTo;
    NumberFormat2Test_FieldPositionHandler handler;
    UErrorCode status = U_ZERO_ERROR;
    assertEquals(
            "",
            expected,
            aaf.formatInt32(
                    value,
                    vf,
                    handler,
                    optPluralRules,
                    appendTo,
                    status));
    if (!assertSuccess("", status)) {
        return;
    }
    if (expectedAttributes != NULL) {
        verifyAttributes(expectedAttributes, handler.attributes);
    }
    DigitList digits;
    digits.set(value);
    verifyAffixesAndPadding(
            expected, aaf, digits, vf, optPluralRules, expectedAttributes);
}

void NumberFormat2Test::verifyAffix(
        const UnicodeString &expected,
        const DigitAffix &affix,
        const NumberFormat2Test_Attributes *expectedAttributes) {
    UnicodeString appendTo;
    NumberFormat2Test_FieldPositionHandler handler;
    assertEquals(
            "",
            expected.unescape(),
            affix.format(handler, appendTo));
    if (expectedAttributes != NULL) {
        verifyAttributes(expectedAttributes, handler.attributes);
    }
}

// Right now only works for positive values.
void NumberFormat2Test::verifyDigitList(
        const UnicodeString &expected,
        const DigitList &digits) {
    DigitFormatter formatter;
    DigitGrouping grouping;
    VisibleDigits visibleDigits;
    FixedPrecision precision;
    precision.fMin.setIntDigitCount(0);
    DigitFormatterOptions options;
    UErrorCode status = U_ZERO_ERROR;
    DigitList dlCopy(digits);
    precision.initVisibleDigits(
            dlCopy, visibleDigits, status);
    if (!assertSuccess("", status)) {
        return;
    }
    verifyDigitFormatter(
            expected,
            formatter,
            visibleDigits,
            grouping,
            options,
            NULL);
}

void NumberFormat2Test::verifyVisibleDigits(
        const UnicodeString &expected,
        UBool bNegative,
        const VisibleDigits &digits) {
    DigitFormatter formatter;
    DigitGrouping grouping;
    DigitFormatterOptions options;
    verifyDigitFormatter(
            expected,
            formatter,
            digits,
            grouping,
            options,
            NULL);
    if (digits.isNegative() != bNegative) {
        errln(expected + ": Wrong sign.");
    }
    if (digits.isNaN() || digits.isInfinite()) {
        errln(expected + ": Require real value.");
    }
}

void NumberFormat2Test::verifyVisibleDigitsWithExponent(
        const UnicodeString &expected,
        UBool bNegative,
        const VisibleDigitsWithExponent &digits) {
    DigitFormatter formatter;
    SciFormatterOptions options;
    verifySciFormatter(
            expected,
            formatter,
            digits,
            options,
            NULL);
    if (digits.isNegative() != bNegative) {
        errln(expected + ": Wrong sign.");
    }
    if (digits.isNaN() || digits.isInfinite()) {
        errln(expected + ": Require real value.");
    }
}

void NumberFormat2Test::verifySciFormatter(
        const UnicodeString &expected,
        const DigitFormatter &formatter,
        const VisibleDigitsWithExponent &digits,
        const SciFormatterOptions &options,
        const NumberFormat2Test_Attributes *expectedAttributes) {
    assertEquals(
            "",
            expected.countChar32(),
            formatter.countChar32(digits, options));
    UnicodeString appendTo;
    NumberFormat2Test_FieldPositionHandler handler;
    assertEquals(
            "",
            expected,
            formatter.format(
                    digits,
                    options,
                    handler,
                    appendTo));
    if (expectedAttributes != NULL) {
        verifyAttributes(expectedAttributes, handler.attributes);
    }
}

void NumberFormat2Test::verifyPositiveIntDigitFormatter(
        const UnicodeString &expected,
        const DigitFormatter &formatter,
        int32_t value,
        int32_t minDigits,
        int32_t maxDigits,
        const NumberFormat2Test_Attributes *expectedAttributes) {
    IntDigitCountRange range(minDigits, maxDigits);
    UnicodeString appendTo;
    NumberFormat2Test_FieldPositionHandler handler;
    assertEquals(
            "",
            expected,
            formatter.formatPositiveInt32(
                    value,
                    range,
                    handler,
                    appendTo));
    if (expectedAttributes != NULL) {
        verifyAttributes(expectedAttributes, handler.attributes);
    }
}

void NumberFormat2Test::verifyDigitFormatter(
        const UnicodeString &expected,
        const DigitFormatter &formatter,
        const VisibleDigits &digits,
        const DigitGrouping &grouping,
        const DigitFormatterOptions &options,
        const NumberFormat2Test_Attributes *expectedAttributes) {
    assertEquals(
            "",
            expected.countChar32(),
            formatter.countChar32(digits, grouping, options));
    UnicodeString appendTo;
    NumberFormat2Test_FieldPositionHandler handler;
    assertEquals(
            "",
            expected,
            formatter.format(
                    digits,
                    grouping,
                    options,
                    handler,
                    appendTo));
    if (expectedAttributes != NULL) {
        verifyAttributes(expectedAttributes, handler.attributes);
    }
}

void NumberFormat2Test::verifySmallIntFormatter(
        const UnicodeString &expected,
        int32_t positiveValue,
        int32_t minDigits,
        int32_t maxDigits) {
    IntDigitCountRange range(minDigits, maxDigits);
    if (!SmallIntFormatter::canFormat(positiveValue, range)) {
        UnicodeString actual;
        assertEquals("", expected, actual);
        return;
    }
    UnicodeString actual;
    assertEquals("", expected, SmallIntFormatter::format(positiveValue, range, actual));
}

void NumberFormat2Test::verifyAttributes(
        const NumberFormat2Test_Attributes *expected,
        const NumberFormat2Test_Attributes *actual) {
    int32_t idx = 0;
    while (expected[idx].spos != -1 && actual[idx].spos != -1) {
        assertEquals("id", expected[idx].id, actual[idx].id);
        assertEquals("spos", expected[idx].spos, actual[idx].spos);
        assertEquals("epos", expected[idx].epos, actual[idx].epos);
        ++idx;
    }
    assertEquals(
            "expected and actual not same length",
            expected[idx].spos,
            actual[idx].spos);
}

void NumberFormat2Test::verifyIntValue(
        int64_t expected, const VisibleDigits &digits) {
    double unusedSource;
    int64_t intValue;
    int64_t unusedF;
    int64_t unusedT;
    int32_t unusedV;
    UBool unusedHasIntValue;
    digits.getFixedDecimal(
            unusedSource, intValue, unusedF,
            unusedT, unusedV, unusedHasIntValue);
    assertEquals("", expected, intValue);
}

void NumberFormat2Test::verifySource(
        double expected, const VisibleDigits &digits) {
    double source;
    int64_t unusedIntValue;
    int64_t unusedF;
    int64_t unusedT;
    int32_t unusedV;
    UBool unusedHasIntValue;
    digits.getFixedDecimal(
            source, unusedIntValue, unusedF,
            unusedT, unusedV, unusedHasIntValue);
    if (expected != source) {
        errln("Expected %f, got %f instead", expected, source);
    }
}

extern IntlTest *createNumberFormat2Test() {
    return new NumberFormat2Test();
}

#endif /* !UCONFIG_NO_FORMATTING */
