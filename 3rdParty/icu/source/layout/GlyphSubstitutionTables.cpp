/*
 *
 * (C) Copyright IBM Corp. 1998-2008 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEGlyphFilter.h"
#include "OpenTypeTables.h"
#include "Lookups.h"
#include "GlyphDefinitionTables.h"
#include "GlyphSubstitutionTables.h"
#include "GlyphSubstLookupProc.h"
#include "ScriptAndLanguage.h"
#include "LEGlyphStorage.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

le_int32 GlyphSubstitutionTableHeader::process(LEGlyphStorage &glyphStorage,
                                               le_bool rightToLeft, 
                                               LETag scriptTag, 
                                               LETag languageTag,
                                               const GlyphDefinitionTableHeader *glyphDefinitionTableHeader, 
                                               const LEGlyphFilter *filter, 
                                               const FeatureMap *featureMap, 
                                               le_int32 featureMapCount, 
                                               le_bool featureOrder,
                                               LEErrorCode &success) const
{
    if (LE_FAILURE(success)) {
        return 0;
    } 

    GlyphSubstitutionLookupProcessor processor(this, scriptTag, languageTag, filter, featureMap, featureMapCount, featureOrder, success);
    return processor.process(glyphStorage, NULL, rightToLeft, glyphDefinitionTableHeader, NULL, success);
}

U_NAMESPACE_END
