// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2009-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File spooftest.c
*
*********************************************************************************/
/*C API TEST for the uspoof Unicode Identifier Spoofing and Security API */
/**
*   This is an API test for ICU spoof detection in plain C.  It doesn't test very many cases, and doesn't
*   try to test the full functionality.  It just calls each function and verifies that it
*   works on a basic level.
*
*   More complete testing of spoof detection functionality is done with the C++ tests.
**/

#include "unicode/utypes.h"
#if !UCONFIG_NO_REGULAR_EXPRESSIONS && !UCONFIG_NO_NORMALIZATION

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unicode/uspoof.h"
#include "unicode/ustring.h"
#include "unicode/uset.h"
#include "cintltst.h"
#include "cmemory.h"

#define TEST_ASSERT_SUCCESS(status) UPRV_BLOCK_MACRO_BEGIN { \
    if (U_FAILURE(status)) { \
        log_err_status(status, "Failure at file %s, line %d, error = %s\n", __FILE__, __LINE__, u_errorName(status)); \
    } \
} UPRV_BLOCK_MACRO_END

#define TEST_ASSERT(expr) UPRV_BLOCK_MACRO_BEGIN { \
    if ((expr)==false) { \
        log_err("Test Failure at file %s, line %d: \"%s\" is false.\n", __FILE__, __LINE__, #expr); \
    } \
} UPRV_BLOCK_MACRO_END

#define TEST_ASSERT_EQ(a, b) UPRV_BLOCK_MACRO_BEGIN { \
    if ((a) != (b)) { \
        log_err("Test Failure at file %s, line %d: \"%s\" (%d) != \"%s\" (%d) \n", \
                __FILE__, __LINE__, #a, (a), #b, (b)); \
    } \
} UPRV_BLOCK_MACRO_END

#define TEST_ASSERT_NE(a, b) UPRV_BLOCK_MACRO_BEGIN { \
    if ((a) == (b)) { \
        log_err("Test Failure at file %s, line %d: \"%s\" (%d) == \"%s\" (%d) \n", \
                __FILE__, __LINE__, #a, (a), #b, (b)); \
    } \
} UPRV_BLOCK_MACRO_END


/*
 *   TEST_SETUP and TEST_TEARDOWN
 *         macros to handle the boilerplate around setting up test case.
 *         Put arbitrary test code between SETUP and TEARDOWN.
 *         "sc" is the ready-to-go  SpoofChecker for use in the tests.
 */
#define TEST_SETUP UPRV_BLOCK_MACRO_BEGIN { \
    UErrorCode status = U_ZERO_ERROR; \
    USpoofChecker *sc;     \
    sc = uspoof_open(&status);  \
    TEST_ASSERT_SUCCESS(status);   \
    if (U_SUCCESS(status)){

#define TEST_TEARDOWN  \
    }  \
    TEST_ASSERT_SUCCESS(status);  \
    uspoof_close(sc);  \
} UPRV_BLOCK_MACRO_END

static void TestOpenFromSource(void);
static void TestUSpoofCAPI(void);

void addUSpoofTest(TestNode** root);

void addUSpoofTest(TestNode** root)
{
#if !UCONFIG_NO_FILE_IO
    addTest(root, &TestOpenFromSource, "uspoof/TestOpenFromSource");
#endif
    addTest(root, &TestUSpoofCAPI, "uspoof/TestUSpoofCAPI");
}

/*
 *  Identifiers for verifying that spoof checking is minimally alive and working.
 */
const UChar goodLatin[] = {(UChar)0x75, (UChar)0x7a, 0};    /* "uz", all ASCII             */
                                                            /*   (not confusable)          */
const UChar scMixed[]  = {(UChar)0x73, (UChar)0x0441, 0};   /* "sc", with Cyrillic 'c'     */
                                                            /*   (mixed script, confusable */

const UChar scLatin[]  = {(UChar)0x73,  (UChar)0x63, 0};    /* "sc", plain ascii.        */
const UChar goodCyrl[] = {(UChar)0x438, (UChar)0x43B, 0};   /* Plain lower case Cyrillic letters,
                                                               no latin confusables         */
        
const UChar goodGreek[]   = {(UChar)0x3c0, (UChar)0x3c6, 0};   /* Plain lower case Greek letters */

