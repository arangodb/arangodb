/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __SCRIPTANDLANGUAGE_H
#define __SCRIPTANDLANGUAGE_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "OpenTypeTables.h"

U_NAMESPACE_BEGIN

typedef TagAndOffsetRecord LangSysRecord;

struct LangSysTable
{
    Offset    lookupOrderOffset;
    le_uint16 reqFeatureIndex;
    le_uint16 featureCount;
    le_uint16 featureIndexArray[ANY_NUMBER];
};
LE_VAR_ARRAY(LangSysTable, featureIndexArray)

struct ScriptTable
{
    Offset              defaultLangSysTableOffset;
    le_uint16           langSysCount;
    LangSysRecord       langSysRecordArray[ANY_NUMBER];

  LEReferenceTo<LangSysTable>  findLanguage(const LETableReference &base, LETag languageTag, LEErrorCode &success, le_bool exactMatch = FALSE) const;
};
LE_VAR_ARRAY(ScriptTable, langSysRecordArray)

typedef TagAndOffsetRecord ScriptRecord;

struct ScriptListTable
{
    le_uint16           scriptCount;
    ScriptRecord        scriptRecordArray[ANY_NUMBER];

  LEReferenceTo<ScriptTable>   findScript(const LETableReference &base, LETag scriptTag, LEErrorCode &success) const;
  LEReferenceTo<LangSysTable>  findLanguage(const LETableReference &base, LETag scriptTag, LETag languageTag, LEErrorCode &success, le_bool exactMatch = FALSE) const;
};
LE_VAR_ARRAY(ScriptListTable, scriptRecordArray)

U_NAMESPACE_END
#endif

