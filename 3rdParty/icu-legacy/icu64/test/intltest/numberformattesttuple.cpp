// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "numberformattesttuple.h"

#if !UCONFIG_NO_FORMATTING

#include "ustrfmt.h"
#include "charstr.h"
#include "cstring.h"
#include "cmemory.h"

static NumberFormatTestTuple *gNullPtr = NULL;

#define FIELD_OFFSET(fieldName) ((int32_t) (((char *) &gNullPtr->fieldName) - ((char *) gNullPtr)))
#define FIELD_FLAG_OFFSET(fieldName) ((int32_t) (((char *) &gNullPtr->fieldName##Flag) - ((char *) gNullPtr)))

#define FIELD_INIT(fieldName, fieldType) {#fieldName, FIELD_OFFSET(fieldName), FIELD_FLAG_OFFSET(fieldName), fieldType}

struct Numberformattesttuple_EnumConversion {
    const char *str;
    int32_t value;
};

static Numberformattesttuple_EnumConversion gRoundingEnum[] = {
    {"ceiling", DecimalFormat::kRoundCeiling},
    {"floor", DecimalFormat::kRoundFloor},
    {"down", DecimalFormat::kRoundDown},
    {"up", DecimalFormat::kRoundUp},
    {"halfEven", DecimalFormat::kRoundHalfEven},
    {"halfDown", DecimalFormat::kRoundHalfDown},
    {"halfUp", DecimalFormat::kRoundHalfUp},
    {"unnecessary", DecimalFormat::kRoundUnnecessary}};

static Numberformattesttuple_EnumConversion gCurrencyUsageEnum[] = {
    {"standard", UCURR_USAGE_STANDARD},
    {"cash", UCURR_USAGE_CASH}};

static Numberformattesttuple_EnumConversion gPadPositionEnum[] = {
    {"beforePrefix", DecimalFormat::kPadBeforePrefix},
    {"afterPrefix", DecimalFormat::kPadAfterPrefix},
    {"beforeSuffix", DecimalFormat::kPadBeforeSuffix},
    {"afterSuffix", DecimalFormat::kPadAfterSuffix}};

static Numberformattesttuple_EnumConversion gFormatStyleEnum[] = {
    {"patternDecimal", UNUM_PATTERN_DECIMAL},
    {"decimal", UNUM_DECIMAL},
    {"currency", UNUM_CURRENCY},
    {"percent", UNUM_PERCENT},
    {"scientific", UNUM_SCIENTIFIC},
    {"spellout", UNUM_SPELLOUT},
    {"ordinal", UNUM_ORDINAL},
    {"duration", UNUM_DURATION},
    {"numberingSystem", UNUM_NUMBERING_SYSTEM},
    {"patternRuleBased", UNUM_PATTERN_RULEBASED},
    {"currencyIso", UNUM_CURRENCY_ISO},
    {"currencyPlural", UNUM_CURRENCY_PLURAL},
    {"currencyAccounting", UNUM_CURRENCY_ACCOUNTING},
    {"cashCurrency", UNUM_CASH_CURRENCY},
    {"default", UNUM_DEFAULT},
    {"ignore", UNUM_IGNORE}};

static int32_t toEnum(
        const Numberformattesttuple_EnumConversion *table,
        int32_t tableLength,
        const UnicodeString &str,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }
    CharString cstr;
    cstr.appendInvariantChars(str, status);
    if (U_FAILURE(status)) {
        return 0;
    }
    for (int32_t i = 0; i < tableLength; ++i) {
        if (uprv_strcmp(cstr.data(), table[i].str) == 0) {
            return table[i].value;
        }
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
    return 0;
}

static void fromEnum(
        const Numberformattesttuple_EnumConversion *table,
        int32_t tableLength,
        int32_t val,
        UnicodeString &appendTo) {
    for (int32_t i = 0; i < tableLength; ++i) {
        if (table[i].value == val) {
            appendTo.append(table[i].str);
        }
    }
}

static void identVal(
        const UnicodeString &str, void *strPtr, UErrorCode & /*status*/) {
    *static_cast<UnicodeString *>(strPtr) = str;
}
 
static void identStr(
        const void *strPtr, UnicodeString &appendTo) {
    appendTo.append(*static_cast<const UnicodeString *>(strPtr));
}

