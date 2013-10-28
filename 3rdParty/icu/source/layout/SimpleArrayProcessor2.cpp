/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor2.h"
#include "NonContextualGlyphSubst.h"
#include "NonContextualGlyphSubstProc2.h"
#include "SimpleArrayProcessor2.h"
#include "LEGlyphStorage.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SimpleArrayProcessor2)

SimpleArrayProcessor2::SimpleArrayProcessor2()
{
}

SimpleArrayProcessor2::SimpleArrayProcessor2(const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success)
  : NonContextualGlyphSubstitutionProcessor2(morphSubtableHeader, success)
{
  const LEReferenceTo<NonContextualGlyphSubstitutionHeader2> header(morphSubtableHeader, success);

  simpleArrayLookupTable = LEReferenceTo<SimpleArrayLookupTable>(morphSubtableHeader, success, &header->table);
  valueArray = LEReferenceToArrayOf<LookupValue>(morphSubtableHeader, success, &simpleArrayLookupTable->valueArray[0], LE_UNBOUNDED_ARRAY);
}

SimpleArrayProcessor2::~SimpleArrayProcessor2()
{
}

void SimpleArrayProcessor2::process(LEGlyphStorage &glyphStorage, LEErrorCode &success)
{
    if (LE_FAILURE(success)) return;
    le_int32 glyphCount = glyphStorage.getGlyphCount();
    le_int32 glyph;

    for (glyph = 0; glyph < glyphCount; glyph += 1) {
        LEGlyphID thisGlyph = glyphStorage[glyph];
        if (LE_GET_GLYPH(thisGlyph) < 0xFFFF) {
          TTGlyphID newGlyph = SWAPW(valueArray(LE_GET_GLYPH(thisGlyph),success));

            glyphStorage[glyph] = LE_SET_GLYPH(thisGlyph, newGlyph);
        }
    }
}
 
U_NAMESPACE_END
