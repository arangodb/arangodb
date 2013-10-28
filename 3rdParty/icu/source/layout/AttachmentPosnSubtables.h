/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __ATTACHMENTPOSITIONINGSUBTABLES_H
#define __ATTACHMENTPOSITIONINGSUBTABLES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "OpenTypeTables.h"
#include "GlyphPositioningTables.h"
#include "ValueRecords.h"
#include "GlyphIterator.h"

U_NAMESPACE_BEGIN

struct AttachmentPositioningSubtable : GlyphPositioningSubtable
{
    Offset    baseCoverageTableOffset;
    le_uint16 classCount;
    Offset    markArrayOffset;
    Offset    baseArrayOffset;

    inline le_int32  getBaseCoverage(const LETableReference &base, LEGlyphID baseGlyphId, LEErrorCode &success) const;

    le_uint32 process(GlyphIterator *glyphIterator) const;
};

inline le_int32 AttachmentPositioningSubtable::getBaseCoverage(const LETableReference &base, LEGlyphID baseGlyphID, LEErrorCode &success) const
{
  return getGlyphCoverage(base, baseCoverageTableOffset, baseGlyphID, success);
}

U_NAMESPACE_END
#endif

