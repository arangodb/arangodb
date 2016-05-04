/*
*******************************************************************************
* Copyright (C) 2012-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdata.cpp
*
* created on: 2012jul28
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "unicode/udata.h"
#include "unicode/uscript.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "uassert.h"
#include "utrie2.h"

U_NAMESPACE_BEGIN

uint32_t
CollationData::getIndirectCE32(uint32_t ce32) const {
    U_ASSERT(Collation::isSpecialCE32(ce32));
    int32_t tag = Collation::tagFromCE32(ce32);
    if(tag == Collation::DIGIT_TAG) {
        // Fetch the non-numeric-collation CE32.
        ce32 = ce32s[Collation::indexFromCE32(ce32)];
    } else if(tag == Collation::LEAD_SURROGATE_TAG) {
        ce32 = Collation::UNASSIGNED_CE32;
    } else if(tag == Collation::U0000_TAG) {
        // Fetch the normal ce32 for U+0000.
        ce32 = ce32s[0];
    }
    return ce32;
}

uint32_t
CollationData::getFinalCE32(uint32_t ce32) const {
    if(Collation::isSpecialCE32(ce32)) {
        ce32 = getIndirectCE32(ce32);
    }
    return ce32;
}

int64_t
CollationData::getSingleCE(UChar32 c, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return 0; }
    // Keep parallel with CollationDataBuilder::getSingleCE().
    const CollationData *d;
    uint32_t ce32 = getCE32(c);
    if(ce32 == Collation::FALLBACK_CE32) {
        d = base;
        ce32 = base->getCE32(c);
    } else {
        d = this;
    }
    while(Collation::isSpecialCE32(ce32)) {
        switch(Collation::tagFromCE32(ce32)) {
        case Collation::LATIN_EXPANSION_TAG:
        case Collation::BUILDER_DATA_TAG:
        case Collation::PREFIX_TAG:
        case Collation::CONTRACTION_TAG:
        case Collation::HANGUL_TAG:
        case Collation::LEAD_SURROGATE_TAG:
            errorCode = U_UNSUPPORTED_ERROR;
            return 0;
        case Collation::FALLBACK_TAG:
        case Collation::RESERVED_TAG_3:
            errorCode = U_INTERNAL_PROGRAM_ERROR;
            return 0;
        case Collation::LONG_PRIMARY_TAG:
            return Collation::ceFromLongPrimaryCE32(ce32);
        case Collation::LONG_SECONDARY_TAG:
            return Collation::ceFromLongSecondaryCE32(ce32);
        case Collation::EXPANSION32_TAG:
            if(Collation::lengthFromCE32(ce32) == 1) {
                ce32 = d->ce32s[Collation::indexFromCE32(ce32)];
                break;
            } else {
                errorCode = U_UNSUPPORTED_ERROR;
                return 0;
            }
        case Collation::EXPANSION_TAG: {
            if(Collation::lengthFromCE32(ce32) == 1) {
                return d->ces[Collation::indexFromCE32(ce32)];
            } else {
                errorCode = U_UNSUPPORTED_ERROR;
                return 0;
            }
        }
        case Collation::DIGIT_TAG:
            // Fetch the non-numeric-collation CE32 and continue.
            ce32 = d->ce32s[Collation::indexFromCE32(ce32)];
            break;
        case Collation::U0000_TAG:
            U_ASSERT(c == 0);
            // Fetch the normal ce32 for U+0000 and continue.
            ce32 = d->ce32s[0];
            break;
        case Collation::OFFSET_TAG:
            return d->getCEFromOffsetCE32(c, ce32);
        case Collation::IMPLICIT_TAG:
            return Collation::unassignedCEFromCodePoint(c);
        }
    }
    return Collation::ceFromSimpleCE32(ce32);
}

uint32_t
CollationData::getFirstPrimaryForGroup(int32_t script) const {
    int32_t index = findScript(script);
    if(index < 0) {
        return 0;
    }
    uint32_t head = scripts[index];
    return (head & 0xff00) << 16;
}

uint32_t
CollationData::getLastPrimaryForGroup(int32_t script) const {
    int32_t index = findScript(script);
    if(index < 0) {
        return 0;
    }
    uint32_t head = scripts[index];
    uint32_t lastByte = head & 0xff;
    return ((lastByte + 1) << 24) - 1;
}

int32_t
CollationData::getGroupForPrimary(uint32_t p) const {
    p >>= 24;  // Reordering groups are distinguished by primary lead bytes.
    for(int32_t i = 0; i < scriptsLength; i = i + 2 + scripts[i + 1]) {
        uint32_t lastByte = scripts[i] & 0xff;
        if(p <= lastByte) {
            return scripts[i + 2];
        }
    }
    return -1;
}

int32_t
CollationData::findScript(int32_t script) const {
    if(script < 0 || 0xffff < script) { return -1; }
    for(int32_t i = 0; i < scriptsLength;) {
        int32_t limit = i + 2 + scripts[i + 1];
        for(int32_t j = i + 2; j < limit; ++j) {
            if(script == scripts[j]) { return i; }
        }
        i = limit;
    }
    return -1;
}

int32_t
CollationData::getEquivalentScripts(int32_t script,
                                    int32_t dest[], int32_t capacity,
                                    UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return 0; }
    int32_t i = findScript(script);
    if(i < 0) { return 0; }
    int32_t length = scripts[i + 1];
    U_ASSERT(length != 0);
    if(length > capacity) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return length;
    }
    i += 2;
    dest[0] = scripts[i++];
    for(int32_t j = 1; j < length; ++j) {
        script = scripts[i++];
        // Sorted insertion.
        for(int32_t k = j;; --k) {
            // Invariant: dest[k] is free to receive either script or dest[k - 1].
            if(k > 0 && script < dest[k - 1]) {
                dest[k] = dest[k - 1];
            } else {
                dest[k] = script;
                break;
            }
        }
    }
    return length;
}

void
CollationData::makeReorderTable(const int32_t *reorder, int32_t length,
                                uint8_t table[256], UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode)) { return; }

    // Initialize the table.
    // Never reorder special low and high primary lead bytes.
    int32_t lowByte;
    for(lowByte = 0; lowByte <= Collation::MERGE_SEPARATOR_BYTE; ++lowByte) {
        table[lowByte] = lowByte;
    }
    // lowByte == 03

    int32_t highByte;
    for(highByte = 0xff; highByte >= Collation::TRAIL_WEIGHT_BYTE; --highByte) {
        table[highByte] = highByte;
    }
    // highByte == FE

    // Set intermediate bytes to 0 to indicate that they have not been set yet.
    for(int32_t i = lowByte; i <= highByte; ++i) {
        table[i] = 0;
    }

    // Get the set of special reorder codes in the input list.
    // This supports up to 32 special reorder codes;
    // it works for data with codes beyond UCOL_REORDER_CODE_LIMIT.
    uint32_t specials = 0;
    for(int32_t i = 0; i < length; ++i) {
        int32_t reorderCode = reorder[i] - UCOL_REORDER_CODE_FIRST;
        if(0 <= reorderCode && reorderCode <= 31) {
            specials |= (uint32_t)1 << reorderCode;
        }
    }

    // Start the reordering with the special low reorder codes that do not occur in the input.
    for(int32_t i = 0;; i += 3) {
        if(scripts[i + 1] != 1) { break; }  // Went beyond special single-code reorder codes.
        int32_t reorderCode = (int32_t)scripts[i + 2] - UCOL_REORDER_CODE_FIRST;
        if(reorderCode < 0) { break; }  // Went beyond special reorder codes.
        if((specials & ((uint32_t)1 << reorderCode)) == 0) {
            int32_t head = scripts[i];
            int32_t firstByte = head >> 8;
            int32_t lastByte = head & 0xff;
            do { table[firstByte++] = lowByte++; } while(firstByte <= lastByte);
        }
    }

    // Reorder according to the input scripts, continuing from the bottom of the bytes range.
    for(int32_t i = 0; i < length;) {
        int32_t script = reorder[i++];
        if(script == USCRIPT_UNKNOWN) {
            // Put the remaining scripts at the top.
            while(i < length) {
                script = reorder[--length];
                if(script == USCRIPT_UNKNOWN ||  // Must occur at most once.
                        script == UCOL_REORDER_CODE_DEFAULT) {
                    errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                    return;
                }
                int32_t index = findScript(script);
                if(index < 0) { continue; }
                int32_t head = scripts[index];
                int32_t firstByte = head >> 8;
                int32_t lastByte = head & 0xff;
                if(table[firstByte] != 0) {  // Duplicate or equivalent script.
                    errorCode = U_ILLEGAL_ARGUMENT_ERROR;
                    return;
                }
                do { table[lastByte--] = highByte--; } while(firstByte <= lastByte);
            }
            break;
        }
        if(script == UCOL_REORDER_CODE_DEFAULT) {
            // The default code must be the only one in the list, and that is handled by the caller.
            // Otherwise it must not be used.
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        int32_t index = findScript(script);
        if(index < 0) { continue; }
        int32_t head = scripts[index];
        int32_t firstByte = head >> 8;
        int32_t lastByte = head & 0xff;
        if(table[firstByte] != 0) {  // Duplicate or equivalent script.
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
        do { table[firstByte++] = lowByte++; } while(firstByte <= lastByte);
    }

    // Put all remaining scripts into the middle.
    // Avoid table[0] which must remain 0.
    for(int32_t i = 1; i <= 0xff; ++i) {
        if(table[i] == 0) { table[i] = lowByte++; }
    }
    U_ASSERT(lowByte == highByte + 1);
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
