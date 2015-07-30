/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* quantityformatter.cpp
*/
#include "quantityformatter.h"
#include "simplepatternformatter.h"
#include "uassert.h"
#include "unicode/unistr.h"
#include "unicode/decimfmt.h"
#include "cstring.h"
#include "plurrule_impl.h"
#include "unicode/plurrule.h"
#include "charstr.h"
#include "unicode/fmtable.h"
#include "unicode/fieldpos.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

// other must always be first.
static const char * const gPluralForms[] = {
        "other", "zero", "one", "two", "few", "many"};

static int32_t getPluralIndex(const char *pluralForm) {
    int32_t len = UPRV_LENGTHOF(gPluralForms);
    for (int32_t i = 0; i < len; ++i) {
        if (uprv_strcmp(pluralForm, gPluralForms[i]) == 0) {
            return i;
        }
    }
    return -1;
}

QuantityFormatter::QuantityFormatter() {
    for (int32_t i = 0; i < UPRV_LENGTHOF(formatters); ++i) {
        formatters[i] = NULL;
    }
}

QuantityFormatter::QuantityFormatter(const QuantityFormatter &other) {
    for (int32_t i = 0; i < UPRV_LENGTHOF(formatters); ++i) {
        if (other.formatters[i] == NULL) {
            formatters[i] = NULL;
        } else {
            formatters[i] = new SimplePatternFormatter(*other.formatters[i]);
        }
    }
}

QuantityFormatter &QuantityFormatter::operator=(
        const QuantityFormatter& other) {
    if (this == &other) {
        return *this;
    }
    for (int32_t i = 0; i < UPRV_LENGTHOF(formatters); ++i) {
        delete formatters[i];
        if (other.formatters[i] == NULL) {
            formatters[i] = NULL;
        } else {
            formatters[i] = new SimplePatternFormatter(*other.formatters[i]);
        }
    }
    return *this;
}

QuantityFormatter::~QuantityFormatter() {
    for (int32_t i = 0; i < UPRV_LENGTHOF(formatters); ++i) {
        delete formatters[i];
    }
}

void QuantityFormatter::reset() {
    for (int32_t i = 0; i < UPRV_LENGTHOF(formatters); ++i) {
        delete formatters[i];
        formatters[i] = NULL;
    }
}

UBool QuantityFormatter::add(
        const char *variant,
        const UnicodeString &rawPattern,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    int32_t pluralIndex = getPluralIndex(variant);
    if (pluralIndex == -1) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    SimplePatternFormatter *newFmt =
            new SimplePatternFormatter(rawPattern);
    if (newFmt == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return FALSE;
    }
    if (newFmt->getPlaceholderCount() > 1) {
        delete newFmt;
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    delete formatters[pluralIndex];
    formatters[pluralIndex] = newFmt;
    return TRUE;
}

UBool QuantityFormatter::isValid() const {
    return formatters[0] != NULL;
}

const SimplePatternFormatter *QuantityFormatter::getByVariant(
        const char *variant) const {
    int32_t pluralIndex = getPluralIndex(variant);
    if (pluralIndex == -1) {
        pluralIndex = 0;
    }
    const SimplePatternFormatter *pattern = formatters[pluralIndex];
    if (pattern == NULL) {
        pattern = formatters[0];
    }
    return pattern;
}

UnicodeString &QuantityFormatter::format(
            const Formattable& quantity,
            const NumberFormat &fmt,
            const PluralRules &rules,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    UnicodeString count;
    const DecimalFormat *decFmt = dynamic_cast<const DecimalFormat *>(&fmt);
    if (decFmt != NULL) {
        FixedDecimal fd = decFmt->getFixedDecimal(quantity, status);
        if (U_FAILURE(status)) {
            return appendTo;
        }
        count = rules.select(fd);
    } else {
        if (quantity.getType() == Formattable::kDouble) {
            count = rules.select(quantity.getDouble());
        } else if (quantity.getType() == Formattable::kLong) {
            count = rules.select(quantity.getLong());
        } else if (quantity.getType() == Formattable::kInt64) {
            count = rules.select((double) quantity.getInt64());
        } else {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return appendTo;
        }
    }
    CharString buffer;
    buffer.appendInvariantChars(count, status);
    if (U_FAILURE(status)) {
        return appendTo;
    }
    const SimplePatternFormatter *pattern = getByVariant(buffer.data());
    if (pattern == NULL) {
        status = U_INVALID_STATE_ERROR;
        return appendTo;
    }
    UnicodeString formattedNumber;
    FieldPosition fpos(pos.getField());
    fmt.format(quantity, formattedNumber, fpos, status);
    const UnicodeString *params[1] = {&formattedNumber};
    int32_t offsets[1];
    pattern->format(params, UPRV_LENGTHOF(params), appendTo, offsets, UPRV_LENGTHOF(offsets), status);
    if (offsets[0] != -1) {
        if (fpos.getBeginIndex() != 0 || fpos.getEndIndex() != 0) {
            pos.setBeginIndex(fpos.getBeginIndex() + offsets[0]);
            pos.setEndIndex(fpos.getEndIndex() + offsets[0]);
        }
    }
    return appendTo;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
