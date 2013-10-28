/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __PAIRPOSITIONINGSUBTABLES_H
#define __PAIRPOSITIONINGSUBTABLES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "GlyphPositioningTables.h"
#include "ValueRecords.h"
#include "GlyphIterator.h"

U_NAMESPACE_BEGIN

// NOTE: ValueRecord has a variable size
struct PairValueRecord
{
    TTGlyphID     secondGlyph;
    ValueRecord valueRecord1;
//  ValueRecord valueRecord2;
};

struct PairSetTable
{
    le_uint16       pairValueCount;
    PairValueRecord pairValueRecordArray[ANY_NUMBER];
};
LE_VAR_ARRAY(PairSetTable, pairValueRecordArray)

struct PairPositioningSubtable : GlyphPositioningSubtable
{
    ValueFormat valueFormat1;
    ValueFormat valueFormat2;

    le_uint32  process(const LEReferenceTo<PairPositioningSubtable> &base, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode &success) const;
};

struct PairPositioningFormat1Subtable : PairPositioningSubtable
{
    le_uint16   pairSetCount;
    Offset      pairSetTableOffsetArray[ANY_NUMBER];

    le_uint32  process(const LEReferenceTo<PairPositioningFormat1Subtable> &base, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode &success) const;

private:
    const PairValueRecord *findPairValueRecord(TTGlyphID glyphID, const PairValueRecord *records,
        le_uint16 recordCount, le_uint16 recordSize) const;
};
LE_VAR_ARRAY(PairPositioningFormat1Subtable, pairSetTableOffsetArray)

// NOTE: ValueRecord has a variable size
struct Class2Record
{
    ValueRecord valueRecord1;
//  ValueRecord valurRecord2;
};

struct Class1Record
{
    Class2Record class2RecordArray[ANY_NUMBER];
};
LE_VAR_ARRAY(Class1Record, class2RecordArray)

struct PairPositioningFormat2Subtable : PairPositioningSubtable
{
    Offset       classDef1Offset;
    Offset       classDef2Offset;
    le_uint16    class1Count;
    le_uint16    class2Count;
    Class1Record class1RecordArray[ANY_NUMBER];

    le_uint32  process(const LEReferenceTo<PairPositioningFormat2Subtable> &base, GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode &success) const;
};
LE_VAR_ARRAY(PairPositioningFormat2Subtable, class1RecordArray)

U_NAMESPACE_END
#endif


