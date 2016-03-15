/*
*******************************************************************************
* Copyright (C) 2012-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationbasedatabuilder.cpp
*
* created on: 2012aug11
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/localpointer.h"
#include "unicode/ucharstriebuilder.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/utf16.h"
#include "collation.h"
#include "collationbasedatabuilder.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "collationrootelements.h"
#include "normalizer2impl.h"
#include "uassert.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

namespace {

/**
 * Compare two signed int64_t values as if they were unsigned.
 */
int32_t
compareInt64AsUnsigned(int64_t a, int64_t b) {
    if((uint64_t)a < (uint64_t)b) {
        return -1;
    } else if((uint64_t)a > (uint64_t)b) {
        return 1;
    } else {
        return 0;
    }
}

// TODO: Try to merge this with the binarySearch in alphaindex.cpp.
/**
 * Like Java Collections.binarySearch(List, String, Comparator).
 *
 * @return the index>=0 where the item was found,
 *         or the index<0 for inserting the string at ~index in sorted order
 */
int32_t
binarySearch(const UVector64 &list, int64_t ce) {
    if (list.size() == 0) { return ~0; }
    int32_t start = 0;
    int32_t limit = list.size();
    for (;;) {
        int32_t i = (start + limit) / 2;
        int32_t cmp = compareInt64AsUnsigned(ce, list.elementAti(i));
        if (cmp == 0) {
            return i;
        } else if (cmp < 0) {
            if (i == start) {
                return ~start;  // insert ce before i
            }
            limit = i;
        } else {
            if (i == start) {
                return ~(start + 1);  // insert ce after i
            }
            start = i;
        }
    }
}

}  // namespace

CollationBaseDataBuilder::CollationBaseDataBuilder(UErrorCode &errorCode)
        : CollationDataBuilder(errorCode),
          numericPrimary(0x12000000),
          firstHanPrimary(0), lastHanPrimary(0), hanStep(2),
          rootElements(errorCode) {
}

CollationBaseDataBuilder::~CollationBaseDataBuilder() {
}

void
CollationBaseDataBuilder::init(UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(trie != NULL) {
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }

    // Not compressible:
    // - digits
    // - Latin
    // - Hani
    // - trail weights
    // Some scripts are compressible, some are not.
    uprv_memset(compressibleBytes, FALSE, 256);
    compressibleBytes[Collation::UNASSIGNED_IMPLICIT_BYTE] = TRUE;

    // For a base, the default is to compute an unassigned-character implicit CE.
    // This includes surrogate code points; see the last option in
    // UCA section 7.1.1 Handling Ill-Formed Code Unit Sequences.
    trie = utrie2_open(Collation::UNASSIGNED_CE32, Collation::FFFD_CE32, &errorCode);

    // Preallocate trie blocks for Latin in the hope that proximity helps with CPU caches.
    for(UChar32 c = 0; c < 0x180; ++c) {
        utrie2_set32(trie, c, Collation::UNASSIGNED_CE32, &errorCode);
    }

    utrie2_set32(trie, 0xfffe, Collation::MERGE_SEPARATOR_CE32, &errorCode);
    // No root element for the merge separator which has 02 weights.
    // Some code assumes that the root first primary CE is the "space first primary"
    // from FractionalUCA.txt.

    uint32_t hangulCE32 = Collation::makeCE32FromTagAndIndex(Collation::HANGUL_TAG, 0);
    utrie2_setRange32(trie, Hangul::HANGUL_BASE, Hangul::HANGUL_END, hangulCE32, TRUE, &errorCode);

    // Add a mapping for the first-unassigned boundary,
    // which is the AlphabeticIndex overflow boundary.
    UnicodeString s((UChar)0xfdd1);  // Script boundary contractions start with U+FDD1.
    s.append((UChar)0xfdd0);  // Zzzz script sample character U+FDD0.
    int64_t ce = Collation::makeCE(Collation::FIRST_UNASSIGNED_PRIMARY);
    add(UnicodeString(), s, &ce, 1, errorCode);

    // Add a tailoring boundary, but not a mapping, for [first trailing].
    ce = Collation::makeCE(Collation::FIRST_TRAILING_PRIMARY);
    rootElements.addElement(ce, errorCode);

    // U+FFFD maps to a CE with the third-highest primary weight,
    // for predictable handling of ill-formed UTF-8.
    uint32_t ce32 = Collation::FFFD_CE32;
    utrie2_set32(trie, 0xfffd, ce32, &errorCode);
    addRootElement(Collation::ceFromSimpleCE32(ce32), errorCode);

    // U+FFFF maps to a CE with the highest primary weight.
    ce32 = Collation::MAX_REGULAR_CE32;
    utrie2_set32(trie, 0xffff, ce32, &errorCode);
    addRootElement(Collation::ceFromSimpleCE32(ce32), errorCode);
}

