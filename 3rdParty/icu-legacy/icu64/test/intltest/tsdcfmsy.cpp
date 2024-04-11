// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2013, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/unum.h"
#include "tsdcfmsy.h"

void IntlTestDecimalFormatSymbols::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) {
        logln("TestSuite DecimalFormatSymbols:");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testSymbols);
    TESTCASE_AUTO(testLastResortData);
    TESTCASE_AUTO(testDigitSymbols);
    TESTCASE_AUTO(testNumberingSystem);
    TESTCASE_AUTO_END;
}

/**
 * Test the API of DecimalFormatSymbols; primarily a simple get/set set.
 */
void IntlTestDecimalFormatSymbols::testSymbols(/* char *par */)
{
    UErrorCode status = U_ZERO_ERROR;

    DecimalFormatSymbols fr(Locale::getFrench(), status);
    if(U_FAILURE(status)) {
        errcheckln(status, "ERROR: Couldn't create French DecimalFormatSymbols - %s", u_errorName(status));
        return;
    }

    status = U_ZERO_ERROR;
    DecimalFormatSymbols en(Locale::getEnglish(), status);
    if(U_FAILURE(status)) {
        errcheckln(status, "ERROR: Couldn't create English DecimalFormatSymbols - %s", u_errorName(status));
        return;
    }

    if(en == fr || ! (en != fr) ) {
        errln("ERROR: English DecimalFormatSymbols equal to French");
    }

    // just do some VERY basic tests to make sure that get/set work

    UnicodeString zero = en.getSymbol(DecimalFormatSymbols::kZeroDigitSymbol);
    fr.setSymbol(DecimalFormatSymbols::kZeroDigitSymbol, zero);
    if(fr.getSymbol(DecimalFormatSymbols::kZeroDigitSymbol) != en.getSymbol(DecimalFormatSymbols::kZeroDigitSymbol)) {
        errln("ERROR: get/set ZeroDigit failed");
    }

    UnicodeString group = en.getSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol);
    fr.setSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol, group);
    if(fr.getSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol) != en.getSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol)) {
        errln("ERROR: get/set GroupingSeparator failed");
    }

    UnicodeString decimal = en.getSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol);
    fr.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, decimal);
    if(fr.getSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol) != en.getSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol)) {
        errln("ERROR: get/set DecimalSeparator failed");
    }

    UnicodeString perMill = en.getSymbol(DecimalFormatSymbols::kPerMillSymbol);
    fr.setSymbol(DecimalFormatSymbols::kPerMillSymbol, perMill);
    if(fr.getSymbol(DecimalFormatSymbols::kPerMillSymbol) != en.getSymbol(DecimalFormatSymbols::kPerMillSymbol)) {
        errln("ERROR: get/set PerMill failed");
    }

    UnicodeString percent = en.getSymbol(DecimalFormatSymbols::kPercentSymbol);
    fr.setSymbol(DecimalFormatSymbols::kPercentSymbol, percent);
    if(fr.getSymbol(DecimalFormatSymbols::kPercentSymbol) != en.getSymbol(DecimalFormatSymbols::kPercentSymbol)) {
        errln("ERROR: get/set Percent failed");
    }

    UnicodeString digit(en.getSymbol(DecimalFormatSymbols::kDigitSymbol));
    fr.setSymbol(DecimalFormatSymbols::kDigitSymbol, digit);
    if(fr.getSymbol(DecimalFormatSymbols::kDigitSymbol) != en.getSymbol(DecimalFormatSymbols::kDigitSymbol)) {
        errln("ERROR: get/set Percent failed");
    }

    UnicodeString patternSeparator = en.getSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol);
    fr.setSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol, patternSeparator);
    if(fr.getSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol) != en.getSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol)) {
        errln("ERROR: get/set PatternSeparator failed");
    }

    UnicodeString infinity(en.getSymbol(DecimalFormatSymbols::kInfinitySymbol));
    fr.setSymbol(DecimalFormatSymbols::kInfinitySymbol, infinity);
    UnicodeString infinity2(fr.getSymbol(DecimalFormatSymbols::kInfinitySymbol));
    if(infinity != infinity2) {
        errln("ERROR: get/set Infinity failed");
    }

    UnicodeString nan(en.getSymbol(DecimalFormatSymbols::kNaNSymbol));
    fr.setSymbol(DecimalFormatSymbols::kNaNSymbol, nan);
    UnicodeString nan2(fr.getSymbol(DecimalFormatSymbols::kNaNSymbol));
    if(nan != nan2) {
        errln("ERROR: get/set NaN failed");
    }

    UnicodeString minusSign = en.getSymbol(DecimalFormatSymbols::kMinusSignSymbol);
    fr.setSymbol(DecimalFormatSymbols::kMinusSignSymbol, minusSign);
    if(fr.getSymbol(DecimalFormatSymbols::kMinusSignSymbol) != en.getSymbol(DecimalFormatSymbols::kMinusSignSymbol)) {
        errln("ERROR: get/set MinusSign failed");
    }

    UnicodeString exponential(en.getSymbol(DecimalFormatSymbols::kExponentialSymbol));
    fr.setSymbol(DecimalFormatSymbols::kExponentialSymbol, exponential);
    if(fr.getSymbol(DecimalFormatSymbols::kExponentialSymbol) != en.getSymbol(DecimalFormatSymbols::kExponentialSymbol)) {
        errln("ERROR: get/set Exponential failed");
    }

    // Test get currency spacing before the currency.
    status = U_ZERO_ERROR;
    for (int32_t i = 0; i < (int32_t)UNUM_CURRENCY_SPACING_COUNT; i++) {
        UnicodeString enCurrencyPattern = en.getPatternForCurrencySpacing(
             (UCurrencySpacing)i, true, status);
        if(U_FAILURE(status)) {
            errln("Error: cannot get CurrencyMatch for locale:en");
            status = U_ZERO_ERROR;
        }
        UnicodeString frCurrencyPattern = fr.getPatternForCurrencySpacing(
             (UCurrencySpacing)i, true, status);
        if(U_FAILURE(status)) {
            errln("Error: cannot get CurrencyMatch for locale:fr");
        }
        if (enCurrencyPattern != frCurrencyPattern) {
           errln("ERROR: get CurrencySpacing failed");
        }
    }
    // Test get currencySpacing after the currency.
    status = U_ZERO_ERROR;
    for (int32_t i = 0; i < UNUM_CURRENCY_SPACING_COUNT; i++) {
        UnicodeString enCurrencyPattern = en.getPatternForCurrencySpacing(
            (UCurrencySpacing)i, false, status);
        if(U_FAILURE(status)) {
            errln("Error: cannot get CurrencyMatch for locale:en");
            status = U_ZERO_ERROR;
        }
        UnicodeString frCurrencyPattern = fr.getPatternForCurrencySpacing(
             (UCurrencySpacing)i, false, status);
        if(U_FAILURE(status)) {
            errln("Error: cannot get CurrencyMatch for locale:fr");
        }
        if (enCurrencyPattern != frCurrencyPattern) {
            errln("ERROR: get CurrencySpacing failed");
        }
    }
    // Test set curerncySpacing APIs
    status = U_ZERO_ERROR;
    UnicodeString dash = UnicodeString("-");
    en.setPatternForCurrencySpacing(UNUM_CURRENCY_INSERT, true, dash);
    UnicodeString enCurrencyInsert = en.getPatternForCurrencySpacing(
        UNUM_CURRENCY_INSERT, true, status);
    if (dash != enCurrencyInsert) {
        errln("Error: Failed to setCurrencyInsert for locale:en");
    }

    status = U_ZERO_ERROR;
    DecimalFormatSymbols foo(status);

    DecimalFormatSymbols bar(foo);

    en = fr;

    if(en != fr || foo != bar) {
        errln("ERROR: Copy Constructor or Assignment failed");
    }

    // test get/setSymbol()
    if((int) UNUM_FORMAT_SYMBOL_COUNT != (int) DecimalFormatSymbols::kFormatSymbolCount) {
        errln("unum.h and decimfmt.h have inconsistent numbers of format symbols!");
        return;
    }

    int i;
    for(i = 0; i < (int)DecimalFormatSymbols::kFormatSymbolCount; ++i) {
        foo.setSymbol((DecimalFormatSymbols::ENumberFormatSymbol)i, UnicodeString((UChar32)(0x10330 + i)));
    }
    for(i = 0; i < (int)DecimalFormatSymbols::kFormatSymbolCount; ++i) {
        if(foo.getSymbol((DecimalFormatSymbols::ENumberFormatSymbol)i) != UnicodeString((UChar32)(0x10330 + i))) {
            errln("get/setSymbol did not roundtrip, got " +
                  foo.getSymbol((DecimalFormatSymbols::ENumberFormatSymbol)i) +
                  ", expected " +
                  UnicodeString((UChar32)(0x10330 + i)));
        }
    }

    DecimalFormatSymbols sym(Locale::getUS(), status);

    UnicodeString customDecSeperator("S");
    Verify(34.5, u"00.00", sym, u"34.50");
    sym.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, customDecSeperator);
    Verify(34.5, u"00.00", sym, u"34S50");
    sym.setSymbol(DecimalFormatSymbols::kPercentSymbol, u"P");
    Verify(34.5, u"00 %", sym, u"3450 P");
    sym.setSymbol(DecimalFormatSymbols::kCurrencySymbol, u"D");
    Verify(34.5, u"\u00a4##.##", sym, u"D\u00a034.50");
    sym.setSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol, u"|");
    Verify(3456.5, u"0,000.##", sym, u"3|456S5");

}

