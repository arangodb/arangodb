/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/scientificformathelper.h"
#include "unicode/dcfmtsym.h"
#include "unicode/fpositer.h"
#include "unicode/utf16.h"
#include "unicode/uniset.h"
#include "decfmtst.h"

U_NAMESPACE_BEGIN

static const UChar kSuperscriptDigits[] = {0x2070, 0xB9, 0xB2, 0xB3, 0x2074, 0x2075, 0x2076, 0x2077, 0x2078, 0x2079};

static const UChar kSuperscriptPlusSign = 0x207A;
static const UChar kSuperscriptMinusSign = 0x207B;

ScientificFormatHelper::ScientificFormatHelper(
        const DecimalFormatSymbols &dfs, UErrorCode &status)
        : fPreExponent(), fStaticSets(NULL) {
    if (U_FAILURE(status)) {
        return;
    }
    fPreExponent.append(dfs.getConstSymbol(
            DecimalFormatSymbols::kExponentMultiplicationSymbol));
    fPreExponent.append(dfs.getSymbol(DecimalFormatSymbols::kOneDigitSymbol));
    fPreExponent.append(dfs.getSymbol(DecimalFormatSymbols::kZeroDigitSymbol));
    fStaticSets = DecimalFormatStaticSets::getStaticSets(status);
}

ScientificFormatHelper::ScientificFormatHelper(
        const ScientificFormatHelper &other)
        : UObject(other),
          fPreExponent(other.fPreExponent),
          fStaticSets(other.fStaticSets) {
}

ScientificFormatHelper &ScientificFormatHelper::operator=(const ScientificFormatHelper &other) {
    if (this == &other) {
        return *this;
    }
    fPreExponent = other.fPreExponent;
    fStaticSets = other.fStaticSets;
    return *this;
}

ScientificFormatHelper::~ScientificFormatHelper() {
}

UnicodeString &ScientificFormatHelper::insertMarkup(
        const UnicodeString &s,
        FieldPositionIterator &fpi,
        const UnicodeString &beginMarkup,
        const UnicodeString &endMarkup,
        UnicodeString &result,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return result;
    }
    FieldPosition fp;
    int32_t copyFromOffset = 0;
    UBool exponentSymbolFieldPresent = FALSE;
    UBool exponentFieldPresent = FALSE;
    while (fpi.next(fp)) {
        switch (fp.getField()) {
        case UNUM_EXPONENT_SYMBOL_FIELD:
            exponentSymbolFieldPresent = TRUE;
            result.append(s, copyFromOffset, fp.getBeginIndex() - copyFromOffset);
            copyFromOffset = fp.getEndIndex();
            result.append(fPreExponent);
            result.append(beginMarkup);
            break;
        case UNUM_EXPONENT_FIELD:
            exponentFieldPresent = TRUE;
            result.append(s, copyFromOffset, fp.getEndIndex() - copyFromOffset);
            copyFromOffset = fp.getEndIndex();
            result.append(endMarkup);
            break;
        default:
            break;
        }
    }
    if (!exponentSymbolFieldPresent || !exponentFieldPresent) {
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return result;
    }
    result.append(s, copyFromOffset, s.length() - copyFromOffset);
    return result;
}

static UBool copyAsSuperscript(
        const UnicodeString &s,
        int32_t beginIndex,
        int32_t endIndex,
        UnicodeString &result,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    for (int32_t i = beginIndex; i < endIndex;) {
        UChar32 c = s.char32At(i);
        int32_t digit = u_charDigitValue(c);
        if (digit < 0) {
            status = U_INVALID_CHAR_FOUND;
            return FALSE;
        }
        result.append(kSuperscriptDigits[digit]);
        i += U16_LENGTH(c);
    }
    return TRUE;
}

UnicodeString &ScientificFormatHelper::toSuperscriptExponentDigits(
        const UnicodeString &s,
        FieldPositionIterator &fpi,
        UnicodeString &result,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
      return result;
    }
    FieldPosition fp;
    int32_t copyFromOffset = 0;
    UBool exponentSymbolFieldPresent = FALSE;
    UBool exponentFieldPresent = FALSE;
    while (fpi.next(fp)) {
        switch (fp.getField()) {
        case UNUM_EXPONENT_SYMBOL_FIELD:
            exponentSymbolFieldPresent = TRUE;
            result.append(s, copyFromOffset, fp.getBeginIndex() - copyFromOffset);
            copyFromOffset = fp.getEndIndex();
            result.append(fPreExponent);
            break;
        case UNUM_EXPONENT_SIGN_FIELD:
            {
                int32_t beginIndex = fp.getBeginIndex();
                int32_t endIndex = fp.getEndIndex();
                UChar32 aChar = s.char32At(beginIndex);
                if (fStaticSets->fMinusSigns->contains(aChar)) {
                    result.append(s, copyFromOffset, beginIndex - copyFromOffset);
                    result.append(kSuperscriptMinusSign);
                } else if (fStaticSets->fPlusSigns->contains(aChar)) {
                    result.append(s, copyFromOffset, beginIndex - copyFromOffset);
                    result.append(kSuperscriptPlusSign);
                } else {
                    status = U_INVALID_CHAR_FOUND;
                    return result;
                }
                copyFromOffset = endIndex;
            }
            break;
        case UNUM_EXPONENT_FIELD:
            exponentFieldPresent = TRUE;
            result.append(s, copyFromOffset, fp.getBeginIndex() - copyFromOffset);
            if (!copyAsSuperscript(
                    s, fp.getBeginIndex(), fp.getEndIndex(), result, status)) {
              return result;
            }
            copyFromOffset = fp.getEndIndex();
            break;
        default:
            break;
        }
    }
    if (!exponentSymbolFieldPresent || !exponentFieldPresent) {
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return result;
    }
    result.append(s, copyFromOffset, s.length() - copyFromOffset);
    return result;
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
