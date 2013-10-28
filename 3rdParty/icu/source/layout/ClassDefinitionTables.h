/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __CLASSDEFINITIONTABLES_H
#define __CLASSDEFINITIONTABLES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "OpenTypeTables.h"

U_NAMESPACE_BEGIN

struct ClassDefinitionTable
{
    le_uint16 classFormat;

    le_int32  getGlyphClass(const LETableReference &base, LEGlyphID glyphID, LEErrorCode &success) const;
    le_bool   hasGlyphClass(const LETableReference &base, le_int32 glyphClass, LEErrorCode &success) const;

  le_int32 getGlyphClass(LEGlyphID glyphID) const {
    LETableReference base((const le_uint8*)this);
    LEErrorCode ignored = LE_NO_ERROR;
    return getGlyphClass(base,glyphID,ignored);
  }

  le_bool hasGlyphClass(le_int32 glyphClass) const {
    LETableReference base((const le_uint8*)this);
    LEErrorCode ignored = LE_NO_ERROR;
    return hasGlyphClass(base,glyphClass,ignored);
  }
};

struct ClassDefFormat1Table : ClassDefinitionTable
{
    TTGlyphID  startGlyph;
    le_uint16  glyphCount;
    le_uint16  classValueArray[ANY_NUMBER];

    le_int32 getGlyphClass(const LETableReference &base, LEGlyphID glyphID, LEErrorCode &success) const;
    le_bool  hasGlyphClass(const LETableReference &base, le_int32 glyphClass, LEErrorCode &success) const;
};
LE_VAR_ARRAY(ClassDefFormat1Table, classValueArray)


struct ClassRangeRecord
{
    TTGlyphID start;
    TTGlyphID end;
    le_uint16 classValue;
};

struct ClassDefFormat2Table : ClassDefinitionTable
{
    le_uint16        classRangeCount;
    GlyphRangeRecord classRangeRecordArray[ANY_NUMBER];

    le_int32 getGlyphClass(const LETableReference &base, LEGlyphID glyphID, LEErrorCode &success) const;
    le_bool hasGlyphClass(const LETableReference &base, le_int32 glyphClass, LEErrorCode &success) const;
};
LE_VAR_ARRAY(ClassDefFormat2Table, classRangeRecordArray)

U_NAMESPACE_END
#endif
