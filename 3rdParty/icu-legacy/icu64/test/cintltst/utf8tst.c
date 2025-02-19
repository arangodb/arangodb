// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1998-2014, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/*
* File utf8tst.c
*
* Modification History:
*
*   Date          Name        Description
*   07/24/2000    Madhu       Creation
*******************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/utf8.h"
#include "unicode/utf_old.h"
#include "cmemory.h"
#include "cintltst.h"

/* lenient UTF-8 ------------------------------------------------------------ */

/*
 * Lenient UTF-8 differs from conformant UTF-8 in that it allows surrogate
 * code points with their "natural" encoding.
 * Effectively, this allows a mix of UTF-8 and CESU-8 as well as encodings of
 * single surrogates.
 *
 * This is not conformant with UTF-8.
 *
 * Supplementary code points may be encoded as pairs of 3-byte sequences, but
 * the macros below do not attempt to assemble such pairs.
 */

#define L8_NEXT(s, i, length, c) { \
    (c)=(uint8_t)(s)[(i)++]; \
    if((c)>=0x80) { \
        if(U8_IS_LEAD(c)) { \
            (c)=utf8_nextCharSafeBody((const uint8_t *)s, &(i), (int32_t)(length), c, -2); \
        } else { \
            (c)=U_SENTINEL; \
        } \
    } \
}

#define L8_PREV(s, start, i, c) { \
    (c)=(uint8_t)(s)[--(i)]; \
    if((c)>=0x80) { \
        if((c)<=0xbf) { \
            (c)=utf8_prevCharSafeBody((const uint8_t *)s, start, &(i), c, -2); \
        } else { \
            (c)=U_SENTINEL; \
        } \
    } \
}

/* -------------------------------------------------------------------------- */

// Obsolete macros from obsolete unicode/utf_old.h, for some old test data.
#ifndef UTF8_ERROR_VALUE_1
#   define UTF8_ERROR_VALUE_1 0x15
#endif
#ifndef UTF8_ERROR_VALUE_2
#   define UTF8_ERROR_VALUE_2 0x9f
#endif
#ifndef UTF_ERROR_VALUE
#   define UTF_ERROR_VALUE 0xffff
#endif
#ifndef UTF_IS_ERROR
#   define UTF_IS_ERROR(c) \
        (((c)&0xfffe)==0xfffe || (c)==UTF8_ERROR_VALUE_1 || (c)==UTF8_ERROR_VALUE_2)
#endif

#if !U_HIDE_OBSOLETE_UTF_OLD_H
static void printUChars(const uint8_t *uchars, int16_t len){
    int16_t i=0;
    for(i=0; i<len; i++){
        log_err("0x%02x ", *(uchars+i));
    }
}
#endif

static void TestCodeUnitValues(void);
static void TestCharLength(void);
static void TestGetChar(void);
static void TestNextPrevChar(void);
static void TestNulTerminated(void);
static void TestNextPrevNonCharacters(void);
static void TestNextPrevCharUnsafe(void);
static void TestFwdBack(void);
static void TestFwdBackUnsafe(void);
static void TestSetChar(void);
static void TestSetCharUnsafe(void);
static void TestTruncateIfIncomplete(void);
static void TestAppendChar(void);
static void TestAppend(void);
static void TestSurrogates(void);

void addUTF8Test(TestNode** root);

void
addUTF8Test(TestNode** root)
{
    addTest(root, &TestCodeUnitValues,          "utf8tst/TestCodeUnitValues");
    addTest(root, &TestCharLength,              "utf8tst/TestCharLength");
    addTest(root, &TestGetChar,                 "utf8tst/TestGetChar");
    addTest(root, &TestNextPrevChar,            "utf8tst/TestNextPrevChar");
    addTest(root, &TestNulTerminated,           "utf8tst/TestNulTerminated");
    addTest(root, &TestNextPrevNonCharacters,   "utf8tst/TestNextPrevNonCharacters");
    addTest(root, &TestNextPrevCharUnsafe,      "utf8tst/TestNextPrevCharUnsafe");
    addTest(root, &TestFwdBack,                 "utf8tst/TestFwdBack");
    addTest(root, &TestFwdBackUnsafe,           "utf8tst/TestFwdBackUnsafe");
    addTest(root, &TestSetChar,                 "utf8tst/TestSetChar");
    addTest(root, &TestSetCharUnsafe,           "utf8tst/TestSetCharUnsafe");
    addTest(root, &TestTruncateIfIncomplete,    "utf8tst/TestTruncateIfIncomplete");
    addTest(root, &TestAppendChar,              "utf8tst/TestAppendChar");
    addTest(root, &TestAppend,                  "utf8tst/TestAppend");
    addTest(root, &TestSurrogates,              "utf8tst/TestSurrogates");
}

static void TestCodeUnitValues()
{
    static const uint8_t codeunit[]={0x00, 0x65, 0x7e, 0x7f, 0xc2, 0xc4, 0xf0, 0xf4, 0x80, 0x81, 0xbc, 0xbe,};

    int16_t i;
    for(i=0; i<UPRV_LENGTHOF(codeunit); i++){
        uint8_t c=codeunit[i];
        log_verbose("Testing code unit value of %x\n", c);
        if(i<4){
            if(
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                    !UTF8_IS_SINGLE(c) || UTF8_IS_LEAD(c) || UTF8_IS_TRAIL(c) ||
#endif
                    !U8_IS_SINGLE(c) || U8_IS_LEAD(c) || U8_IS_TRAIL(c)) {
                log_err("ERROR: 0x%02x is a single byte but results in single: %c lead: %c trail: %c\n",
                    c, U8_IS_SINGLE(c) ? 'y' : 'n', U8_IS_LEAD(c) ? 'y' : 'n', U8_IS_TRAIL(c) ? 'y' : 'n');
            }
        } else if(i< 8){
            if(
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                    !UTF8_IS_LEAD(c) || UTF8_IS_SINGLE(c) || UTF8_IS_TRAIL(c) ||
#endif
                    !U8_IS_LEAD(c) || U8_IS_SINGLE(c) || U8_IS_TRAIL(c)) {
                log_err("ERROR: 0x%02x is a lead byte but results in single: %c lead: %c trail: %c\n",
                    c, U8_IS_SINGLE(c) ? 'y' : 'n', U8_IS_LEAD(c) ? 'y' : 'n', U8_IS_TRAIL(c) ? 'y' : 'n');
            }
        } else if(i< 12){
            if(
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                    !UTF8_IS_TRAIL(c) || UTF8_IS_SINGLE(c) || UTF8_IS_LEAD(c) ||
#endif
                    !U8_IS_TRAIL(c) || U8_IS_SINGLE(c) || U8_IS_LEAD(c)){
                log_err("ERROR: 0x%02x is a trail byte but results in single: %c lead: %c trail: %c\n",
                    c, U8_IS_SINGLE(c) ? 'y' : 'n', U8_IS_LEAD(c) ? 'y' : 'n', U8_IS_TRAIL(c) ? 'y' : 'n');
            }
        }
    }
}

