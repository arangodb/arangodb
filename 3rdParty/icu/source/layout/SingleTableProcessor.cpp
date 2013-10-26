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
#include "SingleTableProcessor.h"
#include "LEGlyphStorage.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SingleTableProcessor)

SingleTableProcessor::SingleTableProcessor()
{
}

SingleTableProcessor::SingleTableProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success)
  : NonContextualGlyphSubstitutionProcessor(morphSubtableHeader, success)
{
  LEReferenceTo<NonContextualGlyphSubstitutionHeader> header(morphSubtableHeader, success);
  singleTableLookupTable = LEReferenceTo<SingleTableLookupTable>(morphSubtableHeader, success, (const SingleTableLookupTable*)&header->table);
}

SingleTableProcessor::~SingleTableProcessor()
{
}

void SingleTableProcessor::process(LEGlyphStorage &glyphStorage, LEErrorCode &success)
{
    const LookupSingle *entries = singleTableLookupTable->entries;
    le_int32 glyph;
    le_int32 glyphCount = glyphStorage.getGlyphCount();

    for (glyph = 0; glyph < glyphCount; glyph += 1) {
      const LookupSingle *lookupSingle = singleTableLookupTable->lookupSingle(singleTableLookupTable, entries, glyphStorage[glyph], success);

        if (lookupSingle != NULL) {
            glyphStorage[glyph] = SWAPW(lookupSingle->value);
        }
    }
} 

U_NAMESPACE_END
