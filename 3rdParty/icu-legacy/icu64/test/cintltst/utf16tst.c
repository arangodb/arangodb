// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1998-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/*
* File test.c
*
* Modification History:
*
*   Date          Name        Description
*   05/01/2000    Madhu       Creation 
*******************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "unicode/utf_old.h"
#include "cmemory.h"
#include "cstring.h"
#include "cintltst.h"
#include <stdbool.h>
#include <stdio.h>

// Obsolete macro from obsolete unicode/utf_old.h, for some old test data.
#ifndef UTF_ERROR_VALUE
#   define UTF_ERROR_VALUE 0xffff
#endif

#if !U_HIDE_OBSOLETE_UTF_OLD_H
static void printUChars(const UChar *uchars) {
    int16_t i=0;
    for(i=0; i<u_strlen(uchars); i++) {
        printf("%x ", *(uchars+i));
    }
}
#endif

static void TestCodeUnitValues(void);
static void TestCharLength(void);
static void TestGetChar(void);
static void TestNextPrevChar(void);
static void TestNulTerminated(void);
static void TestFwdBack(void);
static void TestSetChar(void);
static void TestAppendChar(void);
static void TestAppend(void);
static void TestSurrogate(void);

void addUTF16Test(TestNode** root);

void
addUTF16Test(TestNode** root)
{
    addTest(root, &TestCodeUnitValues,          "utf16tst/TestCodeUnitValues");
    addTest(root, &TestCharLength,              "utf16tst/TestCharLength");
    addTest(root, &TestGetChar,                 "utf16tst/TestGetChar");
    addTest(root, &TestNextPrevChar,            "utf16tst/TestNextPrevChar");
    addTest(root, &TestNulTerminated,           "utf16tst/TestNulTerminated");
    addTest(root, &TestFwdBack,                 "utf16tst/TestFwdBack");
    addTest(root, &TestSetChar,                 "utf16tst/TestSetChar");
    addTest(root, &TestAppendChar,              "utf16tst/TestAppendChar");
    addTest(root, &TestAppend,                  "utf16tst/TestAppend");
    addTest(root, &TestSurrogate,               "utf16tst/TestSurrogate");
}

static void TestCodeUnitValues(void)
{
    static uint16_t codeunit[]={0x0000,0xe065,0x20ac,0xd7ff,0xd800,0xd841,0xd905,0xdbff,0xdc00,0xdc02,0xddee,0xdfff,0};
    
    int16_t i;
    for(i=0; i<UPRV_LENGTHOF(codeunit); i++){
        UChar c=codeunit[i];
        log_verbose("Testing code unit value of %x\n", c);
        if(i<4){
            if(
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                    !UTF16_IS_SINGLE(c) || UTF16_IS_LEAD(c) || UTF16_IS_TRAIL(c) ||
#endif
                    !U16_IS_SINGLE(c) || U16_IS_LEAD(c) || U16_IS_TRAIL(c)) {
                log_err("ERROR: %x is a single character\n", c);
            }
        }
        if(i >= 4 && i< 8){
            if(
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                    !UTF16_IS_LEAD(c) || UTF16_IS_SINGLE(c) || UTF16_IS_TRAIL(c) ||
#endif
                    !U16_IS_LEAD(c) || U16_IS_SINGLE(c) || U16_IS_TRAIL(c)){
                log_err("ERROR: %x is a first surrogate\n", c);
            }
        }
        if(i >= 8 && i< 12){
            if(
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                    !UTF16_IS_TRAIL(c) || UTF16_IS_SINGLE(c) || UTF16_IS_LEAD(c) ||
#endif
                    !U16_IS_TRAIL(c) || U16_IS_SINGLE(c) || U16_IS_LEAD(c)) {
                log_err("ERROR: %x is a second surrogate\n", c);
            }
        }
    }
}

