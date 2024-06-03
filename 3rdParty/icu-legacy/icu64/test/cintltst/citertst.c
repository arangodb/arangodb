// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CITERTST.C
*
* Modification History:
* Date      Name               Description
*           Madhu Katragadda   Ported for C API
* 02/19/01  synwee             Modified test case for new collation iterator
*********************************************************************************/
/*
 * Collation Iterator tests.
 * (Let me reiterate my position...)
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "unicode/ucoleitr.h"
#include "unicode/uloc.h"
#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "unicode/putil.h"
#include "callcoll.h"
#include "cmemory.h"
#include "cintltst.h"
#include "citertst.h"
#include "ccolltst.h"
#include "filestrm.h"
#include "cstring.h"
#include "ucol_imp.h"
#include "uparse.h"
#include <stdio.h>

extern uint8_t ucol_uprv_getCaseBits(const UChar *, uint32_t, UErrorCode *);

void addCollIterTest(TestNode** root)
{
    addTest(root, &TestPrevious, "tscoll/citertst/TestPrevious");
    addTest(root, &TestOffset, "tscoll/citertst/TestOffset");
    addTest(root, &TestSetText, "tscoll/citertst/TestSetText");
    addTest(root, &TestMaxExpansion, "tscoll/citertst/TestMaxExpansion");
    addTest(root, &TestUnicodeChar, "tscoll/citertst/TestUnicodeChar");
    addTest(root, &TestNormalizedUnicodeChar,
                                "tscoll/citertst/TestNormalizedUnicodeChar");
    addTest(root, &TestNormalization, "tscoll/citertst/TestNormalization");
    addTest(root, &TestBug672, "tscoll/citertst/TestBug672");
    addTest(root, &TestBug672Normalize, "tscoll/citertst/TestBug672Normalize");
    addTest(root, &TestSmallBuffer, "tscoll/citertst/TestSmallBuffer");
    addTest(root, &TestDiscontiguos, "tscoll/citertst/TestDiscontiguos");
    addTest(root, &TestSearchCollatorElements, "tscoll/citertst/TestSearchCollatorElements");
}

/* The locales we support */

static const char * LOCALES[] = {"en_AU", "en_BE", "en_CA"};

static void TestBug672() {
    UErrorCode  status = U_ZERO_ERROR;
    UChar       pattern[20];
    UChar       text[50];
    int         i;
    int         result[3][3];

    u_uastrcpy(pattern, "resume");
    u_uastrcpy(text, "Time to resume updating my resume.");

    for (i = 0; i < 3; ++ i) {
        UCollator          *coll = ucol_open(LOCALES[i], &status);
        UCollationElements *pitr = ucol_openElements(coll, pattern, -1,
                                                     &status);
        UCollationElements *titer = ucol_openElements(coll, text, -1,
                                                     &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "ERROR: in creation of either the collator or the collation iterator :%s\n",
                    myErrorName(status));
            return;
        }

        log_verbose("locale tested %s\n", LOCALES[i]);

        while (ucol_next(pitr, &status) != UCOL_NULLORDER &&
               U_SUCCESS(status)) {
        }
        if (U_FAILURE(status)) {
            log_err("ERROR: reversing collation iterator :%s\n",
                    myErrorName(status));
            return;
        }
        ucol_reset(pitr);

        ucol_setOffset(titer, u_strlen(pattern), &status);
        if (U_FAILURE(status)) {
            log_err("ERROR: setting offset in collator :%s\n",
                    myErrorName(status));
            return;
        }
        result[i][0] = ucol_getOffset(titer);
        log_verbose("Text iterator set to offset %d\n", result[i][0]);

        /* Use previous() */
        ucol_previous(titer, &status);
        result[i][1] = ucol_getOffset(titer);
        log_verbose("Current offset %d after previous\n", result[i][1]);

        /* Add one to index */
        log_verbose("Adding one to current offset...\n");
        ucol_setOffset(titer, ucol_getOffset(titer) + 1, &status);
        if (U_FAILURE(status)) {
            log_err("ERROR: setting offset in collator :%s\n",
                    myErrorName(status));
            return;
        }
        result[i][2] = ucol_getOffset(titer);
        log_verbose("Current offset in text = %d\n", result[i][2]);
        ucol_closeElements(pitr);
        ucol_closeElements(titer);
        ucol_close(coll);
    }

    if (uprv_memcmp(result[0], result[1], 3) != 0 ||
        uprv_memcmp(result[1], result[2], 3) != 0) {
        log_err("ERROR: Different locales have different offsets at the same character\n");
    }
}



/*  Running this test with normalization enabled showed up a bug in the incremental
    normalization code. */