void
CollationBaseDataBuilder::initHanRanges(const UChar32 ranges[], int32_t length,
                                        UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || length == 0) { return; }
    if((length & 1) != 0) {  // incomplete start/end pairs
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if(isAssigned(0x4e00)) {  // already set
        errorCode = U_INVALID_STATE_ERROR;
        return;
    }
    int32_t numHanCodePoints = 0;
    for(int32_t i = 0; i < length; i += 2) {
        UChar32 start = ranges[i];
        UChar32 end = ranges[i + 1];
        numHanCodePoints += end - start + 1;
    }
    // Multiply the number of code points by (gap+1).
    // Add hanStep+2 for tailoring after the last Han character.
    int32_t gap = 1;
    hanStep = gap + 1;
    int32_t numHan = numHanCodePoints * hanStep + hanStep + 2;
    // Numbers of Han primaries per lead byte determined by
    // numbers of 2nd (not compressible) times 3rd primary byte values.
    int32_t numHanPerLeadByte = 254 * 254;
    int32_t numHanLeadBytes = (numHan + numHanPerLeadByte - 1) / numHanPerLeadByte;
    uint32_t hanPrimary = (uint32_t)(Collation::UNASSIGNED_IMPLICIT_BYTE - numHanLeadBytes) << 24;
    hanPrimary |= 0x20200;
    firstHanPrimary = hanPrimary;
    for(int32_t i = 0; i < length; i += 2) {
        UChar32 start = ranges[i];
        UChar32 end = ranges[i + 1];
        hanPrimary = setPrimaryRangeAndReturnNext(start, end, hanPrimary, hanStep, errorCode);
    }
    // One past the actual last one, but that is harmless for tailoring.
    // It saves us from subtracting "hanStep" and handling underflows.
    lastHanPrimary = hanPrimary;
}

UBool
CollationBaseDataBuilder::isCompressibleLeadByte(uint32_t b) const {
    return compressibleBytes[b];
}

void
CollationBaseDataBuilder::setCompressibleLeadByte(uint32_t b) {
    compressibleBytes[b] = TRUE;
}

int32_t
CollationBaseDataBuilder::diffTwoBytePrimaries(uint32_t p1, uint32_t p2, UBool isCompressible) {
    if((p1 & 0xff000000) == (p2 & 0xff000000)) {
        // Same lead bytes.
        return (int32_t)(p2 - p1) >> 16;
    } else {
        int32_t linear1;
        int32_t linear2;
        int32_t factor;
        if(isCompressible) {
            // Second byte for compressible lead byte: 251 bytes 04..FE
            linear1 = (int32_t)((p1 >> 16) & 0xff) - 4;
            linear2 = (int32_t)((p2 >> 16) & 0xff) - 4;
            factor = 251;
        } else {
            // Second byte for incompressible lead byte: 254 bytes 02..FF
            linear1 = (int32_t)((p1 >> 16) & 0xff) - 2;
            linear2 = (int32_t)((p2 >> 16) & 0xff) - 2;
            factor = 254;
        }
        linear1 += factor * (int32_t)((p1 >> 24) & 0xff);
        linear2 += factor * (int32_t)((p2 >> 24) & 0xff);
        return linear2 - linear1;
    }
}

