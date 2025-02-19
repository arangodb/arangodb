// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File SCINUMBERFORMATTERTEST.CPP
*
*******************************************************************************
*/
#include "unicode/utypes.h"

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/scientificnumberformatter.h"
#include "unicode/numfmt.h"
#include "unicode/decimfmt.h"
#include "unicode/localpointer.h"

class ScientificNumberFormatterTest : public IntlTest {
public:
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestBasic();
    void TestFarsi();
    void TestPlusSignInExponentMarkup();
    void TestPlusSignInExponentSuperscript();
    void TestFixedDecimalMarkup();
    void TestFixedDecimalSuperscript();
};

void ScientificNumberFormatterTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite ScientificNumberFormatterTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBasic);
    TESTCASE_AUTO(TestFarsi);
    TESTCASE_AUTO(TestPlusSignInExponentMarkup);
    TESTCASE_AUTO(TestPlusSignInExponentSuperscript);
    TESTCASE_AUTO(TestFixedDecimalMarkup);
    TESTCASE_AUTO(TestFixedDecimalSuperscript);
    TESTCASE_AUTO_END;
}

void ScientificNumberFormatterTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString prefix("String: ");
    UnicodeString appendTo(prefix);
    LocalPointer<ScientificNumberFormatter> fmt(
            ScientificNumberFormatter::createMarkupInstance(
                    "en" , "<sup>", "</sup>", status));
    if (!assertSuccess("Can't create ScientificNumberFormatter", status)) {
        return;
    }
    fmt->format(1.23456e-78, appendTo, status);
    const char *expected = "String: 1.23456\\u00d710<sup>-78</sup>";
    assertEquals(
            "markup style",
            UnicodeString(expected).unescape(),
            appendTo);

    // Test superscript style
    fmt.adoptInstead(
            ScientificNumberFormatter::createSuperscriptInstance(
                    "en", status));
    if (!assertSuccess("Can't create ScientificNumberFormatter2", status)) {
        return;
    }
    appendTo = prefix;
    fmt->format(1.23456e-78, appendTo, status);
    expected = "String: 1.23456\\u00d710\\u207b\\u2077\\u2078";
    assertEquals(
            "superscript style",
            UnicodeString(expected).unescape(),
            appendTo);
  
    // Test clone
    LocalPointer<ScientificNumberFormatter> fmt3(fmt->clone());
    if (fmt3.isNull()) {
       errln("Allocating clone failed.");
       return;
    }
    appendTo = prefix;
    fmt3->format(1.23456e-78, appendTo, status);
    expected = "String: 1.23456\\u00d710\\u207b\\u2077\\u2078";
    assertEquals(
            "superscript style",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificNumberFormatterTest::TestFarsi() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString prefix("String: ");
    UnicodeString appendTo(prefix);
    LocalPointer<ScientificNumberFormatter> fmt(
            ScientificNumberFormatter::createMarkupInstance(
                    "fa", "<sup>", "</sup>", status));
    if (!assertSuccess("Can't create ScientificNumberFormatter", status)) {
        return;
    }
    fmt->format(1.23456e-78, appendTo, status);
    const char *expected = "String: \\u06F1\\u066B\\u06F2\\u06F3\\u06F4\\u06F5\\u06F6\\u00d7\\u06F1\\u06F0<sup>\\u200E\\u2212\\u06F7\\u06F8</sup>";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificNumberFormatterTest::TestPlusSignInExponentMarkup() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"en\", status) - %s", u_errorName(status));
        return;
    }
    decfmt->applyPattern("0.00E+0", status);
    if (!assertSuccess("", status)) {
        return;
    }
    UnicodeString appendTo;
    LocalPointer<ScientificNumberFormatter> fmt(
            ScientificNumberFormatter::createMarkupInstance(
                    new DecimalFormat(*decfmt), "<sup>", "</sup>", status));
    if (!assertSuccess("Can't create ScientificNumberFormatter", status)) {
        return;
    }
    fmt->format(6.02e23, appendTo, status);
    const char *expected = "6.02\\u00d710<sup>+23</sup>";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificNumberFormatterTest::TestPlusSignInExponentSuperscript() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"en\", status) - %s", u_errorName(status));
        return;
    }
    decfmt->applyPattern("0.00E+0", status);
    if (!assertSuccess("", status)) {
        return;
    }
    UnicodeString appendTo;
    LocalPointer<ScientificNumberFormatter> fmt(
            ScientificNumberFormatter::createSuperscriptInstance(
                    new DecimalFormat(*decfmt), status));
    if (!assertSuccess("Can't create ScientificNumberFormatter", status)) {
        return;
    }
    fmt->format(6.02e23, appendTo, status);
    const char *expected = "6.02\\u00d710\\u207a\\u00b2\\u00b3";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificNumberFormatterTest::TestFixedDecimalMarkup() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createInstance("en", status));
    if (assertSuccess("NumberFormat::createInstance", status, TRUE) == FALSE) {
        return;
    }
    LocalPointer<ScientificNumberFormatter> fmt(
            ScientificNumberFormatter::createMarkupInstance(
                    new DecimalFormat(*decfmt), "<sup>", "</sup>", status));
    if (!assertSuccess("Can't create ScientificNumberFormatter", status)) {
        return;
    }
    UnicodeString appendTo;
    fmt->format(123456.0, appendTo, status);
    const char *expected = "123,456";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

void ScientificNumberFormatterTest::TestFixedDecimalSuperscript() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createInstance("en", status));
    if (assertSuccess("NumberFormat::createInstance", status, TRUE) == FALSE) {
        return;
    }
    LocalPointer<ScientificNumberFormatter> fmt(
            ScientificNumberFormatter::createSuperscriptInstance(
                    new DecimalFormat(*decfmt), status));
    if (!assertSuccess("Can't create ScientificNumberFormatter", status)) {
        return;
    }
    UnicodeString appendTo;
    fmt->format(123456.0, appendTo, status);
    const char *expected = "123,456";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            appendTo);
    assertSuccess("", status);
}

extern IntlTest *createScientificNumberFormatterTest() {
    return new ScientificNumberFormatterTest();
}

#endif /* !UCONFIG_NO_FORMATTING */
