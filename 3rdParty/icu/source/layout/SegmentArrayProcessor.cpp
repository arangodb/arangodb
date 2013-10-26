/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "NonContextualGlyphSubst.h"
#include "NonContextualGlyphSubstProc.h"
#include "SegmentArrayProcessor.h"
#include "LEGlyphStorage.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SegmentArrayProcessor)

SegmentArrayProcessor::SegmentArrayProcessor()
{
}

SegmentArrayProcessor::SegmentArrayProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success)
  : NonContextualGlyphSubstitutionProcessor(morphSubtableHeader, success)
{
  LEReferenceTo<NonContextualGlyphSubstitutionHeader> header(morphSubtableHeader, success);
  segmentArrayLookupTable = LEReferenceTo<SegmentArrayLookupTable>(morphSubtableHeader, success, (const SegmentArrayLookupTable*)&header->table);
}

SegmentArrayProcessor::~SegmentArrayProcessor()
{
}

void SegmentArrayProcessor::process(LEGlyphStorage &glyphStorage, LEErrorCode &success)
{
    const LookupSegment *segments = segmentArrayLookupTable->segments;
    le_int32 glyphCount = glyphStorage.getGlyphCount();
    le_int32 glyph;

    for (glyph = 0; glyph < glyphCount; glyph += 1) {
        LEGlyphID thisGlyph = glyphStorage[glyph];
        const LookupSegment *lookupSegment = segmentArrayLookupTable->lookupSegment(segmentArrayLookupTable, segments, thisGlyph, success);

        if (lookupSegment != NULL)  {
            TTGlyphID firstGlyph = SWAPW(lookupSegment->firstGlyph);
            le_int16  offset = SWAPW(lookupSegment->value);

            if (offset != 0) {
              LEReferenceToArrayOf<TTGlyphID> glyphArray(subtableHeader, success, offset, LE_UNBOUNDED_ARRAY);
              TTGlyphID   newGlyph   = SWAPW(glyphArray(LE_GET_GLYPH(thisGlyph) - firstGlyph, success));
              glyphStorage[glyph] = LE_SET_GLYPH(thisGlyph, newGlyph);
            } 
        }
    }
}
 
U_NAMESPACE_END