int32_t
CollationBaseDataBuilder::diffThreeBytePrimaries(uint32_t p1, uint32_t p2, UBool isCompressible) {
    if((p1 & 0xffff0000) == (p2 & 0xffff0000)) {
        // Same first two bytes.
        return (int32_t)(p2 - p1) >> 8;
    } else {
        // Third byte: 254 bytes 02..FF
        int32_t linear1 = (int32_t)((p1 >> 8) & 0xff) - 2;
        int32_t linear2 = (int32_t)((p2 >> 8) & 0xff) - 2;
        int32_t factor;
        if(isCompressible) {
            // Second byte for compressible lead byte: 251 bytes 04..FE
            linear1 += 254 * ((int32_t)((p1 >> 16) & 0xff) - 4);
            linear2 += 254 * ((int32_t)((p2 >> 16) & 0xff) - 4);
            factor = 251 * 254;
        } else {
            // Second byte for incompressible lead byte: 254 bytes 02..FF
            linear1 += 254 * ((int32_t)((p1 >> 16) & 0xff) - 2);
            linear2 += 254 * ((int32_t)((p2 >> 16) & 0xff) - 2);
            factor = 254 * 254;
        }
        linear1 += factor * (int32_t)((p1 >> 24) & 0xff);
        linear2 += factor * (int32_t)((p2 >> 24) & 0xff);
        return linear2 - linear1;
    }
}

uint32_t
CollationBaseDataBuilder::encodeCEs(const int64_t ces[], int32_t cesLength, UErrorCode &errorCode) {
    addRootElements(ces, cesLength, errorCode);
    return CollationDataBuilder::encodeCEs(ces, cesLength, errorCode);
}

void
CollationBaseDataBuilder::addRootElements(const int64_t ces[], int32_t cesLength,
                                          UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    for(int32_t i = 0; i < cesLength; ++i) {
        addRootElement(ces[i], errorCode);
    }
}

void
CollationBaseDataBuilder::addRootElement(int64_t ce, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || ce == 0) { return; }
    // Remove case bits.
    ce &= INT64_C(0xffffffffffff3fff);
    U_ASSERT((ce & 0xc0) == 0);  // quaternary==0
    // Ignore the CE if it has a Han primary weight and common secondary/tertiary weights.
    // We will add it later, as part of the Han ranges.
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t secTer = (uint32_t)ce;
    if(secTer == Collation::COMMON_SEC_AND_TER_CE) {
        if(firstHanPrimary <= p && p <= lastHanPrimary) {
            return;
        }
    } else {
        // Check that secondary and tertiary weights are >= "common".
        uint32_t s = secTer >> 16;
        uint32_t t = secTer & Collation::ONLY_TERTIARY_MASK;
        if((s != 0 && s < Collation::COMMON_WEIGHT16) || (t != 0 && t < Collation::COMMON_WEIGHT16)) {
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    }
    // Check that primaries have at most 3 bytes.
    if((p & 0xff) != 0) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    int32_t i = binarySearch(rootElements, ce);
    if(i < 0) {
        rootElements.insertElementAt(ce, ~i, errorCode);
    }
}

