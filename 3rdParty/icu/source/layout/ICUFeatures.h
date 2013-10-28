/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __ICUFEATURES_H
#define __ICUFEATURES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "OpenTypeTables.h"

U_NAMESPACE_BEGIN

struct FeatureRecord
{
    ATag        featureTag;
    Offset      featureTableOffset;
};

struct FeatureTable
{
    Offset      featureParamsOffset;
    le_uint16   lookupCount;
    le_uint16   lookupListIndexArray[ANY_NUMBER];
};
LE_VAR_ARRAY(FeatureTable, lookupListIndexArray)

struct FeatureListTable
{
    le_uint16           featureCount;
    FeatureRecord       featureRecordArray[ANY_NUMBER];

  LEReferenceTo<FeatureTable>  getFeatureTable(const LETableReference &base, le_uint16 featureIndex, LETag *featureTag, LEErrorCode &success) const;

#if 0
  const LEReferenceTo<FeatureTable>  getFeatureTable(const LETableReference &base, LETag featureTag, LEErrorCode &success) const;
#endif
};

LE_VAR_ARRAY(FeatureListTable, featureRecordArray)

U_NAMESPACE_END
#endif
