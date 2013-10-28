/*
******************************************************************************
* Copyright (C) 2001-2013, International Business Machines
*                Corporation and others. All Rights Reserved.
******************************************************************************
*   file name:  ucln_cmn.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001July05
*   created by: George Rhoten
*/

#include "unicode/utypes.h"
#include "unicode/uclean.h"
#include "utracimp.h"
#include "ucln_cmn.h"
#include "cmutex.h"
#include "ucln.h"
#include "cmemory.h"
#include "uassert.h"

/**  Auto-client for UCLN_COMMON **/
#define UCLN_TYPE_IS_COMMON
#include "ucln_imp.h"

static cleanupFunc *gCommonCleanupFunctions[UCLN_COMMON_COUNT];
static cleanupFunc *gLibCleanupFunctions[UCLN_COMMON];


/************************************************
 The cleanup order is important in this function.
 Please be sure that you have read ucln.h
 ************************************************/
U_CAPI void U_EXPORT2
u_cleanup(void)
{
    UTRACE_ENTRY_OC(UTRACE_U_CLEANUP);
    umtx_lock(NULL);     /* Force a memory barrier, so that we are sure to see   */
    umtx_unlock(NULL);   /*   all state left around by any other threads.        */

    ucln_lib_cleanup();

    cmemory_cleanup();       /* undo any heap functions set by u_setMemoryFunctions(). */
    UTRACE_EXIT();           /* Must be before utrace_cleanup(), which turns off tracing. */
/*#if U_ENABLE_TRACING*/
    utrace_cleanup();
/*#endif*/
}

U_CAPI void U_EXPORT2 ucln_cleanupOne(ECleanupLibraryType libType) 
{
    if (gLibCleanupFunctions[libType])
    {
        gLibCleanupFunctions[libType]();
        gLibCleanupFunctions[libType] = NULL;
    }
}

U_CFUNC void
ucln_common_registerCleanup(ECleanupCommonType type,
                            cleanupFunc *func)
{
    U_ASSERT(UCLN_COMMON_START < type && type < UCLN_COMMON_COUNT);
    if (UCLN_COMMON_START < type && type < UCLN_COMMON_COUNT)
    {
        gCommonCleanupFunctions[type] = func;
    }
#if !UCLN_NO_AUTO_CLEANUP && (defined(UCLN_AUTO_ATEXIT) || defined(UCLN_AUTO_LOCAL))
    ucln_registerAutomaticCleanup();
#endif
}

U_CAPI void U_EXPORT2
ucln_registerCleanup(ECleanupLibraryType type,
                     cleanupFunc *func)
{
    U_ASSERT(UCLN_START < type && type < UCLN_COMMON);
    if (UCLN_START < type && type < UCLN_COMMON)
    {
        gLibCleanupFunctions[type] = func;
    }
}

U_CFUNC UBool ucln_lib_cleanup(void) {
    ECleanupLibraryType libType = UCLN_START;
    ECleanupCommonType commonFunc = UCLN_COMMON_START;

    for (libType++; libType<UCLN_COMMON; libType++) {
        ucln_cleanupOne(libType);
    }

    for (commonFunc++; commonFunc<UCLN_COMMON_COUNT; commonFunc++) {
        if (gCommonCleanupFunctions[commonFunc])
        {
            gCommonCleanupFunctions[commonFunc]();
            gCommonCleanupFunctions[commonFunc] = NULL;
        }
    }
#if !UCLN_NO_AUTO_CLEANUP && (defined(UCLN_AUTO_ATEXIT) || defined(UCLN_AUTO_LOCAL))
    ucln_unRegisterAutomaticCleanup();
#endif
    return TRUE;
}