static void TestBug672Normalize() {
    UErrorCode  status = U_ZERO_ERROR;
    UChar       pattern[20];
    UChar       text[50];
    int         i;
    int         result[3][3];

    u_uastrcpy(pattern, "resume");
    u_uastrcpy(text, "Time to resume updating my resume.");

    for (i = 0; i < 3; ++ i) {
        UCollator          *coll = ucol_open(LOCALES[i], &status);
        UCollationElements *pitr = NULL;
        UCollationElements *titer = NULL;

        ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);

        pitr = ucol_openElements(coll, pattern, -1, &status);
        titer = ucol_openElements(coll, text, -1, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "ERROR: in creation of either the collator or the collation iterator :%s\n",
                    myErrorName(status));
            return;
        }

        log_verbose("locale tested %s\n", LOCALES[i]);

        while (ucol_next(pitr, &status) != UCOL_NULLORDER &&
               U_SUCCESS(status)) {
        }
        if (U_FAILURE(status)) {
            log_err("ERROR: reversing collation iterator :%s\n",
                    myErrorName(status));
            return;
        }
        ucol_reset(pitr);

        ucol_setOffset(titer, u_strlen(pattern), &status);
        if (U_FAILURE(status)) {
            log_err("ERROR: setting offset in collator :%s\n",
                    myErrorName(status));
            return;
        }
        result[i][0] = ucol_getOffset(titer);
        log_verbose("Text iterator set to offset %d\n", result[i][0]);

        /* Use previous() */
        ucol_previous(titer, &status);
        result[i][1] = ucol_getOffset(titer);
        log_verbose("Current offset %d after previous\n", result[i][1]);

        /* Add one to index */
        log_verbose("Adding one to current offset...\n");
        ucol_setOffset(titer, ucol_getOffset(titer) + 1, &status);
        if (U_FAILURE(status)) {
            log_err("ERROR: setting offset in collator :%s\n",
                    myErrorName(status));
            return;
        }
        result[i][2] = ucol_getOffset(titer);
        log_verbose("Current offset in text = %d\n", result[i][2]);
        ucol_closeElements(pitr);
        ucol_closeElements(titer);
        ucol_close(coll);
    }

    if (uprv_memcmp(result[0], result[1], 3) != 0 ||
        uprv_memcmp(result[1], result[2], 3) != 0) {
        log_err("ERROR: Different locales have different offsets at the same character\n");
    }
}




/**
 * Test for CollationElementIterator previous and next for the whole set of
 * unicode characters.
 */
static void TestUnicodeChar()
{
    UChar source[0x100];
    UCollator *en_us;
    UCollationElements *iter;
    UErrorCode status = U_ZERO_ERROR;
    UChar codepoint;

    UChar *test;
    en_us = ucol_open("en_US", &status);
    if (U_FAILURE(status)){
       log_err_status(status, "ERROR: in creation of collation data using ucol_open()\n %s\n",
              myErrorName(status));
       return;
    }

    for (codepoint = 1; codepoint < 0xFFFE;)
    {
      test = source;

      while (codepoint % 0xFF != 0)
      {
        if (u_isdefined(codepoint))
          *(test ++) = codepoint;
        codepoint ++;
      }

      if (u_isdefined(codepoint))
        *(test ++) = codepoint;

      if (codepoint != 0xFFFF)
        codepoint ++;

      *test = 0;
      iter=ucol_openElements(en_us, source, u_strlen(source), &status);
      if(U_FAILURE(status)){
          log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
              myErrorName(status));
          ucol_close(en_us);
          return;
      }
      /* A basic test to see if it's working at all */
      log_verbose("codepoint testing %x\n", codepoint);
      backAndForth(iter);
      ucol_closeElements(iter);

      /* null termination test */
      iter=ucol_openElements(en_us, source, -1, &status);
      if(U_FAILURE(status)){
          log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
              myErrorName(status));
          ucol_close(en_us);
          return;
      }
      /* A basic test to see if it's working at all */
      backAndForth(iter);
      ucol_closeElements(iter);
    }

    ucol_close(en_us);
}

/**
 * Test for CollationElementIterator previous and next for the whole set of
 * unicode characters with normalization on.
 */
static void TestNormalizedUnicodeChar()
{
    UChar source[0x100];
    UCollator *th_th;
    UCollationElements *iter;
    UErrorCode status = U_ZERO_ERROR;
    UChar codepoint;

    UChar *test;
    /* thai should have normalization on */
    th_th = ucol_open("th_TH", &status);
    if (U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of thai collation using ucol_open()\n %s\n",
              myErrorName(status));
        return;
    }

    for (codepoint = 1; codepoint < 0xFFFE;)
    {
      test = source;

      while (codepoint % 0xFF != 0)
      {
        if (u_isdefined(codepoint))
          *(test ++) = codepoint;
        codepoint ++;
      }

      if (u_isdefined(codepoint))
        *(test ++) = codepoint;

      if (codepoint != 0xFFFF)
        codepoint ++;

      *test = 0;
      iter=ucol_openElements(th_th, source, u_strlen(source), &status);
      if(U_FAILURE(status)){
          log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
              myErrorName(status));
            ucol_close(th_th);
          return;
      }

      backAndForth(iter);
      ucol_closeElements(iter);

      iter=ucol_openElements(th_th, source, -1, &status);
      if(U_FAILURE(status)){
          log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
              myErrorName(status));
            ucol_close(th_th);
          return;
      }

      backAndForth(iter);
      ucol_closeElements(iter);
    }

    ucol_close(th_th);
}

