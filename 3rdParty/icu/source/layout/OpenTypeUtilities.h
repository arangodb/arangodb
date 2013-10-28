/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __OPENTYPEUTILITIES_H
#define __OPENTYPEUTILITIES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "OpenTypeTables.h"

U_NAMESPACE_BEGIN

class OpenTypeUtilities /* not : public UObject because all methods are static */ {
public:
    static le_int8 highBit(le_int32 value);
    static Offset getTagOffset(LETag tag, const LEReferenceToArrayOf<TagAndOffsetRecord> &records, LEErrorCode &success);
    static le_int32 getGlyphRangeIndex(TTGlyphID glyphID, const GlyphRangeRecord *records, le_int32 recordCount) {
      LEErrorCode success = LE_NO_ERROR;
      LETableReference recordRef0((const le_uint8*)records);
      LEReferenceToArrayOf<GlyphRangeRecord> recordRef(recordRef0, success, (size_t)0, recordCount);
      return getGlyphRangeIndex(glyphID, recordRef, success);
    }
    static le_int32 getGlyphRangeIndex(TTGlyphID glyphID, const LEReferenceToArrayOf<GlyphRangeRecord> &records, LEErrorCode &success);
    static le_int32 search(le_uint16 value, const le_uint16 array[], le_int32 count);
    static le_int32 search(le_uint32 value, const le_uint32 array[], le_int32 count);
    static void sort(le_uint16 *array, le_int32 count);

private:
    OpenTypeUtilities() {} // private - forbid instantiation
};

U_NAMESPACE_END
#endif
