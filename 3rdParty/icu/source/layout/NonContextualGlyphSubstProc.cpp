/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "NonContextualGlyphSubst.h"
#include "NonContextualGlyphSubstProc.h"
#include "SimpleArrayProcessor.h"
#include "SegmentSingleProcessor.h"
#include "SegmentArrayProcessor.h"
#include "SingleTableProcessor.h"
#include "TrimmedArrayProcessor.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

NonContextualGlyphSubstitutionProcessor::NonContextualGlyphSubstitutionProcessor()
{
}

NonContextualGlyphSubstitutionProcessor::NonContextualGlyphSubstitutionProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success)
  : SubtableProcessor(morphSubtableHeader, success)
{
}

NonContextualGlyphSubstitutionProcessor::~NonContextualGlyphSubstitutionProcessor()
{
}

SubtableProcessor *NonContextualGlyphSubstitutionProcessor::createInstance(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success)
{
  LEReferenceTo<NonContextualGlyphSubstitutionHeader> header(morphSubtableHeader, success);

  if(LE_FAILURE(success)) return NULL;

  switch (SWAPW(header->table.format)) {
    case ltfSimpleArray:
      return new SimpleArrayProcessor(morphSubtableHeader, success);

    case ltfSegmentSingle:
      return new SegmentSingleProcessor(morphSubtableHeader, success);

    case ltfSegmentArray:
      return new SegmentArrayProcessor(morphSubtableHeader, success);

    case ltfSingleTable:
      return new SingleTableProcessor(morphSubtableHeader, success);

    case ltfTrimmedArray:
      return new TrimmedArrayProcessor(morphSubtableHeader, success);

    default:
        return NULL;
    }
}

U_NAMESPACE_END
