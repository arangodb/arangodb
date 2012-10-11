/*
********************************************************************************
*   Copyright (C) 2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************/

#ifndef DCFMTIMP_H
#define DCFMTIMP_H

#include "unicode/utypes.h"


#if UCONFIG_FORMAT_FASTPATHS_49

U_NAMESPACE_BEGIN

enum EDecimalFormatFastpathStatus {
  kFastpathNO = 0,
  kFastpathYES = 1,
  kFastpathUNKNOWN = 2 /* not yet set */
};

/**
 * Must be smaller than DecimalFormat::fReserved
 */
struct DecimalFormatInternal {
  uint8_t    fFastpathStatus;
  
#ifdef FMT_DEBUG
  void dump() const {
    printf("DecimalFormatInternal: fFastpathStatus=%c\n",
           "NY?"[(int)fFastpathStatus&3]);
  }
#endif  
};



U_NAMESPACE_END

#endif

#endif
