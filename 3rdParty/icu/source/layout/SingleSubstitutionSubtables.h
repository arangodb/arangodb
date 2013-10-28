/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __SINGLESUBSTITUTIONSUBTABLES_H
#define __SINGLESUBSTITUTIONSUBTABLES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "LEGlyphFilter.h"
#include "OpenTypeTables.h"
#include "GlyphSubstitutionTables.h"
#include "GlyphIterator.h"

U_NAMESPACE_BEGIN

struct SingleSubstitutionSubtable : GlyphSubstitutionSubtable
{
    le_uint32  process(const LEReferenceTo<SingleSubstitutionSubtable> &base, GlyphIterator *glyphIterator, LEErrorCode &success, const LEGlyphFilter *filter = NULL) const;
};

struct SingleSubstitutionFormat1Subtable : SingleSubstitutionSubtable
{
    le_int16   deltaGlyphID;

    le_uint32  process(const LEReferenceTo<SingleSubstitutionFormat1Subtable> &base, GlyphIterator *glyphIterator, LEErrorCode &success, const LEGlyphFilter *filter = NULL) const;
};

struct SingleSubstitutionFormat2Subtable : SingleSubstitutionSubtable
{
    le_uint16  glyphCount;
    TTGlyphID  substituteArray[ANY_NUMBER];

    le_uint32  process(const LEReferenceTo<SingleSubstitutionFormat2Subtable> &base, GlyphIterator *glyphIterator, LEErrorCode &success, const LEGlyphFilter *filter = NULL) const;
};
LE_VAR_ARRAY(SingleSubstitutionFormat2Subtable, substituteArray)

U_NAMESPACE_END
#endif


