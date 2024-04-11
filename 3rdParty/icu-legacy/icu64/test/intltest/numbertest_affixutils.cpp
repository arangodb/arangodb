// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "putilimp.h"
#include "unicode/dcfmtsym.h"
#include "numbertest.h"
#include "number_utils.h"

using namespace icu::number::impl;

class DefaultSymbolProvider : public SymbolProvider {
    DecimalFormatSymbols fSymbols;

  public:
    DefaultSymbolProvider(UErrorCode &status) : fSymbols(Locale("ar_SA"), status) {}

    UnicodeString getSymbol(AffixPatternType type) const override {
        switch (type) {
            case TYPE_MINUS_SIGN:
                return u"−";
            case TYPE_PLUS_SIGN:
                return fSymbols.getConstSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPlusSignSymbol);
            case TYPE_APPROXIMATELY_SIGN:
                return u"≃";
            case TYPE_PERCENT:
                return fSymbols.getConstSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPercentSymbol);
            case TYPE_PERMILLE:
                return fSymbols.getConstSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPerMillSymbol);
            case TYPE_CURRENCY_SINGLE:
                return u"$";
            case TYPE_CURRENCY_DOUBLE:
                return u"XXX";
            case TYPE_CURRENCY_TRIPLE:
                return u"long name";
            case TYPE_CURRENCY_QUAD:
                return u"\uFFFD";
            case TYPE_CURRENCY_QUINT:
                // TODO: Add support for narrow currency symbols here.
                return u"\uFFFD";
            case TYPE_CURRENCY_OVERFLOW:
                return u"\uFFFD";
            default:
                UPRV_UNREACHABLE_EXIT;
        }
    }
};

void AffixUtilsTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite AffixUtilsTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testEscape);
        TESTCASE_AUTO(testUnescape);
        TESTCASE_AUTO(testContainsReplaceType);
        TESTCASE_AUTO(testInvalid);
        TESTCASE_AUTO(testUnescapeWithSymbolProvider);
    TESTCASE_AUTO_END;
}

void AffixUtilsTest::testEscape() {
    static const char16_t *cases[][2] = {{u"", u""},
                                         {u"abc", u"abc"},
                                         {u"-", u"'-'"},
                                         {u"-!", u"'-'!"},
                                         {u"−", u"−"},
                                         {u"---", u"'---'"},
                                         {u"-%-", u"'-%-'"},
                                         {u"'", u"''"},
                                         {u"-'", u"'-'''"},
                                         {u"-'-", u"'-''-'"},
                                         {u"a-'-", u"a'-''-'"}};

    for (auto &cas : cases) {
        UnicodeString input(cas[0]);
        UnicodeString expected(cas[1]);
        UnicodeString result = AffixUtils::escape(input);
        assertEquals(input, expected, result);
    }
}

void AffixUtilsTest::testUnescape() {
    static struct TestCase {
        const char16_t *input;
        bool currency;
        int32_t expectedLength;
        const char16_t *output;
    } cases[] = {{u"", false, 0, u""},
                 {u"abc", false, 3, u"abc"},
                 {u"-", false, 1, u"−"},
                 {u"-!", false, 2, u"−!"},
                 {u"+", false, 1, u"\u061C+"},
                 {u"+!", false, 2, u"\u061C+!"},
                 {u"~", false, 1, u"≃"},
                 {u"‰", false, 1, u"؉"},
                 {u"‰!", false, 2, u"؉!"},
                 {u"-x", false, 2, u"−x"},
                 {u"'-'x", false, 2, u"-x"},
                 {u"'--''-'-x", false, 6, u"--'-−x"},
                 {u"''", false, 1, u"'"},
                 {u"''''", false, 2, u"''"},
                 {u"''''''", false, 3, u"'''"},
                 {u"''x''", false, 3, u"'x'"},
                 {u"¤", true, 1, u"$"},
                 {u"¤¤", true, 2, u"XXX"},
                 {u"¤¤¤", true, 3, u"long name"},
                 {u"¤¤¤¤", true, 4, u"\uFFFD"},
                 {u"¤¤¤¤¤", true, 5, u"\uFFFD"},
                 {u"¤¤¤¤¤¤", true, 6, u"\uFFFD"},
                 {u"¤¤¤a¤¤¤¤", true, 8, u"long namea\uFFFD"},
                 {u"a¤¤¤¤b¤¤¤¤¤c", true, 12, u"a\uFFFDb\uFFFDc"},
                 {u"¤!", true, 2, u"$!"},
                 {u"¤¤!", true, 3, u"XXX!"},
                 {u"¤¤¤!", true, 4, u"long name!"},
                 {u"-¤¤", true, 3, u"−XXX"},
                 {u"¤¤-", true, 3, u"XXX−"},
                 {u"'¤'", false, 1, u"¤"},
                 {u"%", false, 1, u"٪\u061C"},
                 {u"'%'", false, 1, u"%"},
                 {u"¤'-'%", true, 3, u"$-٪\u061C"},
                 {u"#0#@#*#;#", false, 9, u"#0#@#*#;#"}};

    UErrorCode status = U_ZERO_ERROR;
    DefaultSymbolProvider defaultProvider(status);
    assertSuccess("Constructing DefaultSymbolProvider", status);

    for (TestCase cas : cases) {
        UnicodeString input(cas.input);
        UnicodeString output(cas.output);

        assertEquals(input, cas.currency, AffixUtils::hasCurrencySymbols(input, status));
        assertSuccess("Spot 1", status);
        assertEquals(input, cas.expectedLength, AffixUtils::estimateLength(input, status));
        assertSuccess("Spot 2", status);

        UnicodeString actual = unescapeWithDefaults(defaultProvider, input, status);
        assertSuccess("Spot 3", status);
        assertEquals(input, output, actual);

        int32_t ulength = AffixUtils::unescapedCodePointCount(input, defaultProvider, status);
        assertSuccess("Spot 4", status);
        assertEquals(input, output.countChar32(), ulength);
    }
}

