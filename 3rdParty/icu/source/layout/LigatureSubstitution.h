/*
 *
 * (C) Copyright IBM Corp. and Others 1998-2013 - All Rights Reserved
 *
 */

#ifndef __LIGATURESUBSTITUTION_H
#define __LIGATURESUBSTITUTION_H

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

struct LigatureSubstitutionHeader : MorphStateTableHeader
{
    ByteOffset ligatureActionTableOffset;
    ByteOffset componentTableOffset;
    ByteOffset ligatureTableOffset;
};

struct LigatureSubstitutionHeader2 : MorphStateTableHeader2
{
    le_uint32 ligActionOffset;
    le_uint32 componentOffset;
    le_uint32 ligatureOffset;
};

enum LigatureSubstitutionFlags
{
    lsfSetComponent     = 0x8000,
    lsfDontAdvance      = 0x4000,
    lsfActionOffsetMask = 0x3FFF, // N/A in morx
    lsfPerformAction    = 0x2000
};

struct LigatureSubstitutionStateEntry : StateEntry
{
};

struct LigatureSubstitutionStateEntry2
{
    le_uint16 nextStateIndex;
    le_uint16 entryFlags;
    le_uint16 ligActionIndex;
};

typedef le_uint32 LigatureActionEntry;

enum LigatureActionFlags
{
    lafLast                 = 0x80000000,
    lafStore                = 0x40000000,
    lafComponentOffsetMask  = 0x3FFFFFFF
};

U_NAMESPACE_END
#endif
