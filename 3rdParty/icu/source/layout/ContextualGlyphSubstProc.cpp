/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "StateTables.h"
#include "MorphStateTables.h"
#include "SubtableProcessor.h"
#include "StateTableProcessor.h"
#include "ContextualGlyphSubstProc.h"
#include "LEGlyphStorage.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(ContextualGlyphSubstitutionProcessor)

ContextualGlyphSubstitutionProcessor::ContextualGlyphSubstitutionProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success)
  : StateTableProcessor(morphSubtableHeader, success), entryTable(), contextualGlyphSubstitutionHeader(morphSubtableHeader, success)
{
  contextualGlyphSubstitutionHeader.orphan();
  substitutionTableOffset = SWAPW(contextualGlyphSubstitutionHeader->substitutionTableOffset);

  
  entryTable = LEReferenceToArrayOf<ContextualGlyphSubstitutionStateEntry>(stateTableHeader, success, 
                                                                           (const ContextualGlyphSubstitutionStateEntry*)(&stateTableHeader->stHeader),
                                                                           entryTableOffset, LE_UNBOUNDED_ARRAY);
  int16Table = LEReferenceToArrayOf<le_int16>(stateTableHeader, success, (const le_int16*)(&stateTableHeader->stHeader),
                                              0, LE_UNBOUNDED_ARRAY); // rest of the table as le_int16s
}

ContextualGlyphSubstitutionProcessor::~ContextualGlyphSubstitutionProcessor()
{
}

void ContextualGlyphSubstitutionProcessor::beginStateTable()
{
    markGlyph = 0;
}

ByteOffset ContextualGlyphSubstitutionProcessor::processStateEntry(LEGlyphStorage &glyphStorage, le_int32 &currGlyph, EntryTableIndex index)
{
  LEErrorCode success = LE_NO_ERROR;
  const ContextualGlyphSubstitutionStateEntry *entry = entryTable.getAlias(index, success);
  ByteOffset newState = SWAPW(entry->newStateOffset);
  le_int16 flags = SWAPW(entry->flags);
  WordOffset markOffset = SWAPW(entry->markOffset);
  WordOffset currOffset = SWAPW(entry->currOffset);
  
  if (markOffset != 0 && LE_SUCCESS(success)) {
    LEGlyphID mGlyph = glyphStorage[markGlyph];
    TTGlyphID newGlyph = SWAPW(int16Table.getObject(markOffset + LE_GET_GLYPH(mGlyph), success)); // whew. 

    glyphStorage[markGlyph] = LE_SET_GLYPH(mGlyph, newGlyph);  
  }

  if (currOffset != 0) {
    LEGlyphID thisGlyph = glyphStorage[currGlyph];
    TTGlyphID newGlyph = SWAPW(int16Table.getObject(currOffset + LE_GET_GLYPH(thisGlyph), success)); // whew. 
    
    glyphStorage[currGlyph] = LE_SET_GLYPH(thisGlyph, newGlyph);
  }

    if (flags & cgsSetMark) {
        markGlyph = currGlyph;
    }

    if (!(flags & cgsDontAdvance)) {
        // should handle reverse too!
        currGlyph += 1;
    }

    return newState;
}

void ContextualGlyphSubstitutionProcessor::endStateTable()
{
}

U_NAMESPACE_END
