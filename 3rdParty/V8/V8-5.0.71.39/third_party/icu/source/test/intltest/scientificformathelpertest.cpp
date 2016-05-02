/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File SCIFORMATHELPERTEST.CPP
*
*******************************************************************************
*/
#include "unicode/utypes.h"

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/scientificformathelper.h"
#include "unicode/numfmt.h"
#include "unicode/decimfmt.h"
#include "unicode/localpointer.h"

class ScientificFormatHelperTest : public IntlTest {
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

void ScientificFormatHelperTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite ScientificFormatHelperTest: ");
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

void ScientificFormatHelperTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"en\", status) - %s", u_errorName(status));
        return;
    }
    UnicodeString appendTo("String: ");
    FieldPositionIterator fpositer;
    decfmt->format(1.23456e-78, appendTo, &fpositer, status);
    FieldPositionIterator fpositer2(fpositer);
    FieldPositionIterator fpositer3(fpositer);
    ScientificFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
    UnicodeString result;
    const char *expected = "String: 1.23456\\u00d710<sup>-78</sup>";
    assertEquals(
            "insertMarkup",
            UnicodeString(expected).unescape(),
            helper.insertMarkup(appendTo, fpositer, "<sup>", "</sup>", result, status));
    result.remove();
    expected = "String: 1.23456\\u00d710\\u207b\\u2077\\u2078";
    assertEquals(
            "toSuperscriptExponentDigits",
            UnicodeString(expected).unescape(),
            helper.toSuperscriptExponentDigits(appendTo, fpositer2, result, status));
    assertSuccess("", status);
    result.remove();

    // The 'a' is an invalid exponent character.
    helper.toSuperscriptExponentDigits("String: 1.23456e-7a", fpositer3, result, status);
    if (status != U_INVALID_CHAR_FOUND) {
        errln("Expected U_INVALID_CHAR_FOUND");
    }
}

void ScientificFormatHelperTest::TestFarsi() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("fa", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"fa\", status) - %s", u_errorName(status));
        return;
    }
    UnicodeString appendTo("String: ");
    FieldPositionIterator fpositer;
    decfmt->format(1.23456e-78, appendTo, &fpositer, status);
    ScientificFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
    UnicodeString result;
    const char *expected = "String: \\u06F1\\u066B\\u06F2\\u06F3\\u06F4\\u06F5\\u06F6\\u00d7\\u06F1\\u06F0<sup>\\u200E\\u2212\\u06F7\\u06F8</sup>";
    assertEquals(
            "insertMarkup",
            UnicodeString(expected).unescape(),
            helper.insertMarkup(appendTo, fpositer, "<sup>", "</sup>", result, status));
    assertSuccess("", status);
}

void ScientificFormatHelperTest::TestPlusSignInExponentMarkup() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"en\", status) - %s", u_errorName(status));
        return;
    }
    decfmt->applyPattern("0.00E+0", status);
    assertSuccess("", status);
    UnicodeString appendTo;
    FieldPositionIterator fpositer;
    decfmt->format(6.02e23, appendTo, &fpositer, status);
    ScientificFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
    UnicodeString result;
    const char *expected = "6.02\\u00d710<sup>+23</sup>";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            helper.insertMarkup(appendTo, fpositer, "<sup>", "</sup>", result, status));
    assertSuccess("", status);
}

void ScientificFormatHelperTest::TestPlusSignInExponentSuperscript() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createScientificInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Failed call NumberFormat::createScientificInstance(\"en\", status) - %s", u_errorName(status));
        return;
    }
    decfmt->applyPattern("0.00E+0", status);
    assertSuccess("", status);
    UnicodeString appendTo;
    FieldPositionIterator fpositer;
    decfmt->format(6.02e23, appendTo, &fpositer, status);
    ScientificFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
    UnicodeString result;
    const char *expected = "6.02\\u00d710\\u207a\\u00b2\\u00b3";
    assertEquals(
            "",
            UnicodeString(expected).unescape(),
            helper.toSuperscriptExponentDigits(appendTo, fpositer, result, status));
    assertSuccess("", status);
}

void ScientificFormatHelperTest::TestFixedDecimalMarkup() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createInstance("en", status));
    if (assertSuccess("NumberFormat::createInstance", status, TRUE) == FALSE) {
        return;
    }
    UnicodeString appendTo;
    FieldPositionIterator fpositer;
    decfmt->format(123456.0, appendTo, &fpositer, status);
    ScientificFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
    assertSuccess("", status);
    UnicodeString result;
    helper.insertMarkup(appendTo, fpositer, "<sup>", "</sup>", result, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR with fixed decimal number.");
    }
}

void ScientificFormatHelperTest::TestFixedDecimalSuperscript() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createInstance("en", status));
    if (assertSuccess("NumberFormat::createInstance", status, TRUE) == FALSE) {
        return;
    }
    UnicodeString appendTo;
    FieldPositionIterator fpositer;
    decfmt->format(123456.0, appendTo, &fpositer, status);
    ScientificFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
    assertSuccess("", status);
    UnicodeString result;
    helper.toSuperscriptExponentDigits(appendTo, fpositer, result, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR with fixed decimal number.");
    }
}

extern IntlTest *createScientificFormatHelperTest() {
    return new ScientificFormatHelperTest();
}

#endif /* !UCONFIG_NO_FORMATTING */
