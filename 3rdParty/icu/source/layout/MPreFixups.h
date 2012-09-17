/*
 *
 * (C) Copyright IBM Corp. 2002-2008 - All Rights Reserved
 *
 */

#ifndef __MPREFIXUPS_H
#define __MPREFIXUPS_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

// Might want to make this a private member...
struct FixupData;

class MPreFixups : public UMemory
{
public:
    MPreFixups(le_int32 charCount);
   ~MPreFixups();

    void add(le_int32 baseIndex, le_int32 mpreIndex);
    
    void apply(LEGlyphStorage &glyphStorage, LEErrorCode& success);

private:
    FixupData *fFixupData;
    le_int32   fFixupCount;
};

U_NAMESPACE_END
#endif


