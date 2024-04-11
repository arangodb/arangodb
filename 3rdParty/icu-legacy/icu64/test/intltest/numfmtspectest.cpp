// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2014-2015, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File numfmtspectest.cpp
*
*******************************************************************************
*/
#include <stdio.h>
#include <stdlib.h>

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/localpointer.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "uassert.h"

static const char16_t kJPY[] = {0x4A, 0x50, 0x59};

static void fixNonBreakingSpace(UnicodeString &str) {
    for (int32_t i = 0; i < str.length(); ++i) {
        if (str[i] == 0xa0) {
            str.setCharAt(i, 0x20);
        }
    }    
}

static NumberFormat *nfWithPattern(const char *pattern) {
    UnicodeString upattern(pattern, -1, US_INV);
    upattern = upattern.unescape();
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat *result = new DecimalFormat(
            upattern, new DecimalFormatSymbols("fr", status), status);
    if (U_FAILURE(status)) {
        return nullptr;
    }

    return result;
}

static UnicodeString format(double d, const NumberFormat &fmt) {
    UnicodeString result;
    fmt.format(d, result);
    fixNonBreakingSpace(result);
    return result;
}

class NumberFormatSpecificationTest : public IntlTest {
public:
    NumberFormatSpecificationTest() {
    }
    void TestBasicPatterns();
    void TestNfSetters();
    void TestRounding();
    void TestSignificantDigits();
    void TestScientificNotation();
    void TestPercent();
    void TestPerMilli();
    void TestPadding();
    void runIndexedTest(int32_t index, UBool exec, const char*& name, char* par = nullptr) override;

private:
    void assertPatternFr(
            const char *expected, double x, const char *pattern, UBool possibleDataError=false);
    
};

void NumberFormatSpecificationTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite NumberFormatSpecificationTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBasicPatterns);
    TESTCASE_AUTO(TestNfSetters);
    TESTCASE_AUTO(TestRounding);
    TESTCASE_AUTO(TestSignificantDigits);
    TESTCASE_AUTO(TestScientificNotation);
    TESTCASE_AUTO(TestPercent);
    TESTCASE_AUTO(TestPerMilli);
    TESTCASE_AUTO(TestPadding);
    TESTCASE_AUTO_END;
}

void NumberFormatSpecificationTest::TestBasicPatterns() {
    assertPatternFr("1\\u202F234,57", 1234.567, "#,##0.##", true);
    assertPatternFr("1234,57", 1234.567, "0.##", true);
    assertPatternFr("1235", 1234.567, "0", true);
    assertPatternFr("1\\u202F234,567", 1234.567, "#,##0.###", true);
    assertPatternFr("1234,567", 1234.567, "###0.#####", true);
    assertPatternFr("1234,5670", 1234.567, "###0.0000#", true);
    assertPatternFr("01234,5670", 1234.567, "00000.0000", true);
    assertPatternFr("1\\u202F234,57 \\u20ac", 1234.567, "#,##0.00 \\u00a4", true);
}

void NumberFormatSpecificationTest::TestNfSetters() {
    LocalPointer<NumberFormat> nf(nfWithPattern("#,##0.##"));
    if (nf == nullptr) {
        dataerrln("Error creating NumberFormat");
        return;
    }
    nf->setMaximumIntegerDigits(5);
    nf->setMinimumIntegerDigits(4);
    assertEquals("", u"34\u202F567,89", format(1234567.89, *nf), true);
    assertEquals("", u"0\u202F034,56", format(34.56, *nf), true);
}

void NumberFormatSpecificationTest::TestRounding() {
    assertPatternFr("1,0", 1.25, "0.5", true);
    assertPatternFr("2,0", 1.75, "0.5", true);
    assertPatternFr("-1,0", -1.25, "0.5", true);
    assertPatternFr("-02,0", -1.75, "00.5", true);
    assertPatternFr("0", 2.0, "4", true);
    assertPatternFr("8", 6.0, "4", true);
    assertPatternFr("8", 10.0, "4", true);
    assertPatternFr("99,90", 99.0, "2.70", true);
    assertPatternFr("273,00", 272.0, "2.73", true);
    assertPatternFr("1\\u202F03,60", 104.0, "#,#3.70", true);
}

void NumberFormatSpecificationTest::TestSignificantDigits() {
    assertPatternFr("1230", 1234.0, "@@@", true);
    assertPatternFr("1\\u202F234", 1234.0, "@,@@@", true);
    assertPatternFr("1\\u202F235\\u202F000", 1234567.0, "@,@@@", true);
    assertPatternFr("1\\u202F234\\u202F567", 1234567.0, "@@@@,@@@", true);
    assertPatternFr("12\\u202F34\\u202F567,00", 1234567.0, "@@@@,@@,@@@", true);
    assertPatternFr("12\\u202F34\\u202F567,0", 1234567.0, "@@@@,@@,@@#", true);
    assertPatternFr("12\\u202F34\\u202F567", 1234567.0, "@@@@,@@,@##", true);
    assertPatternFr("12\\u202F34\\u202F567", 1234567.001, "@@@@,@@,@##", true);
    assertPatternFr("12\\u202F34\\u202F567", 1234567.001, "@@@@,@@,###", true);
    assertPatternFr("1\\u202F200", 1234.0, "#,#@@", true);
}

