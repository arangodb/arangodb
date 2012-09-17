/*
******************************************************************************
*                                                                            *
* Copyright (C) 2001-2011, International Business Machines                   *
*                Corporation and others. All Rights Reserved.                *
*                                                                            *
******************************************************************************
*   file name:  uinit.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001July05
*   created by: George Rhoten
*/

#include "unicode/utypes.h"
#include "unicode/icuplug.h"
#include "unicode/uclean.h"
#include "cmemory.h"
#include "icuplugimp.h"
#include "ucln.h"
#include "ucnv_io.h"
#include "umutex.h"
#include "utracimp.h"

/*
 * ICU Initialization Function. Need not be called.
 */
U_CAPI void U_EXPORT2
u_init(UErrorCode *status) {
    UTRACE_ENTRY_OC(UTRACE_U_INIT);
    /* initialize plugins */
    uplug_init(status);

    umtx_lock(&gICUInitMutex);
    if (gICUInitialized || U_FAILURE(*status)) {
        umtx_unlock(&gICUInitMutex);
        UTRACE_EXIT_STATUS(*status);
        return;
    }

    /*
     * 2005-may-02
     *
     * ICU4C 3.4 (jitterbug 4497) hardcodes the data for Unicode character
     * properties for APIs that want to be fast.
     * Therefore, we need not load them here nor check for errors.
     * Instead, we load the converter alias table to see if any ICU data
     * is available.
     * Users should really open the service objects they need and check
     * for errors there, to make sure that the actual items they need are
     * available.
     */
#if !UCONFIG_NO_CONVERSION
    ucnv_io_countKnownConverters(status);
#endif

    gICUInitialized = TRUE;    /* TODO:  don't set if U_FAILURE? */
    umtx_unlock(&gICUInitMutex);
    UTRACE_EXIT_STATUS(*status);
}