void IntlTestDecimalFormatSymbols::testLastResortData() {
    IcuTestErrorCode errorCode(*this, "testLastResortData");
    LocalPointer<DecimalFormatSymbols> lastResort(
        DecimalFormatSymbols::createWithLastResortData(errorCode));
    if(errorCode.errIfFailureAndReset("DecimalFormatSymbols::createWithLastResortData() failed")) {
        return;
    }
    DecimalFormatSymbols root(Locale::getRoot(), errorCode);
    if(errorCode.errDataIfFailureAndReset("DecimalFormatSymbols(root) failed")) {
        return;
    }
    // Note: It is not necessary that the last resort data matches the root locale,
    // but it seems weird if most symbols did not match.
    // Also, one purpose for calling operator==() is to find uninitialized memory in a debug build.
    if(*lastResort == root) {
        errln("DecimalFormatSymbols last resort data unexpectedly matches root");
    }
    // Here we adjust for expected differences.
    assertEquals("last-resort grouping separator",
                 "", lastResort->getSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol));
    lastResort->setSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol, ",");
    assertEquals("last-resort monetary grouping separator",
                 "", lastResort->getSymbol(DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol));
    lastResort->setSymbol(DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol, ",");
    assertEquals("last-resort NaN",
                 UnicodeString((char16_t)0xfffd), lastResort->getSymbol(DecimalFormatSymbols::kNaNSymbol));
    lastResort->setSymbol(DecimalFormatSymbols::kNaNSymbol, "NaN");
    // Check that now all of the symbols match root.
    for(int32_t i = 0; i < DecimalFormatSymbols::kFormatSymbolCount; ++i) {
        DecimalFormatSymbols::ENumberFormatSymbol e = (DecimalFormatSymbols::ENumberFormatSymbol)i;
        assertEquals("last-resort symbol vs. root", root.getSymbol(e), lastResort->getSymbol(e));
    }
    // Also, the CurrencySpacing patterns are empty in the last resort instance,
    // but not in root.
    Verify(1234567.25, "#,##0.##", *lastResort, "1,234,567.25");
}