/**
* Test the incremental normalization
*/
static void TestNormalization()
{
          UErrorCode          status = U_ZERO_ERROR;
    const char               *str    =
                            "&a < \\u0300\\u0315 < A\\u0300\\u0315 < \\u0316\\u0315B < \\u0316\\u0300\\u0315";
          UCollator          *coll;
          UChar               rule[50];
          int                 rulelen = u_unescape(str, rule, 50);
          int                 count = 0;
    const char                *testdata[] =
                        {"\\u1ED9", "o\\u0323\\u0302",
                        "\\u0300\\u0315", "\\u0315\\u0300",
                        "A\\u0300\\u0315B", "A\\u0315\\u0300B",
                        "A\\u0316\\u0315B", "A\\u0315\\u0316B",
                        "\\u0316\\u0300\\u0315", "\\u0315\\u0300\\u0316",
                        "A\\u0316\\u0300\\u0315B", "A\\u0315\\u0300\\u0316B",
                        "\\u0316\\u0315\\u0300", "A\\u0316\\u0315\\u0300B"};
    int32_t   srclen;
    UChar source[10];
    UCollationElements *iter;

    coll = ucol_openRules(rule, rulelen, UCOL_ON, UCOL_TERTIARY, NULL, &status);
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    if (U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of collator using ucol_openRules()\n %s\n",
              myErrorName(status));
        return;
    }

    srclen = u_unescape(testdata[0], source, 10);
    iter = ucol_openElements(coll, source, srclen, &status);
    backAndForth(iter);
    ucol_closeElements(iter);

    srclen = u_unescape(testdata[1], source, 10);
    iter = ucol_openElements(coll, source, srclen, &status);
    backAndForth(iter);
    ucol_closeElements(iter);

    while (count < 12) {
        srclen = u_unescape(testdata[count], source, 10);
        iter = ucol_openElements(coll, source, srclen, &status);

        if (U_FAILURE(status)){
            log_err("ERROR: in creation of collator element iterator\n %s\n",
                  myErrorName(status));
            return;
        }
        backAndForth(iter);
        ucol_closeElements(iter);

        iter = ucol_openElements(coll, source, -1, &status);

        if (U_FAILURE(status)){
            log_err("ERROR: in creation of collator element iterator\n %s\n",
                  myErrorName(status));
            return;
        }
        backAndForth(iter);
        ucol_closeElements(iter);
        count ++;
    }
    ucol_close(coll);
}

/**
 * Test for CollationElementIterator.previous()
 *
 * @bug 4108758 - Make sure it works with contracting characters
 *
 */
static void TestPrevious()
{
    UCollator *coll=NULL;
    UChar rule[50];
    UChar *source;
    UCollator *c1, *c2, *c3;
    UCollationElements *iter;
    UErrorCode status = U_ZERO_ERROR;
    UChar test1[50];
    UChar test2[50];

    u_uastrcpy(test1, "What subset of all possible test cases?");
    u_uastrcpy(test2, "has the highest probability of detecting");
    coll = ucol_open("en_US", &status);

    iter=ucol_openElements(coll, test1, u_strlen(test1), &status);
    log_verbose("English locale testing back and forth\n");
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
        ucol_close(coll);
        return;
    }
    /* A basic test to see if it's working at all */
    backAndForth(iter);
    ucol_closeElements(iter);
    ucol_close(coll);

    /* Test with a contracting character sequence */
    u_uastrcpy(rule, "&a,A < b,B < c,C, d,D < z,Z < ch,cH,Ch,CH");
    c1 = ucol_openRules(rule, u_strlen(rule), UCOL_OFF, UCOL_DEFAULT_STRENGTH, NULL, &status);

    log_verbose("Contraction rule testing back and forth with no normalization\n");

    if (c1 == NULL || U_FAILURE(status))
    {
        log_err("Couldn't create a RuleBasedCollator with a contracting sequence\n %s\n",
            myErrorName(status));
        return;
    }
    source=(UChar*)malloc(sizeof(UChar) * 20);
    u_uastrcpy(source, "abchdcba");
    iter=ucol_openElements(c1, source, u_strlen(source), &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
        return;
    }
    backAndForth(iter);
    ucol_closeElements(iter);
    ucol_close(c1);

    /* Test with an expanding character sequence */
    u_uastrcpy(rule, "&a < b < c/abd < d");
    c2 = ucol_openRules(rule, u_strlen(rule), UCOL_OFF, UCOL_DEFAULT_STRENGTH, NULL, &status);
    log_verbose("Expansion rule testing back and forth with no normalization\n");
    if (c2 == NULL || U_FAILURE(status))
    {
        log_err("Couldn't create a RuleBasedCollator with a contracting sequence.\n %s\n",
            myErrorName(status));
        return;
    }
    u_uastrcpy(source, "abcd");
    iter=ucol_openElements(c2, source, u_strlen(source), &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
        return;
    }
    backAndForth(iter);
    ucol_closeElements(iter);
    ucol_close(c2);
    /* Now try both */
    u_uastrcpy(rule, "&a < b < c/aba < d < z < ch");
    c3 = ucol_openRules(rule, u_strlen(rule), UCOL_DEFAULT,  UCOL_DEFAULT_STRENGTH,NULL, &status);
    log_verbose("Expansion/contraction rule testing back and forth with no normalization\n");

    if (c3 == NULL || U_FAILURE(status))
    {
        log_err("Couldn't create a RuleBasedCollator with a contracting sequence.\n %s\n",
            myErrorName(status));
        return;
    }
    u_uastrcpy(source, "abcdbchdc");
    iter=ucol_openElements(c3, source, u_strlen(source), &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
        return;
    }
    backAndForth(iter);
    ucol_closeElements(iter);
    ucol_close(c3);
    source[0] = 0x0e41;
    source[1] = 0x0e02;
    source[2] = 0x0e41;
    source[3] = 0x0e02;
    source[4] = 0x0e27;
    source[5] = 0x61;
    source[6] = 0x62;
    source[7] = 0x63;
    source[8] = 0;

    coll = ucol_open("th_TH", &status);
    log_verbose("Thai locale testing back and forth with normalization\n");
    iter=ucol_openElements(coll, source, u_strlen(source), &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
        return;
    }
    backAndForth(iter);
    ucol_closeElements(iter);
    ucol_close(coll);

    /* prev test */
    source[0] = 0x0061;
    source[1] = 0x30CF;
    source[2] = 0x3099;
    source[3] = 0x30FC;
    source[4] = 0;

    coll = ucol_open("ja_JP", &status);
    log_verbose("Japanese locale testing back and forth with normalization\n");
    iter=ucol_openElements(coll, source, u_strlen(source), &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
        return;
    }
    backAndForth(iter);
    ucol_closeElements(iter);
    ucol_close(coll);

    free(source);
}