const UChar lll_Latin_a[] = {(UChar)0x6c, (UChar)0x49, (UChar)0x31, 0};   /* lI1, all ASCII */

                             /*  Full-width I, Small Roman Numeral fifty, Latin Cap Letter IOTA*/
const UChar lll_Latin_b[] = {(UChar)0xff29, (UChar)0x217c, (UChar)0x196, 0};     

const UChar lll_Cyrl[]    = {(UChar)0x0406, (UChar)0x04C0, (UChar)0x31, 0};

/* The skeleton transform for all of these 'lll' lookalikes is all lower case l. */
const UChar lll_Skel[]    = {(UChar)0x6c, (UChar)0x6c, (UChar)0x6c, 0};  

const UChar han_Hiragana[] = {(UChar)0x3086, (UChar)0x308A, (UChar)0x0020, (UChar)0x77F3, (UChar)0x7530, 0};

/* Provide better code coverage */
const char goodLatinUTF8[]    = {0x75, 0x77, 0};

// Test open from source rules.
// Run this in isolation to verify initialization.
static void TestOpenFromSource(void) {
    // No TEST_SETUP because that calls uspoof_open().
    UErrorCode status = U_ZERO_ERROR;
    const char *dataSrcDir;
    char       *fileName;
    char       *confusables;
    int         confusablesLength = 0;
    char       *confusablesWholeScript;
    int         confusablesWholeScriptLength = 0;
    FILE       *f;
    UParseError pe;
    int32_t     errType;
    int32_t     checkResults;
    USpoofChecker *rsc;

    dataSrcDir = ctest_dataSrcDir();
    fileName = malloc(strlen(dataSrcDir) + 100);
    strcpy(fileName, dataSrcDir);
    strcat(fileName, U_FILE_SEP_STRING "unidata" U_FILE_SEP_STRING "confusables.txt");
    f = fopen(fileName, "rb");
    TEST_ASSERT_NE(f, NULL);
    confusables = malloc(3000000);
    if (f != NULL) {
        confusablesLength = (int)fread(confusables, 1, 3000000, f);
        fclose(f);
    }

    strcpy(fileName, dataSrcDir);
    strcat(fileName, U_FILE_SEP_STRING "unidata" U_FILE_SEP_STRING "confusablesWholeScript.txt");
    f = fopen(fileName, "rb");
    TEST_ASSERT_NE(f, NULL);
    confusablesWholeScript = malloc(1000000);
    if (f != NULL) {
        confusablesWholeScriptLength = (int)fread(confusablesWholeScript, 1, 1000000, f);
        fclose(f);
    }

    rsc = uspoof_openFromSource(confusables, confusablesLength,
                                confusablesWholeScript, confusablesWholeScriptLength,
                                &errType, &pe, &status);
    TEST_ASSERT_SUCCESS(status);

    // Ticket #11860: uspoof_openFromSource() did not initialize for use.
    // Verify that the spoof checker does not crash.
    checkResults = uspoof_check(rsc, goodLatin, -1, NULL, &status);
    TEST_ASSERT_SUCCESS(status);
    TEST_ASSERT_EQ(0, checkResults);

    free(confusablesWholeScript);
    free(confusables);
    free(fileName);
    uspoof_close(rsc);
    /*  printf("ParseError Line is %d\n", pe.line);  */
}

/*
 *   Spoof Detection C API Tests
 */
