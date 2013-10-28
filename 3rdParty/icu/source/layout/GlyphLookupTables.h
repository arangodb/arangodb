/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __GLYPHLOOKUPTABLES_H
#define __GLYPHLOOKUPTABLES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "OpenTypeTables.h"

U_NAMESPACE_BEGIN

struct GlyphLookupTableHeader
{
    fixed32 version;
    Offset  scriptListOffset;
    Offset  featureListOffset;
    Offset  lookupListOffset;

  le_bool coversScript(const LETableReference &base, LETag scriptTag, LEErrorCode &success) const;
  le_bool coversScriptAndLanguage(const LETableReference &base, LETag scriptTag, LETag languageTag, LEErrorCode &success, le_bool exactMatch = FALSE) const;
};

U_NAMESPACE_END

#endif

