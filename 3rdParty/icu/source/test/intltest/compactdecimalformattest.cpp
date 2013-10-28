/*
*******************************************************************************
* Copyright (C) 1997-2013, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File COMPACTDECIMALFORMATTEST.CPP
*
********************************************************************************
*/
#include <stdio.h>
#include <stdlib.h>

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/compactdecimalformat.h"
#include "unicode/unum.h"

#define LENGTHOF(array) (int32_t)(sizeof(array) / sizeof((array)[0]))

typedef struct ExpectedResult {
  double value;
  const char *expected;
} ExpectedResult;

static const char *kShortStr = "Short";
static const char *kLongStr = "Long";

static ExpectedResult kEnglishShort[] = {
  {0.0, "0"},
  {0.17, "0.17"},
  {1.0, "1"},
  {1234.0, "1.2K"},
  {12345.0, "12K"},
  {123456.0, "120K"},
  {1234567.0, "1.2M"},
  {12345678.0, "12M"},
  {123456789.0, "120M"},
  {1.23456789E9, "1.2B"},
  {1.23456789E10, "12B"},
  {1.23456789E11, "120B"},
  {1.23456789E12, "1.2T"},
  {1.23456789E13, "12T"},
  {1.23456789E14, "120T"},
  {1.23456789E15, "1200T"}};

static ExpectedResult kSerbianShort[] = {
  {1234.0, "1200"},
  {12345.0, "12\\u00a0\\u0445\\u0438\\u0459"},
  {20789.0, "21\\u00a0\\u0445\\u0438\\u0459"},
  {123456.0, "120\\u00a0\\u0445\\u0438\\u0459"},
  {1234567.0, "1,2\\u00A0\\u043C\\u0438\\u043B"},
  {12345678.0, "12\\u00A0\\u043C\\u0438\\u043B"},
  {123456789.0, "120\\u00A0\\u043C\\u0438\\u043B"},
  {1.23456789E9, "1,2\\u00A0\\u043C\\u043B\\u0440\\u0434"},
  {1.23456789E10, "12\\u00A0\\u043C\\u043B\\u0440\\u0434"},
  {1.23456789E11, "120\\u00A0\\u043C\\u043B\\u0440\\u0434"},
  {1.23456789E12, "1,2\\u00A0\\u0431\\u0438\\u043B"},
  {1.23456789E13, "12\\u00A0\\u0431\\u0438\\u043B"},
  {1.23456789E14, "120\\u00A0\\u0431\\u0438\\u043B"},
  {1.23456789E15, "1200\\u00A0\\u0431\\u0438\\u043B"}};