static void TestUSpoofCAPI(void) {

    /*
     *  basic uspoof_open().
     */
    {
        USpoofChecker *sc;
        UErrorCode  status = U_ZERO_ERROR;
        sc = uspoof_open(&status);
        TEST_ASSERT_SUCCESS(status);
        if (U_FAILURE(status)) {
            /* If things are so broken that we can't even open a default spoof checker,  */
            /*   don't even try the rest of the tests.  They would all fail.             */
            return;
        }
        uspoof_close(sc);
    }

    /*
     * openFromSerialized and serialize
    */
    TEST_SETUP
        int32_t        serializedSize = 0;
        int32_t        actualLength = 0;
        char           *buf;
        USpoofChecker  *sc2;
        int32_t         checkResults;

        
        serializedSize = uspoof_serialize(sc, NULL, 0, &status);
        TEST_ASSERT_EQ(status, U_BUFFER_OVERFLOW_ERROR);
        TEST_ASSERT(serializedSize > 0);

        /* Serialize the default spoof checker */
        status = U_ZERO_ERROR;
        buf = (char *)malloc(serializedSize + 10);
        TEST_ASSERT(buf != NULL);
        buf[serializedSize] = 42;
        uspoof_serialize(sc, buf, serializedSize, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(42, buf[serializedSize]);

        /* Create a new spoof checker from the freshly serialized data */
        sc2 = uspoof_openFromSerialized(buf, serializedSize+10, &actualLength, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_NE(NULL, sc2);
        TEST_ASSERT_EQ(serializedSize, actualLength);

        /* Verify that the new spoof checker at least wiggles */
        checkResults = uspoof_check(sc2, goodLatin, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);

        checkResults = uspoof_check(sc2, scMixed, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT, checkResults);

        uspoof_close(sc2);
        free(buf);
    TEST_TEARDOWN;
        
        
        
    /*
     * Set & Get Check Flags
    */
    TEST_SETUP
        int32_t t;
        uspoof_setChecks(sc, USPOOF_ALL_CHECKS, &status);
        TEST_ASSERT_SUCCESS(status);
        t = uspoof_getChecks(sc, &status);
        TEST_ASSERT_EQ(t, USPOOF_ALL_CHECKS);
    
        uspoof_setChecks(sc, 0, &status);
        TEST_ASSERT_SUCCESS(status);
        t = uspoof_getChecks(sc, &status);
        TEST_ASSERT_EQ(0, t);
        
        uspoof_setChecks(sc,
                        USPOOF_WHOLE_SCRIPT_CONFUSABLE | USPOOF_MIXED_SCRIPT_CONFUSABLE | USPOOF_ANY_CASE,
                        &status);
        TEST_ASSERT_SUCCESS(status);
        t = uspoof_getChecks(sc, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_WHOLE_SCRIPT_CONFUSABLE | USPOOF_MIXED_SCRIPT_CONFUSABLE | USPOOF_ANY_CASE, t);
    TEST_TEARDOWN;

    /*
    * get & setAllowedChars
    */
    TEST_SETUP
        USet *us;
        const USet *uset;

        uset = uspoof_getAllowedChars(sc, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(uset_isFrozen(uset));
        us = uset_open((UChar32)0x41, (UChar32)0x5A);   /*  [A-Z]  */
        uspoof_setAllowedChars(sc, us, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_NE(us, uspoof_getAllowedChars(sc, &status));
        TEST_ASSERT(uset_equals(us, uspoof_getAllowedChars(sc, &status)));
        TEST_ASSERT_SUCCESS(status);
        uset_close(us);
    TEST_TEARDOWN;

    /*
    *  clone()
    */

    TEST_SETUP
        USpoofChecker *clone1 = NULL;
        USpoofChecker *clone2 = NULL;
        int32_t        checkResults = 0;
        
        clone1 = uspoof_clone(sc, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_NE(clone1, sc);

        clone2 = uspoof_clone(clone1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_NE(clone2, clone1);

        uspoof_close(clone1);
        
        /* Verify that the cloned spoof checker is alive */
        checkResults = uspoof_check(clone2, goodLatin, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);

        checkResults = uspoof_check(clone2, scMixed, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT, checkResults);
        uspoof_close(clone2);
    TEST_TEARDOWN;

     /*
     *  basic uspoof_check()
     */
     TEST_SETUP
         int32_t result;
         result = uspoof_check(sc, goodLatin, -1, NULL, &status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT_EQ(0, result);

         result = uspoof_check(sc, han_Hiragana, -1, NULL, &status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT_EQ(0, result);

         result = uspoof_check(sc, scMixed, -1, NULL, &status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT, result);
     TEST_TEARDOWN;


    /*
     *  get & set Checks
    */
    TEST_SETUP
        int32_t   checks;
        int32_t   checks2;
        int32_t   checkResults;

        checks = uspoof_getChecks(sc, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_ALL_CHECKS, checks);

        checks &= ~(USPOOF_SINGLE_SCRIPT | USPOOF_MIXED_SCRIPT_CONFUSABLE);
        uspoof_setChecks(sc, checks, &status);
        TEST_ASSERT_SUCCESS(status);
        checks2 = uspoof_getChecks(sc, &status);
        TEST_ASSERT_EQ(checks, checks2);

        /* The checks that were disabled just above are the same ones that the "scMixed" test fails.
            So with those tests gone checking that Identifier should now succeed */
        checkResults = uspoof_check(sc, scMixed, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);
    TEST_TEARDOWN;
        
    /*
     *  AllowedLoacles
     */

    TEST_SETUP
        const char  *allowedLocales;
        int32_t  checkResults;

        /* Default allowed locales list should be empty */
        allowedLocales = uspoof_getAllowedLocales(sc, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(strcmp("", allowedLocales) == 0);

        /* Allow en and ru, which should enable Latin and Cyrillic only to pass */
        uspoof_setAllowedLocales(sc, "en, ru_RU", &status);
        TEST_ASSERT_SUCCESS(status);
        allowedLocales = uspoof_getAllowedLocales(sc, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(strstr(allowedLocales, "en") != NULL);
        TEST_ASSERT(strstr(allowedLocales, "ru") != NULL);

        /* Limit checks to USPOOF_CHAR_LIMIT.  Some of the test data has whole script confusables also,
         * which we don't want to see in this test. */
        uspoof_setChecks(sc, USPOOF_CHAR_LIMIT, &status);
        TEST_ASSERT_SUCCESS(status);

        checkResults = uspoof_check(sc, goodLatin, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);
        
        checkResults = uspoof_check(sc, goodGreek, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_CHAR_LIMIT, checkResults);

        checkResults = uspoof_check(sc, goodCyrl, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);

        /* Reset with an empty locale list, which should allow all characters to pass */
        uspoof_setAllowedLocales(sc, " ", &status);
        TEST_ASSERT_SUCCESS(status);

        checkResults = uspoof_check(sc, goodGreek, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);
    TEST_TEARDOWN;

    /*
     * AllowedChars   set/get the USet of allowed characters.
     */
    TEST_SETUP
        const USet  *set;
        USet        *tmpSet;
        int32_t      checkResults;
        
        /* By default, we should see no restriction; the USet should allow all characters. */
        set = uspoof_getAllowedChars(sc, &status);
        TEST_ASSERT_SUCCESS(status);
        tmpSet = uset_open(0, 0x10ffff);
        TEST_ASSERT(uset_equals(tmpSet, set));

        /* Setting the allowed chars should enable the check. */
        uspoof_setChecks(sc, USPOOF_ALL_CHECKS & ~USPOOF_CHAR_LIMIT, &status);
        TEST_ASSERT_SUCCESS(status);

        /* Remove a character that is in our good Latin test identifier from the allowed chars set. */
        uset_remove(tmpSet, goodLatin[1]);
        uspoof_setAllowedChars(sc, tmpSet, &status);
        TEST_ASSERT_SUCCESS(status);
        uset_close(tmpSet);

        /* Latin Identifier should now fail; other non-latin test cases should still be OK
         *  Note: fail of CHAR_LIMIT also causes the restriction level to be USPOOF_UNRESTRICTIVE
         *        which will give us a USPOOF_RESTRICTION_LEVEL failure.
         */
        checkResults = uspoof_check(sc, goodLatin, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_CHAR_LIMIT | USPOOF_RESTRICTION_LEVEL, checkResults);

        checkResults = uspoof_check(sc, goodGreek, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);
    TEST_TEARDOWN;

    /*
     * check UTF-8
     */
    TEST_SETUP
        char    utf8buf[200];
        int32_t checkResults, checkResults2;
        int32_t position;

        u_strToUTF8(utf8buf, sizeof(utf8buf), NULL, goodLatin, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        position = 666;
        checkResults = uspoof_checkUTF8(sc, utf8buf, -1, &position, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);
        TEST_ASSERT_EQ(0, position);

        u_strToUTF8(utf8buf, sizeof(utf8buf), NULL, goodCyrl, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        checkResults = uspoof_checkUTF8(sc, utf8buf, -1, &position, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);

        u_strToUTF8(utf8buf, sizeof(utf8buf), NULL, scMixed, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        position = 666;
        checkResults = uspoof_checkUTF8(sc, utf8buf, -1, &position, &status);
        checkResults2 = uspoof_check(sc, scMixed, -1, NULL, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT , checkResults);
        TEST_ASSERT_EQ(0, position);
        TEST_ASSERT_EQ(checkResults , checkResults2);

    TEST_TEARDOWN;

    /*
     * uspoof_check2 variants
     */
    TEST_SETUP
        int32_t result1, result2;
        char utf8buf[200];
        uspoof_setChecks(sc, USPOOF_ALL_CHECKS | USPOOF_AUX_INFO, &status);
        USpoofCheckResult* checkResult = uspoof_openCheckResult(&status);
        TEST_ASSERT_SUCCESS(status);

        const UChar* tests[] = { goodLatin, scMixed, scLatin,
                goodCyrl, goodGreek, lll_Latin_a, lll_Latin_b, han_Hiragana };

        for (int32_t i=0; i<UPRV_LENGTHOF(tests); i++) {
            const UChar* str = tests[i];

            // Basic test
            result1 = uspoof_check(sc, str, -1, NULL, &status);
            result2 = uspoof_check2(sc, str, -1, NULL, &status);
            TEST_ASSERT_SUCCESS(status);
            TEST_ASSERT_EQ(result1, result2);

            // With check result parameter
            result1 = uspoof_check(sc, str, -1, NULL, &status);
            result2 = uspoof_check2(sc, str, -1, checkResult, &status);
            TEST_ASSERT_SUCCESS(status);
            TEST_ASSERT_EQ(result1, result2);

            // Checks from checkResult should be same as those from bitmask
            TEST_ASSERT_EQ(result1 & USPOOF_ALL_CHECKS, uspoof_getCheckResultChecks(checkResult, &status));

            // Restriction level from checkResult should be same as that from bitmask
            URestrictionLevel restrictionLevel = uspoof_getCheckResultRestrictionLevel(checkResult, &status);
            TEST_ASSERT_EQ(result1 & restrictionLevel, restrictionLevel);

            // UTF8 endpoint
            u_strToUTF8(utf8buf, sizeof(utf8buf), NULL, goodLatin, -1, &status);
            TEST_ASSERT_SUCCESS(status);
            result1 = uspoof_checkUTF8(sc, utf8buf, -1, NULL, &status);
            result2 = uspoof_check2UTF8(sc, utf8buf, -1, NULL, &status);
            TEST_ASSERT_SUCCESS(status);
            TEST_ASSERT_EQ(result1, result2);
        }

        uspoof_closeCheckResult(checkResult);
    TEST_TEARDOWN;

    /*
     * uspoof_areConfusable()
     */
    TEST_SETUP
        int32_t  checkResults;
        
        checkResults = uspoof_areConfusable(sc, scLatin, -1, scMixed, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_MIXED_SCRIPT_CONFUSABLE, checkResults);

        checkResults = uspoof_areConfusable(sc, goodGreek, -1, scLatin, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);

        checkResults = uspoof_areConfusable(sc, lll_Latin_a, -1, lll_Latin_b, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT_CONFUSABLE, checkResults);

    TEST_TEARDOWN;

    /*
     * uspoof_areBidiConfusable()
     */
    TEST_SETUP
        int32_t checkResults;

        checkResults = uspoof_areBidiConfusable(sc, UBIDI_LTR, scLatin, -1, scMixed, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_MIXED_SCRIPT_CONFUSABLE, checkResults);

        checkResults = uspoof_areBidiConfusable(sc, UBIDI_LTR, goodGreek, -1, scLatin, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);

        checkResults = uspoof_areBidiConfusable(sc, UBIDI_LTR, lll_Latin_a, -1, lll_Latin_b, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT_CONFUSABLE, checkResults);

    TEST_TEARDOWN;

    /*
     * areConfusableUTF8
     */
    TEST_SETUP
        int32_t checkResults;
        char s1[200];
        char s2[200];


        u_strToUTF8(s1, sizeof(s1), NULL, scLatin, -1, &status);
        u_strToUTF8(s2, sizeof(s2), NULL, scMixed, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        checkResults = uspoof_areConfusableUTF8(sc, s1, -1, s2, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_MIXED_SCRIPT_CONFUSABLE, checkResults);

        u_strToUTF8(s1, sizeof(s1), NULL, goodGreek, -1, &status);
        u_strToUTF8(s2, sizeof(s2), NULL, scLatin, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        checkResults = uspoof_areConfusableUTF8(sc, s1, -1, s2, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);
        
        u_strToUTF8(s1, sizeof(s1), NULL, lll_Latin_a, -1, &status);
        u_strToUTF8(s2, sizeof(s2), NULL, lll_Latin_b, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        checkResults = uspoof_areConfusableUTF8(sc, s1, -1, s2, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT_CONFUSABLE, checkResults);

    TEST_TEARDOWN;

    /*
     * areBidiConfusableUTF8
     */
    TEST_SETUP
        int32_t checkResults;
        char s1[200];
        char s2[200];


        u_strToUTF8(s1, sizeof(s1), NULL, scLatin, -1, &status);
        u_strToUTF8(s2, sizeof(s2), NULL, scMixed, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        checkResults = uspoof_areBidiConfusableUTF8(sc, UBIDI_LTR, s1, -1, s2, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_MIXED_SCRIPT_CONFUSABLE, checkResults);

        u_strToUTF8(s1, sizeof(s1), NULL, goodGreek, -1, &status);
        u_strToUTF8(s2, sizeof(s2), NULL, scLatin, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        checkResults = uspoof_areBidiConfusableUTF8(sc, UBIDI_LTR, s1, -1, s2, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, checkResults);

        u_strToUTF8(s1, sizeof(s1), NULL, lll_Latin_a, -1, &status);
        u_strToUTF8(s2, sizeof(s2), NULL, lll_Latin_b, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        checkResults = uspoof_areBidiConfusableUTF8(sc, UBIDI_LTR, s1, -1, s2, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(USPOOF_SINGLE_SCRIPT_CONFUSABLE, checkResults);

    TEST_TEARDOWN;


  /*
   * getSkeleton
   */

    TEST_SETUP
        UChar dest[100];
        int32_t   skelLength;

        skelLength = uspoof_getSkeleton(sc, USPOOF_ANY_CASE, lll_Latin_a, -1, dest, UPRV_LENGTHOF(dest), &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, u_strcmp(lll_Skel, dest));
        TEST_ASSERT_EQ(u_strlen(lll_Skel), skelLength);

        skelLength = uspoof_getSkeletonUTF8(sc, USPOOF_ANY_CASE, goodLatinUTF8, -1, (char*)dest, 
                                            UPRV_LENGTHOF(dest), &status);
        TEST_ASSERT_SUCCESS(status);

        skelLength = uspoof_getSkeleton(sc, USPOOF_ANY_CASE, lll_Latin_a, -1, NULL, 0, &status);
        TEST_ASSERT_EQ(U_BUFFER_OVERFLOW_ERROR, status);
        TEST_ASSERT_EQ(3, skelLength);
        status = U_ZERO_ERROR;

    TEST_TEARDOWN;


    /*
     * getBidiSkeleton
     */

    TEST_SETUP
        UChar dest[100];
        int32_t   skelLength;

        skelLength = uspoof_getBidiSkeleton(sc, UBIDI_LTR, lll_Latin_a, -1, dest, UPRV_LENGTHOF(dest), &status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(0, u_strcmp(lll_Skel, dest));
        TEST_ASSERT_EQ(u_strlen(lll_Skel), skelLength);

        skelLength = uspoof_getBidiSkeletonUTF8(sc, UBIDI_LTR, goodLatinUTF8, -1, (char *)dest,
                                                UPRV_LENGTHOF(dest), &status);
        TEST_ASSERT_SUCCESS(status);

        skelLength = uspoof_getBidiSkeleton(sc, UBIDI_LTR, lll_Latin_a, -1, NULL, 0, &status);
        TEST_ASSERT_EQ(U_BUFFER_OVERFLOW_ERROR, status);
        TEST_ASSERT_EQ(3, skelLength);
        status = U_ZERO_ERROR;

    TEST_TEARDOWN;

    /*
     * get Inclusion and Recommended sets
     */
    TEST_SETUP
        const USet *inclusions = NULL;
        const USet *recommended = NULL;

        inclusions = uspoof_getInclusionSet(&status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(true, uset_isFrozen(inclusions));

        status = U_ZERO_ERROR;
        recommended = uspoof_getRecommendedSet(&status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT_EQ(true, uset_isFrozen(recommended));
    TEST_TEARDOWN;

}

#endif  /* UCONFIG_NO_REGULAR_EXPRESSIONS */