void NumberFormatSpecificationTest::TestScientificNotation() {
    assertPatternFr("1,23E4", 12345.0, "0.00E0", true);
    assertPatternFr("123,00E2", 12300.0, "000.00E0", true);
    assertPatternFr("123,0E2", 12300.0, "000.0#E0", true);
    assertPatternFr("123,0E2", 12300.1, "000.0#E0", true);
    assertPatternFr("123,01E2", 12301.0, "000.0#E0", true);
    assertPatternFr("123,01E+02", 12301.0, "000.0#E+00", true);
    assertPatternFr("12,3E3", 12345.0, "##0.00E0", true);
    assertPatternFr("12,300E3", 12300.1, "##0.0000E0", true);
    assertPatternFr("12,30E3", 12300.1, "##0.000#E0", true);
    assertPatternFr("12,301E3", 12301.0, "##0.000#E0", true);
    assertPatternFr("1,25E4", 12301.2, "0.05E0");
    assertPatternFr("170,0E-3", 0.17, "##0.000#E0", true);

}

void NumberFormatSpecificationTest::TestPercent() {
    assertPatternFr("57,3%", 0.573, "0.0%", true);
    assertPatternFr("%57,3", 0.573, "%0.0", true);
    assertPatternFr("p%p57,3", 0.573, "p%p0.0", true);
    assertPatternFr("p%p0,6", 0.573, "p'%'p0.0", true);
    assertPatternFr("%3,260", 0.0326, "%@@@@", true);
    assertPatternFr("%1\\u202F540", 15.43, "%#,@@@", true);
    assertPatternFr("%1\\u202F656,4", 16.55, "%#,##4.1", true);
    assertPatternFr("%16,3E3", 162.55, "%##0.00E0", true);
}

void NumberFormatSpecificationTest::TestPerMilli() {
    assertPatternFr("573,0\\u2030", 0.573, "0.0\\u2030", true);
    assertPatternFr("\\u2030573,0", 0.573, "\\u20300.0", true);
    assertPatternFr("p\\u2030p573,0", 0.573, "p\\u2030p0.0", true);
    assertPatternFr("p\\u2030p0,6", 0.573, "p'\\u2030'p0.0", true);
    assertPatternFr("\\u203032,60", 0.0326, "\\u2030@@@@", true);
    assertPatternFr("\\u203015\\u202F400", 15.43, "\\u2030#,@@@", true);
    assertPatternFr("\\u203016\\u202F551,7", 16.55, "\\u2030#,##4.1", true);
    assertPatternFr("\\u2030163E3", 162.55, "\\u2030##0.00E0", true);
}

void NumberFormatSpecificationTest::TestPadding() {
    assertPatternFr("$***1\\u202F234", 1234, "$**####,##0", true);
    assertPatternFr("xxx$1\\u202F234", 1234, "*x$####,##0", true);
    assertPatternFr("1\\u202F234xxx$", 1234, "####,##0*x$", true);
    assertPatternFr("1\\u202F234$xxx", 1234, "####,##0$*x", true);
    assertPatternFr("ne1\\u202F234nx", -1234, "####,##0$*x;ne#n", true);
    assertPatternFr("n1\\u202F234*xx", -1234, "####,##0$*x;n#'*'", true);
    assertPatternFr("yyyy%432,6", 4.33, "*y%4.2######",  true);
    assertPatternFr("EUR *433,00", 433.0, "\\u00a4\\u00a4 **####0.00");
    assertPatternFr("EUR *433,00", 433.0, "\\u00a4\\u00a4 **#######0");
    {
        UnicodeString upattern("\\u00a4\\u00a4 **#######0", -1, US_INV);
        upattern = upattern.unescape();
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString result;
        DecimalFormat fmt(
                upattern, new DecimalFormatSymbols("fr", status), status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        } else {
            fmt.setCurrency(kJPY);
            fmt.format(433.22, result);
            assertSuccess("", status);
            assertEquals("", "JPY ****433", result, true);
        }
    }
    {
        UnicodeString upattern(
            "\\u00a4\\u00a4 **#######0;\\u00a4\\u00a4 (#)", -1, US_INV);
        upattern = upattern.unescape();
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString result;
        DecimalFormat fmt(
                upattern,
                new DecimalFormatSymbols("en_US", status),
                status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        } else {
            fmt.format(-433.22, result);
            assertSuccess("", status);
            assertEquals("", "USD (433.22)", result, true);
        }
    }
    const char *paddedSciPattern = "QU**00.#####E0";
    assertPatternFr("QU***43,3E-1", 4.33, paddedSciPattern, true);
    {
        UErrorCode status = U_ZERO_ERROR;
        DecimalFormatSymbols *sym = new DecimalFormatSymbols("fr", status);
        sym->setSymbol(DecimalFormatSymbols::kExponentialSymbol, "EE");
        DecimalFormat fmt(
                paddedSciPattern,
                sym,
                status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        } else {
            UnicodeString result;
            fmt.format(4.33, result);
            assertSuccess("", status);
            assertEquals("", "QU**43,3EE-1", result, true);
        }
    }
    // padding cannot work as intended with scientific notation.
    assertPatternFr("QU**43,32E-1", 4.332, paddedSciPattern, true);
}

void NumberFormatSpecificationTest::assertPatternFr(
        const char *expected,
        double x,
        const char *pattern,
        UBool possibleDataError) {
    UnicodeString upattern(pattern, -1, US_INV);
    UnicodeString uexpected(expected, -1, US_INV);
    upattern = upattern.unescape();
    uexpected = uexpected.unescape();
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString result;
    DecimalFormat fmt(
            upattern, new DecimalFormatSymbols("fr_FR", status), status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating DecimalFormatSymbols - %s", u_errorName(status));
        return;
    }
    fmt.format(x, result);
    fixNonBreakingSpace(result);
    assertSuccess("", status);
    assertEquals("", uexpected, result, possibleDataError);
}

extern IntlTest *createNumberFormatSpecificationTest() {
    return new NumberFormatSpecificationTest();
}

#endif
