/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "OpenTypeTables.h"
#include "ScriptAndLanguage.h"
#include "GlyphLookupTables.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

le_bool GlyphLookupTableHeader::coversScript(const LETableReference &base, LETag scriptTag, LEErrorCode &success) const
{
  LEReferenceTo<ScriptListTable> scriptListTable(base, success, SWAPW(scriptListOffset));

  return (scriptListOffset != 0) && scriptListTable->findScript(scriptListTable, scriptTag, success) .isValid();
}

le_bool GlyphLookupTableHeader::coversScriptAndLanguage(const LETableReference &base, LETag scriptTag, LETag languageTag, LEErrorCode &success, le_bool exactMatch) const
{
  LEReferenceTo<ScriptListTable> scriptListTable(base, success, SWAPW(scriptListOffset));
  LEReferenceTo<LangSysTable> langSysTable = scriptListTable->findLanguage(scriptListTable,
                                    scriptTag, languageTag, success, exactMatch);

    // FIXME: could check featureListOffset, lookupListOffset, and lookup count...
    // Note: don't have to SWAPW langSysTable->featureCount to check for non-zero.
  return LE_SUCCESS(success)&&langSysTable.isValid() && langSysTable->featureCount != 0;
}

U_NAMESPACE_END
