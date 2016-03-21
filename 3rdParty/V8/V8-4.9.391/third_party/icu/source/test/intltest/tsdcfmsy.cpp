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
             (UCurrencySpacing)i, TRUE, status);
        if(U_FAILURE(status)) {
            errln("Error: cannot get CurrencyMatch for locale:en");
            status = U_ZERO_ERROR;
        }
        UnicodeString frCurrencyPattern = fr.getPatternForCurrencySpacing(
             (UCurrencySpacing)i, TRUE, status);
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
            (UCurrencySpacing)i, FALSE, status);
        if(U_FAILURE(status)) {
            errln("Error: cannot get CurrencyMatch for locale:en");
            status = U_ZERO_ERROR;
        }
        UnicodeString frCurrencyPattern = fr.getPatternForCurrencySpacing(
             (UCurrencySpacing)i, FALSE, status);
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
    en.setPatternForCurrencySpacing(UNUM_CURRENCY_INSERT, TRUE, dash);
    UnicodeString enCurrencyInsert = en.getPatternForCurrencySpacing(
        UNUM_CURRENCY_INSERT, TRUE, status);
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
    Verify(34.5, (UnicodeString)"00.00", sym, (UnicodeString)"34.50");
    sym.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, customDecSeperator);
    Verify(34.5, (UnicodeString)"00.00", sym, (UnicodeString)"34S50");
    sym.setSymbol(DecimalFormatSymbols::kPercentSymbol, (UnicodeString)"P");
    Verify(34.5, (UnicodeString)"00 %", sym, (UnicodeString)"3450 P");
    sym.setSymbol(DecimalFormatSymbols::kCurrencySymbol, (UnicodeString)"D");
    Verify(34.5, CharsToUnicodeString("\\u00a4##.##"), sym, (UnicodeString)"D34.5");
    sym.setSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol, (UnicodeString)"|");
    Verify(3456.5, (UnicodeString)"0,000.##", sym, (UnicodeString)"3|456S5");

}

void IntlTestDecimalFormatSymbols::testLastResortData() {
    IcuTestErrorCode errorCode(*this, "testLastResortData");
    LocalPointer<DecimalFormatSymbols> lastResort(
        DecimalFormatSymbols::createWithLastResortData(errorCode));
    if(errorCode.logIfFailureAndReset("DecimalFormatSymbols::createWithLastResortData() failed")) {
        return;
    }
    DecimalFormatSymbols root(Locale::getRoot(), errorCode);
    if(errorCode.logDataIfFailureAndReset("DecimalFormatSymbols(root) failed")) {
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
                 UnicodeString((UChar)0xfffd), lastResort->getSymbol(DecimalFormatSymbols::kNaNSymbol));
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