static void strToLocale(
        const UnicodeString &str, void *localePtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    CharString localeStr;
    localeStr.appendInvariantChars(str, status);
    *static_cast<Locale *>(localePtr) = Locale(localeStr.data());
}

static void localeToStr(
        const void *localePtr, UnicodeString &appendTo) {
    appendTo.append(
            UnicodeString(
                    static_cast<const Locale *>(localePtr)->getName()));
}

static void strToInt(
        const UnicodeString &str, void *intPtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    int32_t len = str.length();
    int32_t start = 0;
    UBool neg = FALSE;
    if (len > 0 && str[0] == 0x2D) { // negative
        neg = TRUE;
        start = 1;
    }
    if (start == len) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    int64_t value = 0;
    for (int32_t i = start; i < len; ++i) {
        UChar ch = str[i];
        if (ch < 0x30 || ch > 0x39) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        value = value * 10 - 0x30 + (int32_t) ch;
    }
    int32_t signedValue = neg ? static_cast<int32_t>(-value) : static_cast<int32_t>(value);
    *static_cast<int32_t *>(intPtr) = signedValue;
}

static void intToStr(
        const void *intPtr, UnicodeString &appendTo) {
    UChar buffer[20];
    // int64_t such that all int32_t values can be negated
    int64_t xSigned = *static_cast<const int32_t *>(intPtr);
    uint32_t x;
    if (xSigned < 0) {
        appendTo.append((UChar)0x2D);
        x = static_cast<uint32_t>(-xSigned);
    } else {
        x = static_cast<uint32_t>(xSigned);
    }
    int32_t len = uprv_itou(buffer, UPRV_LENGTHOF(buffer), x, 10, 1);
    appendTo.append(buffer, 0, len);
}

static void strToDouble(
        const UnicodeString &str, void *doublePtr, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    CharString buffer;
    buffer.appendInvariantChars(str, status);
    if (U_FAILURE(status)) {
        return;
    }
    *static_cast<double *>(doublePtr) = atof(buffer.data());
}

static void doubleToStr(
        const void *doublePtr, UnicodeString &appendTo) {
    char buffer[256];
    double x = *static_cast<const double *>(doublePtr);
    sprintf(buffer, "%f", x);
    appendTo.append(buffer);
}

static void strToERounding(
        const UnicodeString &str, void *roundPtr, UErrorCode &status) {
    int32_t val = toEnum(
            gRoundingEnum, UPRV_LENGTHOF(gRoundingEnum), str, status);
    *static_cast<DecimalFormat::ERoundingMode *>(roundPtr) = (DecimalFormat::ERoundingMode) val;
}

static void eRoundingToStr(
        const void *roundPtr, UnicodeString &appendTo) {
    DecimalFormat::ERoundingMode rounding = 
            *static_cast<const DecimalFormat::ERoundingMode *>(roundPtr);
    fromEnum(
            gRoundingEnum,
            UPRV_LENGTHOF(gRoundingEnum),
            rounding,
            appendTo);
}

static void strToCurrencyUsage(
        const UnicodeString &str, void *currencyUsagePtr, UErrorCode &status) {
    int32_t val = toEnum(
            gCurrencyUsageEnum, UPRV_LENGTHOF(gCurrencyUsageEnum), str, status);
    *static_cast<UCurrencyUsage *>(currencyUsagePtr) = (UCurrencyUsage) val;
}

static void currencyUsageToStr(
        const void *currencyUsagePtr, UnicodeString &appendTo) {
    UCurrencyUsage currencyUsage = 
            *static_cast<const UCurrencyUsage *>(currencyUsagePtr);
    fromEnum(
            gCurrencyUsageEnum,
            UPRV_LENGTHOF(gCurrencyUsageEnum),
            currencyUsage,
            appendTo);
}

static void strToEPadPosition(
        const UnicodeString &str, void *padPositionPtr, UErrorCode &status) {
    int32_t val = toEnum(
            gPadPositionEnum, UPRV_LENGTHOF(gPadPositionEnum), str, status);
    *static_cast<DecimalFormat::EPadPosition *>(padPositionPtr) =
            (DecimalFormat::EPadPosition) val;
}

