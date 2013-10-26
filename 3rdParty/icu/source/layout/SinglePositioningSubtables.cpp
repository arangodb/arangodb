/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "GlyphPositioningTables.h"
#include "SinglePositioningSubtables.h"
#include "ValueRecords.h"
#include "GlyphIterator.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

le_uint32 SinglePositioningSubtable::process(const LEReferenceTo<SinglePositioningSubtable> &base, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode &success) const
{
    switch(SWAPW(subtableFormat))
    {
    case 0:
        return 0;

    case 1:
    {
      const LEReferenceTo<SinglePositioningFormat1Subtable> subtable(base, success, (const SinglePositioningFormat1Subtable *) this);

      return subtable->process(subtable, glyphIterator, fontInstance, success);
    }

    case 2:
    {
      const LEReferenceTo<SinglePositioningFormat2Subtable> subtable(base, success, (const SinglePositioningFormat2Subtable *) this);

      return subtable->process(subtable, glyphIterator, fontInstance, success);
    }

    default:
        return 0;
    }
}

le_uint32 SinglePositioningFormat1Subtable::process(const LEReferenceTo<SinglePositioningFormat1Subtable> &base, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode &success) const
{
    LEGlyphID glyph = glyphIterator->getCurrGlyphID();
    le_int32 coverageIndex = getGlyphCoverage(base, glyph, success);

    if (coverageIndex >= 0) {
        valueRecord.adjustPosition(SWAPW(valueFormat), (const char *) this, *glyphIterator, fontInstance);

        return 1;
    }

    return 0;
}

le_uint32 SinglePositioningFormat2Subtable::process(const LEReferenceTo<SinglePositioningFormat2Subtable> &base, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode &success) const
{
    LEGlyphID glyph = glyphIterator->getCurrGlyphID();
    le_int16 coverageIndex = (le_int16) getGlyphCoverage(base, glyph, success);

    if (coverageIndex >= 0) {
        valueRecordArray[0].adjustPosition(coverageIndex, SWAPW(valueFormat), (const char *) this, *glyphIterator, fontInstance);

        return 1;
    }

    return 0;
}

U_NAMESPACE_END