static void TestCharLength()
{
    static const uint32_t codepoint[]={
        1, 0x0061,
        1, 0x007f,
        2, 0x016f,
        2, 0x07ff,
        3, 0x0865,
        3, 0x20ac,
        4, 0x20402,
        4, 0x23456,
        4, 0x24506,
        4, 0x20402,
        4, 0x10402,
        3, 0xd7ff,
        3, 0xe000,

    };

    int16_t i;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    UBool multiple;
#endif
    for(i=0; i<UPRV_LENGTHOF(codepoint); i=(int16_t)(i+2)){
        UChar32 c=codepoint[i+1];
        if(
#if !U_HIDE_OBSOLETE_UTF_OLD_H
                UTF8_CHAR_LENGTH(c) != (uint16_t)codepoint[i] ||
#endif
                U8_LENGTH(c) != (uint16_t)codepoint[i]) {
            log_err("The no: of code units for %lx:- Expected: %d Got: %d\n", c, codepoint[i], U8_LENGTH(c));
        }else{
              log_verbose("The no: of code units for %lx is %d\n",c, U8_LENGTH(c));
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        multiple=(UBool)(codepoint[i] == 1 ? FALSE : TRUE);
        if(UTF8_NEED_MULTIPLE_UCHAR(c) != multiple){
              log_err("ERROR: UTF8_NEED_MULTIPLE_UCHAR failed for %lx\n", c);
        }
#endif
    }
}

static void TestGetChar()
{
    static const uint8_t input[]={
    /*  code unit,*/
        0x61,
        0x7f,
        0xe4,
        0xba,
        0x8c,
        0xF0,
        0x90,
        0x90,
        0x81,
        0xc0,
        0x65,
        0x31,
        0x9a,
        0xc9
    };
    static const UChar32 result[]={
    /*  codepoint-unsafe, codepoint-safe(not strict)  codepoint-safe(strict) */
        0x61,             0x61,                       0x61,
        0x7f,             0x7f,                       0x7f,
        0x4e8c,           0x4e8c,                     0x4e8c,
        0x4e8c,           0x4e8c,                     0x4e8c ,
        0x4e8c,           0x4e8c,                     0x4e8c,
        0x10401,          0x10401,                    0x10401 ,
        0x10401,          0x10401,                    0x10401 ,
        0x10401,          0x10401,                    0x10401 ,
        0x10401,          0x10401,                    0x10401,
        -1,               UTF8_ERROR_VALUE_1,         UTF8_ERROR_VALUE_1,
        0x65,             0x65,                       0x65,
        0x31,             0x31,                       0x31,
        -1,               UTF8_ERROR_VALUE_1,         UTF8_ERROR_VALUE_1,
        -1,               UTF8_ERROR_VALUE_1,         UTF8_ERROR_VALUE_1
    };
    uint16_t i=0;
    UChar32 c, expected;
    uint32_t offset=0;

    for(offset=0; offset<sizeof(input); offset++) {
        expected = result[i];
        if (expected >= 0 && offset < sizeof(input) - 1) {
#if !U_HIDE_OBSOLETE_UTF_OLD_H
            UTF8_GET_CHAR_UNSAFE(input, offset, c);
            if(c != expected) {
                log_err("ERROR: UTF8_GET_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n",
                        offset, expected, c);

            }
#endif
            U8_GET_UNSAFE(input, offset, c);
            if(c != expected) {
                log_err("ERROR: U8_GET_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n",
                        offset, expected, c);

            }
        }
        expected=result[i+1];
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        UTF8_GET_CHAR_SAFE(input, 0, offset, sizeof(input), c, FALSE);
        if(c != expected){
            log_err("ERROR: UTF8_GET_CHAR_SAFE failed for offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#endif
        U8_GET(input, 0, offset, sizeof(input), c);
        if(UTF_IS_ERROR(expected)) { expected=U_SENTINEL; }
        if(c != expected){
            log_err("ERROR: U8_GET failed for offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }

        U8_GET_OR_FFFD(input, 0, offset, sizeof(input), c);
        if(expected<0) { expected=0xfffd; }
        if(c != expected){
            log_err("ERROR: U8_GET_OR_FFFD failed for offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        UTF8_GET_CHAR_SAFE(input, 0, offset, sizeof(input), c, TRUE);
        if(c != result[i+2]){
            log_err("ERROR: UTF8_GET_CHAR_SAFE(strict) failed for offset=%ld. Expected:%lx Got:%lx\n", offset, result[i+2], c);
        }
#endif
        i=(uint16_t)(i+3);
    }
}

static void TestNextPrevChar() {
    static const uint8_t input[]={
        0x61,
        0xf0, 0x90, 0x90, 0x81,
        0xc0, 0x80,  // non-shortest form
        0xf3, 0xbe,  // truncated
        0xc2,  // truncated
        0x61,
        0x81, 0x90, 0x90, 0xf0,  // "backwards" sequence
        0x00
    };
    static const UChar32 result[]={
    /*  next_safe_ns        next_safe_s          prev_safe_ns        prev_safe_s */
        0x0061,             0x0061,              0x0000,             0x0000,
        0x10401,            0x10401,             UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  0x61,               0x61,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        UTF8_ERROR_VALUE_2, UTF8_ERROR_VALUE_2,  UTF8_ERROR_VALUE_2, UTF8_ERROR_VALUE_2,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        0x61,               0x61,                UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  0x10401,            0x10401,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF_ERROR_VALUE,    UTF_ERROR_VALUE,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF8_ERROR_VALUE_2, UTF8_ERROR_VALUE_2,
        UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,  UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_1,
        0x0000,             0x0000,              0x0061,             0x0061
    };
    static const int32_t movedOffset[]={
    /*  next_safe    prev_safe_s */
        1,           15,
        5,           14,
        3,           13,
        4,           12,
        5,           11,
        6,           10,
        7,           9,
        9,           7,
        9,           7,
        10,          6,
        11,          5,
        12,          1,
        13,          1,
        14,          1,
        15,          1,
        16,          0,
    };

    UChar32 c, expected;
    uint32_t i=0, j=0;
    uint32_t offset=0;
    int32_t setOffset=0;
    for(offset=0; offset<sizeof(input); offset++){
        expected=result[i];  // next_safe_ns
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        setOffset=offset;
        UTF8_NEXT_CHAR_SAFE(input, setOffset, sizeof(input), c, FALSE);
        if(setOffset != movedOffset[j]) {
            log_err("ERROR: UTF8_NEXT_CHAR_SAFE failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[j], setOffset);
        }
        if(c != expected) {
            log_err("ERROR: UTF8_NEXT_CHAR_SAFE failed at offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#endif
        setOffset=offset;
        U8_NEXT(input, setOffset, sizeof(input), c);
        if(setOffset != movedOffset[j]) {
            log_err("ERROR: U8_NEXT failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[j], setOffset);
        }
        if(UTF_IS_ERROR(expected)) { expected=U_SENTINEL; }
        if(c != expected) {
            log_err("ERROR: U8_NEXT failed at offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }

        setOffset=offset;
        U8_NEXT_OR_FFFD(input, setOffset, sizeof(input), c);
        if(setOffset != movedOffset[j]) {
            log_err("ERROR: U8_NEXT_OR_FFFD failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[j], setOffset);
        }
        if(expected<0) { expected=0xfffd; }
        if(c != expected) {
            log_err("ERROR: U8_NEXT_OR_FFFD failed at offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        setOffset=offset;
        UTF8_NEXT_CHAR_SAFE(input, setOffset, sizeof(input), c, TRUE);
        if(setOffset != movedOffset[j]) {
            log_err("ERROR: UTF8_NEXT_CHAR_SAFE(strict) failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[j], setOffset);
        }
        expected=result[i+1];  // next_safe_s
        if(c != expected) {
            log_err("ERROR: UTF8_NEXT_CHAR_SAFE(strict) failed at offset=%ld. Expected:%lx Got:%lx\n",
                    offset, expected, c);
        }
#endif
        i=i+4;
        j=j+2;
    }

    i=j=0;
    for(offset=sizeof(input); offset > 0; --offset){
        expected=result[i+2];  // prev_safe_ns
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        setOffset=offset;
        UTF8_PREV_CHAR_SAFE(input, 0, setOffset, c, FALSE);
        if(setOffset != movedOffset[j+1]) {
            log_err("ERROR: UTF8_PREV_CHAR_SAFE failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[j+1], setOffset);
        }
        if(c != expected) {
            log_err("ERROR: UTF8_PREV_CHAR_SAFE failed at offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#endif
        setOffset=offset;
        U8_PREV(input, 0, setOffset, c);
        if(setOffset != movedOffset[j+1]) {
            log_err("ERROR: U8_PREV failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[j+1], setOffset);
        }
        if(UTF_IS_ERROR(expected)) { expected=U_SENTINEL; }
        if(c != expected) {
            log_err("ERROR: U8_PREV failed at offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }

        setOffset=offset;
        U8_PREV_OR_FFFD(input, 0, setOffset, c);
        if(setOffset != movedOffset[j+1]) {
            log_err("ERROR: U8_PREV_OR_FFFD failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[j+1], setOffset);
        }
        if(expected<0) { expected=0xfffd; }
        if(c != expected) {
            log_err("ERROR: U8_PREV_OR_FFFD failed at offset=%ld. Expected:%lx Got:%lx\n", offset, expected, c);
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        setOffset=offset;
        UTF8_PREV_CHAR_SAFE(input, 0,  setOffset, c, TRUE);
        if(setOffset != movedOffset[j+1]) {
            log_err("ERROR: UTF8_PREV_CHAR_SAFE(strict) failed to move the offset correctly at %d\n ExpectedOffset:%d Got %d\n",
                offset, movedOffset[j+1], setOffset);
        }
        expected=result[i+3];  // prev_safe_s
        if(c != expected) {
            log_err("ERROR: UTF8_PREV_CHAR_SAFE(strict) failed at offset=%ld. Expected:%lx Got:%lx\n",
                    offset, expected, c);
        }
#endif
        i=i+4;
        j=j+2;
    }
}

/* keep this in sync with utf16tst.c's TestNulTerminated() */
static void TestNulTerminated() {
    static const uint8_t input[]={
        /*  0 */  0x61,
        /*  1 */  0xf0, 0x90, 0x90, 0x81,
        /*  5 */  0xc0,
        /*  6 */  0x80,
        /*  7 */  0xdf, 0x80,
        /*  9 */  0xc2,
        /* 10 */  0x62,
        /* 11 */  0xfd,
        /* 12 */  0xbe,
        /* 13 */  0xe0, 0xa0, 0x80,
        /* 16 */  0xe2, 0x82, 0xac,
        /* 19 */  0xf0, 0x90, 0x90,
        /* 22 */  0x00
        /* 23 */
    };
    static const UChar32 result[]={
        0x61,
        0x10401,
        U_SENTINEL,  // C0 not a lead byte
        U_SENTINEL,  // 80
        0x7c0,
        U_SENTINEL,  // C2
        0x62,
        U_SENTINEL,  // FD not a lead byte
        U_SENTINEL,  // BE
        0x800,
        0x20ac,
        U_SENTINEL,  // truncated F0 90 90
        0
    };

    UChar32 c, c2, expected;
    int32_t i0, i=0, j, k, expectedIndex;
    int32_t cpIndex=0;
    do {
        i0=i;
        U8_NEXT(input, i, -1, c);
        expected=result[cpIndex];
        if(c!=expected) {
            log_err("U8_NEXT(from %d)=U+%04x != U+%04x\n", i0, c, expected);
        }
        j=i0;
        U8_NEXT_OR_FFFD(input, j, -1, c);
        if(expected<0) { expected=0xfffd; }
        if(c!=expected) {
            log_err("U8_NEXT_OR_FFFD(from %d)=U+%04x != U+%04x\n", i0, c, expected);
        }
        if(j!=i) {
            log_err("U8_NEXT_OR_FFFD() moved to index %d but U8_NEXT() moved to %d\n", j, i);
        }
        j=i0;
        U8_FWD_1(input, j, -1);
        if(j!=i) {
            log_err("U8_FWD_1() moved to index %d but U8_NEXT() moved to %d\n", j, i);
        }
        ++cpIndex;
        /*
         * Move by this many code points from the start.
         * U8_FWD_N() stops at the end of the string, that is, at the NUL if necessary.
         */
        expectedIndex= (c==0) ? i-1 : i;
        k=0;
        U8_FWD_N(input, k, -1, cpIndex);
        if(k!=expectedIndex) {
            log_err("U8_FWD_N(code points from 0) moved to index %d but expected %d\n", k, expectedIndex);
        }
    } while(c!=0);

    i=0;
    do {
        j=i0=i;
        U8_NEXT(input, i, -1, c);
        do {
            U8_GET(input, 0, j, -1, c2);
            if(c2!=c) {
                log_err("U8_NEXT(from %d)=U+%04x != U+%04x=U8_GET(at %d)\n", i0, c, c2, j);
            }
            U8_GET_OR_FFFD(input, 0, j, -1, c2);
            expected= (c>=0) ? c : 0xfffd;
            if(c2!=expected) {
                log_err("U8_NEXT_OR_FFFD(from %d)=U+%04x != U+%04x=U8_GET_OR_FFFD(at %d)\n", i0, expected, c2, j);
            }
            /* U8_SET_CP_LIMIT moves from a non-lead byte to the limit of the code point */
            k=j+1;
            U8_SET_CP_LIMIT(input, 0, k, -1);
            if(k!=i) {
                log_err("U8_NEXT() moved to %d but U8_SET_CP_LIMIT(%d) moved to %d\n", i, j+1, k);
            }
        } while(++j<i);
    } while(c!=0);
}

static void TestNextPrevNonCharacters() {
    /* test non-characters */
    static const uint8_t nonChars[]={
        0xef, 0xb7, 0x90,       /* U+fdd0 */
        0xef, 0xbf, 0xbf,       /* U+feff */
        0xf0, 0x9f, 0xbf, 0xbe, /* U+1fffe */
        0xf0, 0xbf, 0xbf, 0xbf, /* U+3ffff */
        0xf4, 0x8f, 0xbf, 0xbe  /* U+10fffe */
    };

    UChar32 ch;
    int32_t idx;

    for(idx=0; idx<(int32_t)sizeof(nonChars);) {
        U8_NEXT(nonChars, idx, sizeof(nonChars), ch);
        if(!U_IS_UNICODE_NONCHAR(ch)) {
            log_err("U8_NEXT(before %d) failed to read a non-character\n", idx);
        }
    }
    for(idx=(int32_t)sizeof(nonChars); idx>0;) {
        U8_PREV(nonChars, 0, idx, ch);
        if(!U_IS_UNICODE_NONCHAR(ch)) {
            log_err("U8_PREV(at %d) failed to read a non-character\n", idx);
        }
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    for(idx=0; idx<(int32_t)sizeof(nonChars);) {
        UChar32 expected= nonChars[idx]<0xf0 ? 0xffff : 0x10ffff;
        UTF8_NEXT_CHAR_SAFE(nonChars, idx, sizeof(nonChars), ch, TRUE);
        if(ch!=expected) {
            log_err("UTF8_NEXT_CHAR_SAFE(strict, before %d) failed to read a non-character\n", idx);
        }
    }
    for(idx=(int32_t)sizeof(nonChars); idx>0;) {
        UTF8_PREV_CHAR_SAFE(nonChars, 0, idx, ch, TRUE);
        UChar32 expected= nonChars[idx]<0xf0 ? 0xffff : 0x10ffff;
        if(ch!=expected) {
            log_err("UTF8_PREV_CHAR_SAFE(strict, at %d) failed to read a non-character\n", idx);
        }
    }
#endif
}

static void TestNextPrevCharUnsafe() {
    /*
     * Use a (mostly) well-formed UTF-8 string and test at code point boundaries.
     * The behavior of _UNSAFE macros for ill-formed strings is undefined.
     */
    static const uint8_t input[]={
        0x61,
        0xf0, 0x90, 0x90, 0x81,
        0xc0, 0x80,  /* non-shortest form */
        0xe2, 0x82, 0xac,
        0xc2, 0xa1,
        0xf4, 0x8f, 0xbf, 0xbf,
        0x00
    };
    static const UChar32 codePoints[]={
        0x61,
        0x10401,
        -1,
        0x20ac,
        0xa1,
        0x10ffff,
        0
    };

    UChar32 c, expected;
    int32_t i;
    uint32_t offset;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    for(i=0, offset=0; offset<sizeof(input); ++i) {
        UTF8_NEXT_CHAR_UNSAFE(input, offset, c);
        expected = codePoints[i];
        if(expected >= 0 && c != expected) {
            log_err("ERROR: UTF8_NEXT_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n",
                    offset, expected, c);
        }
        if(offset==6) {
            // The obsolete UTF8_NEXT_CHAR_UNSAFE() skips 1+UTF8_COUNT_TRAIL_BYTES(lead) bytes
            // while the new one skips C0 80 together.
            ++offset;
        }
    }
#endif
    for(i=0, offset=0; offset<sizeof(input); ++i) {
        U8_NEXT_UNSAFE(input, offset, c);
        expected = codePoints[i];
        if(expected >= 0 && c != expected) {
            log_err("ERROR: U8_NEXT_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n",
                    offset, expected, c);
        }
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    for(i=UPRV_LENGTHOF(codePoints)-1, offset=sizeof(input); offset > 0; --i){
        UTF8_PREV_CHAR_UNSAFE(input, offset, c);
        expected = codePoints[i];
        if(expected >= 0 && c != expected) {
            log_err("ERROR: UTF8_PREV_CHAR_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n",
                    offset, expected, c);
        }
    }
#endif
    for(i=UPRV_LENGTHOF(codePoints)-1, offset=sizeof(input); offset > 0; --i){
        U8_PREV_UNSAFE(input, offset, c);
        expected = codePoints[i];
        if(expected >= 0 && c != expected) {
            log_err("ERROR: U8_PREV_UNSAFE failed for offset=%ld. Expected:%lx Got:%lx\n",
                    offset, expected, c);
        }
    }
}

static void TestFwdBack() {
    static const uint8_t input[]={
        0x61,
        0xF0, 0x90, 0x90, 0x81,
        0xff,
        0x62,
        0xc0,
        0x80,
        0x7f,
        0x8f,
        0xc0,
        0x63,
        0x81,
        0x90,
        0x90,
        0xF0,
        0x00
    };
    static const uint16_t fwd_safe[]   ={1, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
    static const uint16_t back_safe[]  ={17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 1, 0};

    static const uint16_t Nvalue[]= {0, 1, 2, 4, 1, 2, 1, 5};
    static const uint16_t fwd_N_safe[]   ={0, 1, 6, 10, 11, 13, 14, 18}; /*safe macro keeps it at the end of the string */
    static const uint16_t back_N_safe[]  ={18, 17, 15, 11, 10, 8, 7, 0};

    uint32_t offsafe=0;

    uint32_t i=0;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    while(offsafe < sizeof(input)){
        UTF8_FWD_1_SAFE(input, offsafe, sizeof(input));
        if(offsafe != fwd_safe[i]){
            log_err("ERROR: Forward_safe offset expected:%d, Got:%d\n", fwd_safe[i], offsafe);
        }
        i++;
    }
#endif
    offsafe=0;
    i=0;
    while(offsafe < sizeof(input)){
        U8_FWD_1(input, offsafe, sizeof(input));
        if(offsafe != fwd_safe[i]){
            log_err("ERROR: U8_FWD_1 offset expected:%d, Got:%d\n", fwd_safe[i], offsafe);
        }
        i++;
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    i=0;
    offsafe=sizeof(input);
    while(offsafe > 0){
        UTF8_BACK_1_SAFE(input, 0,  offsafe);
        if(offsafe != back_safe[i]){
            log_err("ERROR: Backward_safe offset expected:%d, Got:%d\n", back_safe[i], offsafe);
        }
        i++;
    }
#endif
    i=0;
    offsafe=sizeof(input);
    while(offsafe > 0){
        U8_BACK_1(input, 0,  offsafe);
        if(offsafe != back_safe[i]){
            log_err("ERROR: U8_BACK_1 offset expected:%d, Got:%d\n", back_safe[i], offsafe);
        }
        i++;
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    offsafe=0;
    for(i=0; i<UPRV_LENGTHOF(Nvalue); i++){
        UTF8_FWD_N_SAFE(input, offsafe, sizeof(input), Nvalue[i]);
        if(offsafe != fwd_N_safe[i]){
            log_err("ERROR: Forward_N_safe offset=%d expected:%d, Got:%d\n", i, fwd_N_safe[i], offsafe);
        }

    }
#endif
    offsafe=0;
    for(i=0; i<UPRV_LENGTHOF(Nvalue); i++){
        U8_FWD_N(input, offsafe, sizeof(input), Nvalue[i]);
        if(offsafe != fwd_N_safe[i]){
            log_err("ERROR: U8_FWD_N offset=%d expected:%d, Got:%d\n", i, fwd_N_safe[i], offsafe);
        }

    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    offsafe=sizeof(input);
    for(i=0; i<UPRV_LENGTHOF(Nvalue); i++){
        UTF8_BACK_N_SAFE(input, 0, offsafe, Nvalue[i]);
        if(offsafe != back_N_safe[i]){
            log_err("ERROR: backward_N_safe offset=%d expected:%d, Got:%ld\n", i, back_N_safe[i], offsafe);
        }
    }
#endif
    offsafe=sizeof(input);
    for(i=0; i<UPRV_LENGTHOF(Nvalue); i++){
        U8_BACK_N(input, 0, offsafe, Nvalue[i]);
        if(offsafe != back_N_safe[i]){
            log_err("ERROR: U8_BACK_N offset=%d expected:%d, Got:%ld\n", i, back_N_safe[i], offsafe);
        }
    }
}

/**
* Ticket #13636 - Visual Studio 2017 has problems optimizing this function.
* As a workaround, we will turn off optimization just for this function on VS2017 and above.
*/
#if defined(_MSC_VER) && (_MSC_VER > 1900)
#pragma optimize( "", off )
#endif

static void TestFwdBackUnsafe() {
    /*
     * Use a (mostly) well-formed UTF-8 string and test at code point boundaries.
     * The behavior of _UNSAFE macros for ill-formed strings is undefined.
     */
    static const uint8_t input[]={
        0x61,
        0xf0, 0x90, 0x90, 0x81,
        0xc0, 0x80,  /* non-shortest form */
        0xe2, 0x82, 0xac,
        0xc2, 0xa1,
        0xf4, 0x8f, 0xbf, 0xbf,
        0x00
    };
    // forward unsafe skips only C0
    static const int8_t boundaries[]={ 0, 1, 5, 6, 7, 10, 12, 16, 17 };
    // backward unsafe skips C0 80 together
    static const int8_t backBoundaries[]={ 0, 1, 5, 7, 10, 12, 16, 17 };

    int32_t offset;
    int32_t i;
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    for(i=1, offset=0; offset<UPRV_LENGTHOF(input); ++i) {
        UTF8_FWD_1_UNSAFE(input, offset);
        if(offset != boundaries[i]){
            log_err("ERROR: UTF8_FWD_1_UNSAFE offset expected:%d, Got:%d\n", boundaries[i], offset);
        }
    }
#endif
    for(i=1, offset=0; offset<UPRV_LENGTHOF(input); ++i) {
        U8_FWD_1_UNSAFE(input, offset);
        if(offset != boundaries[i]){
            log_err("ERROR: U8_FWD_1_UNSAFE offset expected:%d, Got:%d\n", boundaries[i], offset);
        }
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    for(i=UPRV_LENGTHOF(backBoundaries)-2, offset=UPRV_LENGTHOF(input); offset>0; --i) {
        UTF8_BACK_1_UNSAFE(input, offset);
        if(offset != backBoundaries[i]){
            log_err("ERROR: UTF8_BACK_1_UNSAFE offset expected:%d, Got:%d\n", backBoundaries[i], offset);
        }
    }
#endif
    for(i=UPRV_LENGTHOF(backBoundaries)-2, offset=UPRV_LENGTHOF(input); offset>0; --i) {
        U8_BACK_1_UNSAFE(input, offset);
        if(offset != backBoundaries[i]){
            log_err("ERROR: U8_BACK_1_UNSAFE offset expected:%d, Got:%d\n", backBoundaries[i], offset);
        }
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    for(i=0; i<UPRV_LENGTHOF(boundaries); ++i) {
        offset=0;
        UTF8_FWD_N_UNSAFE(input, offset, i);
        if(offset != boundaries[i]) {
            log_err("ERROR: UTF8_FWD_N_UNSAFE offset expected:%d, Got:%d\n", boundaries[i], offset);
        }
    }
#endif
    for(i=0; i<UPRV_LENGTHOF(boundaries); ++i) {
        offset=0;
        U8_FWD_N_UNSAFE(input, offset, i);
        if(offset != boundaries[i]) {
            log_err("ERROR: U8_FWD_N_UNSAFE offset expected:%d, Got:%d\n", boundaries[i], offset);
        }
    }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    for(i=0; i<UPRV_LENGTHOF(backBoundaries); ++i) {
        int32_t j=UPRV_LENGTHOF(backBoundaries)-1-i;
        offset=UPRV_LENGTHOF(input);
        UTF8_BACK_N_UNSAFE(input, offset, i);
        if(offset != backBoundaries[j]) {
            log_err("ERROR: UTF8_BACK_N_UNSAFE offset expected:%d, Got:%d\n", backBoundaries[j], offset);
        }
    }
#endif
    for(i=0; i<UPRV_LENGTHOF(backBoundaries); ++i) {
        int32_t j=UPRV_LENGTHOF(backBoundaries)-1-i;
        offset=UPRV_LENGTHOF(input);
        U8_BACK_N_UNSAFE(input, offset, i);
        if(offset != backBoundaries[j]) {
            log_err("ERROR: U8_BACK_N_UNSAFE offset expected:%d, Got:%d\n", backBoundaries[j], offset);
        }
    }
}

/**
* Ticket #13636 - Turn optimization back on.
*/
#if defined(_MSC_VER) && (_MSC_VER > 1900)
#pragma optimize( "", on )
#endif

static void TestSetChar() {
    static const uint8_t input[]
        = {0x61, 0xe4, 0xba, 0x8c, 0x7f, 0xfe, 0x62, 0xc5, 0x7f, 0x61, 0x80, 0x80, 0xe0, 0x00 };
    static const int16_t start_safe[]
        = {0,    1,    1,    1,    4,    5,    6,    7,    8,    9,    10,   11,   12,   13,  14 };
    static const int16_t limit_safe[]
        = {0,    1,    4,    4,    4,    5,    6,    7,    8,    9,    10,   11,   12,   13,  14 };

    uint32_t i=0;
    int32_t offset=0, setOffset=0;
    for(offset=0; offset<=UPRV_LENGTHOF(input); offset++){
        if (offset<UPRV_LENGTHOF(input)){
#if !U_HIDE_OBSOLETE_UTF_OLD_H
            setOffset=offset;
            UTF8_SET_CHAR_START_SAFE(input, 0, setOffset);
            if(setOffset != start_safe[i]){
                log_err("ERROR: UTF8_SET_CHAR_START_SAFE failed for offset=%ld. Expected:%ld Got:%ld\n", offset, start_safe[i], setOffset);
            }
#endif
            setOffset=offset;
            U8_SET_CP_START(input, 0, setOffset);
            if(setOffset != start_safe[i]){
                log_err("ERROR: U8_SET_CP_START failed for offset=%ld. Expected:%ld Got:%ld\n", offset, start_safe[i], setOffset);
            }
        }
#if !U_HIDE_OBSOLETE_UTF_OLD_H
        setOffset=offset;
        UTF8_SET_CHAR_LIMIT_SAFE(input,0, setOffset, sizeof(input));
        if(setOffset != limit_safe[i]){
            log_err("ERROR: UTF8_SET_CHAR_LIMIT_SAFE failed for offset=%ld. Expected:%ld Got:%ld\n", offset, limit_safe[i], setOffset);
        }
#endif
        setOffset=offset;
        U8_SET_CP_LIMIT(input,0, setOffset, sizeof(input));
        if(setOffset != limit_safe[i]){
            log_err("ERROR: U8_SET_CP_LIMIT failed for offset=%ld. Expected:%ld Got:%ld\n", offset, limit_safe[i], setOffset);
        }

        i++;
    }
}

static void TestSetCharUnsafe() {
    static const uint8_t input[]
        = {0x61, 0xe4, 0xba, 0x8c, 0x7f, 0x2e, 0x62, 0xc5, 0x7f, 0x61, 0x80, 0x80, 0xe0, 0x80, 0x80, 0x00 };
    static const int16_t start_unsafe[]
        = {0,    1,    1,    1,    4,    5,    6,    7,    8,    9,    9,    9,    12,   12,   12,   15 };
    static const int16_t limit_unsafe[]
        = {0,    1,    4,    4,    4,    5,    6,    7,    9,    9,    10,   10,   10,   15,   15,   15,   16 };

    uint32_t i=0;
    int32_t offset=0, setOffset=0;
    for(offset=0; offset<=UPRV_LENGTHOF(input); offset++){
        if (offset<UPRV_LENGTHOF(input)){
#if !U_HIDE_OBSOLETE_UTF_OLD_H
            setOffset=offset;
            UTF8_SET_CHAR_START_UNSAFE(input, setOffset);
            if(setOffset != start_unsafe[i]){
                log_err("ERROR: UTF8_SET_CHAR_START_UNSAFE failed for offset=%ld. Expected:%ld Got:%ld\n", offset, start_unsafe[i], setOffset);
            }
#endif
            setOffset=offset;
            U8_SET_CP_START_UNSAFE(input, setOffset);
            if(setOffset != start_unsafe[i]){
                log_err("ERROR: U8_SET_CP_START_UNSAFE failed for offset=%ld. Expected:%ld Got:%ld\n", offset, start_unsafe[i], setOffset);
            }
        }

        if (offset != 0) { /* Can't have it go off the end of the array */
#if !U_HIDE_OBSOLETE_UTF_OLD_H
            setOffset=offset;
            UTF8_SET_CHAR_LIMIT_UNSAFE(input, setOffset);
            if(setOffset != limit_unsafe[i]){
                log_err("ERROR: UTF8_SET_CHAR_LIMIT_UNSAFE failed for offset=%ld. Expected:%ld Got:%ld\n", offset, limit_unsafe[i], setOffset);
            }
#endif
            setOffset=offset;
            U8_SET_CP_LIMIT_UNSAFE(input, setOffset);
            if(setOffset != limit_unsafe[i]){
                log_err("ERROR: U8_SET_CP_LIMIT_UNSAFE failed for offset=%ld. Expected:%ld Got:%ld\n", offset, limit_unsafe[i], setOffset);
            }
        }

        i++;
    }
}

static void TestTruncateIfIncomplete() {
    // Difference from U8_SET_CP_START():
    // U8_TRUNCATE_IF_INCOMPLETE() does not look at s[length].
    // Therefore, if the last byte is a lead byte, then this macro truncates
    // even if the byte at the input index cannot continue a valid sequence
    // (including when that is not a trail byte).
    // On the other hand, if the last byte is a trail byte, then the two macros behave the same.
    static const struct {
        const char *s;
        int32_t expected;
    } cases[] = {
        { "", 0 },
        { "a", 1 },
        { "\x80", 1 },
        { "\xC1", 1 },
        { "\xC2", 0 },
        { "\xE0", 0 },
        { "\xF4", 0 },
        { "\xF5", 1 },
        { "\x80\x80", 2 },
        { "\xC2\xA0", 2 },
        { "\xE0\x9F", 2 },
        { "\xE0\xA0", 0 },
        { "\xED\x9F", 0 },
        { "\xED\xA0", 2 },
        { "\xF0\x8F", 2 },
        { "\xF0\x90", 0 },
        { "\xF4\x8F", 0 },
        { "\xF4\x90", 2 },
        { "\xF5\x80", 2 },
        { "\x80\x80\x80", 3 },
        { "\xC2\xA0\x80", 3 },
        { "\xE0\xA0\x80", 3 },
        { "\xF0\x8F\x80", 3 },
        { "\xF0\x90\x80", 0 },
        { "\xF4\x8F\x80", 0 },
        { "\xF4\x90\x80", 3 },
        { "\xF5\x80\x80", 3 },
        { "\x80\x80\x80\x80", 4 },
        { "\xC2\xA0\x80\x80", 4 },
        { "\xE0\xA0\x80\x80", 4 },
        { "\xF0\x90\x80\x80", 4 },
        { "\xF5\x80\x80\x80", 4 }
    };
    int32_t i;
    for (i = 0; i < UPRV_LENGTHOF(cases); ++i) {
        const char *s = cases[i].s;
        int32_t expected = cases[i].expected;
        int32_t length = (int32_t)strlen(s);
        int32_t adjusted = length;
        U8_TRUNCATE_IF_INCOMPLETE(s, 0, adjusted);
        if (adjusted != expected) {
            log_err("ERROR: U8_TRUNCATE_IF_INCOMPLETE failed for i=%d, length=%d. Expected:%d Got:%d\n",
                    (int)i, (int)length, (int)expected, (int)adjusted);
        }
    }
}

static void TestAppendChar(){
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    static const uint8_t s[11]={0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00};
    static const uint32_t test[]={
    /*  append-position(unsafe),  CHAR to be appended */
        0,                        0x10401,
        2,                        0x0028,
        2,                        0x007f,
        3,                        0xd801,
        1,                        0x20402,
        8,                        0x10401,
        5,                        0xc0,
        5,                        0xc1,
        5,                        0xfd,
        6,                        0x80,
        6,                        0x81,
        6,                        0xbf,
        7,                        0xfe,

    /*  append-position(safe),    CHAR to be appended */
        0,                        0x10401,
        2,                        0x0028,
        3,                        0x7f,
        3,                        0xd801,   /* illegal for UTF-8 starting with Unicode 3.2 */
        1,                        0x20402,
        9,                        0x10401,
        5,                        0xc0,
        5,                        0xc1,
        5,                        0xfd,
        6,                        0x80,
        6,                        0x81,
        6,                        0xbf,
        7,                        0xfe,

    };
    static const uint16_t movedOffset[]={
    /* offset-moved-to(unsafe) */
          4,              /*for append-pos: 0 , CHAR 0x10401*/
          3,
          3,
          6,
          5,
          12,
          7,
          7,
          7,
          8,
          8,
          8,
          9,

    /* offset-moved-to(safe) */
          4,              /*for append-pos: 0, CHAR  0x10401*/
          3,
          4,
          6,
          5,
          11,
          7,
          7,
          7,
          8,
          8,
          8,
          9,

    };

    static const uint8_t result[][11]={
        /*unsafe*/
        {0xF0, 0x90, 0x90, 0x81, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x28, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x7f, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0xed, 0xa0, 0x81, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0xF0, 0xa0, 0x90, 0x82, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0xF0, 0x90, 0x90},

        {0x61, 0x62, 0x63, 0x64, 0x65, 0xc3, 0x80, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0xc3, 0x81, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0xc3, 0xbd, 0x68, 0x69, 0x6a, 0x00},

        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0xc2, 0x80, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0xc2, 0x81, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0xc2, 0xbf, 0x69, 0x6a, 0x00},

        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0xc3, 0xbe, 0x6a, 0x00},
        /*safe*/
        {0xF0, 0x90, 0x90, 0x81, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x28, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x7f, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0xef, 0xbf, 0xbf, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0xF0, 0xa0, 0x90, 0x82, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xc2, 0x9f}, /*gets UTF8_ERROR_VALUE_2 which takes 2 bytes 0xc0, 0x9f*/

        {0x61, 0x62, 0x63, 0x64, 0x65, 0xc3, 0x80, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0xc3, 0x81, 0x68, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0xc3, 0xbd, 0x68, 0x69, 0x6a, 0x00},

        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0xc2, 0x80, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0xc2, 0x81, 0x69, 0x6a, 0x00},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0xc2, 0xbf, 0x69, 0x6a, 0x00},

        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0xc3, 0xbe, 0x6a, 0x00},

    };
    uint16_t i, count=0;
    uint8_t str[12];
    uint32_t offset;
/*    UChar32 c=0;*/
    uint16_t size=UPRV_LENGTHOF(s);
    for(i=0; i<UPRV_LENGTHOF(test); i=(uint16_t)(i+2)){
        uprv_memcpy(str, s, size);
        offset=test[i];
        if(count<13){
            UTF8_APPEND_CHAR_UNSAFE(str, offset, test[i+1]);
            if(offset != movedOffset[count]){
                log_err("ERROR: UTF8_APPEND_CHAR_UNSAFE failed to move the offset correctly for count=%d.\nExpectedOffset=%d  currentOffset=%d\n",
                    count, movedOffset[count], offset);

            }
            if(uprv_memcmp(str, result[count], size) !=0){
                log_err("ERROR: UTF8_APPEND_CHAR_UNSAFE failed for count=%d. \nExpected:", count);
                printUChars(result[count], size);
                log_err("\nGot:      ");
                printUChars(str, size);
                log_err("\n");
            }
        }else{
            UTF8_APPEND_CHAR_SAFE(str, offset, size, test[i+1]);
            if(offset != movedOffset[count]){
                log_err("ERROR: UTF8_APPEND_CHAR_SAFE failed to move the offset correctly for count=%d.\nExpectedOffset=%d  currentOffset=%d\n",
                    count, movedOffset[count], offset);

            }
            if(uprv_memcmp(str, result[count], size) !=0){
                log_err("ERROR: UTF8_APPEND_CHAR_SAFE failed for count=%d. \nExpected:", count);
                printUChars(result[count], size);
                log_err("\nGot:     ");
                printUChars(str, size);
                log_err("\n");
            }
            /*call the API instead of MACRO
            uprv_memcpy(str, s, size);
            offset=test[i];
            c=test[i+1];
            if((uint32_t)(c)<=0x7f) {
                  (str)[(offset)++]=(uint8_t)(c);
            } else {
                 (offset)=utf8_appendCharSafeBody(str, (int32_t)(offset), (int32_t)(size), c);
            }
            if(offset != movedOffset[count]){
                log_err("ERROR: utf8_appendCharSafeBody() failed to move the offset correctly for count=%d.\nExpectedOffset=%d  currentOffset=%d\n",
                    count, movedOffset[count], offset);

            }
            if(uprv_memcmp(str, result[count], size) !=0){
                log_err("ERROR: utf8_appendCharSafeBody() failed for count=%d. \nExpected:", count);
                printUChars(result[count], size);
                printf("\nGot:     ");
                printUChars(str, size);
                printf("\n");
            }
            */
        }
        count++;
    }
#endif
}

static void TestAppend() {
    static const UChar32 codePoints[]={
        0x61, 0xdf, 0x901, 0x3040,
        0xac00, 0xd800, 0xdbff, 0xdcde,
        0xdffd, 0xe000, 0xffff, 0x10000,
        0x12345, 0xe0021, 0x10ffff, 0x110000,
        0x234567, 0x7fffffff, -1, -1000,
        0, 0x400
    };
    static const uint8_t expectUnsafe[]={
        0x61,  0xc3, 0x9f,  0xe0, 0xa4, 0x81,  0xe3, 0x81, 0x80,
        0xea, 0xb0, 0x80,  0xed, 0xa0, 0x80,  0xed, 0xaf, 0xbf,  0xed, 0xb3, 0x9e,
        0xed, 0xbf, 0xbd,  0xee, 0x80, 0x80,  0xef, 0xbf, 0xbf,  0xf0, 0x90, 0x80, 0x80,
        0xf0, 0x92, 0x8d, 0x85,  0xf3, 0xa0, 0x80, 0xa1,  0xf4, 0x8f, 0xbf, 0xbf,  /* not 0x110000 */
        /* none from this line */
        0,  0xd0, 0x80
    }, expectSafe[]={
        0x61,  0xc3, 0x9f,  0xe0, 0xa4, 0x81,  0xe3, 0x81, 0x80,
        0xea, 0xb0, 0x80,  /* no surrogates */
        /* no surrogates */  0xee, 0x80, 0x80,  0xef, 0xbf, 0xbf,  0xf0, 0x90, 0x80, 0x80,
        0xf0, 0x92, 0x8d, 0x85,  0xf3, 0xa0, 0x80, 0xa1,  0xf4, 0x8f, 0xbf, 0xbf,  /* not 0x110000 */
        /* none from this line */
        0,  0xd0, 0x80
    };

    uint8_t buffer[100];
    UChar32 c;
    int32_t i, length;
    UBool isError, expectIsError, wrongIsError;

    length=0;
    for(i=0; i<UPRV_LENGTHOF(codePoints); ++i) {
        c=codePoints[i];
        if(c<0 || 0x10ffff<c) {
            continue; /* skip non-code points for U8_APPEND_UNSAFE */
        }

        U8_APPEND_UNSAFE(buffer, length, c);
    }
    if(length!=UPRV_LENGTHOF(expectUnsafe) || 0!=memcmp(buffer, expectUnsafe, length)) {
        log_err("U8_APPEND_UNSAFE did not generate the expected output\n");
    }

    length=0;
    wrongIsError=FALSE;
    for(i=0; i<UPRV_LENGTHOF(codePoints); ++i) {
        c=codePoints[i];
        expectIsError= c<0 || 0x10ffff<c || U_IS_SURROGATE(c);
        isError=FALSE;

        U8_APPEND(buffer, length, UPRV_LENGTHOF(buffer), c, isError);
        wrongIsError|= isError!=expectIsError;
    }
    if(wrongIsError) {
        log_err("U8_APPEND did not set isError correctly\n");
    }
    if(length!=UPRV_LENGTHOF(expectSafe) || 0!=memcmp(buffer, expectSafe, length)) {
        log_err("U8_APPEND did not generate the expected output\n");
    }
}

static void
TestSurrogates() {
    static const uint8_t b[]={
        0xc3, 0x9f,             /*  00DF */
        0xed, 0x9f, 0xbf,       /*  D7FF */
        0xed, 0xa0, 0x81,       /*  D801 */
        0xed, 0xbf, 0xbe,       /*  DFFE */
        0xee, 0x80, 0x80,       /*  E000 */
        0xf0, 0x97, 0xbf, 0xbe  /* 17FFE */
    };
    static const UChar32 cp[]={
        0xdf, 0xd7ff, 0xd801, 0xdffe, 0xe000, 0x17ffe
    };

    UChar32 cu, cs, cl;
    int32_t i, j, k, iu, is, il, length;

    k=0; /* index into cp[] */
    length=UPRV_LENGTHOF(b);
    for(i=0; i<length;) {
        j=i;
        U8_NEXT_UNSAFE(b, j, cu);
        iu=j;

        j=i;
        U8_NEXT(b, j, length, cs);
        is=j;

        j=i;
        L8_NEXT(b, j, length, cl);
        il=j;

        if(cu!=cp[k]) {
            log_err("U8_NEXT_UNSAFE(b[%ld])=U+%04lX != U+%04lX\n", (long)i, (long)cu, (long)cp[k]);
        }

        /* U8_NEXT() returns <0 for surrogate code points */
        if(U_IS_SURROGATE(cu) ? cs>=0 : cs!=cu) {
            log_err("U8_NEXT(b[%ld])=U+%04lX != U+%04lX\n", (long)i, (long)cs, (long)cu);
        }

        /* L8_NEXT() returns surrogate code points like U8_NEXT_UNSAFE() */
        if(cl!=cu) {
            log_err("L8_NEXT(b[%ld])=U+%04lX != U+%04lX\n", (long)i, (long)cl, (long)cu);
        }

        // U8_NEXT() skips only the first byte of a surrogate byte sequence.
        if(U_IS_SURROGATE(cu) ? is!=(i+1) : is!=iu) {
            log_err("U8_NEXT(b[%ld]) did not advance the index correctly\n", (long)i, (long)i);
        }
        if(il!=iu) {
            log_err("L8_NEXT(b[%ld]) did not advance the index correctly\n", (long)i, (long)i);
        }

        ++k;    /* next code point */
        i=iu;   /* advance by one UTF-8 sequence */
    }

    while(i>0) {
        --k; /* previous code point */

        j=i;
        U8_PREV_UNSAFE(b, j, cu);
        iu=j;

        j=i;
        U8_PREV(b, 0, j, cs);
        is=j;

        j=i;
        L8_PREV(b, 0, j, cl);
        il=j;

        if(cu!=cp[k]) {
            log_err("U8_PREV_UNSAFE(b[%ld])=U+%04lX != U+%04lX\n", (long)i, (long)cu, (long)cp[k]);
        }

        /* U8_PREV() returns <0 for surrogate code points */
        if(U_IS_SURROGATE(cu) ? cs>=0 : cs!=cu) {
            log_err("U8_PREV(b[%ld])=U+%04lX != U+%04lX\n", (long)i, (long)cs, (long)cu);
        }

        /* L8_PREV() returns surrogate code points like U8_PREV_UNSAFE() */
        if(cl!=cu) {
            log_err("L8_PREV(b[%ld])=U+%04lX != U+%04lX\n", (long)i, (long)cl, (long)cu);
        }

        // U8_PREV() skips only the last byte of a surrogate byte sequence.
        if(U_IS_SURROGATE(cu) ? is!=(i-1) : is!=iu) {
            log_err("U8_PREV(b[%ld]) did not advance the index correctly\n", (long)i, (long)i);
        }
        if(il !=iu) {
            log_err("L8_PREV(b[%ld]) did not advance the index correctly\n", (long)i, (long)i);
        }

        i=iu;   /* go back by one UTF-8 sequence */
    }
}
