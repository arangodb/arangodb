/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEGlyphFilter.h"
#include "OpenTypeTables.h"
#include "GlyphSubstitutionTables.h"
#include "AlternateSubstSubtables.h"
#include "GlyphIterator.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

le_uint32 AlternateSubstitutionSubtable::process(const LEReferenceTo<AlternateSubstitutionSubtable> &base,
                                       GlyphIterator *glyphIterator, LEErrorCode &success, const LEGlyphFilter *filter) const
{
    // NOTE: For now, we'll just pick the first alternative...
    LEGlyphID glyph = glyphIterator->getCurrGlyphID();
    le_int32 coverageIndex = getGlyphCoverage(base, glyph, success);

    if (coverageIndex >= 0 && LE_SUCCESS(success)) {
        le_uint16 altSetCount = SWAPW(alternateSetCount);

        if (coverageIndex < altSetCount) {
            Offset alternateSetTableOffset = SWAPW(alternateSetTableOffsetArray[coverageIndex]);
            const LEReferenceTo<AlternateSetTable> alternateSetTable(base, success,
                                  (const AlternateSetTable *) ((char *) this + alternateSetTableOffset));
            TTGlyphID alternate = SWAPW(alternateSetTable->alternateArray[0]);

            if (filter == NULL || filter->accept(LE_SET_GLYPH(glyph, alternate))) {
                glyphIterator->setCurrGlyphID(SWAPW(alternateSetTable->alternateArray[0]));
            }

            return 1;
        }

        // XXXX If we get here, the table's mal-formed...
    }

    return 0;
}

U_NAMESPACE_END