/**
 * Test for getOffset() and setOffset()
 */
static void TestOffset()
{
    UErrorCode status= U_ZERO_ERROR;
    UCollator *en_us=NULL;
    UCollationElements *iter, *pristine;
    int32_t offset;
    OrderAndOffset *orders;
    int32_t orderLength=0;
    int     count = 0;
    UChar test1[50];
    UChar test2[50];

    u_uastrcpy(test1, "What subset of all possible test cases?");
    u_uastrcpy(test2, "has the highest probability of detecting");
    en_us = ucol_open("en_US", &status);
    log_verbose("Testing getOffset and setOffset for collations\n");
    iter = ucol_openElements(en_us, test1, u_strlen(test1), &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
        ucol_close(en_us);
        return;
    }

    /* testing boundaries */
    ucol_setOffset(iter, 0, &status);
    if (U_FAILURE(status) || ucol_previous(iter, &status) != UCOL_NULLORDER) {
        log_err("Error: After setting offset to 0, we should be at the end "
                "of the backwards iteration");
    }
    ucol_setOffset(iter, u_strlen(test1), &status);
    if (U_FAILURE(status) || ucol_next(iter, &status) != UCOL_NULLORDER) {
        log_err("Error: After setting offset to end of the string, we should "
                "be at the end of the backwards iteration");
    }

    /* Run all the way through the iterator, then get the offset */

    orders = getOrders(iter, &orderLength);

    offset = ucol_getOffset(iter);

    if (offset != u_strlen(test1))
    {
        log_err("offset at end != length %d vs %d\n", offset,
            u_strlen(test1) );
    }

    /* Now set the offset back to the beginning and see if it works */
    pristine=ucol_openElements(en_us, test1, u_strlen(test1), &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
    ucol_close(en_us);
        return;
    }
    status = U_ZERO_ERROR;

    ucol_setOffset(iter, 0, &status);
    if (U_FAILURE(status))
    {
        log_err("setOffset failed. %s\n",    myErrorName(status));
    }
    else
    {
        assertEqual(iter, pristine);
    }

    ucol_closeElements(pristine);
    ucol_closeElements(iter);
    free(orders);

    /* testing offsets in normalization buffer */
    test1[0] = 0x61;
    test1[1] = 0x300;
    test1[2] = 0x316;
    test1[3] = 0x62;
    test1[4] = 0;
    ucol_setAttribute(en_us, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    iter = ucol_openElements(en_us, test1, 4, &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of collation element iterator using ucol_openElements()\n %s\n",
            myErrorName(status));
        ucol_close(en_us);
        return;
    }

    count = 0;
    while (ucol_next(iter, &status) != UCOL_NULLORDER &&
        U_SUCCESS(status)) {
        switch (count) {
        case 0:
            if (ucol_getOffset(iter) != 1) {
                log_err("ERROR: Offset of iteration should be 1\n");
            }
            break;
        case 3:
            if (ucol_getOffset(iter) != 4) {
                log_err("ERROR: Offset of iteration should be 4\n");
            }
            break;
        default:
            if (ucol_getOffset(iter) != 3) {
                log_err("ERROR: Offset of iteration should be 3\n");
            }
        }
        count ++;
    }

    ucol_reset(iter);
    count = 0;
    while (ucol_previous(iter, &status) != UCOL_NULLORDER &&
        U_SUCCESS(status)) {
        switch (count) {
        case 0:
        case 1:
            if (ucol_getOffset(iter) != 3) {
                log_err("ERROR: Offset of iteration should be 3\n");
            }
            break;
        case 2:
            if (ucol_getOffset(iter) != 1) {
                log_err("ERROR: Offset of iteration should be 1\n");
            }
            break;
        default:
            if (ucol_getOffset(iter) != 0) {
                log_err("ERROR: Offset of iteration should be 0\n");
            }
        }
        count ++;
    }

    if(U_FAILURE(status)){
        log_err("ERROR: in iterating collation elements %s\n",
            myErrorName(status));
    }

    ucol_closeElements(iter);
    ucol_close(en_us);
}

