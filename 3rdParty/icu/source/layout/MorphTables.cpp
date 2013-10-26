/*
 * %W% %W%
 *
 * (C) Copyright IBM Corp. 1998 - 2013 - All Rights Reserved
 *
 */


#include "LETypes.h"
#include "LayoutTables.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "IndicRearrangementProcessor.h"
#include "ContextualGlyphSubstProc.h"
#include "LigatureSubstProc.h"
#include "NonContextualGlyphSubstProc.h"
//#include "ContextualGlyphInsertionProcessor.h"
#include "LEGlyphStorage.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

void MorphTableHeader::process(const LETableReference &base, LEGlyphStorage &glyphStorage, LEErrorCode &success) const
{
  le_uint32 chainCount = SWAPL(this->nChains);
  LEReferenceTo<ChainHeader> chainHeader(base, success, chains); // moving header
    LEReferenceToArrayOf<ChainHeader> chainHeaderArray(base, success, chains, chainCount);
    le_uint32 chain;

    for (chain = 0; LE_SUCCESS(success) && (chain < chainCount); chain += 1) {
        FeatureFlags defaultFlags = SWAPL(chainHeader->defaultFlags);
        le_uint32 chainLength = SWAPL(chainHeader->chainLength);
        le_int16 nFeatureEntries = SWAPW(chainHeader->nFeatureEntries);
        le_int16 nSubtables = SWAPW(chainHeader->nSubtables);
        LEReferenceTo<MorphSubtableHeader> subtableHeader =
          LEReferenceTo<MorphSubtableHeader>(chainHeader,success, &(chainHeader->featureTable[nFeatureEntries]));
        le_int16 subtable;

        for (subtable = 0; LE_SUCCESS(success) && (subtable < nSubtables); subtable += 1) {
            le_int16 length = SWAPW(subtableHeader->length);
            SubtableCoverage coverage = SWAPW(subtableHeader->coverage);
            FeatureFlags subtableFeatures = SWAPL(subtableHeader->subtableFeatures);

            // should check coverage more carefully...
            if ((coverage & scfVertical) == 0 && (subtableFeatures & defaultFlags) != 0  && LE_SUCCESS(success)) {
              subtableHeader->process(subtableHeader, glyphStorage, success);
            }

            subtableHeader.addOffset(length, success);
        }
        chainHeader.addOffset(chainLength, success);
    }
}

void MorphSubtableHeader::process(const LEReferenceTo<MorphSubtableHeader> &base, LEGlyphStorage &glyphStorage, LEErrorCode &success) const
{
    SubtableProcessor *processor = NULL;

    switch (SWAPW(coverage) & scfTypeMask)
    {
    case mstIndicRearrangement:
      processor = new IndicRearrangementProcessor(base, success);
        break;

    case mstContextualGlyphSubstitution:
      processor = new ContextualGlyphSubstitutionProcessor(base, success);
        break;

    case mstLigatureSubstitution:
      processor = new LigatureSubstitutionProcessor(base, success);
        break;

    case mstReservedUnused:
        break;

    case mstNonContextualGlyphSubstitution:
      processor = NonContextualGlyphSubstitutionProcessor::createInstance(base, success);
        break;

    /*
    case mstContextualGlyphInsertion:
        processor = new ContextualGlyphInsertionProcessor(this);
        break;
    */

    default:
        break;
    }

    if (processor != NULL) {
      if(LE_SUCCESS(success)) {
        processor->process(glyphStorage, success);
      }
      delete processor;
    }
}

U_NAMESPACE_END
