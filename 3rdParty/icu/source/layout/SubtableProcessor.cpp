/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

SubtableProcessor::SubtableProcessor()
{
}

SubtableProcessor::SubtableProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success)
  : length(0), coverage(0), subtableFeatures(0L), subtableHeader(morphSubtableHeader)
{
  if(LE_FAILURE(success)) return;
    length = SWAPW(subtableHeader->length);
    coverage = SWAPW(subtableHeader->coverage);
    subtableFeatures = SWAPL(subtableHeader->subtableFeatures);
}

SubtableProcessor::~SubtableProcessor()
{
}

U_NAMESPACE_END
