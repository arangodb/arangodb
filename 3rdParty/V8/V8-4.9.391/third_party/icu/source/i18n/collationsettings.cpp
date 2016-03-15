/*
*******************************************************************************
* Copyright (C) 2013-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationsettings.cpp
*
* created on: 2013feb07
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "cmemory.h"
#include "collation.h"
#include "collationsettings.h"
#include "sharedobject.h"
#include "uassert.h"
#include "umutex.h"

U_NAMESPACE_BEGIN

CollationSettings::CollationSettings(const CollationSettings &other)
        : SharedObject(other),
          options(other.options), variableTop(other.variableTop),
          reorderTable(NULL),
          reorderCodes(NULL), reorderCodesLength(0), reorderCodesCapacity(0),
          fastLatinOptions(other.fastLatinOptions) {
    int32_t length = other.reorderCodesLength;
    if(length == 0) {
        U_ASSERT(other.reorderTable == NULL);
    } else {
        U_ASSERT(other.reorderTable != NULL);
        if(other.reorderCodesCapacity == 0) {
            aliasReordering(other.reorderCodes, length, other.reorderTable);
        } else {
            setReordering(other.reorderCodes, length, other.reorderTable);
        }
    }
    if(fastLatinOptions >= 0) {
        uprv_memcpy(fastLatinPrimaries, other.fastLatinPrimaries, sizeof(fastLatinPrimaries));
    }
}

CollationSettings::~CollationSettings() {
    if(reorderCodesCapacity != 0) {
        uprv_free(const_cast<int32_t *>(reorderCodes));
    }
}

UBool
CollationSettings::operator==(const CollationSettings &other) const {
    if(options != other.options) { return FALSE; }
    if((options & ALTERNATE_MASK) != 0 && variableTop != other.variableTop) { return FALSE; }
    if(reorderCodesLength != other.reorderCodesLength) { return FALSE; }
    for(int32_t i = 0; i < reorderCodesLength; ++i) {
        if(reorderCodes[i] != other.reorderCodes[i]) { return FALSE; }
    }
    return TRUE;
}

int32_t
CollationSettings::hashCode() const {
    int32_t h = options << 8;
    if((options & ALTERNATE_MASK) != 0) { h ^= variableTop; }
    h ^= reorderCodesLength;
    for(int32_t i = 0; i < reorderCodesLength; ++i) {
        h ^= (reorderCodes[i] << i);
    }
    return h;
}

void
CollationSettings::resetReordering() {
    // When we turn off reordering, we want to set a NULL permutation
    // rather than a no-op permutation.
    // Keep the memory via reorderCodes and its capacity.
    reorderTable = NULL;
    reorderCodesLength = 0;
}

void
CollationSettings::aliasReordering(const int32_t *codes, int32_t length, const uint8_t *table) {
    if(length == 0) {
        resetReordering();
    } else {
        // We need to release the memory before setting the alias pointer.
        if(reorderCodesCapacity != 0) {
            uprv_free(const_cast<int32_t *>(reorderCodes));
            reorderCodesCapacity = 0;
        }
        reorderTable = table;
        reorderCodes = codes;
        reorderCodesLength = length;
    }
}

UBool
CollationSettings::setReordering(const int32_t *codes, int32_t length, const uint8_t table[256]) {
    if(length == 0) {
        resetReordering();
    } else {
        uint8_t *ownedTable;
        int32_t *ownedCodes;
        if(length <= reorderCodesCapacity) {
            ownedTable = const_cast<uint8_t *>(reorderTable);
            ownedCodes = const_cast<int32_t *>(reorderCodes);
        } else {
            // Allocate one memory block for the codes and the 16-aligned table.
            int32_t capacity = (length + 3) & ~3;  // round up to a multiple of 4 ints
            uint8_t *bytes = (uint8_t *)uprv_malloc(256 + capacity * 4);
            if(bytes == NULL) { return FALSE; }
            if(reorderCodesCapacity != 0) {
                uprv_free(const_cast<int32_t *>(reorderCodes));
            }
            reorderTable = ownedTable = bytes + capacity * 4;
            reorderCodes = ownedCodes = (int32_t *)bytes;
            reorderCodesCapacity = capacity;
        }
        uprv_memcpy(ownedTable, table, 256);
        uprv_memcpy(ownedCodes, codes, length * 4);
        reorderCodesLength = length;
    }
    return TRUE;
}

void
CollationSettings::setStrength(int32_t value, int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noStrength = options & ~STRENGTH_MASK;
    switch(value) {
    case UCOL_PRIMARY:
    case UCOL_SECONDARY:
    case UCOL_TERTIARY:
    case UCOL_QUATERNARY:
    case UCOL_IDENTICAL:
        options = noStrength | (value << STRENGTH_SHIFT);
        break;
    case UCOL_DEFAULT:
        options = noStrength | (defaultOptions & STRENGTH_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationSettings::setFlag(int32_t bit, UColAttributeValue value,
                           int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    switch(value) {
    case UCOL_ON:
        options |= bit;
        break;
    case UCOL_OFF:
        options &= ~bit;
        break;
    case UCOL_DEFAULT:
        options = (options & ~bit) | (defaultOptions & bit);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationSettings::setCaseFirst(UColAttributeValue value,
                                int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noCaseFirst = options & ~CASE_FIRST_AND_UPPER_MASK;
    switch(value) {
    case UCOL_OFF:
        options = noCaseFirst;
        break;
    case UCOL_LOWER_FIRST:
        options = noCaseFirst | CASE_FIRST;
        break;
    case UCOL_UPPER_FIRST:
        options = noCaseFirst | CASE_FIRST_AND_UPPER_MASK;
        break;
    case UCOL_DEFAULT:
        options = noCaseFirst | (defaultOptions & CASE_FIRST_AND_UPPER_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationSettings::setAlternateHandling(UColAttributeValue value,
                                        int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noAlternate = options & ~ALTERNATE_MASK;
    switch(value) {
    case UCOL_NON_IGNORABLE:
        options = noAlternate;
        break;
    case UCOL_SHIFTED:
        options = noAlternate | SHIFTED;
        break;
    case UCOL_DEFAULT:
        options = noAlternate | (defaultOptions & ALTERNATE_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

void
CollationSettings::setMaxVariable(int32_t value, int32_t defaultOptions, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    int32_t noMax = options & ~MAX_VARIABLE_MASK;
    switch(value) {
    case MAX_VAR_SPACE:
    case MAX_VAR_PUNCT:
    case MAX_VAR_SYMBOL:
    case MAX_VAR_CURRENCY:
        options = noMax | (value << MAX_VARIABLE_SHIFT);
        break;
    case UCOL_DEFAULT:
        options = noMax | (defaultOptions & MAX_VARIABLE_MASK);
        break;
    default:
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        break;
    }
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
