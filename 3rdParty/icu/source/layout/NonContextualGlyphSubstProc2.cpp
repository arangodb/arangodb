/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor2.h"
#include "NonContextualGlyphSubst.h"
#include "NonContextualGlyphSubstProc2.h"
#include "SimpleArrayProcessor2.h"
#include "SegmentSingleProcessor2.h"
#include "SegmentArrayProcessor2.h"
#include "SingleTableProcessor2.h"
#include "TrimmedArrayProcessor2.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

NonContextualGlyphSubstitutionProcessor2::NonContextualGlyphSubstitutionProcessor2()
{
}

NonContextualGlyphSubstitutionProcessor2::NonContextualGlyphSubstitutionProcessor2(
     const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success)
  : SubtableProcessor2(morphSubtableHeader, success)
{
}

NonContextualGlyphSubstitutionProcessor2::~NonContextualGlyphSubstitutionProcessor2()
{
}

SubtableProcessor2 *NonContextualGlyphSubstitutionProcessor2::createInstance(
      const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success)
{
    const LEReferenceTo<NonContextualGlyphSubstitutionHeader2> header(morphSubtableHeader, success);
    if(LE_FAILURE(success)) return NULL;

    switch (SWAPW(header->table.format))
    {
    case ltfSimpleArray:
      return new SimpleArrayProcessor2(morphSubtableHeader, success);

    case ltfSegmentSingle:
      return new SegmentSingleProcessor2(morphSubtableHeader, success);

    case ltfSegmentArray:
      return new SegmentArrayProcessor2(morphSubtableHeader, success);

    case ltfSingleTable:
      return new SingleTableProcessor2(morphSubtableHeader, success);

    case ltfTrimmedArray:
      return new TrimmedArrayProcessor2(morphSubtableHeader, success);

    default:
        return NULL;
    }
}

U_NAMESPACE_END