void AffixUtilsTest::testContainsReplaceType() {
    static struct TestCase {
        const char16_t *input;
        bool hasMinusSign;
        const char16_t *output;
    } cases[] = {{u"", false, u""},
                 {u"-", true, u"+"},
                 {u"-a", true, u"+a"},
                 {u"a-", true, u"a+"},
                 {u"a-b", true, u"a+b"},
                 {u"--", true, u"++"},
                 {u"x", false, u"x"}};

    UErrorCode status = U_ZERO_ERROR;
    for (TestCase cas : cases) {
        UnicodeString input(cas.input);
        bool hasMinusSign = cas.hasMinusSign;
        UnicodeString output(cas.output);

        assertEquals(
                input, hasMinusSign, AffixUtils::containsType(input, TYPE_MINUS_SIGN, status));
        assertSuccess("Spot 1", status);
        assertEquals(
                input, output, AffixUtils::replaceType(input, TYPE_MINUS_SIGN, u'+', status));
        assertSuccess("Spot 2", status);
    }
}

void AffixUtilsTest::testInvalid() {
    static const char16_t *invalidExamples[] = {
            u"'", u"x'", u"'x", u"'x''", u"''x'"};

    UErrorCode status = U_ZERO_ERROR;
    DefaultSymbolProvider defaultProvider(status);
    assertSuccess("Constructing DefaultSymbolProvider", status);

    for (const char16_t *strPtr : invalidExamples) {
        UnicodeString str(strPtr);

        status = U_ZERO_ERROR;
        AffixUtils::hasCurrencySymbols(str, status);
        assertEquals("Should set error code spot 1", status, U_ILLEGAL_ARGUMENT_ERROR);

        status = U_ZERO_ERROR;
        AffixUtils::estimateLength(str, status);
        assertEquals("Should set error code spot 2", status, U_ILLEGAL_ARGUMENT_ERROR);

        status = U_ZERO_ERROR;
        unescapeWithDefaults(defaultProvider, str, status);
        assertEquals("Should set error code spot 3", status, U_ILLEGAL_ARGUMENT_ERROR);
    }
}

class NumericSymbolProvider : public SymbolProvider {
  public:
    virtual UnicodeString getSymbol(AffixPatternType type) const override {
        return Int64ToUnicodeString(type < 0 ? -type : type);
    }
};

void AffixUtilsTest::testUnescapeWithSymbolProvider() {
    static const char16_t* cases[][2] = {
            {u"", u""},
            {u"-", u"1"},
            {u"'-'", u"-"},
            {u"- + ~ % ‰ ¤ ¤¤ ¤¤¤ ¤¤¤¤ ¤¤¤¤¤", u"1 2 3 4 5 6 7 8 9 10"},
            {u"'¤¤¤¤¤¤'", u"¤¤¤¤¤¤"},
            {u"¤¤¤¤¤¤", u"\uFFFD"}
    };

    NumericSymbolProvider provider;

    UErrorCode status = U_ZERO_ERROR;
    FormattedStringBuilder sb;
    for (auto& cas : cases) {
        UnicodeString input(cas[0]);
        UnicodeString expected(cas[1]);
        sb.clear();
        AffixUtils::unescape(input, sb, 0, provider, kUndefinedField, status);
        assertSuccess("Spot 1", status);
        assertEquals(input, expected, sb.toUnicodeString());
        assertEquals(input, expected, sb.toTempUnicodeString());
    }

    // Test insertion position
    sb.clear();
    sb.append(u"abcdefg", kUndefinedField, status);
    assertSuccess("Spot 2", status);
    AffixUtils::unescape(u"-+~", sb, 4, provider, kUndefinedField, status);
    assertSuccess("Spot 3", status);
    assertEquals(u"Symbol provider into middle", u"abcd123efg", sb.toUnicodeString());
}

UnicodeString AffixUtilsTest::unescapeWithDefaults(const SymbolProvider &defaultProvider,
                                                          UnicodeString input, UErrorCode &status) {
    FormattedStringBuilder nsb;
    int32_t length = AffixUtils::unescape(input, nsb, 0, defaultProvider, kUndefinedField, status);
    assertEquals("Return value of unescape", nsb.length(), length);
    return nsb.toUnicodeString();
}

#endif /* #if !UCONFIG_NO_FORMATTING */
