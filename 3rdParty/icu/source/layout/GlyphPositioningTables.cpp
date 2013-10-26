/*
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "Lookups.h"
#include "GlyphDefinitionTables.h"
#include "GlyphPositioningTables.h"
#include "GlyphPosnLookupProc.h"
#include "CursiveAttachmentSubtables.h"
#include "LEGlyphStorage.h"
#include "GlyphPositionAdjustments.h"

U_NAMESPACE_BEGIN

void GlyphPositioningTableHeader::process(const LEReferenceTo<GlyphPositioningTableHeader> &base, LEGlyphStorage &glyphStorage, GlyphPositionAdjustments *glyphPositionAdjustments, le_bool rightToLeft,
                                          LETag scriptTag, LETag languageTag,
                                          const LEReferenceTo<GlyphDefinitionTableHeader> &glyphDefinitionTableHeader, LEErrorCode &success,
                                          const LEFontInstance *fontInstance, const FeatureMap *featureMap, le_int32 featureMapCount, le_bool featureOrder) const
{
    if (LE_FAILURE(success)) {
        return;
    } 

    GlyphPositioningLookupProcessor processor(base, scriptTag, languageTag, featureMap, featureMapCount, featureOrder, success);
    if (LE_FAILURE(success)) {
        return;
    } 

    processor.process(glyphStorage, glyphPositionAdjustments, rightToLeft, glyphDefinitionTableHeader, fontInstance, success);

    glyphPositionAdjustments->applyCursiveAdjustments(glyphStorage, rightToLeft, fontInstance);
}

U_NAMESPACE_END