/**
 * Test for setText()
 */
static void TestSetText()
{
    int32_t c,i;
    UErrorCode status = U_ZERO_ERROR;
    UCollator *en_us=NULL;
    UCollationElements *iter1, *iter2;
    UChar test1[50];
    UChar test2[50];

    u_uastrcpy(test1, "What subset of all possible test cases?");
    u_uastrcpy(test2, "has the highest probability of detecting");
    en_us = ucol_open("en_US", &status);
    log_verbose("testing setText for Collation elements\n");
    iter1=ucol_openElements(en_us, test1, u_strlen(test1), &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of collation element iterator1 using ucol_openElements()\n %s\n",
            myErrorName(status));
    ucol_close(en_us);
        return;
    }
    iter2=ucol_openElements(en_us, test2, u_strlen(test2), &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of collation element iterator2 using ucol_openElements()\n %s\n",
            myErrorName(status));
    ucol_close(en_us);
        return;
    }

    /* Run through the second iterator just to exercise it */
    c = ucol_next(iter2, &status);
    i = 0;

    while ( ++i < 10 && (c != UCOL_NULLORDER))
    {
        if (U_FAILURE(status))
        {
            log_err("iter2->next() returned an error. %s\n", myErrorName(status));
            ucol_closeElements(iter2);
            ucol_closeElements(iter1);
    ucol_close(en_us);
            return;
        }

        c = ucol_next(iter2, &status);
    }

    /* Now set it to point to the same string as the first iterator */
    ucol_setText(iter2, test1, u_strlen(test1), &status);
    if (U_FAILURE(status))
    {
        log_err("call to iter2->setText(test1) failed. %s\n", myErrorName(status));
    }
    else
    {
        assertEqual(iter1, iter2);
    }

    /* Now set it to point to a null string with fake length*/
    ucol_setText(iter2, NULL, 2, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR)
    {
        log_err("call to iter2->setText(null, 2) should yield an illegal-argument-error - %s\n",
                myErrorName(status));
    }

    ucol_closeElements(iter2);
    ucol_closeElements(iter1);
    ucol_close(en_us);
}

/** @bug 4108762
 * Test for getMaxExpansion()
 */
static void TestMaxExpansion()
{
    UErrorCode          status = U_ZERO_ERROR;
    UCollator          *coll   ;/*= ucol_open("en_US", &status);*/
    UChar               ch     = 0;
    UChar32             unassigned = 0xEFFFD;
    UChar               supplementary[2];
    uint32_t            stringOffset = 0;
    UBool               isError = FALSE;
    uint32_t            sorder = 0;
    UCollationElements *iter   ;/*= ucol_openElements(coll, &ch, 1, &status);*/
    uint32_t            temporder = 0;

    UChar rule[256];
    u_uastrcpy(rule, "&a < ab < c/aba < d < z < ch");
    coll = ucol_openRules(rule, u_strlen(rule), UCOL_DEFAULT,
        UCOL_DEFAULT_STRENGTH,NULL, &status);
    if(U_SUCCESS(status) && coll) {
      iter = ucol_openElements(coll, &ch, 1, &status);

      while (ch < 0xFFFF && U_SUCCESS(status)) {
          int      count = 1;
          uint32_t order;
          int32_t  size = 0;

          ch ++;

          ucol_setText(iter, &ch, 1, &status);
          order = ucol_previous(iter, &status);

          /* thai management */
          if (order == 0)
              order = ucol_previous(iter, &status);

          while (U_SUCCESS(status) &&
              ucol_previous(iter, &status) != UCOL_NULLORDER) {
              count ++;
          }

          size = ucol_getMaxExpansion(iter, order);
          if (U_FAILURE(status) || size < count) {
              log_err("Failure at codepoint %d, maximum expansion count < %d\n",
                  ch, count);
          }
      }

      /* testing for exact max expansion */
      ch = 0;
      while (ch < 0x61) {
          uint32_t order;
          int32_t  size;
          ucol_setText(iter, &ch, 1, &status);
          order = ucol_previous(iter, &status);
          size  = ucol_getMaxExpansion(iter, order);
          if (U_FAILURE(status) || size != 1) {
              log_err("Failure at codepoint %d, maximum expansion count < %d\n",
                  ch, 1);
          }
          ch ++;
      }

      ch = 0x63;
      ucol_setText(iter, &ch, 1, &status);
      temporder = ucol_previous(iter, &status);

      if (U_FAILURE(status) || ucol_getMaxExpansion(iter, temporder) != 3) {
          log_err("Failure at codepoint %d, maximum expansion count != %d\n",
                  ch, 3);
      }

      ch = 0x64;
      ucol_setText(iter, &ch, 1, &status);
      temporder = ucol_previous(iter, &status);

      if (U_FAILURE(status) || ucol_getMaxExpansion(iter, temporder) != 1) {
          log_err("Failure at codepoint %d, maximum expansion count != %d\n",
                  ch, 3);
      }

      U16_APPEND(supplementary, stringOffset, 2, unassigned, isError);
      (void)isError;    /* Suppress set but not used warning. */
      ucol_setText(iter, supplementary, 2, &status);
      sorder = ucol_previous(iter, &status);

      if (U_FAILURE(status) || ucol_getMaxExpansion(iter, sorder) != 2) {
          log_err("Failure at codepoint %d, maximum expansion count < %d\n",
                  ch, 2);
      }

      /* testing jamo */
      ch = 0x1165;

      ucol_setText(iter, &ch, 1, &status);
      temporder = ucol_previous(iter, &status);
      if (U_FAILURE(status) || ucol_getMaxExpansion(iter, temporder) > 3) {
          log_err("Failure at codepoint %d, maximum expansion count > %d\n",
                  ch, 3);
      }

      ucol_closeElements(iter);
      ucol_close(coll);

      /* testing special jamo &a<\u1160 */
      rule[0] = 0x26;
      rule[1] = 0x71;
      rule[2] = 0x3c;
      rule[3] = 0x1165;
      rule[4] = 0x2f;
      rule[5] = 0x71;
      rule[6] = 0x71;
      rule[7] = 0x71;
      rule[8] = 0x71;
      rule[9] = 0;

      coll = ucol_openRules(rule, u_strlen(rule), UCOL_DEFAULT,
          UCOL_DEFAULT_STRENGTH,NULL, &status);
      iter = ucol_openElements(coll, &ch, 1, &status);

      temporder = ucol_previous(iter, &status);
      if (U_FAILURE(status) || ucol_getMaxExpansion(iter, temporder) != 6) {
          log_err("Failure at codepoint %d, maximum expansion count > %d\n",
                  ch, 5);
      }

      ucol_closeElements(iter);
      ucol_close(coll);
    } else {
      log_err_status(status, "Couldn't open collator -> %s\n", u_errorName(status));
    }

}