void
CollationBaseDataBuilder::addReorderingGroup(uint32_t firstByte, uint32_t lastByte,
                                             const UnicodeString &groupScripts,
                                             UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    if(groupScripts.isEmpty()) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    if(groupScripts.indexOf((UChar)USCRIPT_UNKNOWN) >= 0) {
        // Zzzz must not occur.
        // It is the code used in the API to separate low and high scripts.
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    // Note: We are mostly trusting the input data,
    // rather than verifying that reordering groups do not intersect
    // with their lead byte ranges nor their sets of scripts,
    // and that all script codes are valid.
    scripts.append((UChar)((firstByte << 8) | lastByte));
    scripts.append((UChar)groupScripts.length());
    scripts.append(groupScripts);
}

void
CollationBaseDataBuilder::build(CollationData &data, UErrorCode &errorCode) {
    buildMappings(data, errorCode);
    data.numericPrimary = numericPrimary;
    data.compressibleBytes = compressibleBytes;
    data.scripts = reinterpret_cast<const uint16_t *>(scripts.getBuffer());
    data.scriptsLength = scripts.length();
    buildFastLatinTable(data, errorCode);
}

void
CollationBaseDataBuilder::buildRootElementsTable(UVector32 &table, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) { return; }
    uint32_t nextHanPrimary = firstHanPrimary;  // Set to 0xffffffff after the last Han range.
    uint32_t prevPrimary = 0;  // Start with primary ignorable CEs.
    UBool tryRange = FALSE;
    for(int32_t i = 0; i < rootElements.size(); ++i) {
        int64_t ce = rootElements.elementAti(i);
        uint32_t p = (uint32_t)(ce >> 32);
        uint32_t secTer = (uint32_t)ce & Collation::ONLY_SEC_TER_MASK;
        if(p != prevPrimary) {
            U_ASSERT((p & 0xff) == 0);
            int32_t end;
            if(p >= nextHanPrimary) {
                // Add a Han primary weight or range.
                // We omitted them initially, and omitted all CEs with Han primaries
                // and common secondary/tertiary weights.
                U_ASSERT(p > lastHanPrimary || secTer != Collation::COMMON_SEC_AND_TER_CE);
                if(p == nextHanPrimary) {
                    // One single Han primary with non-common secondary/tertiary weights.
                    table.addElement((int32_t)p, errorCode);
                    if(p < lastHanPrimary) {
                        // Prepare for the next Han range.
                        nextHanPrimary = Collation::incThreeBytePrimaryByOffset(p, FALSE, hanStep);
                    } else {
                        // p is the last Han primary.
                        nextHanPrimary = 0xffffffff;
                    }
                } else {
                    // p > nextHanPrimary: Add a Han primary range, starting with nextHanPrimary.
                    table.addElement((int32_t)nextHanPrimary, errorCode);
                    if(nextHanPrimary == lastHanPrimary) {
                        // nextHanPrimary == lastHanPrimary < p
                        // We just wrote the single last Han primary.
                        nextHanPrimary = 0xffffffff;
                    } else if(p < lastHanPrimary) {
                        // nextHanPrimary < p < lastHanPrimary
                        // End the Han range on p, prepare for the next range.
                        table.addElement((int32_t)p | hanStep, errorCode);
                        nextHanPrimary = Collation::incThreeBytePrimaryByOffset(p, FALSE, hanStep);
                    } else if(p == lastHanPrimary) {
                        // nextHanPrimary < p == lastHanPrimary
                        // End the last Han range on p.
                        table.addElement((int32_t)p | hanStep, errorCode);
                        nextHanPrimary = 0xffffffff;
                    } else {
                        // nextHanPrimary < lastHanPrimary < p
                        // End the last Han range, then write p.
                        table.addElement((int32_t)lastHanPrimary | hanStep, errorCode);
                        nextHanPrimary = 0xffffffff;
                        table.addElement((int32_t)p, errorCode);
                    }
                }
            } else if(tryRange && secTer == Collation::COMMON_SEC_AND_TER_CE &&
                    (end = writeRootElementsRange(prevPrimary, p, i + 1, table, errorCode)) != 0) {
                // Multiple CEs with only common secondary/tertiary weights were
                // combined into a primary range.
                // The range end was written, ending with the primary of rootElements[end].
                ce = rootElements.elementAti(end);
                p = (uint32_t)(ce >> 32);
                secTer = (uint32_t)ce & Collation::ONLY_SEC_TER_MASK;
                i = end;
            } else {
                // Write the primary weight of a normal CE.
                table.addElement((int32_t)p, errorCode);
            }
            prevPrimary = p;
        }
        if(secTer == Collation::COMMON_SEC_AND_TER_CE) {
            // The common secondar/tertiary weights are implied in the primary unit.
            // If there is no intervening delta unit, then we will try to combine
            // the next several primaries into a range.
            tryRange = TRUE;
        } else {
            // For each new set of secondary/tertiary weights we write a delta unit.
            table.addElement((int32_t)secTer | CollationRootElements::SEC_TER_DELTA_FLAG, errorCode);
            tryRange = FALSE;
        }
    }

    // Limit sentinel for root elements.
    // This allows us to reduce range checks at runtime.
    table.addElement(CollationRootElements::PRIMARY_SENTINEL, errorCode);
}

