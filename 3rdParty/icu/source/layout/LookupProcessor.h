/*
 * %W% %E%
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __LOOKUPPROCESSOR_H
#define __LOOKUPPROCESSOR_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "LETableReference.h"
//#include "Lookups.h"
//#include "Features.h"

U_NAMESPACE_BEGIN

class  LEFontInstance;
class  LEGlyphStorage;
class  GlyphIterator;
class  GlyphPositionAdjustments;
struct FeatureTable;
struct FeatureListTable;
struct GlyphDefinitionTableHeader;
struct LookupSubtable;
struct LookupTable;

class LookupProcessor : public UMemory {
public:
    le_int32 process(LEGlyphStorage &glyphStorage, GlyphPositionAdjustments *glyphPositionAdjustments,
                 le_bool rightToLeft, const LEReferenceTo<GlyphDefinitionTableHeader> &glyphDefinitionTableHeader, const LEFontInstance *fontInstance, LEErrorCode& success) const;

    le_uint32 applyLookupTable(const LEReferenceTo<LookupTable> &lookupTable, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode& success) const;

    le_uint32 applySingleLookup(le_uint16 lookupTableIndex, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode& success) const;

    virtual le_uint32 applySubtable(const LEReferenceTo<LookupSubtable> &lookupSubtable, le_uint16 subtableType,
        GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode& success) const = 0;

    virtual ~LookupProcessor();

    const LETableReference &getReference() const { return fReference; }

protected:
    LookupProcessor(const LETableReference &baseAddress,
        Offset scriptListOffset, 
        Offset featureListOffset, 
        Offset lookupListOffset,
        LETag scriptTag, 
        LETag languageTag, 
        const FeatureMap *featureMap, 
        le_int32 featureMapCount, 
        le_bool orderFeatures, 
        LEErrorCode& success);

    LookupProcessor();

    le_int32 selectLookups(const LEReferenceTo<FeatureTable> &featureTable, FeatureMask featureMask, le_int32 order, LEErrorCode &success);

    LEReferenceTo<LookupListTable>   lookupListTable;
    LEReferenceTo<FeatureListTable>  featureListTable;

    FeatureMask            *lookupSelectArray;
    le_uint32              lookupSelectCount;

    le_uint16               *lookupOrderArray;
    le_uint32               lookupOrderCount;

    LETableReference        fReference;

private:

    LookupProcessor(const LookupProcessor &other); // forbid copying of this class
    LookupProcessor &operator=(const LookupProcessor &other); // forbid copying of this class
};

U_NAMESPACE_END
#endif