static ExpectedResult kSerbianLong[] = {
  {1234.0, "1,2 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"}, // 10^3 few
  {12345.0, "12 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"}, // 10^3 other
  {21789.0, "22 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"}, // 10^3 few
  {123456.0, "120 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"}, // 10^3 other
  {999999.0, "1 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D"}, // 10^6 one
  {1234567.0, "1,2 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 few
  {12345678.0, "12 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 other
  {123456789.0, "120 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^6 other
  {1.23456789E9, "1,2 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"}, // 10^9 few
  {1.23456789E10, "12 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"}, // 10^9 other
  {2.08901234E10, "21 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0430"}, // 10^9 one
  {2.18901234E10, "22 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"}, // 10^9 few
  {1.23456789E11, "120 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"}, // 10^9 other
  {1.23456789E12, "1,2 \\u0442\\u0440\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^12 few
  {1.23456789E13, "12 \\u0442\\u0440\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^12 other
  {1.23456789E14, "120 \\u0442\\u0440\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}, // 10^12 other
  {1.23456789E15, "1200 \\u0442\\u0440\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}}; // 10^12 other

static ExpectedResult kSerbianLongNegative[] = {
  {-1234.0, "-1,2 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"},
  {-12345.0, "-12 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"},
  {-21789.0, "-22 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0435"},
  {-123456.0, "-120 \\u0445\\u0438\\u0459\\u0430\\u0434\\u0430"},
  {-999999.0, "-1 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D"},
  {-1234567.0, "-1,2 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-12345678.0, "-12 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-123456789.0, "-120 \\u043C\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-1.23456789E9, "-1,2 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"},
  {-1.23456789E10, "-12 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"},
  {-2.08901234E10, "-21 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0430"},
  {-2.18901234E10, "-22 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0435"},
  {-1.23456789E11, "-120 \\u043C\\u0438\\u043B\\u0438\\u0458\\u0430\\u0440\\u0434\\u0438"},
  {-1.23456789E12, "-1,2 \\u0442\\u0440\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-1.23456789E13, "-12 \\u0442\\u0440\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-1.23456789E14, "-120 \\u0442\\u0440\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"},
  {-1.23456789E15, "-1200 \\u0442\\u0440\\u0438\\u043B\\u0438\\u043E\\u043D\\u0430"}};

static ExpectedResult kJapaneseShort[] = {
  {1234.0, "1.2\\u5343"},
  {12345.0, "1.2\\u4E07"},
  {123456.0, "12\\u4E07"},
  {1234567.0, "120\\u4E07"},
  {12345678.0, "1200\\u4E07"},
  {123456789.0, "1.2\\u5104"},
  {1.23456789E9, "12\\u5104"},
  {1.23456789E10, "120\\u5104"},
  {1.23456789E11, "1200\\u5104"},
  {1.23456789E12, "1.2\\u5146"},
  {1.23456789E13, "12\\u5146"},
  {1.23456789E14, "120\\u5146"}};

static ExpectedResult kSwahiliShort[] = {
  {1234.0, "elfu\\u00a01.2"},
  {12345.0, "elfu\\u00a012"},
  {123456.0, "laki1.2"},
  {1234567.0, "M1.2"},
  {12345678.0, "M12"},
  {123456789.0, "M120"},
  {1.23456789E9, "B1.2"},
  {1.23456789E10, "B12"},
  {1.23456789E11, "B120"},
  {1.23456789E12, "T1.2"},
  {1.23456789E13, "T12"},
  {1.23456789E15, "T1200"}};

static ExpectedResult kCsShort[] = {
  {1000.0, "1\\u00a0tis."},
  {1500.0, "1,5\\u00a0tis."},
  {5000.0, "5\\u00a0tis."},
  {23000.0, "23\\u00a0tis."},
  {127123.0, "130\\u00a0tis."},
  {1271234.0, "1,3\\u00a0mil."},
  {12712345.0, "13\\u00a0mil."},
  {127123456.0, "130\\u00a0mil."},
  {1.27123456E9, "1,3\\u00a0mld."},
  {1.27123456E10, "13\\u00a0mld."},
  {1.27123456E11, "130\\u00a0mld."},
  {1.27123456E12, "1,3\\u00a0bil."},
  {1.27123456E13, "13\\u00a0bil."},
  {1.27123456E14, "130\\u00a0bil."}};

static ExpectedResult kSkLong[] = {
  {1000.0, "1 tis\\u00edc"},
  {1572.0, "1,6 tis\\u00edc"},
  {5184.0, "5,2 tis\\u00edc"}};

static ExpectedResult kSwahiliShortNegative[] = {
  {-1234.0, "elfu\\u00a0-1.2"},
  {-12345.0, "elfu\\u00a0-12"},
  {-123456.0, "laki-1.2"},
  {-1234567.0, "M-1.2"},
  {-12345678.0, "M-12"},
  {-123456789.0, "M-120"},
  {-1.23456789E9, "B-1.2"},
  {-1.23456789E10, "B-12"},
  {-1.23456789E11, "B-120"},
  {-1.23456789E12, "T-1.2"},
  {-1.23456789E13, "T-12"},
  {-1.23456789E15, "T-1200"}};

static ExpectedResult kArabicLong[] = {
  {-5300.0, "\\u200F-\\u0665\\u066B\\u0663 \\u0623\\u0644\\u0641"}};


class CompactDecimalFormatTest : public IntlTest {
public:
    CompactDecimalFormatTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestEnglishShort();
    void TestSerbianShort();
    void TestSerbianLong();
    void TestSerbianLongNegative();
    void TestJapaneseShort();
    void TestSwahiliShort();
    void TestCsShort();
    void TestSkLong();
    void TestSwahiliShortNegative();
    void TestArabicLong();
    void TestFieldPosition();
    void TestSignificantDigits();
    void CheckLocale(
        const Locale& locale, UNumberCompactStyle style,
        const ExpectedResult* expectedResult, int32_t expectedResultLength);
    void CheckExpectedResult(
        const CompactDecimalFormat* cdf, const ExpectedResult* expectedResult,
        const char* description);
    CompactDecimalFormat* createCDFInstance(const Locale& locale, UNumberCompactStyle style, UErrorCode& status);
    static const char *StyleStr(UNumberCompactStyle style);
};

void CompactDecimalFormatTest::runIndexedTest(
    int32_t index, UBool exec, const char *&name, char *) {
  if (exec) {
    logln("TestSuite CompactDecimalFormatTest: ");
  }
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestEnglishShort);
  TESTCASE_AUTO(TestSerbianShort);
  TESTCASE_AUTO(TestSerbianLong);
  TESTCASE_AUTO(TestSerbianLongNegative);
  TESTCASE_AUTO(TestJapaneseShort);
  TESTCASE_AUTO(TestSwahiliShort);
  TESTCASE_AUTO(TestCsShort);
  TESTCASE_AUTO(TestSkLong);
  TESTCASE_AUTO(TestSwahiliShortNegative);
  TESTCASE_AUTO(TestArabicLong);
  TESTCASE_AUTO(TestFieldPosition);
  TESTCASE_AUTO(TestSignificantDigits);
  TESTCASE_AUTO_END;
}

void CompactDecimalFormatTest::TestEnglishShort() {
  CheckLocale("en", UNUM_SHORT, kEnglishShort, LENGTHOF(kEnglishShort));
}

void CompactDecimalFormatTest::TestSerbianShort() {
  CheckLocale("sr", UNUM_SHORT, kSerbianShort, LENGTHOF(kSerbianShort));
}

void CompactDecimalFormatTest::TestSerbianLong() {
  CheckLocale("sr", UNUM_LONG, kSerbianLong, LENGTHOF(kSerbianLong));
}

void CompactDecimalFormatTest::TestSerbianLongNegative() {
  CheckLocale("sr", UNUM_LONG, kSerbianLongNegative, LENGTHOF(kSerbianLongNegative));
}

void CompactDecimalFormatTest::TestJapaneseShort() {
  CheckLocale(Locale::getJapan(), UNUM_SHORT, kJapaneseShort, LENGTHOF(kJapaneseShort));
}

void CompactDecimalFormatTest::TestSwahiliShort() {
  CheckLocale("sw", UNUM_SHORT, kSwahiliShort, LENGTHOF(kSwahiliShort));
}

void CompactDecimalFormatTest::TestFieldPosition() {
  // Swahili uses prefixes which forces offsets in field position to change
  UErrorCode status = U_ZERO_ERROR;
  LocalPointer<CompactDecimalFormat> cdf(createCDFInstance("sw", UNUM_SHORT, status));
  if (U_FAILURE(status)) {
    dataerrln("Unable to create format object - %s", u_errorName(status));
    return;
  }
  FieldPosition fp(UNUM_INTEGER_FIELD);
  UnicodeString result;
  cdf->format(1234567.0, result, fp);
  UnicodeString subString = result.tempSubString(fp.getBeginIndex(), fp.getEndIndex() - fp.getBeginIndex());
  if (subString != UnicodeString("1", -1, US_INV)) {
    errln(UnicodeString("Expected 1, got ") + subString);
  }
}

void CompactDecimalFormatTest::TestCsShort() {
  CheckLocale("cs", UNUM_SHORT, kCsShort, LENGTHOF(kCsShort));
}

void CompactDecimalFormatTest::TestSkLong() {
  // In CLDR we have:
  // 1000 {
  //   few{"0"}
  //   one{"0"}
  //   other{"0"}
  CheckLocale("sk", UNUM_LONG, kSkLong, LENGTHOF(kSkLong));
}

void CompactDecimalFormatTest::TestSwahiliShortNegative() {
  CheckLocale("sw", UNUM_SHORT, kSwahiliShortNegative, LENGTHOF(kSwahiliShortNegative));
}

void CompactDecimalFormatTest::TestArabicLong() {
  CheckLocale("ar", UNUM_LONG, kArabicLong, LENGTHOF(kArabicLong));
}

void CompactDecimalFormatTest::TestSignificantDigits() {
  UErrorCode status = U_ZERO_ERROR;
  LocalPointer<CompactDecimalFormat> cdf(CompactDecimalFormat::createInstance("en", UNUM_SHORT, status));
  if (U_FAILURE(status)) {
    dataerrln("Unable to create format object - %s", u_errorName(status));
    return;
  }
  UnicodeString actual;
  cdf->format(123456.0, actual);
  // We expect 3 significant digits by default
  UnicodeString expected("123K", -1, US_INV);
  if (actual != expected) {
    errln(UnicodeString("Fail: Expected: ") + expected + UnicodeString(" Got: ") + actual);
  }
}

void CompactDecimalFormatTest::CheckLocale(const Locale& locale, UNumberCompactStyle style, const ExpectedResult* expectedResults, int32_t expectedResultLength) {
  UErrorCode status = U_ZERO_ERROR;
  LocalPointer<CompactDecimalFormat> cdf(createCDFInstance(locale, style, status));
  if (U_FAILURE(status)) {
    dataerrln("Unable to create format object - %s", u_errorName(status));
    return;
  }
  char description[256];
  sprintf(description,"%s - %s", locale.getName(), StyleStr(style));
  for (int32_t i = 0; i < expectedResultLength; i++) {
    CheckExpectedResult(cdf.getAlias(), &expectedResults[i], description);
  }
}

void CompactDecimalFormatTest::CheckExpectedResult(
    const CompactDecimalFormat* cdf, const ExpectedResult* expectedResult, const char* description) {
  UnicodeString actual;
  cdf->format(expectedResult->value, actual);
  UnicodeString expected(expectedResult->expected, -1, US_INV);
  expected = expected.unescape();
  if (actual != expected) {
    errln(UnicodeString("Fail: Expected: ") + expected
          + UnicodeString(" Got: ") + actual
          + UnicodeString(" for: ") + UnicodeString(description));
  }
}

CompactDecimalFormat*
CompactDecimalFormatTest::createCDFInstance(const Locale& locale, UNumberCompactStyle style, UErrorCode& status) {
  CompactDecimalFormat* result = CompactDecimalFormat::createInstance(locale, style, status);
  if (U_FAILURE(status)) {
    return NULL;
  }
  // All tests are written for two significant digits, so we explicitly set here
  // in case default significant digits change.
  result->setMaximumSignificantDigits(2);
  return result;
}

const char *CompactDecimalFormatTest::StyleStr(UNumberCompactStyle style) {
  if (style == UNUM_SHORT) {
    return kShortStr;
  }
  return kLongStr;
}

extern IntlTest *createCompactDecimalFormatTest() {
  return new CompactDecimalFormatTest();
}

#endif
