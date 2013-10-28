/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __LOOKUPS_H
#define __LOOKUPS_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "OpenTypeTables.h"

U_NAMESPACE_BEGIN

enum LookupFlags
{
    lfBaselineIsLogicalEnd  = 0x0001,  // The MS spec. calls this flag "RightToLeft" but this name is more accurate 
    lfIgnoreBaseGlyphs      = 0x0002,
    lfIgnoreLigatures       = 0x0004,
    lfIgnoreMarks           = 0x0008,
    lfReservedMask          = 0x00F0,
    lfMarkAttachTypeMask    = 0xFF00,
    lfMarkAttachTypeShift   = 8
};

struct LookupSubtable
{
    le_uint16 subtableFormat;
    Offset    coverageTableOffset;

  inline le_int32  getGlyphCoverage(const LEReferenceTo<LookupSubtable> &base, LEGlyphID glyphID, LEErrorCode &success) const;

  le_int32  getGlyphCoverage(const LEReferenceTo<LookupSubtable> &base, Offset tableOffset, LEGlyphID glyphID, LEErrorCode &success) const;

  // convenience
  inline le_int32  getGlyphCoverage(const LETableReference &base, LEGlyphID glyphID, LEErrorCode &success) const;

  inline le_int32  getGlyphCoverage(const LETableReference &base, Offset tableOffset, LEGlyphID glyphID, LEErrorCode &success) const;
};

struct LookupTable
{
    le_uint16       lookupType;
    le_uint16       lookupFlags;
    le_uint16       subTableCount;
    Offset          subTableOffsetArray[ANY_NUMBER];

  const LEReferenceTo<LookupSubtable> getLookupSubtable(const LEReferenceTo<LookupTable> &base, le_uint16 subtableIndex, LEErrorCode &success) const;
};
LE_VAR_ARRAY(LookupTable, subTableOffsetArray)

struct LookupListTable
{
    le_uint16   lookupCount;
    Offset      lookupTableOffsetArray[ANY_NUMBER];

  const LEReferenceTo<LookupTable> getLookupTable(const LEReferenceTo<LookupListTable> &base, le_uint16 lookupTableIndex, LEErrorCode &success) const;
};
LE_VAR_ARRAY(LookupListTable, lookupTableOffsetArray)

inline le_int32 LookupSubtable::getGlyphCoverage(const LEReferenceTo<LookupSubtable> &base, LEGlyphID glyphID, LEErrorCode &success) const
{
  return getGlyphCoverage(base, coverageTableOffset, glyphID, success);
}

inline le_int32  LookupSubtable::getGlyphCoverage(const LETableReference &base, LEGlyphID glyphID, LEErrorCode &success) const {
  LEReferenceTo<LookupSubtable> thisRef(base, success, this);
  return getGlyphCoverage(thisRef, glyphID, success);
}

inline le_int32  LookupSubtable::getGlyphCoverage(const LETableReference &base, Offset tableOffset, LEGlyphID glyphID, LEErrorCode &success) const {
  LEReferenceTo<LookupSubtable> thisRef(base, success, this);
  return getGlyphCoverage(thisRef, tableOffset, glyphID, success);
}

U_NAMESPACE_END
#endif
