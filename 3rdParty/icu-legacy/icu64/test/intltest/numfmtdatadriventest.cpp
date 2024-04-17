// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numfmtst.h"
#include "number_decimalquantity.h"
#include "putilimp.h"
#include "charstr.h"
#include <cmath>

using icu::number::impl::DecimalQuantity;

void NumberFormatDataDrivenTest::runIndexedTest(int32_t index, UBool exec, const char*& name, char*) {
    if (exec) {
        logln("TestSuite NumberFormatDataDrivenTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(TestNumberFormatTestTuple);
        TESTCASE_AUTO(TestDataDrivenICU4C);
    TESTCASE_AUTO_END;
}

static DecimalQuantity&
strToDigitList(const UnicodeString& str, DecimalQuantity& digitList, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return digitList;
    }
    if (str == "NaN") {
        digitList.setToDouble(uprv_getNaN());
        return digitList;
    }
    if (str == "-Inf") {
        digitList.setToDouble(-1 * uprv_getInfinity());
        return digitList;
    }
    if (str == "Inf") {
        digitList.setToDouble(uprv_getInfinity());
        return digitList;
    }
    CharString formatValue;
    formatValue.appendInvariantChars(str, status);
    digitList.setToDecNumber({formatValue.data(), formatValue.length()}, status);
    return digitList;
}

static UnicodeString&
format(const DecimalFormat& fmt, const DecimalQuantity& digitList, UnicodeString& appendTo,
       UErrorCode& status) {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    FieldPosition fpos(FieldPosition::DONT_CARE);
    return fmt.format(digitList, appendTo, fpos, status);
}

template<class T>
static UnicodeString&
format(const DecimalFormat& fmt, T value, UnicodeString& appendTo, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    FieldPosition fpos(FieldPosition::DONT_CARE);
    return fmt.format(value, appendTo, fpos, status);
}

static void adjustDecimalFormat(const NumberFormatTestTuple& tuple, DecimalFormat& fmt,
                                UnicodeString& appendErrorMessage) {
    if (tuple.minIntegerDigitsFlag) {
        fmt.setMinimumIntegerDigits(tuple.minIntegerDigits);
    }
    if (tuple.maxIntegerDigitsFlag) {
        fmt.setMaximumIntegerDigits(tuple.maxIntegerDigits);
    }
    if (tuple.minFractionDigitsFlag) {
        fmt.setMinimumFractionDigits(tuple.minFractionDigits);
    }
    if (tuple.maxFractionDigitsFlag) {
        fmt.setMaximumFractionDigits(tuple.maxFractionDigits);
    }
    if (tuple.currencyFlag) {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString currency(tuple.currency);
        const UChar* terminatedCurrency = currency.getTerminatedBuffer();
        fmt.setCurrency(terminatedCurrency, status);
        if (U_FAILURE(status)) {
            appendErrorMessage.append("Error setting currency.");
        }
    }
    if (tuple.minGroupingDigitsFlag) {
        fmt.setMinimumGroupingDigits(tuple.minGroupingDigits);
    }
    if (tuple.useSigDigitsFlag) {
        fmt.setSignificantDigitsUsed(tuple.useSigDigits != 0);
    }
    if (tuple.minSigDigitsFlag) {
        fmt.setMinimumSignificantDigits(tuple.minSigDigits);
    }
    if (tuple.maxSigDigitsFlag) {
        fmt.setMaximumSignificantDigits(tuple.maxSigDigits);
    }
    if (tuple.useGroupingFlag) {
        fmt.setGroupingUsed(tuple.useGrouping != 0);
    }
    if (tuple.multiplierFlag) {
        fmt.setMultiplier(tuple.multiplier);
    }
    if (tuple.roundingIncrementFlag) {
        fmt.setRoundingIncrement(tuple.roundingIncrement);
    }
    if (tuple.formatWidthFlag) {
        fmt.setFormatWidth(tuple.formatWidth);
    }
    if (tuple.padCharacterFlag) {
        fmt.setPadCharacter(tuple.padCharacter);
    }
    if (tuple.useScientificFlag) {
        fmt.setScientificNotation(tuple.useScientific != 0);
    }
    if (tuple.groupingFlag) {
        fmt.setGroupingSize(tuple.grouping);
    }
    if (tuple.grouping2Flag) {
        fmt.setSecondaryGroupingSize(tuple.grouping2);
    }
    if (tuple.roundingModeFlag) {
        fmt.setRoundingMode(tuple.roundingMode);
    }
    if (tuple.currencyUsageFlag) {
        UErrorCode status = U_ZERO_ERROR;
        fmt.setCurrencyUsage(tuple.currencyUsage, &status);
        if (U_FAILURE(status)) {
            appendErrorMessage.append("CurrencyUsage: error setting.");
        }
    }
    if (tuple.minimumExponentDigitsFlag) {
        fmt.setMinimumExponentDigits(tuple.minimumExponentDigits);
    }
    if (tuple.exponentSignAlwaysShownFlag) {
        fmt.setExponentSignAlwaysShown(tuple.exponentSignAlwaysShown != 0);
    }
    if (tuple.decimalSeparatorAlwaysShownFlag) {
        fmt.setDecimalSeparatorAlwaysShown(
                tuple.decimalSeparatorAlwaysShown != 0);
    }
    if (tuple.padPositionFlag) {
        fmt.setPadPosition(tuple.padPosition);
    }
    if (tuple.positivePrefixFlag) {
        fmt.setPositivePrefix(tuple.positivePrefix);
    }
    if (tuple.positiveSuffixFlag) {
        fmt.setPositiveSuffix(tuple.positiveSuffix);
    }
    if (tuple.negativePrefixFlag) {
        fmt.setNegativePrefix(tuple.negativePrefix);
    }
    if (tuple.negativeSuffixFlag) {
        fmt.setNegativeSuffix(tuple.negativeSuffix);
    }
    if (tuple.signAlwaysShownFlag) {
        fmt.setSignAlwaysShown(tuple.signAlwaysShown != 0);
    }
    if (tuple.localizedPatternFlag) {
        UErrorCode status = U_ZERO_ERROR;
        fmt.applyLocalizedPattern(tuple.localizedPattern, status);
        if (U_FAILURE(status)) {
            appendErrorMessage.append("Error setting localized pattern.");
        }
    }
    fmt.setLenient(NFTT_GET_FIELD(tuple, lenient, 1) != 0);
    if (tuple.parseIntegerOnlyFlag) {
        fmt.setParseIntegerOnly(tuple.parseIntegerOnly != 0);
    }
    if (tuple.decimalPatternMatchRequiredFlag) {
        fmt.setDecimalPatternMatchRequired(
                tuple.decimalPatternMatchRequired != 0);
    }
    if (tuple.parseNoExponentFlag) {
        UErrorCode status = U_ZERO_ERROR;
        fmt.setAttribute(
                UNUM_PARSE_NO_EXPONENT, tuple.parseNoExponent, status);
        if (U_FAILURE(status)) {
            appendErrorMessage.append("Error setting parse no exponent flag.");
        }
    }
    if (tuple.parseCaseSensitiveFlag) {
        fmt.setParseCaseSensitive(tuple.parseCaseSensitive != 0);
    }
}

static DecimalFormat*
newDecimalFormat(const Locale& locale, const UnicodeString& pattern, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    LocalPointer<DecimalFormatSymbols> symbols(
            new DecimalFormatSymbols(locale, status), status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    UParseError perror;
    LocalPointer<DecimalFormat> result(
            new DecimalFormat(
                    pattern, symbols.getAlias(), perror, status), status);
    if (!result.isNull()) {
        symbols.orphan();
    }
    if (U_FAILURE(status)) {
        return NULL;
    }
    return result.orphan();
}

static DecimalFormat* newDecimalFormat(const NumberFormatTestTuple& tuple, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    Locale en("en");
    return newDecimalFormat(NFTT_GET_FIELD(tuple, locale, en),
            NFTT_GET_FIELD(tuple, pattern, "0"),
            status);
}

UBool NumberFormatDataDrivenTest::isFormatPass(const NumberFormatTestTuple& tuple,
                                               UnicodeString& appendErrorMessage, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    LocalPointer<DecimalFormat> fmtPtr(newDecimalFormat(tuple, status));
    if (U_FAILURE(status)) {
        appendErrorMessage.append("Error creating DecimalFormat.");
        return FALSE;
    }
    adjustDecimalFormat(tuple, *fmtPtr, appendErrorMessage);
    if (appendErrorMessage.length() > 0) {
        return FALSE;
    }
    DecimalQuantity digitList;
    strToDigitList(tuple.format, digitList, status);
    {
        UnicodeString appendTo;
        format(*fmtPtr, digitList, appendTo, status);
        if (U_FAILURE(status)) {
            appendErrorMessage.append("Error formatting.");
            return FALSE;
        }
        if (appendTo != tuple.output) {
            appendErrorMessage.append(
                    UnicodeString("Expected: ") + tuple.output + ", got: " + appendTo);
            return FALSE;
        }
    }
    double doubleVal = digitList.toDouble();
    DecimalQuantity doubleCheck;
    doubleCheck.setToDouble(doubleVal);
    if (digitList == doubleCheck) { // skip cases where the double does not round-trip
        UnicodeString appendTo;
        format(*fmtPtr, doubleVal, appendTo, status);
        if (U_FAILURE(status)) {
            appendErrorMessage.append("Error formatting.");
            return FALSE;
        }
        if (appendTo != tuple.output) {
            appendErrorMessage.append(
                    UnicodeString("double Expected: ") + tuple.output + ", got: " + appendTo);
            return FALSE;
        }
    }
    if (!uprv_isNaN(doubleVal) && !uprv_isInfinite(doubleVal) && digitList.fitsInLong()) {
        int64_t intVal = digitList.toLong();
        {
            UnicodeString appendTo;
            format(*fmtPtr, intVal, appendTo, status);
            if (U_FAILURE(status)) {
                appendErrorMessage.append("Error formatting.");
                return FALSE;
            }
            if (appendTo != tuple.output) {
                appendErrorMessage.append(
                        UnicodeString("int64 Expected: ") + tuple.output + ", got: " + appendTo);
                return FALSE;
            }
        }
    }
    return TRUE;
}

UBool NumberFormatDataDrivenTest::isToPatternPass(const NumberFormatTestTuple& tuple,
                                                  UnicodeString& appendErrorMessage, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    LocalPointer<DecimalFormat> fmtPtr(newDecimalFormat(tuple, status));
    if (U_FAILURE(status)) {
        appendErrorMessage.append("Error creating DecimalFormat.");
        return FALSE;
    }
    adjustDecimalFormat(tuple, *fmtPtr, appendErrorMessage);
    if (appendErrorMessage.length() > 0) {
        return FALSE;
    }
    if (tuple.toPatternFlag) {
        UnicodeString actual;
        fmtPtr->toPattern(actual);
        if (actual != tuple.toPattern) {
            appendErrorMessage.append(
                    UnicodeString("Expected: ") + tuple.toPattern + ", got: " + actual + ". ");
        }
    }
    if (tuple.toLocalizedPatternFlag) {
        UnicodeString actual;
        fmtPtr->toLocalizedPattern(actual);
        if (actual != tuple.toLocalizedPattern) {
            appendErrorMessage.append(
                    UnicodeString("Expected: ") + tuple.toLocalizedPattern + ", got: " + actual + ". ");
        }
    }
    return appendErrorMessage.length() == 0;
}

UBool NumberFormatDataDrivenTest::isParsePass(const NumberFormatTestTuple& tuple,
                                              UnicodeString& appendErrorMessage, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    LocalPointer<DecimalFormat> fmtPtr(newDecimalFormat(tuple, status));
    if (U_FAILURE(status)) {
        appendErrorMessage.append("Error creating DecimalFormat.");
        return FALSE;
    }
    adjustDecimalFormat(tuple, *fmtPtr, appendErrorMessage);
    if (appendErrorMessage.length() > 0) {
        return FALSE;
    }
    Formattable result;
    ParsePosition ppos;
    fmtPtr->parse(tuple.parse, result, ppos);
    if (ppos.getIndex() == 0) {
        appendErrorMessage.append("Parse failed; got error index ");
        appendErrorMessage = appendErrorMessage + ppos.getErrorIndex();
        return FALSE;
    }
    if (tuple.output == "fail") {
        appendErrorMessage.append(
                UnicodeString("Parse succeeded: ") + result.getDouble() + ", but was expected to fail.");
        return TRUE; // TRUE because failure handling is in the test suite
    }
    if (tuple.output == "NaN") {
        if (!uprv_isNaN(result.getDouble())) {
            appendErrorMessage.append(UnicodeString("Expected NaN, but got: ") + result.getDouble());
            return FALSE;
        }
        return TRUE;
    } else if (tuple.output == "Inf") {
        if (!uprv_isInfinite(result.getDouble()) || result.getDouble() < 0) {
            appendErrorMessage.append(UnicodeString("Expected Inf, but got: ") + result.getDouble());
            return FALSE;
        }
        return TRUE;
    } else if (tuple.output == "-Inf") {
        if (!uprv_isInfinite(result.getDouble()) || result.getDouble() > 0) {
            appendErrorMessage.append(UnicodeString("Expected -Inf, but got: ") + result.getDouble());
            return FALSE;
        }
        return TRUE;
    } else if (tuple.output == "-0.0") {
        if (!std::signbit(result.getDouble()) || result.getDouble() != 0) {
            appendErrorMessage.append(UnicodeString("Expected -0.0, but got: ") + result.getDouble());
            return FALSE;
        }
        return TRUE;
    }
    // All other cases parse to a DecimalQuantity, not a double.

    DecimalQuantity expectedQuantity;
    strToDigitList(tuple.output, expectedQuantity, status);
    UnicodeString expectedString = expectedQuantity.toScientificString();
    if (U_FAILURE(status)) {
        appendErrorMessage.append("[Error parsing decnumber] ");
        // If this happens, assume that tuple.output is exactly the same format as
        // DecimalQuantity.toScientificString()
        expectedString = tuple.output;
        status = U_ZERO_ERROR;
    }
    UnicodeString actualString = result.getDecimalQuantity()->toScientificString();
    if (expectedString != actualString) {
        appendErrorMessage.append(
                UnicodeString("Expected: ") + tuple.output + " (i.e., " + expectedString + "), but got: " +
                actualString + " (" + ppos.getIndex() + ":" + ppos.getErrorIndex() + ")");
        return FALSE;
    }

    return TRUE;
}

UBool NumberFormatDataDrivenTest::isParseCurrencyPass(const NumberFormatTestTuple& tuple,
                                                      UnicodeString& appendErrorMessage,
                                                      UErrorCode& status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    LocalPointer<DecimalFormat> fmtPtr(newDecimalFormat(tuple, status));
    if (U_FAILURE(status)) {
        appendErrorMessage.append("Error creating DecimalFormat.");
        return FALSE;
    }
    adjustDecimalFormat(tuple, *fmtPtr, appendErrorMessage);
    if (appendErrorMessage.length() > 0) {
        return FALSE;
    }
    ParsePosition ppos;
    LocalPointer<CurrencyAmount> currAmt(
            fmtPtr->parseCurrency(tuple.parse, ppos));
    if (ppos.getIndex() == 0) {
        appendErrorMessage.append("Parse failed; got error index ");
        appendErrorMessage = appendErrorMessage + ppos.getErrorIndex();
        return FALSE;
    }
    UnicodeString currStr(currAmt->getISOCurrency());
    U_ASSERT(currAmt->getNumber().getDecimalQuantity() != nullptr); // no doubles in currency tests
    UnicodeString resultStr = currAmt->getNumber().getDecimalQuantity()->toScientificString();
    if (tuple.output == "fail") {
        appendErrorMessage.append(
                UnicodeString("Parse succeeded: ") + resultStr + ", but was expected to fail.");
        return TRUE; // TRUE because failure handling is in the test suite
    }

    DecimalQuantity expectedQuantity;
    strToDigitList(tuple.output, expectedQuantity, status);
    UnicodeString expectedString = expectedQuantity.toScientificString();
    if (U_FAILURE(status)) {
        appendErrorMessage.append("Error parsing decnumber");
        // If this happens, assume that tuple.output is exactly the same format as
        // DecimalQuantity.toNumberString()
        expectedString = tuple.output;
        status = U_ZERO_ERROR;
    }
    if (expectedString != resultStr) {
        appendErrorMessage.append(
                UnicodeString("Expected: ") + tuple.output + " (i.e., " + expectedString + "), but got: " +
                resultStr + " (" + ppos.getIndex() + ":" + ppos.getErrorIndex() + ")");
        return FALSE;
    }

    if (currStr != tuple.outputCurrency) {
        appendErrorMessage.append(
                UnicodeString(
                        "Expected currency: ") + tuple.outputCurrency + ", got: " + currStr + ". ");
        return FALSE;
    }
    return TRUE;
}

void NumberFormatDataDrivenTest::TestNumberFormatTestTuple() {
    NumberFormatTestTuple tuple;
    UErrorCode status = U_ZERO_ERROR;

    tuple.setField(
            NumberFormatTestTuple::getFieldByName("locale"), "en", status);
    tuple.setField(
            NumberFormatTestTuple::getFieldByName("pattern"), "#,##0.00", status);
    tuple.setField(
            NumberFormatTestTuple::getFieldByName("minIntegerDigits"), "-10", status);
    if (!assertSuccess("", status)) {
        return;
    }

    // only what we set should be set.
    assertEquals("", "en", tuple.locale.getName());
    assertEquals("", "#,##0.00", tuple.pattern);
    assertEquals("", -10, tuple.minIntegerDigits);
    assertTrue("", tuple.localeFlag);
    assertTrue("", tuple.patternFlag);
    assertTrue("", tuple.minIntegerDigitsFlag);
    assertFalse("", tuple.formatFlag);

    UnicodeString appendTo;
    assertEquals(
            "", "{locale: en, pattern: #,##0.00, minIntegerDigits: -10}", tuple.toString(appendTo));

    tuple.clear();
    appendTo.remove();
    assertEquals(
            "", "{}", tuple.toString(appendTo));
    tuple.setField(
            NumberFormatTestTuple::getFieldByName("aBadFieldName"), "someValue", status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
    status = U_ZERO_ERROR;
    tuple.setField(
            NumberFormatTestTuple::getFieldByName("minIntegerDigits"), "someBadValue", status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
}

void NumberFormatDataDrivenTest::TestDataDrivenICU4C() {
    run("numberformattestspecification.txt", TRUE);
}

#endif  // !UCONFIG_NO_FORMATTING