int32_t
CollationBaseDataBuilder::writeRootElementsRange(
        uint32_t prevPrimary, uint32_t p, int32_t i,
        UVector32 &table, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode) || i >= rootElements.size()) { return 0; }
    U_ASSERT(prevPrimary < p);
    // No ranges of single-byte primaries.
    if((p & prevPrimary & 0xff0000) == 0) { return 0; }
    // Lead bytes of compressible primaries must match.
    UBool isCompressible = isCompressiblePrimary(p);
    if((isCompressible || isCompressiblePrimary(prevPrimary)) &&
            (p & 0xff000000) != (prevPrimary & 0xff000000)) {
        return 0;
    }
    // Number of bytes in the primaries.
    UBool twoBytes;
    // Number of primaries from prevPrimary to p.
    int32_t step;
    if((p & 0xff00) == 0) {
        // 2-byte primary
        if((prevPrimary & 0xff00) != 0) { return 0; }  // length mismatch
        twoBytes = TRUE;
        step = diffTwoBytePrimaries(prevPrimary, p, isCompressible);
    } else {
        // 3-byte primary
        if((prevPrimary & 0xff00) == 0) { return 0; }  // length mismatch
        twoBytes = FALSE;
        step = diffThreeBytePrimaries(prevPrimary, p, isCompressible);
    }
    if(step > (int32_t)CollationRootElements::PRIMARY_STEP_MASK) { return 0; }
    // See if there are more than two CEs with primaries increasing by "step"
    // and with only common secondary/tertiary weights on all but the last one.
    int32_t end = 0;  // Initially 0: No range for just two primaries.
    for(;;) {
        prevPrimary = p;
        // Calculate which primary we expect next.
        uint32_t nextPrimary;  // = p + step
        if(twoBytes) {
            nextPrimary = Collation::incTwoBytePrimaryByOffset(p, isCompressible, step);
        } else {
            nextPrimary = Collation::incThreeBytePrimaryByOffset(p, isCompressible, step);
        }
        // Fetch the actual next CE.
        int64_t ce = rootElements.elementAti(i);
        p = (uint32_t)(ce >> 32);
        uint32_t secTer = (uint32_t)ce & Collation::ONLY_SEC_TER_MASK;
        // Does this primary increase by "step" from the last one?
        if(p != nextPrimary ||
                // Do not cross into a new lead byte if either is compressible.
                ((p & 0xff000000) != (prevPrimary & 0xff000000) &&
                    (isCompressible || isCompressiblePrimary(p)))) {
            // The range ends with the previous CE.
            p = prevPrimary;
            break;
        }
        // Extend the range to include this primary.
        end = i++;
        // This primary is the last in the range if it has non-common weights
        // or if we are at the end of the list.
        if(secTer != Collation::COMMON_SEC_AND_TER_CE || i >= rootElements.size()) { break; }
    }
    if(end != 0) {
        table.addElement((int32_t)p | step, errorCode);
    }
    return end;
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