static void TestCharLength(void)
{
    static uint32_t codepoint[]={
        1, 0x0061,
        1, 0xe065,
        1, 0x20ac,
        2, 0x20402,
        2, 0x23456,
        2, 0x24506,
        2, 0x20402,
        2, 0x10402,
        1, 0xd7ff,
        1, 0xe000
    };
    
    int16_t i;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    UBool multiple;
#endif
    for(i=0; i<UPRV_LENGTHOF(codepoint); i=(int16_t)(i+2)){
        UChar32 c=codepoint[i+1];
        if(
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                UTF16_CHAR_LENGTH(c) != (uint16_t)codepoint[i] ||
#endif
                U16_LENGTH(c) != (uint16_t)codepoint[i]) {
          log_err("The no: of code units for %lx:- Expected: %d Got: %d\n", c, codepoint[i], U16_LENGTH(c));
        }else{
              log_verbose("The no: of code units for %lx is %d\n",c, U16_LENGTH(c));
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        multiple=(UBool)(codepoint[i] == 1 ? false : true);
        if(UTF16_NEED_MULTIPLE_UCHAR(c) != multiple){
              log_err("ERROR: UTF16_NEED_MULTIPLE_UCHAR failed for %lx\n", c);
        }
#endif
    }
}

static void TestGetChar(void)
{
    static UChar input[]={
    /*  code unit,*/
        0xdc00,
        0x20ac,    
        0xd841,      
        0x61,      
        0xd841,      
        0xdc02,     
        0xd842,    
        0xdc06,     
        0,
        0xd842,
        0xd7ff,
        0xdc41,
        0xe000,
        0xd800
    };
    static UChar32 result[]={
     /*codepoint-unsafe,  codepoint-safe(not strict)  codepoint-safe(strict)*/
        (UChar32)0xfca10000, 0xdc00,                  UTF_ERROR_VALUE,   
        0x20ac,           0x20ac,                     0x20ac,
        0x12861,          0xd841,                     UTF_ERROR_VALUE,
        0x61,             0x61,                       0x61, 
        0x20402,          0x20402,                    0x20402,  
        0x20402,          0x20402,                    0x20402,
        0x20806,          0x20806,                    0x20806,
        0x20806,          0x20806,                    0x20806,
        0x00,             0x00,                       0x00,
        0x203ff,          0xd842,                     UTF_ERROR_VALUE,
        0xd7ff,           0xd7ff,                     0xd7ff,  
        0xfc41,           0xdc41,                     UTF_ERROR_VALUE,
        0xe000,           0xe000,                     0xe000, 
        0x11734,          0xd800,                     UTF_ERROR_VALUE  
    };
    uint16_t i=0;
    UChar32 c, expected;
    uint16_t offset=0;
    for(offset=0; offset<UPRV_LENGTHOF(input); offset++) {
        if(0<offset && offset<UPRV_LENGTHOF(input)-1){
#if !U_HIDE_OBSOLETE_UTF_OLD_H
            UTF16_GET_CHAR_UNSAFE(input, offset, c);
            if(c != result[i]){
                log_err("ERROR: UTF16_GET_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, result[i], c);
            }
#endif
            U16_GET_UNSAFE(input, offset, c);
            if(c != result[i]){
                log_err("ERROR: U16_GET_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, result[i], c);
            }
        }
        expected=result[i+1];
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        UTF16_GET_CHAR_SAFE(input, 0, offset, UPRV_LENGTHOF(input), c, false);
        if(c != expected) {
            log_err("ERROR: UTF16_GET_CHAR_SAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#endif
        U16_GET(input, 0, offset, UPRV_LENGTHOF(input), c);
        if(c != expected) {
            log_err("ERROR: U16_GET failed for offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }

        U16_GET_OR_FFFD(input, 0, offset, UPRV_LENGTHOF(input), c);
        if(U_IS_SURROGATE(expected)) { expected=0xfffd; }
        if(c != expected) {
            log_err("ERROR: U16_GET_OR_FFFD failed for offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        UTF16_GET_CHAR_SAFE(input, 0, offset, UPRV_LENGTHOF(input), c, true);
        if(c != result[i+2]){
            log_err("ERROR: UTF16_GET_CHAR_SAFE(strict) failed for offset=%ld. Expected:%lx Got:%lx\n", offset, result[i+2], c);
        }
#endif
        i=(uint16_t)(i+3);
    }
}

static void TestNextPrevChar(void){

    static UChar input[]={0x0061, 0xd800, 0xdc00, 0xdbff, 0xdfff, 0x0062, 0xd841, 0xd7ff, 0xd841, 0xdc41, 0xdc00, 0x0000};
    static UChar32 result[]={
    /*next_unsafe    next_safe_ns  next_safe_s       prev_unsafe   prev_safe_ns     prev_safe_s*/
        0x0061,        0x0061,       0x0061,           0x0000,       0x0000,          0x0000,
        0x10000,       0x10000,      0x10000,          0x120400,     0xdc00,          UTF_ERROR_VALUE, 
        0xdc00,        0xdc00,       UTF_ERROR_VALUE,  0x20441,      0x20441,         0x20441,
        0x10ffff,      0x10ffff,     0x10ffff,         0xd841,       0xd841,          UTF_ERROR_VALUE, 
        0xdfff,        0xdfff,       UTF_ERROR_VALUE,  0xd7ff,       0xd7ff,          0xd7ff,   
        0x0062,        0x0062,       0x0062,           0xd841,       0xd841,          UTF_ERROR_VALUE,     
        0x1ffff,       0xd841,       UTF_ERROR_VALUE,  0x0062,       0x0062,          0x0062,
        0xd7ff,        0xd7ff,       0xd7ff,           0x10ffff,     0x10ffff,        0x10ffff,
        0x20441,       0x20441,      0x20441,          0xdbff,       0xdbff,          UTF_ERROR_VALUE,      
        0xdc41,        0xdc41,       UTF_ERROR_VALUE,  0x10000,      0x10000,         0x10000,
        0xdc00,        0xdc00,       UTF_ERROR_VALUE,  0xd800,       0xd800,          UTF_ERROR_VALUE,
        0x0000,        0x0000,       0x0000,           0x0061,       0x0061,          0x0061
    };
    static uint16_t movedOffset[]={
   /*next_unsafe    next_safe_ns  next_safe_s       prev_unsafe   prev_safe_ns     prev_safe_s*/
        1,            1,           1,                11,           11,               11,
        3,            3,           3,                9,            10 ,              10, 
        3,            3,           3,                8,            8,                8,  
        5,            5,           4,                8,            8,                8, 
        5,            5,           5,                7,            7,                7,
        6,            6,           6,                6,            6,                6,
        8,            7,           7,                5,            5,                5,
        8,            8,           8,                3,            3,                3, 
        10,           10,          10,               3,            3,                3,         
        10,           10,          10,               1,            1,                1, 
        11,           11,          11,               1,            1,                1, 
        12,           12,          12,               0,            0,                0, 
    };
      

    UChar32 c=0x0000, expected;
    uint16_t i=0;
    uint16_t offset=0, setOffset=0;
    for(offset=0; offset<UPRV_LENGTHOF(input); offset++){
         setOffset=offset;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
         UTF16_NEXT_CHAR_UNSAFE(input, setOffset, c);
         if(setOffset != movedOffset[i]){
             log_err("ERROR: UTF16_NEXT_CHAR_UNSAFE failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i], setOffset);
         }
         if(c != result[i]){
             log_err("ERROR: UTF16_NEXT_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, result[i], c);
         }
#endif
         setOffset=offset;
         U16_NEXT_UNSAFE(input, setOffset, c);
         if(setOffset != movedOffset[i]){
             log_err("ERROR: U16_NEXT_CHAR_UNSAFE failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i], setOffset);
         }
         if(c != result[i]){
             log_err("ERROR: U16_NEXT_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, result[i], c);
         }
        expected=result[i+1];
#if !U_HIDE_OBSOLETE_UTF_OLD_H
         setOffset=offset;
         UTF16_NEXT_CHAR_SAFE(input, setOffset, UPRV_LENGTHOF(input), c, false);
         if(setOffset != movedOffset[i+1]){
             log_err("ERROR: UTF16_NEXT_CHAR_SAFE failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i+1], setOffset);
         }
        if(c != expected) {
            log_err("ERROR: UTF16_NEXT_CHAR_SAFE failed for input=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#endif
         setOffset=offset;
         U16_NEXT(input, setOffset, UPRV_LENGTHOF(input), c);
         if(setOffset != movedOffset[i+1]){
             log_err("ERROR: U16_NEXT failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i+1], setOffset);
         }
        if(c != expected){
            log_err("ERROR: U16_NEXT failed for input=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }

        setOffset=offset;
        U16_NEXT_OR_FFFD(input, setOffset, UPRV_LENGTHOF(input), c);
        if(setOffset != movedOffset[i+1]){
            log_err("ERROR: U16_NEXT_OR_FFFD failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[i+1], setOffset);
        }
        if(U_IS_SURROGATE(expected)) { expected=0xfffd; }
        if(c != expected){
            log_err("ERROR: U16_NEXT_OR_FFFD failed for input=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
         setOffset=offset;
         UTF16_NEXT_CHAR_SAFE(input, setOffset, UPRV_LENGTHOF(input), c, true);
         if(setOffset != movedOffset[i+1]){
             log_err("ERROR: UTF16_NEXT_CHAR_SAFE(strict) failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i+2], setOffset);
         }
         if(c != result[i+2]){
             log_err("ERROR: UTF16_NEXT_CHAR_SAFE(strict) failed for input=%ld. Expected:%lx Got:%lx\n", offset, result[i+2], c);
         }
#endif
         i=(uint16_t)(i+6);
    }
    i=0;
    for(offset=(uint16_t)UPRV_LENGTHOF(input); offset > 0; --offset){
         setOffset=offset;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
         UTF16_PREV_CHAR_UNSAFE(input, setOffset, c);
         if(setOffset != movedOffset[i+3]){
             log_err("ERROR: UTF16_PREV_CHAR_UNSAFE failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i+3], setOffset);
         }
         if(c != result[i+3]){
             log_err("ERROR: UTF16_PREV_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, result[i+3], c);
         }
#endif
         setOffset=offset;
         U16_PREV_UNSAFE(input, setOffset, c);
         if(setOffset != movedOffset[i+3]){
             log_err("ERROR: U16_PREV_CHAR_UNSAFE failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i+3], setOffset);
         }
         if(c != result[i+3]){
             log_err("ERROR: U16_PREV_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, result[i+3], c);
         }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
         setOffset=offset;
         UTF16_PREV_CHAR_SAFE(input, 0, setOffset, c, false);
         if(setOffset != movedOffset[i+4]){
             log_err("ERROR: UTF16_PREV_CHAR_SAFE failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i+4], setOffset);
         }
         if(c != result[i+4]){
             log_err("ERROR: UTF16_PREV_CHAR_SAFE failed for input=%ld. Expected:%lx Got:%lx\n", offset, result[i+4], c);
         }
#endif
         setOffset=offset;
         U16_PREV(input, 0, setOffset, c);
         if(setOffset != movedOffset[i+4]){
             log_err("ERROR: U16_PREV failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i+4], setOffset);
         }
        expected = result[i+4];
        if(c != expected) {
            log_err("ERROR: U16_PREV failed for input=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }

        setOffset=offset;
        U16_PREV_OR_FFFD(input, 0, setOffset, c);
        if(setOffset != movedOffset[i+4]){
            log_err("ERROR: U16_PREV_OR_FFFD failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[i+4], setOffset);
        }
        if(U_IS_SURROGATE(expected)) { expected=0xfffd; }
        if(c != expected) {
            log_err("ERROR: U16_PREV_OR_FFFD failed for input=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
         setOffset=offset;
         UTF16_PREV_CHAR_SAFE(input, 0,  setOffset, c, true);
         if(setOffset != movedOffset[i+5]){
             log_err("ERROR: UTF16_PREV_CHAR_SAFE(strict) failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                 offset, movedOffset[i+5], setOffset);
         } 
         if(c != result[i+5]){
             log_err("ERROR: UTF16_PREV_CHAR_SAFE(strict) failed for input=%ld. Expected:%lx Got:%lx\n", offset, result[i+5], c);
         }
#endif
         i=(uint16_t)(i+6);
    }

}

/* keep this in sync with utf8tst.c's TestNulTerminated() */
static void TestNulTerminated(void) {
    static const UChar input[]={
        /*  0 */  0x61,
        /*  1 */  0xd801, 0xdc01,
        /*  3 */  0xdc01,
        /*  4 */  0x62,
        /*  5 */  0xd801,
        /*  6 */  0x00
        /*  7 */
    };
    static const UChar32 result[]={
        0x61,
        0x10401,
        0xdc01,
        0x62,
        0xd801,
        0
    };

    UChar32 c, c2, expected;
    int32_t i0, i=0, j, k, expectedIndex;
    int32_t cpIndex=0;
    do {
        i0=i;
        U16_NEXT(input, i, -1, c);
        expected=result[cpIndex];
        if(c!=expected) {
            log_err("U16_NEXT(from %d)=U+%04x != U+%04x\n", i0, c, expected);
        }
        j=i0;
        U16_NEXT_OR_FFFD(input, j, -1, c);
        if(U_IS_SURROGATE(expected)) { expected=0xfffd; }
        if(c!=expected) {
            log_err("U16_NEXT_OR_FFFD(from %d)=U+%04x != U+%04x\n", i0, c, expected);
        }
        if(j!=i) {
            log_err("U16_NEXT_OR_FFFD() moved to index %d but U16_NEXT() moved to %d\n", j, i);
        }
        j=i0;
        U16_FWD_1(input, j, -1);
        if(j!=i) {
            log_err("U16_FWD_1() moved to index %d but U16_NEXT() moved to %d\n", j, i);
        }
        ++cpIndex;
        /*
         * Move by this many code points from the start.
         * U16_FWD_N() stops at the end of the string, that is, at the NUL if necessary.
         */
        expectedIndex= (c==0) ? i-1 : i;
        k=0;
        U16_FWD_N(input, k, -1, cpIndex);
        if(k!=expectedIndex) {
            log_err("U16_FWD_N(code points from 0) moved to index %d but expected %d\n", k, expectedIndex);
        }
    } while(c!=0);

    i=0;
    do {
        j=i0=i;
        U16_NEXT(input, i, -1, c);
        do {
            U16_GET(input, 0, j, -1, c2);
            if(c2!=c) {
                log_err("U16_NEXT(from %d)=U+%04x != U+%04x=U16_GET(at %d)\n", i0, c, c2, j);
            }
            U16_GET_OR_FFFD(input, 0, j, -1, c2);
            expected= U_IS_SURROGATE(c) ? 0xfffd : c;
            if(c2!=expected) {
                log_err("U16_NEXT_OR_FFFD(from %d)=U+%04x != U+%04x=U16_GET_OR_FFFD(at %d)\n", i0, expected, c2, j);
            }
            /* U16_SET_CP_LIMIT moves from a non-lead byte to the limit of the code point */
            k=j+1;
            U16_SET_CP_LIMIT(input, 0, k, -1);
            if(k!=i) {
                log_err("U16_NEXT() moved to %d but U16_SET_CP_LIMIT(%d) moved to %d\n", i, j+1, k);
            }
        } while(++j<i);
    } while(c!=0);
}

static void TestFwdBack(void){
    static UChar input[]={0x0061, 0xd800, 0xdc00, 0xdbff, 0xdfff, 0x0062, 0xd841, 0xd7ff, 0xd841, 0xdc41, 0xdc00, 0x0000};
    static uint16_t fwd_unsafe[] ={1, 3, 5, 6,  8, 10, 11, 12};
    static uint16_t fwd_safe[]   ={1, 3, 5, 6, 7, 8, 10, 11, 12};
    static uint16_t back_unsafe[]={11, 9, 8, 7, 6, 5, 3, 1, 0};
    static uint16_t back_safe[]  ={11, 10, 8, 7, 6, 5, 3, 1, 0};

    static uint16_t Nvalue[]= {0, 1, 2, 3, 1, 2, 1};
    static uint16_t fwd_N_unsafe[] ={0, 1, 5, 10, 11};
    static uint16_t fwd_N_safe[]   ={0, 1, 5, 8, 10, 12, 12}; /*safe macro keeps it at the end of the string */
    static uint16_t back_N_unsafe[]={12, 11, 8, 5, 3};
    static uint16_t back_N_safe[]  ={12, 11, 8, 5, 3, 0, 0};   

    uint16_t offunsafe=0, offsafe=0;
    uint16_t i=0;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    while(offunsafe < UPRV_LENGTHOF(input)){
        UTF16_FWD_1_UNSAFE(input, offunsafe);
        if(offunsafe != fwd_unsafe[i]){
            log_err("ERROR: Forward_unsafe offset expected:%d, Got:%d\n", fwd_unsafe[i], offunsafe);
        }
        i++;
    }
#endif
    offunsafe=0, offsafe=0;
    i=0;
    while(offunsafe < UPRV_LENGTHOF(input)){
        U16_FWD_1_UNSAFE(input, offunsafe);
        if(offunsafe != fwd_unsafe[i]){
            log_err("ERROR: U16_FWD_1_UNSAFE offset expected:%d, Got:%d\n", fwd_unsafe[i], offunsafe);
        }
        i++;
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    offunsafe=0, offsafe=0;
    i=0;
    while(offsafe < UPRV_LENGTHOF(input)){
        UTF16_FWD_1_SAFE(input, offsafe, UPRV_LENGTHOF(input));
        if(offsafe != fwd_safe[i]){
            log_err("ERROR: Forward_safe offset expected:%d, Got:%d\n", fwd_safe[i], offsafe);
        }
        i++;
    }
#endif
    offunsafe=0, offsafe=0;
    i=0;
    while(offsafe < UPRV_LENGTHOF(input)){
        U16_FWD_1(input, offsafe, UPRV_LENGTHOF(input));
        if(offsafe != fwd_safe[i]){
            log_err("ERROR: U16_FWD_1 offset expected:%d, Got:%d\n", fwd_safe[i], offsafe);
        }
        i++;
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    offunsafe=UPRV_LENGTHOF(input);
    offsafe=UPRV_LENGTHOF(input);
    i=0;
    while(offunsafe > 0){
        UTF16_BACK_1_UNSAFE(input, offunsafe);
        if(offunsafe != back_unsafe[i]){
            log_err("ERROR: Backward_unsafe offset expected:%d, Got:%d\n", back_unsafe[i], offunsafe);
        }
        i++;
    }
#endif
    offunsafe=UPRV_LENGTHOF(input);
    offsafe=UPRV_LENGTHOF(input);
    i=0;
    while(offunsafe > 0){
        U16_BACK_1_UNSAFE(input, offunsafe);
        if(offunsafe != back_unsafe[i]){
            log_err("ERROR: U16_BACK_1_UNSAFE offset expected:%d, Got:%d\n", back_unsafe[i], offunsafe);
        }
        i++;
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    offunsafe=UPRV_LENGTHOF(input);
    offsafe=UPRV_LENGTHOF(input);
    i=0;
    while(offsafe > 0){
        UTF16_BACK_1_SAFE(input,0,  offsafe);
        if(offsafe != back_safe[i]){
            log_err("ERROR: Backward_safe offset expected:%d, Got:%d\n", back_unsafe[i], offsafe);
        }
        i++;
    }
#endif
    offunsafe=UPRV_LENGTHOF(input);
    offsafe=UPRV_LENGTHOF(input);
    i=0;
    while(offsafe > 0){
        U16_BACK_1(input,0,  offsafe);
        if(offsafe != back_safe[i]){
            log_err("ERROR: U16_BACK_1 offset expected:%d, Got:%d\n", back_unsafe[i], offsafe);
        }
        i++;
    }

    offunsafe=0;
    offsafe=0;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    for(i=0; i<UPRV_LENGTHOF(Nvalue)-2; i++){  /*didn't want it to fail(we assume 0<i<length)*/
        UTF16_FWD_N_UNSAFE(input, offunsafe, Nvalue[i]);
        if(offunsafe != fwd_N_unsafe[i]){
            log_err("ERROR: Forward_N_unsafe offset expected:%d, Got:%d\n", fwd_N_unsafe[i], offunsafe);
        }
    }
#endif
    offunsafe=0;
    for(i=0; i<UPRV_LENGTHOF(Nvalue)-2; i++){  /*didn't want it to fail(we assume 0<i<length)*/
        U16_FWD_N_UNSAFE(input, offunsafe, Nvalue[i]);
        if(offunsafe != fwd_N_unsafe[i]){
            log_err("ERROR: U16_FWD_N_UNSAFE offset expected:%d, Got:%d\n", fwd_N_unsafe[i], offunsafe);
        }
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    offsafe=0;
    for(i=0; i<UPRV_LENGTHOF(Nvalue); i++){
        UTF16_FWD_N_SAFE(input, offsafe, UPRV_LENGTHOF(input), Nvalue[i]);
        if(offsafe != fwd_N_safe[i]){
            log_err("ERROR: Forward_N_safe offset expected:%d, Got:%d\n", fwd_N_safe[i], offsafe);
        }
    }
#endif
    offsafe=0;
    for(i=0; i<UPRV_LENGTHOF(Nvalue); i++){
        U16_FWD_N(input, offsafe, UPRV_LENGTHOF(input), Nvalue[i]);
        if(offsafe != fwd_N_safe[i]){
            log_err("ERROR: U16_FWD_N offset expected:%d, Got:%d\n", fwd_N_safe[i], offsafe);
        }
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    offunsafe=UPRV_LENGTHOF(input);
    for(i=0; i<UPRV_LENGTHOF(Nvalue)-2; i++){
        UTF16_BACK_N_UNSAFE(input, offunsafe, Nvalue[i]);
        if(offunsafe != back_N_unsafe[i]){
            log_err("ERROR: backward_N_unsafe offset expected:%d, Got:%d\n", back_N_unsafe[i], offunsafe);
        }
    }
#endif
    offunsafe=UPRV_LENGTHOF(input);
    for(i=0; i<UPRV_LENGTHOF(Nvalue)-2; i++){
        U16_BACK_N_UNSAFE(input, offunsafe, Nvalue[i]);
        if(offunsafe != back_N_unsafe[i]){
            log_err("ERROR: U16_BACK_N_UNSAFE offset expected:%d, Got:%d\n", back_N_unsafe[i], offunsafe);
        }
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    offsafe=UPRV_LENGTHOF(input);
    for(i=0; i<UPRV_LENGTHOF(Nvalue); i++){
        UTF16_BACK_N_SAFE(input, 0, offsafe, Nvalue[i]);
        if(offsafe != back_N_safe[i]){
            log_err("ERROR: backward_N_safe offset expected:%d, Got:%d\n", back_N_safe[i], offsafe);
        }
    }
#endif
    offsafe=UPRV_LENGTHOF(input);
    for(i=0; i<UPRV_LENGTHOF(Nvalue); i++){
        U16_BACK_N(input, 0, offsafe, Nvalue[i]);
        if(offsafe != back_N_safe[i]){
            log_err("ERROR: U16_BACK_N offset expected:%d, Got:%d\n", back_N_safe[i], offsafe);
        }
    }
}

static void TestSetChar(void){
    static UChar input[]={0x0061, 0xd800, 0xdc00, 0xdbff, 0xdfff, 0x0062, 0xd841, 0xd7ff, 0xd841, 0xdc41, 0xdc00, 0x0000};
    static uint16_t start_unsafe[]={0, 1, 1, 3, 3, 5, 6, 7, 8, 8, 9, 11};
    static uint16_t start_safe[]  ={0, 1, 1, 3, 3, 5, 6, 7, 8, 8, 10, 11};
    static uint16_t limit_unsafe[]={0, 1, 3, 3, 5, 5, 6, 8, 8, 10, 10, 11};
    static uint16_t limit_safe[]  ={0, 1, 3, 3, 5, 5, 6, 7, 8, 10, 10, 11};
    
    uint16_t i=0;
    uint16_t offset=0, setOffset=0;
    for(offset=0; offset<UPRV_LENGTHOF(input); offset++){
#if !U_HIDE_OBSOLETE_UTF_OLD_H
         setOffset=offset;
         UTF16_SET_CHAR_START_UNSAFE(input, setOffset);
         if(setOffset != start_unsafe[i]){
             log_err("ERROR: UTF16_SET_CHAR_START_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, start_unsafe[i], setOffset);
         }
#endif
         setOffset=offset;
         U16_SET_CP_START_UNSAFE(input, setOffset);
         if(setOffset != start_unsafe[i]){
             log_err("ERROR: U16_SET_CHAR_START_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, start_unsafe[i], setOffset);
         }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
         setOffset=offset;
         UTF16_SET_CHAR_START_SAFE(input, 0, setOffset);
         if(setOffset != start_safe[i]){
             log_err("ERROR: UTF16_SET_CHAR_START_SAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, start_safe[i], setOffset);
         }
#endif
         setOffset=offset;
         U16_SET_CP_START(input, 0, setOffset);
         if(setOffset != start_safe[i]){
             log_err("ERROR: U16_SET_CHAR_START failed for offset=%ld. Expected:%lx Got:%lx\n", offset, start_safe[i], setOffset);
         }

         if (offset > 0) {
#if !U_HIDE_OBSOLETE_UTF_OLD_H
             setOffset=offset;
             UTF16_SET_CHAR_LIMIT_UNSAFE(input, setOffset);
             if(setOffset != limit_unsafe[i]){
                 log_err("ERROR: UTF16_SET_CHAR_LIMIT_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, limit_unsafe[i], setOffset);
             }
#endif
             setOffset=offset;
             U16_SET_CP_LIMIT_UNSAFE(input, setOffset);
             if(setOffset != limit_unsafe[i]){
                 log_err("ERROR: U16_SET_CHAR_LIMIT_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, limit_unsafe[i], setOffset);
             }
         }

         setOffset=offset; 
         U16_SET_CP_LIMIT(input,0, setOffset, UPRV_LENGTHOF(input));
         if(setOffset != limit_safe[i]){
             log_err("ERROR: U16_SET_CHAR_LIMIT failed for offset=%ld. Expected:%lx Got:%lx\n", offset, limit_safe[i], setOffset);
         }

         i++;
    }
}

static void TestAppendChar(void){
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    static UChar s[5]={0x0061, 0x0062, 0x0063, 0x0064, 0x0000};
    static uint32_t test[]={
     /*append-position(unsafe),  CHAR to be appended  */
        0,                        0x20441,
        2,                        0x0028,
        2,                        0xdc00, 
        3,                        0xd800,
        1,                        0x20402,

    /*append-position(safe),     CHAR to be appended */
        0,                        0x20441,
        2,                        0xdc00, 
        3,                        0xd800,
        1,                        0x20402,
        3,                        0x20402,
        3,                        0x10402,
        2,                        0x10402,
   
    };
    static uint16_t movedOffset[]={
        /*offset-moved-to(unsafe)*/
          2,              /*for append-pos: 0 , CHAR 0x20441*/
          3,              
          3,
          4,
          3,
          /*offse-moved-to(safe)*/
          2,              /*for append-pos: 0, CHAR  0x20441*/
          3,
          4,
          3,
          4,
          4,
          4
    };
          
    static UChar result[][5]={
        /*unsafe*/
        {0xd841, 0xdc41, 0x0063, 0x0064, 0x0000},    
        {0x0061, 0x0062, 0x0028, 0x0064, 0x0000},
        {0x0061, 0x0062, 0xdc00, 0x0064, 0x0000},
        {0x0061, 0x0062, 0x0063, 0xd800, 0x0000},
        {0x0061, 0xd841, 0xdc02, 0x0064, 0x0000},

        /*safe*/
        {0xd841, 0xdc41, 0x0063, 0x0064, 0x0000},    
        {0x0061, 0x0062, 0xdc00, 0x0064, 0x0000},
        {0x0061, 0x0062, 0x0063, 0xd800, 0x0000},
        {0x0061, 0xd841, 0xdc02, 0x0064, 0x0000},
        {0x0061, 0x0062, 0x0063, UTF_ERROR_VALUE, 0x0000},
        {0x0061, 0x0062, 0x0063, UTF_ERROR_VALUE, 0x0000},
        {0x0061, 0x0062, 0xd801, 0xdc02, 0x0000},
      
        
    };
    uint16_t i, count=0;
    UChar *str=(UChar*)malloc(sizeof(UChar) * (u_strlen(s)+1));
    uint16_t offset;
    for(i=0; i<UPRV_LENGTHOF(test); i=(uint16_t)(i+2)){
        if(count<5){
            u_strcpy(str, s);
            offset=(uint16_t)test[i];
            UTF16_APPEND_CHAR_UNSAFE(str, offset, test[i+1]);
            if(offset != movedOffset[count]){
                log_err("ERROR: UTF16_APPEND_CHAR_UNSAFE failed to move the offset correctly for count=%d.\nExpectedOffset=%d  currentOffset=%d\n", 
                    count, movedOffset[count], offset);
                     
            }
            if(u_strcmp(str, result[count]) !=0){
                log_err("ERROR: UTF16_APPEND_CHAR_UNSAFE failed for count=%d. Expected:", count);
                printUChars(result[count]);
                printf("\nGot:");
                printUChars(str);
                printf("\n");
            }
        }else{
            u_strcpy(str, s);
            offset=(uint16_t)test[i];
            UTF16_APPEND_CHAR_SAFE(str, offset, (uint16_t)u_strlen(str), test[i+1]);
            if(offset != movedOffset[count]){
                log_err("ERROR: UTF16_APPEND_CHAR_SAFE failed to move the offset correctly for count=%d.\nExpectedOffset=%d  currentOffset=%d\n", 
                    count, movedOffset[count], offset);
                     
            }
            if(u_strcmp(str, result[count]) !=0){
                log_err("ERROR: UTF16_APPEND_CHAR_SAFE failed for count=%d. Expected:", count);
                printUChars(result[count]);
                printf("\nGot:");
                printUChars(str);
                printf("\n");
            }
        }
        count++;
    }  
    free(str);
#endif
}

static void TestAppend(void) {
    static const UChar32 codePoints[]={
        0x61, 0xdf, 0x901, 0x3040,
        0xac00, 0xd800, 0xdbff, 0xdcde,
        0xdffd, 0xe000, 0xffff, 0x10000,
        0x12345, 0xe0021, 0x10ffff, 0x110000,
        0x234567, 0x7fffffff, -1, -1000,
        0, 0x400
    };
    static const UChar expectUnsafe[]={
        0x61, 0xdf, 0x901, 0x3040,
        0xac00, 0xd800, 0xdbff, 0xdcde,
        0xdffd, 0xe000, 0xffff, 0xd800, 0xdc00,
        0xd808, 0xdf45, 0xdb40, 0xdc21, 0xdbff, 0xdfff, /* not 0x110000 */
        /* none from this line */
        0, 0x400
    }, expectSafe[]={
        0x61, 0xdf, 0x901, 0x3040,
        0xac00, 0xd800, 0xdbff, 0xdcde,
        0xdffd, 0xe000, 0xffff, 0xd800, 0xdc00,
        0xd808, 0xdf45, 0xdb40, 0xdc21, 0xdbff, 0xdfff, /* not 0x110000 */
        /* none from this line */
        0, 0x400
    };

    UChar buffer[100];
    UChar32 c;
    int32_t i, length;
    UBool isError, expectIsError, wrongIsError;

    length=0;
    for(i=0; i<UPRV_LENGTHOF(codePoints); ++i) {
        c=codePoints[i];
        if(c<0 || 0x10ffff<c) {
            continue; /* skip non-code points for U16_APPEND_UNSAFE */
        }

        U16_APPEND_UNSAFE(buffer, length, c);
    }
    if(length!=UPRV_LENGTHOF(expectUnsafe) || 0!=memcmp(buffer, expectUnsafe, length*U_SIZEOF_UCHAR)) {
        log_err("U16_APPEND_UNSAFE did not generate the expected output\n");
    }

    length=0;
    wrongIsError=false;
    for(i=0; i<UPRV_LENGTHOF(codePoints); ++i) {
        c=codePoints[i];
        expectIsError= c<0 || 0x10ffff<c; /* || U_IS_SURROGATE(c); */ /* surrogates in UTF-32 shouldn't be used, but it's okay to pass them around internally. */
        isError=false;

        U16_APPEND(buffer, length, UPRV_LENGTHOF(buffer), c, isError);
        wrongIsError|= isError!=expectIsError;
    }
    if(wrongIsError) {
        log_err("U16_APPEND did not set isError correctly\n");
    }
    if(length!=UPRV_LENGTHOF(expectSafe) || 0!=memcmp(buffer, expectSafe, length*U_SIZEOF_UCHAR)) {
        log_err("U16_APPEND did not generate the expected output\n");
    }
}

static void TestSurrogate(void){
    static UChar32 s[] = {0x10000, 0x10ffff, 0x50000, 0x100000, 0x1abcd};
    int i = 0;
    while (i < 5) {
        UChar first  = U16_LEAD(s[i]);
        UChar second = U16_TRAIL(s[i]);
        /* algorithm from the Unicode consortium */
        UChar firstresult  = (UChar)(((s[i] - 0x10000) / 0x400) + 0xD800);
        UChar secondresult = (UChar)(((s[i] - 0x10000) % 0x400) + 0xDC00);

        if (
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                first != UTF16_LEAD(s[i]) || first != UTF_FIRST_SURROGATE(s[i]) ||
#endif
                first != firstresult) {
            log_err("Failure in first surrogate in 0x%x expected to be 0x%x\n",
                    s[i], firstresult);
        }
        if (
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                second != UTF16_TRAIL(s[i]) || second != UTF_SECOND_SURROGATE(s[i]) ||
#endif
                second != secondresult) {
            log_err("Failure in second surrogate in 0x%x expected to be 0x%x\n",
                    s[i], secondresult);
        }
        i ++;
    }
}