void IntlTestDecimalFormatSymbols::testDigitSymbols() {
    // This test does more in ICU4J than in ICU4C right now.
    // In ICU4C, it is basically just a test for codePointZero and getConstDigitSymbol.
    char16_t defZero = u'0';
    UChar32 osmanyaZero = U'\U000104A0';
    static const char16_t* osmanyaDigitStrings[] = {
        u"\U000104A0", u"\U000104A1", u"\U000104A2", u"\U000104A3", u"\U000104A4",
        u"\U000104A5", u"\U000104A6", u"\U000104A7", u"\U000104A8", u"\U000104A9"
    };

    IcuTestErrorCode status(*this, "testDigitSymbols()");
    DecimalFormatSymbols symbols(Locale("en"), status);

    if (defZero != symbols.getCodePointZero()) {
        errln("ERROR: Code point zero be ASCII 0");
    }
    for (int32_t i=0; i<=9; i++) {
        assertEquals(UnicodeString("i. ASCII Digit at index ") + Int64ToUnicodeString(i),
            UnicodeString(u'0' + i),
            symbols.getConstDigitSymbol(i));
    }

    for (int32_t i=0; i<=9; i++) {
        DecimalFormatSymbols::ENumberFormatSymbol key =
            i == 0
            ? DecimalFormatSymbols::kZeroDigitSymbol
            : static_cast<DecimalFormatSymbols::ENumberFormatSymbol>
                (DecimalFormatSymbols::kOneDigitSymbol + i - 1);
        symbols.setSymbol(key, UnicodeString(osmanyaDigitStrings[i]), false);
    }
    // NOTE: in ICU4J, the calculation of codePointZero is smarter;
    // in ICU4C, it is more conservative and is only set if propagateDigits is true.
    if (-1 != symbols.getCodePointZero()) {
        errln("ERROR: Code point zero be invalid");
    }
    for (int32_t i=0; i<=9; i++) {
        assertEquals(UnicodeString("ii. Osmanya digit at index ") + Int64ToUnicodeString(i),
            UnicodeString(osmanyaDigitStrings[i]),
            symbols.getConstDigitSymbol(i));
    }

    // Check Osmanya codePointZero
    symbols.setSymbol(
        DecimalFormatSymbols::kZeroDigitSymbol,
        UnicodeString(osmanyaDigitStrings[0]), true);
    if (osmanyaZero != symbols.getCodePointZero()) {
        errln("ERROR: Code point zero be Osmanya code point zero");
    }
    for (int32_t i=0; i<=9; i++) {
        assertEquals(UnicodeString("iii. Osmanya digit at index ") + Int64ToUnicodeString(i),
            UnicodeString(osmanyaDigitStrings[i]),
            symbols.getConstDigitSymbol(i));
    }

    // Check after copy
    DecimalFormatSymbols copy(symbols);
    if (osmanyaZero != copy.getCodePointZero()) {
        errln("ERROR: Code point zero be Osmanya code point zero");
    }
    for (int32_t i=0; i<=9; i++) {
        assertEquals(UnicodeString("iv. After copy at index ") + Int64ToUnicodeString(i),
            UnicodeString(osmanyaDigitStrings[i]),
            copy.getConstDigitSymbol(i));
    }

    // Check when loaded from resource bundle
    DecimalFormatSymbols fromData(Locale("en@numbers=osma"), status);
    if (osmanyaZero != fromData.getCodePointZero()) {
        errln("ERROR: Code point zero be Osmanya code point zero");
    }
    for (int32_t i=0; i<=9; i++) {
        assertEquals(UnicodeString("v. Resource bundle at index ") + Int64ToUnicodeString(i),
            UnicodeString(osmanyaDigitStrings[i]),
            fromData.getConstDigitSymbol(i));
    }

    // Setting a digit somewhere in the middle should invalidate codePointZero
    symbols.setSymbol(DecimalFormatSymbols::kOneDigitSymbol, u"foo", false);
    if (-1 != symbols.getCodePointZero()) {
        errln("ERROR: Code point zero be invalid");
    }

    // Reset digits to Latin
    symbols.setSymbol(
        DecimalFormatSymbols::kZeroDigitSymbol,
        UnicodeString(defZero));
    if (defZero != symbols.getCodePointZero()) {
        errln("ERROR: Code point zero be ASCII 0");
    }
    for (int32_t i=0; i<=9; i++) {
        assertEquals(UnicodeString("vi. ASCII Digit at index ") + Int64ToUnicodeString(i),
            UnicodeString(u'0' + i),
            symbols.getConstDigitSymbol(i));
    }
}