static void ePadPositionToStr(
        const void *padPositionPtr, UnicodeString &appendTo) {
    DecimalFormat::EPadPosition padPosition = 
            *static_cast<const DecimalFormat::EPadPosition *>(padPositionPtr);
    fromEnum(
            gPadPositionEnum,
            UPRV_LENGTHOF(gPadPositionEnum),
            padPosition,
            appendTo);
}

static void strToFormatStyle(
        const UnicodeString &str, void *formatStylePtr, UErrorCode &status) {
    int32_t val = toEnum(
            gFormatStyleEnum, UPRV_LENGTHOF(gFormatStyleEnum), str, status);
    *static_cast<UNumberFormatStyle *>(formatStylePtr) = (UNumberFormatStyle) val;
}

static void formatStyleToStr(
        const void *formatStylePtr, UnicodeString &appendTo) {
    UNumberFormatStyle formatStyle = 
            *static_cast<const UNumberFormatStyle *>(formatStylePtr);
    fromEnum(
            gFormatStyleEnum,
            UPRV_LENGTHOF(gFormatStyleEnum),
            formatStyle,
            appendTo);
}

struct NumberFormatTestTupleFieldOps {
    void (*toValue)(const UnicodeString &str, void *valPtr, UErrorCode &);
    void (*toString)(const void *valPtr, UnicodeString &appendTo);
};

const NumberFormatTestTupleFieldOps gStrOps = {identVal, identStr};
const NumberFormatTestTupleFieldOps gIntOps = {strToInt, intToStr};
const NumberFormatTestTupleFieldOps gLocaleOps = {strToLocale, localeToStr};
const NumberFormatTestTupleFieldOps gDoubleOps = {strToDouble, doubleToStr};
const NumberFormatTestTupleFieldOps gERoundingOps = {strToERounding, eRoundingToStr};
const NumberFormatTestTupleFieldOps gCurrencyUsageOps = {strToCurrencyUsage, currencyUsageToStr};
const NumberFormatTestTupleFieldOps gEPadPositionOps = {strToEPadPosition, ePadPositionToStr};
const NumberFormatTestTupleFieldOps gFormatStyleOps = {strToFormatStyle, formatStyleToStr};

struct NumberFormatTestTupleFieldData {
    const char *name;
    int32_t offset;
    int32_t flagOffset;
    const NumberFormatTestTupleFieldOps *ops;
};

// Order must correspond to ENumberFormatTestTupleField
const NumberFormatTestTupleFieldData gFieldData[] = {
    FIELD_INIT(locale, &gLocaleOps),
    FIELD_INIT(currency, &gStrOps),
    FIELD_INIT(pattern, &gStrOps),
    FIELD_INIT(format, &gStrOps),
    FIELD_INIT(output, &gStrOps),
    FIELD_INIT(comment, &gStrOps),
    FIELD_INIT(minIntegerDigits, &gIntOps),
    FIELD_INIT(maxIntegerDigits, &gIntOps),
    FIELD_INIT(minFractionDigits, &gIntOps),
    FIELD_INIT(maxFractionDigits, &gIntOps),
    FIELD_INIT(minGroupingDigits, &gIntOps),
    FIELD_INIT(breaks, &gStrOps),
    FIELD_INIT(useSigDigits, &gIntOps),
    FIELD_INIT(minSigDigits, &gIntOps),
    FIELD_INIT(maxSigDigits, &gIntOps),
    FIELD_INIT(useGrouping, &gIntOps),
    FIELD_INIT(multiplier, &gIntOps),
    FIELD_INIT(roundingIncrement, &gDoubleOps),
    FIELD_INIT(formatWidth, &gIntOps),
    FIELD_INIT(padCharacter, &gStrOps),
    FIELD_INIT(useScientific, &gIntOps),
    FIELD_INIT(grouping, &gIntOps),
    FIELD_INIT(grouping2, &gIntOps),
    FIELD_INIT(roundingMode, &gERoundingOps),
    FIELD_INIT(currencyUsage, &gCurrencyUsageOps),
    FIELD_INIT(minimumExponentDigits, &gIntOps),
    FIELD_INIT(exponentSignAlwaysShown, &gIntOps),
    FIELD_INIT(decimalSeparatorAlwaysShown, &gIntOps),
    FIELD_INIT(padPosition, &gEPadPositionOps),
    FIELD_INIT(positivePrefix, &gStrOps),
    FIELD_INIT(positiveSuffix, &gStrOps),
    FIELD_INIT(negativePrefix, &gStrOps),
    FIELD_INIT(negativeSuffix, &gStrOps),
    FIELD_INIT(signAlwaysShown, &gIntOps),
    FIELD_INIT(localizedPattern, &gStrOps),
    FIELD_INIT(toPattern, &gStrOps),
    FIELD_INIT(toLocalizedPattern, &gStrOps),
    FIELD_INIT(style, &gFormatStyleOps),
    FIELD_INIT(parse, &gStrOps),
    FIELD_INIT(lenient, &gIntOps),
    FIELD_INIT(plural, &gStrOps),
    FIELD_INIT(parseIntegerOnly, &gIntOps),
    FIELD_INIT(decimalPatternMatchRequired, &gIntOps),
    FIELD_INIT(parseNoExponent, &gIntOps),
    FIELD_INIT(parseCaseSensitive, &gIntOps),
    FIELD_INIT(outputCurrency, &gStrOps)
};

