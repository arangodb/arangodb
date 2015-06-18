/*
*******************************************************************************
* Copyright (C) 2012-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationbasedatabuilder.h
*
* created on: 2012aug11
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONBASEDATABUILDER_H__
#define __COLLATIONBASEDATABUILDER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "collation.h"
#include "collationdata.h"
#include "collationdatabuilder.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

/**
 * Low-level base CollationData builder.
 */
class U_I18N_API CollationBaseDataBuilder : public CollationDataBuilder {
public:
    CollationBaseDataBuilder(UErrorCode &errorCode);

    virtual ~CollationBaseDataBuilder();

    void init(UErrorCode &errorCode);

    /**
     * Sets the Han ranges as ranges of offset CE32s.
     * Note: Unihan extension A sorts after the other BMP ranges.
     * See http://www.unicode.org/reports/tr10/#Implicit_Weights
     *
     * @param ranges array of ranges of [:Unified_Ideograph:] in collation order,
     *               as (start, end) code point pairs
     * @param length number of code points (not pairs)
     * @param errorCode in/out error code
     */
    void initHanRanges(const UChar32 ranges[], int32_t length, UErrorCode &errorCode);

    void setNumericPrimary(uint32_t np) { numericPrimary = np; }

    virtual UBool isCompressibleLeadByte(uint32_t b) const;

    void setCompressibleLeadByte(uint32_t b);

    static int32_t diffTwoBytePrimaries(uint32_t p1, uint32_t p2, UBool isCompressible);
    static int32_t diffThreeBytePrimaries(uint32_t p1, uint32_t p2, UBool isCompressible);

    virtual uint32_t encodeCEs(const int64_t ces[], int32_t cesLength, UErrorCode &errorCode);

    void addRootElements(const int64_t ces[], int32_t cesLength, UErrorCode &errorCode);
    void addRootElement(int64_t ce, UErrorCode &errorCode);

    void addReorderingGroup(uint32_t firstByte, uint32_t lastByte,
                            const UnicodeString &groupScripts,
                            UErrorCode &errorCode);

    virtual void build(CollationData &data, UErrorCode &errorCode);

    void buildRootElementsTable(UVector32 &table, UErrorCode &errorCode);

private:
    int32_t writeRootElementsRange(
            uint32_t prevPrimary, uint32_t p, int32_t i,
            UVector32 &table, UErrorCode &errorCode);

    // Flags for which primary-weight lead bytes are compressible.
    UBool compressibleBytes[256];
    uint32_t numericPrimary;
    uint32_t firstHanPrimary;
    uint32_t lastHanPrimary;
    int32_t hanStep;
    UVector64 rootElements;
    UnicodeString scripts;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONBASEDATABUILDER_H__