static void assertEqual(UCollationElements *i1, UCollationElements *i2)
{
    int32_t c1, c2;
    int32_t count = 0;
    UErrorCode status = U_ZERO_ERROR;

    do
    {
        c1 = ucol_next(i1, &status);
        c2 = ucol_next(i2, &status);

        if (c1 != c2)
        {
            log_err("Error in iteration %d assetEqual between\n  %d  and   %d, they are not equal\n", count, c1, c2);
            break;
        }

        count += 1;
    }
    while (c1 != UCOL_NULLORDER);
}

/**
 * Testing iterators with extremely small buffers
 */
static void TestSmallBuffer()
{
    UErrorCode          status = U_ZERO_ERROR;
    UCollator          *coll;
    UCollationElements *testiter,
                       *iter;
    int32_t             count = 0;
    OrderAndOffset     *testorders,
                       *orders;

    UChar teststr[500];
    UChar str[] = {0x300, 0x31A, 0};
    /*
    creating a long string of decomposable characters,
    since by default the writable buffer is of size 256
    */
    while (count < 500) {
        if ((count & 1) == 0) {
            teststr[count ++] = 0x300;
        }
        else {
            teststr[count ++] = 0x31A;
        }
    }

    coll = ucol_open("th_TH", &status);
    if(U_SUCCESS(status) && coll) {
      testiter = ucol_openElements(coll, teststr, 500, &status);
      iter = ucol_openElements(coll, str, 2, &status);

      orders     = getOrders(iter, &count);
      if (count != 2) {
          log_err("Error collation elements size is not 2 for \\u0300\\u031A\n");
      }

      /*
      this will rearrange the string data to 250 characters of 0x300 first then
      250 characters of 0x031A
      */
      testorders = getOrders(testiter, &count);

      if (count != 500) {
          log_err("Error decomposition does not give the right sized collation elements\n");
      }

      while (count != 0) {
          /* UCA collation element for 0x0F76 */
          if ((count > 250 && testorders[-- count].order != orders[1].order) ||
              (count <= 250 && testorders[-- count].order != orders[0].order)) {
              log_err("Error decomposition does not give the right collation element at %d count\n", count);
              break;
          }
      }

      free(testorders);
      free(orders);

      ucol_reset(testiter);

      /* ensures closing of elements done properly to clear writable buffer */
      ucol_next(testiter, &status);
      ucol_next(testiter, &status);
      ucol_closeElements(testiter);
      ucol_closeElements(iter);
      ucol_close(coll);
    } else {
      log_err_status(status, "Couldn't open collator -> %s\n", u_errorName(status));
    }
}

