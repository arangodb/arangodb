/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __NONCONTEXTUALGLYPHSUBSTITUTIONPROCESSOR_H
#define __NONCONTEXTUALGLYPHSUBSTITUTIONPROCESSOR_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "NonContextualGlyphSubst.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class NonContextualGlyphSubstitutionProcessor : public SubtableProcessor
{
public:
  virtual void process(LEGlyphStorage &glyphStorage, LEErrorCode &success) = 0;

    static SubtableProcessor *createInstance(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success);

protected:
    NonContextualGlyphSubstitutionProcessor();
    NonContextualGlyphSubstitutionProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &status);

    virtual ~NonContextualGlyphSubstitutionProcessor();

private:
    NonContextualGlyphSubstitutionProcessor(const NonContextualGlyphSubstitutionProcessor &other); // forbid copying of this class
    NonContextualGlyphSubstitutionProcessor &operator=(const NonContextualGlyphSubstitutionProcessor &other); // forbid copying of this class
};

U_NAMESPACE_END
#endif
