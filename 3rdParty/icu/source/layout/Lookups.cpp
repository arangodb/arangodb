/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "OpenTypeTables.h"
#include "Lookups.h"
#include "CoverageTables.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

const LEReferenceTo<LookupTable> LookupListTable::getLookupTable(const LEReferenceTo<LookupListTable> &base, le_uint16 lookupTableIndex, LEErrorCode &success) const
{
  LEReferenceToArrayOf<Offset> lookupTableOffsetArrayRef(base, success, (const Offset*)&lookupTableOffsetArray, SWAPW(lookupCount));

  if(LE_FAILURE(success) || lookupTableIndex>lookupTableOffsetArrayRef.getCount()) {
    return LEReferenceTo<LookupTable>();
  } else {
    return LEReferenceTo<LookupTable>(base, success, SWAPW(lookupTableOffsetArrayRef.getObject(lookupTableIndex, success)));
  }
}

const LEReferenceTo<LookupSubtable> LookupTable::getLookupSubtable(const LEReferenceTo<LookupTable> &base, le_uint16 subtableIndex, LEErrorCode &success) const
{
  LEReferenceToArrayOf<Offset> subTableOffsetArrayRef(base, success, (const Offset*)&subTableOffsetArray, SWAPW(subTableCount));

  if(LE_FAILURE(success) || subtableIndex>subTableOffsetArrayRef.getCount()) {
    return LEReferenceTo<LookupSubtable>();
  } else {
    return LEReferenceTo<LookupSubtable>(base, success, SWAPW(subTableOffsetArrayRef.getObject(subtableIndex, success)));
  }
}

le_int32 LookupSubtable::getGlyphCoverage(const LEReferenceTo<LookupSubtable> &base, Offset tableOffset, LEGlyphID glyphID, LEErrorCode &success) const
{
  const LEReferenceTo<CoverageTable> coverageTable(base, success, SWAPW(tableOffset));

  if(LE_FAILURE(success)) return 0; 

  return coverageTable->getGlyphCoverage(glyphID);
}

U_NAMESPACE_END
