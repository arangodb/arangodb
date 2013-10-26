/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEGlyphFilter.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "ICUFeatures.h"
#include "Lookups.h"
#include "ScriptAndLanguage.h"
#include "GlyphDefinitionTables.h"
#include "GlyphSubstitutionTables.h"
#include "SingleSubstitutionSubtables.h"
#include "MultipleSubstSubtables.h"
#include "AlternateSubstSubtables.h"
#include "LigatureSubstSubtables.h"
#include "ContextualSubstSubtables.h"
#include "ExtensionSubtables.h"
#include "LookupProcessor.h"
#include "GlyphSubstLookupProc.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

GlyphSubstitutionLookupProcessor::GlyphSubstitutionLookupProcessor(
        const LEReferenceTo<GlyphSubstitutionTableHeader> &glyphSubstitutionTableHeader,
        LETag scriptTag, 
        LETag languageTag, 
        const LEGlyphFilter *filter, 
        const FeatureMap *featureMap, 
        le_int32 featureMapCount, 
        le_bool featureOrder,
        LEErrorCode& success)
    : LookupProcessor(
                      glyphSubstitutionTableHeader,
                      SWAPW(glyphSubstitutionTableHeader->scriptListOffset),
                      SWAPW(glyphSubstitutionTableHeader->featureListOffset),
                      SWAPW(glyphSubstitutionTableHeader->lookupListOffset),
                      scriptTag, languageTag, featureMap, featureMapCount, featureOrder, success), fFilter(filter)
{
    // anything?
}

GlyphSubstitutionLookupProcessor::GlyphSubstitutionLookupProcessor()
{
}

le_uint32 GlyphSubstitutionLookupProcessor::applySubtable(const LEReferenceTo<LookupSubtable> &lookupSubtable, le_uint16 lookupType,
                                                       GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode& success) const
{
    if (LE_FAILURE(success)) {
        return 0;
    }

    le_uint32 delta = 0;

    switch(lookupType)
    {
    case 0:
        break;

    case gsstSingle:
    {
        const LEReferenceTo<SingleSubstitutionSubtable> subtable(lookupSubtable, success);

        delta = subtable->process(subtable, glyphIterator, success, fFilter);
        break;
    }

    case gsstMultiple:
    {
        const LEReferenceTo<MultipleSubstitutionSubtable> subtable(lookupSubtable, success);

        delta = subtable->process(subtable, glyphIterator, success, fFilter);
        break;
    }

    case gsstAlternate:
    {
        const LEReferenceTo<AlternateSubstitutionSubtable> subtable(lookupSubtable, success);

        delta = subtable->process(subtable, glyphIterator, success, fFilter);
        break;
    }

    case gsstLigature:
    {
        const LEReferenceTo<LigatureSubstitutionSubtable> subtable(lookupSubtable, success);

        delta = subtable->process(subtable, glyphIterator, success, fFilter);
        break;
    }

    case gsstContext:
    {
        const LEReferenceTo<ContextualSubstitutionSubtable> subtable(lookupSubtable, success);

        delta = subtable->process(this, glyphIterator, fontInstance, success);
        break;
    }

    case gsstChainingContext:
    {
        const LEReferenceTo<ChainingContextualSubstitutionSubtable> subtable(lookupSubtable, success);

        delta = subtable->process(this, glyphIterator, fontInstance, success);
        break;
    }

    case gsstExtension:
    {
        const LEReferenceTo<ExtensionSubtable> subtable(lookupSubtable, success);

        delta = subtable->process(this, lookupType, glyphIterator, fontInstance, success);
        break;
    }

    default:
        break;
    }

    return delta;
}

GlyphSubstitutionLookupProcessor::~GlyphSubstitutionLookupProcessor()
{
}

U_NAMESPACE_END
