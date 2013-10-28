/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "StateTables.h"
#include "MorphStateTables.h"
#include "SubtableProcessor2.h"
#include "StateTableProcessor2.h"
#include "ContextualGlyphSubstProc2.h"
#include "LEGlyphStorage.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(ContextualGlyphSubstitutionProcessor2)

ContextualGlyphSubstitutionProcessor2::ContextualGlyphSubstitutionProcessor2(
                                  const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success)
  : StateTableProcessor2(morphSubtableHeader, success), contextualGlyphHeader(morphSubtableHeader, success)
{
    if(LE_FAILURE(success)) return;
    le_uint32 perGlyphTableOffset = SWAPL(contextualGlyphHeader->perGlyphTableOffset);
    perGlyphTable = LEReferenceToArrayOf<le_uint32> (stHeader, success, perGlyphTableOffset, LE_UNBOUNDED_ARRAY);
    entryTable = LEReferenceToArrayOf<ContextualGlyphStateEntry2>(stHeader, success, entryTableOffset, LE_UNBOUNDED_ARRAY);
}

ContextualGlyphSubstitutionProcessor2::~ContextualGlyphSubstitutionProcessor2()
{
}

void ContextualGlyphSubstitutionProcessor2::beginStateTable()
{
    markGlyph = 0;
}

le_uint16 ContextualGlyphSubstitutionProcessor2::processStateEntry(LEGlyphStorage &glyphStorage, le_int32 &currGlyph,
    EntryTableIndex2 index, LEErrorCode &success)
{
    if(LE_FAILURE(success)) return 0;
    const ContextualGlyphStateEntry2 *entry = entryTable.getAlias(index, success);
    if(LE_FAILURE(success)) return 0;
    le_uint16 newState = SWAPW(entry->newStateIndex);
    le_uint16 flags = SWAPW(entry->flags);
    le_int16 markIndex = SWAPW(entry->markIndex);
    le_int16 currIndex = SWAPW(entry->currIndex);
    
    if (markIndex != -1) {
        le_uint32 offset = SWAPL(perGlyphTable(markIndex, success));
        LEGlyphID mGlyph = glyphStorage[markGlyph];
        TTGlyphID newGlyph = lookup(offset, mGlyph, success);        
        glyphStorage[markGlyph] = LE_SET_GLYPH(mGlyph, newGlyph);
    }

    if (currIndex != -1) {
        le_uint32 offset = SWAPL(perGlyphTable(currIndex, success));
        LEGlyphID thisGlyph = glyphStorage[currGlyph];
        TTGlyphID newGlyph = lookup(offset, thisGlyph, success);
        glyphStorage[currGlyph] = LE_SET_GLYPH(thisGlyph, newGlyph);
    }
    
    if (flags & cgsSetMark) {
        markGlyph = currGlyph;
    }

    if (!(flags & cgsDontAdvance)) {
        currGlyph += dir;
    }

    return newState;
}

TTGlyphID ContextualGlyphSubstitutionProcessor2::lookup(le_uint32 offset, LEGlyphID gid, LEErrorCode &success)
{
    TTGlyphID newGlyph = 0xFFFF;
    if(LE_FAILURE(success))  return newGlyph;
    LEReferenceTo<LookupTable> lookupTable(perGlyphTable, success, offset);
    if(LE_FAILURE(success))  return newGlyph;
    le_int16 format = SWAPW(lookupTable->format);

    switch (format) {
        case ltfSimpleArray: {
#ifdef TEST_FORMAT
            // Disabled pending for design review
            LEReferenceTo<SimpleArrayLookupTable> lookupTable0(lookupTable, success);
            LEReferenceToArrayOf<LookupValue> valueArray(lookupTable0, success, &lookupTable0->valueArray[0], LE_UNBOUNDED_ARRAY);
            if(LE_FAILURE(success))  return newGlyph;
            TTGlyphID glyphCode = (TTGlyphID) LE_GET_GLYPH(gid);
            newGlyph = SWAPW(lookupTable0->valueArray(glyphCode, success));
#endif
            break;
        }
        case ltfSegmentSingle: {
#ifdef TEST_FORMAT
            // Disabled pending for design review
            LEReferenceTo<SegmentSingleLookupTable> lookupTable2 = (SegmentSingleLookupTable *) lookupTable;
            const LookupSegment *segment = lookupTable2->lookupSegment(lookupTable2->segments, gid);
            if (segment != NULL) {
                newGlyph = SWAPW(segment->value);
            }
#endif
            break;
        }
        case ltfSegmentArray: {
            //printf("Context Lookup Table Format4: specific interpretation needed!\n");
            break;
        }
        case ltfSingleTable:
        {
#ifdef TEST_FORMAT
            // Disabled pending for design review
            LEReferenceTo<SingleTableLookupTable> lookupTable6 = (SingleTableLookupTable *) lookupTable;
            const LEReferenceTo<LookupSingle> segment = lookupTable6->lookupSingle(lookupTable6->entries, gid);
            if (segment != NULL) {
                newGlyph = SWAPW(segment->value);
            }
#endif
            break;
        }
        case ltfTrimmedArray: {
            LEReferenceTo<TrimmedArrayLookupTable> lookupTable8(lookupTable, success);
            if (LE_FAILURE(success)) return newGlyph;
            TTGlyphID firstGlyph = SWAPW(lookupTable8->firstGlyph);    
            TTGlyphID glyphCount = SWAPW(lookupTable8->glyphCount);
            TTGlyphID lastGlyph  = firstGlyph + glyphCount;
            TTGlyphID glyphCode = (TTGlyphID) LE_GET_GLYPH(gid);
            if ((glyphCode >= firstGlyph) && (glyphCode < lastGlyph)) {
              LEReferenceToArrayOf<LookupValue> valueArray(lookupTable8, success, &lookupTable8->valueArray[0], glyphCount);
              newGlyph = SWAPW(valueArray(glyphCode - firstGlyph, success));
            }
        }
        default:
            break;
    }
    return newGlyph;
}

void ContextualGlyphSubstitutionProcessor2::endStateTable()
{
}

U_NAMESPACE_END
