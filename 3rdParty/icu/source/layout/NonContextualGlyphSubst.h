/*
 * (C) Copyright IBM Corp. and others 1998-2013 - All Rights Reserved
 *
 */

#ifndef __NONCONTEXTUALGLYPHSUBSTITUTION_H
#define __NONCONTEXTUALGLYPHSUBSTITUTION_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "LayoutTables.h"
#include "LookupTables.h"
#include "MorphTables.h"

U_NAMESPACE_BEGIN

struct NonContextualGlyphSubstitutionHeader : MorphSubtableHeader
{
    LookupTable table;
};

struct NonContextualGlyphSubstitutionHeader2 : MorphSubtableHeader2
{
    LookupTable table;
};

U_NAMESPACE_END
#endif

