/*
******************************************************************************
*
*   Copyright (C) 2009-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*/


/**
 * \file
 * \brief C API: access to ICU Data Version number
 */

#ifndef __ICU_DATA_VER_H__
#define __ICU_DATA_VER_H__

#include "unicode/utypes.h"

#ifndef U_HIDE_DRAFT_API
/**
 * @draft ICU 49
 */
#define U_ICU_VERSION_BUNDLE "icuver"

/**
 * @draft ICU 49
 */
#define U_ICU_DATA_KEY "DataVersion"

/**
 * Retrieves the data version from icuver and stores it in dataVersionFillin.
 * 
 * @param dataVersionFillin icuver data version information to be filled in if not-null
 * @param status stores the error code from the calls to resource bundle
 * 
 * @draft ICU 49
 */
U_DRAFT void U_EXPORT2 u_getDataVersion(UVersionInfo dataVersionFillin, UErrorCode *status);
#endif  /* U_HIDE_DRAFT_API */

#endif
