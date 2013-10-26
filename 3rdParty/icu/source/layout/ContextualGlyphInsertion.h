/**
 *
 * (C) Copyright IBM Corp. and Others 1998-2013 - All Rights Reserved
 *
 */

#ifndef __CONTEXTUALGLYPHINSERTION_H
#define __CONTEXTUALGLYPHINSERTION_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "LayoutTables.h"
#include "StateTables.h"
#include "MorphTables.h"
#include "MorphStateTables.h"

U_NAMESPACE_BEGIN

struct ContextualGlyphInsertionHeader : MorphStateTableHeader
{
};

struct ContextualGlyphInsertionHeader2 : MorphStateTableHeader2
{
    le_uint32 insertionTableOffset;
};

enum ContextualGlyphInsertionFlags
{
    cgiSetMark                  = 0x8000,
    cgiDontAdvance              = 0x4000,
    cgiCurrentIsKashidaLike     = 0x2000,
    cgiMarkedIsKashidaLike      = 0x1000,
    cgiCurrentInsertBefore      = 0x0800,
    cgiMarkInsertBefore         = 0x0400,
    cgiCurrentInsertCountMask   = 0x03E0,
    cgiMarkedInsertCountMask    = 0x001F
};

struct ContextualGlyphInsertionStateEntry : StateEntry
{
    ByteOffset currentInsertionListOffset;
    ByteOffset markedInsertionListOffset;
};

struct ContextualGlyphInsertionStateEntry2 : StateEntry2
{
    le_uint16 currentInsertionListIndex;
    le_uint16 markedInsertionListIndex;
};

U_NAMESPACE_END
#endif
