/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2013 - All Rights Reserved
 *
 */

#ifndef __SUBTABLEPROCESSOR2_H
#define __SUBTABLEPROCESSOR2_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class SubtableProcessor2 : public UMemory {
public:
    virtual void process(LEGlyphStorage &glyphStorage, LEErrorCode &success) = 0;
    virtual ~SubtableProcessor2();

protected:
    SubtableProcessor2(const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success);

    SubtableProcessor2();

    le_uint32 length;
    SubtableCoverage2 coverage;
    FeatureFlags subtableFeatures;

    const LEReferenceTo<MorphSubtableHeader2> subtableHeader;

private:

    SubtableProcessor2(const SubtableProcessor2 &other); // forbid copying of this class
    SubtableProcessor2 &operator=(const SubtableProcessor2 &other); // forbid copying of this class
};

U_NAMESPACE_END
#endif

