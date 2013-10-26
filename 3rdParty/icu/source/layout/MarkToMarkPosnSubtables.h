/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __MARKTOMARKPOSITIONINGSUBTABLES_H
#define __MARKTOMARKPOSITIONINGSUBTABLES_H

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

struct MarkToMarkPositioningSubtable : AttachmentPositioningSubtable
{
  le_int32   process(const LETableReference &base, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode &success) const;
    LEGlyphID  findMark2Glyph(GlyphIterator *glyphIterator) const;
};

struct Mark2Record
{
    Offset mark2AnchorTableOffsetArray[ANY_NUMBER];
};
LE_VAR_ARRAY(Mark2Record, mark2AnchorTableOffsetArray)

struct Mark2Array
{
    le_uint16 mark2RecordCount;
    Mark2Record mark2RecordArray[ANY_NUMBER];
};
LE_VAR_ARRAY(Mark2Array, mark2RecordArray)

U_NAMESPACE_END
#endif

