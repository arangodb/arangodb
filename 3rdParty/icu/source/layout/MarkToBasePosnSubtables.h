/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __MARKTOBASEPOSITIONINGSUBTABLES_H
#define __MARKTOBASEPOSITIONINGSUBTABLES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "GlyphPositioningTables.h"
#include "AttachmentPosnSubtables.h"
#include "GlyphIterator.h"

U_NAMESPACE_BEGIN

struct MarkToBasePositioningSubtable : AttachmentPositioningSubtable
{
  le_int32   process(const LETableReference &base, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode &success) const;
    LEGlyphID  findBaseGlyph(GlyphIterator *glyphIterator) const;
};

struct BaseRecord
{
    Offset baseAnchorTableOffsetArray[ANY_NUMBER];
};
LE_VAR_ARRAY(BaseRecord, baseAnchorTableOffsetArray)

struct BaseArray
{
    le_int16 baseRecordCount;
    BaseRecord baseRecordArray[ANY_NUMBER];
};
LE_VAR_ARRAY(BaseArray, baseRecordArray)

U_NAMESPACE_END
#endif