void IntlTestDecimalFormatSymbols::testNumberingSystem() {
    IcuTestErrorCode errorCode(*this, "testNumberingSystem");
    struct testcase {
        const char* locid;
        const char* nsname;
        const char16_t* expected1; // Expected number format string
        const char16_t* expected2; // Expected pattern separator
    };
    static const testcase cases[] = {
            {"en", "latn", u"1,234.56", u"%"},
            {"en", "arab", u"١٬٢٣٤٫٥٦", u"٪\u061C"},
            {"en", "mathsanb", u"𝟭,𝟮𝟯𝟰.𝟱𝟲", u"%"},
            {"en", "mymr", u"၁,၂၃၄.၅၆", u"%"},
            {"my", "latn", u"1,234.56", u"%"},
            {"my", "arab", u"١٬٢٣٤٫٥٦", u"٪\u061C"},
            {"my", "mathsanb", u"𝟭,𝟮𝟯𝟰.𝟱𝟲", u"%"},
            {"my", "mymr", u"၁,၂၃၄.၅၆", u"%"},
            {"ar", "latn", u"1,234.56", u"\u200E%\u200E"},
            {"ar", "arab", u"١٬٢٣٤٫٥٦", u"٪\u061C"},
            {"en@numbers=thai", "mymr", u"၁,၂၃၄.၅၆", u"%"}, // conflicting numbering system
    };

    for (int i=0; i<8; i++) {
        testcase cas = cases[i];
        Locale loc(cas.locid);
        LocalPointer<NumberingSystem> ns(NumberingSystem::createInstanceByName(cas.nsname, errorCode));
        if (errorCode.errDataIfFailureAndReset("NumberingSystem failed")) {
            return;
        }
        UnicodeString expected1(cas.expected1);
        UnicodeString expected2(cas.expected2);
        DecimalFormatSymbols dfs(loc, *ns, errorCode);
        if (errorCode.errDataIfFailureAndReset("DecimalFormatSymbols failed")) {
            return;
        }
        Verify(1234.56, "#,##0.##", dfs, expected1);
        // The percent sign differs by numbering system.
        UnicodeString actual2 = dfs.getSymbol(DecimalFormatSymbols::kPercentSymbol);
        assertEquals((UnicodeString) "Percent sign with " + cas.locid + " and " + cas.nsname,
            expected2,
            actual2);
    }
}

void IntlTestDecimalFormatSymbols::Verify(double value, const UnicodeString& pattern,
                                          const DecimalFormatSymbols &sym, const UnicodeString& expected){
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat df(pattern, sym, status);
    if(U_FAILURE(status)){
        errln("ERROR: construction of decimal format failed - %s", u_errorName(status));
    }
    UnicodeString buffer;
    FieldPosition pos(FieldPosition::DONT_CARE);
    buffer = df.format(value, buffer, pos);
    if(buffer != expected){
        errln((UnicodeString)"ERROR: format() returns wrong result\n Expected " +
            expected + ", Got " + buffer);
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