UBool
NumberFormatTestTuple::setField(
        ENumberFormatTestTupleField fieldId, 
        const UnicodeString &fieldValue,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (fieldId == kNumberFormatTestTupleFieldCount) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    gFieldData[fieldId].ops->toValue(
            fieldValue, getMutableFieldAddress(fieldId), status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    setFlag(fieldId, TRUE);
    return TRUE;
}

UBool
NumberFormatTestTuple::clearField(
        ENumberFormatTestTupleField fieldId, 
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    if (fieldId == kNumberFormatTestTupleFieldCount) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    setFlag(fieldId, FALSE);
    return TRUE;
}

void
NumberFormatTestTuple::clear() {
    for (int32_t i = 0; i < kNumberFormatTestTupleFieldCount; ++i) {
        setFlag(i, FALSE);
    }
}

UnicodeString &
NumberFormatTestTuple::toString(
        UnicodeString &appendTo) const {
    appendTo.append("{");
    UBool first = TRUE;
    for (int32_t i = 0; i < kNumberFormatTestTupleFieldCount; ++i) {
        if (!isFlag(i)) {
            continue;
        }
        if (!first) {
            appendTo.append(", ");
        }
        first = FALSE;
        appendTo.append(gFieldData[i].name);
        appendTo.append(": ");
        gFieldData[i].ops->toString(getFieldAddress(i), appendTo);
    }
    appendTo.append("}");
    return appendTo;
}

ENumberFormatTestTupleField
NumberFormatTestTuple::getFieldByName(
        const UnicodeString &name) {
    CharString buffer;
    UErrorCode status = U_ZERO_ERROR;
    buffer.appendInvariantChars(name, status);
    if (U_FAILURE(status)) {
        return kNumberFormatTestTupleFieldCount;
    }
    int32_t result = -1;
    for (int32_t i = 0; i < UPRV_LENGTHOF(gFieldData); ++i) {
        if (uprv_strcmp(gFieldData[i].name, buffer.data()) == 0) {
            result = i;
            break;
        }
    }
    if (result == -1) {
        return kNumberFormatTestTupleFieldCount;
    }
    return (ENumberFormatTestTupleField) result;
}

const void *
NumberFormatTestTuple::getFieldAddress(int32_t fieldId) const {
    return reinterpret_cast<const char *>(this) + gFieldData[fieldId].offset;
}

void *
NumberFormatTestTuple::getMutableFieldAddress(int32_t fieldId) {
    return reinterpret_cast<char *>(this) + gFieldData[fieldId].offset;
}

void 
NumberFormatTestTuple::setFlag(int32_t fieldId, UBool value) {
    void *flagAddr = reinterpret_cast<char *>(this) + gFieldData[fieldId].flagOffset;
    *static_cast<UBool *>(flagAddr) = value;
}

UBool
NumberFormatTestTuple::isFlag(int32_t fieldId) const {
    const void *flagAddr = reinterpret_cast<const char *>(this) + gFieldData[fieldId].flagOffset;
    return *static_cast<const UBool *>(flagAddr);
}

#endif /* !UCONFIG_NO_FORMATTING */