/**
* Testing the discontigous contractions
*/
static void TestDiscontiguos() {
    const char               *rulestr    =
                            "&z < AB < X\\u0300 < ABC < X\\u0300\\u0315";
          UChar               rule[50];
          int                 rulelen = u_unescape(rulestr, rule, 50);
    const char               *src[] = {
     "ADB", "ADBC", "A\\u0315B", "A\\u0315BC",
    /* base character blocked */
     "XD\\u0300", "XD\\u0300\\u0315",
    /* non blocking combining character */
     "X\\u0319\\u0300", "X\\u0319\\u0300\\u0315",
     /* blocking combining character */
     "X\\u0314\\u0300", "X\\u0314\\u0300\\u0315",
     /* contraction prefix */
     "ABDC", "AB\\u0315C","X\\u0300D\\u0315", "X\\u0300\\u0319\\u0315",
     "X\\u0300\\u031A\\u0315",
     /* ends not with a contraction character */
     "X\\u0319\\u0300D", "X\\u0319\\u0300\\u0315D", "X\\u0300D\\u0315D",
     "X\\u0300\\u0319\\u0315D", "X\\u0300\\u031A\\u0315D"
    };
    const char               *tgt[] = {
     /* non blocking combining character */
     "A D B", "A D BC", "A \\u0315 B", "A \\u0315 BC",
    /* base character blocked */
     "X D \\u0300", "X D \\u0300\\u0315",
    /* non blocking combining character */
     "X\\u0300 \\u0319", "X\\u0300\\u0315 \\u0319",
     /* blocking combining character */
     "X \\u0314 \\u0300", "X \\u0314 \\u0300\\u0315",
     /* contraction prefix */
     "AB DC", "AB \\u0315 C","X\\u0300 D \\u0315", "X\\u0300\\u0315 \\u0319",
     "X\\u0300 \\u031A \\u0315",
     /* ends not with a contraction character */
     "X\\u0300 \\u0319D", "X\\u0300\\u0315 \\u0319D", "X\\u0300 D\\u0315D",
     "X\\u0300\\u0315 \\u0319D", "X\\u0300 \\u031A\\u0315D"
    };
          int                 size   = 20;
          UCollator          *coll;
          UErrorCode          status    = U_ZERO_ERROR;
          int                 count     = 0;
          UCollationElements *iter;
          UCollationElements *resultiter;

    coll       = ucol_openRules(rule, rulelen, UCOL_OFF, UCOL_DEFAULT_STRENGTH,NULL, &status);
    iter       = ucol_openElements(coll, rule, 1, &status);
    resultiter = ucol_openElements(coll, rule, 1, &status);

    if (U_FAILURE(status)) {
        log_err_status(status, "Error opening collation rules -> %s\n", u_errorName(status));
        return;
    }

    while (count < size) {
        UChar  str[20];
        UChar  tstr[20];
        int    strLen = u_unescape(src[count], str, 20);
        UChar *s;

        ucol_setText(iter, str, strLen, &status);
        if (U_FAILURE(status)) {
            log_err("Error opening collation iterator\n");
            return;
        }

        u_unescape(tgt[count], tstr, 20);
        s = tstr;

        log_verbose("count %d\n", count);

        for (;;) {
            uint32_t  ce;
            UChar    *e = u_strchr(s, 0x20);
            if (e == 0) {
                e = u_strchr(s, 0);
            }
            ucol_setText(resultiter, s, (int32_t)(e - s), &status);
            ce = ucol_next(resultiter, &status);
            if (U_FAILURE(status)) {
                log_err("Error manipulating collation iterator\n");
                return;
            }
            while (ce != UCOL_NULLORDER) {
                if (ce != (uint32_t)ucol_next(iter, &status) ||
                    U_FAILURE(status)) {
                    log_err("Discontiguos contraction test mismatch\n");
                    return;
                }
                ce = ucol_next(resultiter, &status);
                if (U_FAILURE(status)) {
                    log_err("Error getting next collation element\n");
                    return;
                }
            }
            s = e + 1;
            if (*e == 0) {
                break;
            }
        }
        ucol_reset(iter);
        backAndForth(iter);
        count ++;
    }
    ucol_closeElements(resultiter);
    ucol_closeElements(iter);
    ucol_close(coll);
}

/**
* TestSearchCollatorElements tests iterator behavior (forwards and backwards) with
* normalization on AND jamo tailoring, among other things.
*
* Note: This test is sensitive to changes of the root collator,
* for example whether the ae-ligature maps to three CEs (as in the DUCET)
* or to two CEs (as in the CLDR 24 FractionalUCA.txt).
* It is also sensitive to how those CEs map to the iterator's 32-bit CE encoding.
* For example, the DUCET's artificial secondary CE in the ae-ligature
* may map to two 32-bit iterator CEs (as it did until ICU 52).
*/
static const UChar tsceText[] = {   /* Nothing in here should be ignorable */
    0x0020, 0xAC00,                 /* simple LV Hangul */
    0x0020, 0xAC01,                 /* simple LVT Hangul */
    0x0020, 0xAC0F,                 /* LVTT, last jamo expands for search */
    0x0020, 0xAFFF,                 /* LLVVVTT, every jamo expands for search */
    0x0020, 0x1100, 0x1161, 0x11A8, /* 0xAC01 as conjoining jamo */
    0x0020, 0x3131, 0x314F, 0x3131, /* 0xAC01 as compatibility jamo */
    0x0020, 0x1100, 0x1161, 0x11B6, /* 0xAC0F as conjoining jamo; last expands for search */
    0x0020, 0x1101, 0x1170, 0x11B6, /* 0xAFFF as conjoining jamo; all expand for search */
    0x0020, 0x00E6,                 /* small letter ae, expands */
    0x0020, 0x1E4D,                 /* small letter o with tilde and acute, decomposes */
    0x0020
};
enum { kLen_tsceText = UPRV_LENGTHOF(tsceText) };

