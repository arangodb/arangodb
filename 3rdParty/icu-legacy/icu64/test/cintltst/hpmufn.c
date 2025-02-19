// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2003-2015, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/*
* File hpmufn.c
*
*/

#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "unicode/uclean.h"
#include "unicode/uchar.h"
#include "unicode/ures.h"
#include "cintltst.h"
#include "unicode/utrace.h"
#include <stdlib.h>
#include <string.h>

/**
 * This should align the memory properly on any machine.
 */
typedef union {
    long    t1;
    double  t2;
    void   *t3;
} ctest_AlignedMemory;

static void TestHeapFunctions(void);

void addHeapMutexTest(TestNode **root);


void
addHeapMutexTest(TestNode** root)
{
    addTest(root, &TestHeapFunctions,       "hpmufn/TestHeapFunctions"  );
}

static int32_t gMutexFailures = 0;

#define TEST_STATUS(status, expected) \
if (status != expected) { \
log_err_status(status, "FAIL at  %s:%d. Actual status = \"%s\";  Expected status = \"%s\"\n", \
  __FILE__, __LINE__, u_errorName(status), u_errorName(expected)); gMutexFailures++; }


#define TEST_ASSERT(expr) \
if (!(expr)) { \
    log_err("FAILED Assertion \"" #expr "\" at  %s:%d.\n", __FILE__, __LINE__); \
    gMutexFailures++; \
}


/*  These tests do cleanup and reinitialize ICU in the course of their operation.
 *    The ICU data directory must be preserved across these operations.
 *    Here is a helper function to assist with that.
 */
static char *safeGetICUDataDirectory() {
    const char *dataDir = u_getDataDirectory();  /* Returned string vanashes with u_cleanup */
    char *retStr = NULL;
    if (dataDir != NULL) {
        retStr = (char *)malloc(strlen(dataDir)+1);
        strcpy(retStr, dataDir);
    }
    return retStr;
}


    
/*
 *  Test Heap Functions.
 *    Implemented on top of the standard malloc heap.
 *    All blocks increased in size by 8 to 16 bytes, and the poiner returned to ICU is
 *       offset up by 8 to 16, which should cause a good heap corruption if one of our "blocks"
 *       ends up being freed directly, without coming through us.
 *    Allocations are counted, to check that ICU actually does call back to us.
 */
int    gBlockCount = 0;
const void  *gContext;

static void * U_CALLCONV myMemAlloc(const void *context, size_t size) {
    char *retPtr = (char *)malloc(size+sizeof(ctest_AlignedMemory));
    if (retPtr != NULL) {
        retPtr += sizeof(ctest_AlignedMemory);
    }
    gBlockCount ++;
    return retPtr;
}

static void U_CALLCONV myMemFree(const void *context, void *mem) {
    char *freePtr = (char *)mem;
    if (freePtr != NULL) {
        freePtr -= sizeof(ctest_AlignedMemory);
    }
    free(freePtr);
}



static void * U_CALLCONV myMemRealloc(const void *context, void *mem, size_t size) {
    char *p = (char *)mem;
    char *retPtr;

    if (p!=NULL) {
        p -= sizeof(ctest_AlignedMemory);
    }
    retPtr = realloc(p, size+sizeof(ctest_AlignedMemory));
    if (retPtr != NULL) {
        p += sizeof(ctest_AlignedMemory);
    }
    return retPtr;
}


static void TestHeapFunctions() {
    UErrorCode       status = U_ZERO_ERROR;
    UResourceBundle *rb     = NULL;
    char            *icuDataDir;
    UVersionInfo unicodeVersion = {0,0,0,0};

    icuDataDir = safeGetICUDataDirectory();   /* save icu data dir, so we can put it back
                                               *  after doing u_cleanup().                */


    /* Verify that ICU can be cleaned up and reinitialized successfully.
     *  Failure here usually means that some ICU service didn't clean up successfully,
     *  probably because some earlier test accidently left something open. */
    ctest_resetICU();

    /* Un-initialize ICU */
    u_cleanup();

    /* Can not set memory functions with NULL values */
    status = U_ZERO_ERROR;
    u_setMemoryFunctions(&gContext, NULL, myMemRealloc, myMemFree, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;
    u_setMemoryFunctions(&gContext, myMemAlloc, NULL, myMemFree, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);
    status = U_ZERO_ERROR;
    u_setMemoryFunctions(&gContext, myMemAlloc, myMemRealloc, NULL, &status);
    TEST_STATUS(status, U_ILLEGAL_ARGUMENT_ERROR);

    /* u_setMemoryFunctions() should work with null or non-null context pointer */
    status = U_ZERO_ERROR;
    u_setMemoryFunctions(NULL, myMemAlloc, myMemRealloc, myMemFree, &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    u_setMemoryFunctions(&gContext, myMemAlloc, myMemRealloc, myMemFree, &status);
    TEST_STATUS(status, U_ZERO_ERROR);


    /* After reinitializing ICU, we can not set the memory funcs again. */
    status = U_ZERO_ERROR;
    u_setDataDirectory(icuDataDir);
    u_init(&status);
    TEST_STATUS(status, U_ZERO_ERROR);

    /* Doing ICU operations should cause allocations to come through our test heap */
    gBlockCount = 0;
    status = U_ZERO_ERROR;
    rb = ures_open(NULL, "es", &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    if (gBlockCount == 0) {
        log_err("Heap functions are not being called from ICU.\n");
    }
    ures_close(rb);

    /* Cleanup should put the heap back to its default implementation. */
    ctest_resetICU();
    u_getUnicodeVersion(unicodeVersion);
    if (unicodeVersion[0] <= 0) {
        log_err("Properties doesn't reinitialize without u_init.\n");
    }
    status = U_ZERO_ERROR;
    u_init(&status);
    TEST_STATUS(status, U_ZERO_ERROR);

    /* ICU operations should no longer cause allocations to come through our test heap */
    gBlockCount = 0;
    status = U_ZERO_ERROR;
    rb = ures_open(NULL, "fr", &status);
    TEST_STATUS(status, U_ZERO_ERROR);
    if (gBlockCount != 0) {
        log_err("Heap functions did not reset after u_cleanup.\n");
    }
    ures_close(rb);
    free(icuDataDir);

    ctest_resetICU();
}