static const int32_t rootStandardOffsets[] = {
    0,  1,2,
    2,  3,4,4,
    4,  5,6,6,
    6,  7,8,8,
    8,  9,10,11,
    12, 13,14,15,
    16, 17,18,19,
    20, 21,22,23,
    24, 25,26,  /* plus another 1-2 offset=26 if ae-ligature maps to three CEs */
    26, 27,28,28,
    28,
    29
};
enum { kLen_rootStandardOffsets = UPRV_LENGTHOF(rootStandardOffsets) };

static const int32_t rootSearchOffsets[] = {
    0,  1,2,
    2,  3,4,4,
    4,  5,6,6,6,
    6,  7,8,8,8,8,8,8,
    8,  9,10,11,
    12, 13,14,15,
    16, 17,18,19,20,
    20, 21,22,22,23,23,23,24,
    24, 25,26,  /* plus another 1-2 offset=26 if ae-ligature maps to three CEs */
    26, 27,28,28,
    28,
    29
};
enum { kLen_rootSearchOffsets = UPRV_LENGTHOF(rootSearchOffsets) };

typedef struct {
    const char *    locale;
    const int32_t * offsets;
    int32_t         offsetsLen;
} TSCEItem;

static const TSCEItem tsceItems[] = {
    { "root",                  rootStandardOffsets, kLen_rootStandardOffsets },
    { "root@collation=search", rootSearchOffsets,   kLen_rootSearchOffsets   },
    { NULL,                    NULL,                0                        }
};

static void TestSearchCollatorElements(void)
{
    const TSCEItem * tsceItemPtr;
    for (tsceItemPtr = tsceItems; tsceItemPtr->locale != NULL; tsceItemPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UCollator* ucol = ucol_open(tsceItemPtr->locale, &status);
        if ( U_SUCCESS(status) ) {
            UCollationElements * uce = ucol_openElements(ucol, tsceText, kLen_tsceText, &status);
            if ( U_SUCCESS(status) ) {
                int32_t offset, element;
                const int32_t * nextOffsetPtr;
                const int32_t * limitOffsetPtr;

                nextOffsetPtr = tsceItemPtr->offsets;
                limitOffsetPtr = tsceItemPtr->offsets + tsceItemPtr->offsetsLen;
                do {
                    offset = ucol_getOffset(uce);
                    element = ucol_next(uce, &status);
                    log_verbose("(%s) offset=%2d  ce=%08x\n", tsceItemPtr->locale, offset, element);
                    if ( element == 0 ) {
                        log_err("error, locale %s, ucol_next returned element 0\n", tsceItemPtr->locale );
                    }
                    if ( nextOffsetPtr < limitOffsetPtr ) {
                        if (offset != *nextOffsetPtr) {
                            log_err("error, locale %s, expected ucol_next -> ucol_getOffset %d, got %d\n",
                                                            tsceItemPtr->locale, *nextOffsetPtr, offset );
                            nextOffsetPtr = limitOffsetPtr;
                            break;
                        }
                        nextOffsetPtr++;
                    } else {
                        log_err("error, locale %s, ucol_next returned more elements than expected\n", tsceItemPtr->locale );
                    }
                } while ( U_SUCCESS(status) && element != UCOL_NULLORDER );
                if ( nextOffsetPtr < limitOffsetPtr ) {
                    log_err("error, locale %s, ucol_next returned fewer elements than expected\n", tsceItemPtr->locale );
                }

                ucol_setOffset(uce, kLen_tsceText, &status);
                status = U_ZERO_ERROR;
                nextOffsetPtr = tsceItemPtr->offsets + tsceItemPtr->offsetsLen;
                limitOffsetPtr = tsceItemPtr->offsets;
                do {
                    offset = ucol_getOffset(uce);
                    element = ucol_previous(uce, &status);
                    if ( element == 0 ) {
                        log_err("error, locale %s, ucol_previous returned element 0\n", tsceItemPtr->locale );
                    }
                    if ( nextOffsetPtr > limitOffsetPtr ) {
                        nextOffsetPtr--;
                        if (offset != *nextOffsetPtr) {
                            log_err("error, locale %s, expected ucol_previous -> ucol_getOffset %d, got %d\n",
                                                                tsceItemPtr->locale, *nextOffsetPtr, offset );
                            nextOffsetPtr = limitOffsetPtr;
                            break;
                        }
                   } else {
                        log_err("error, locale %s, ucol_previous returned more elements than expected\n", tsceItemPtr->locale );
                    }
                } while ( U_SUCCESS(status) && element != UCOL_NULLORDER );
                if ( nextOffsetPtr > limitOffsetPtr ) {
                    log_err("error, locale %s, ucol_previous returned fewer elements than expected\n", tsceItemPtr->locale );
                }

                ucol_closeElements(uce);
            } else {
                log_err("error, locale %s, ucol_openElements failed: %s\n", tsceItemPtr->locale, u_errorName(status) );
            }
            ucol_close(ucol);
        } else {
            log_data_err("error, locale %s, ucol_open failed: %s\n", tsceItemPtr->locale, u_errorName(status) );
        }
    }
}

#endif /* #if !UCONFIG_NO_COLLATION */
