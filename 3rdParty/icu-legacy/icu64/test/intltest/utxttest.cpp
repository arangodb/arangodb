// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2005-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/************************************************************************
*   Tests for the UText and UTextIterator text abstraction classes
*
************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "unicode/utypes.h"
#include "unicode/utext.h"
#include "unicode/utf8.h"
#include "unicode/utf16.h"
#include "unicode/ustring.h"
#include "unicode/uchriter.h"
#include "cmemory.h"
#include "cstr.h"
#include "utxttest.h"

static UBool  gFailed = false;
static int    gTestNum = 0;

// Forward decl
UText *openFragmentedUnicodeString(UText *ut, UnicodeString *s, UErrorCode *status);

#define TEST_ASSERT(x) UPRV_BLOCK_MACRO_BEGIN { \
    if ((x)==false) { \
        errln("Test #%d failure in file %s at line %d\n", gTestNum, __FILE__, __LINE__); \
        gFailed = true; \
    } \
} UPRV_BLOCK_MACRO_END


#define TEST_SUCCESS(status) UPRV_BLOCK_MACRO_BEGIN { \
    if (U_FAILURE(status)) { \
        errln("Test #%d failure in file %s at line %d. Error = \"%s\"\n", \
              gTestNum, __FILE__, __LINE__, u_errorName(status)); \
        gFailed = true; \
    } \
} UPRV_BLOCK_MACRO_END

UTextTest::UTextTest() {
}

UTextTest::~UTextTest() {
}


void
UTextTest::runIndexedTest(int32_t index, UBool exec,
                          const char* &name, char* /*par*/) {
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TextTest);
    TESTCASE_AUTO(ErrorTest);
    TESTCASE_AUTO(FreezeTest);
    TESTCASE_AUTO(Ticket5560);
    TESTCASE_AUTO(Ticket6847);
    TESTCASE_AUTO(Ticket10562);
    TESTCASE_AUTO(Ticket10983);
    TESTCASE_AUTO(Ticket12130);
    TESTCASE_AUTO(Ticket13344);
    TESTCASE_AUTO(AccessChangesChunkSize);
    TESTCASE_AUTO_END;
}

//
// Quick and dirty random number generator.
//   (don't use library so that results are portable.
static uint32_t m_seed = 1;
static uint32_t m_rand()
{
    m_seed = m_seed * 1103515245 + 12345;
    return (uint32_t)(m_seed/65536) % 32768;
}


//
//   TextTest()
//
//       Top Level function for UText testing.
//       Specifies the strings to be tested, with the actual testing itself
//       being carried out in another function, TestString().
//
void  UTextTest::TextTest() {
    int32_t i, j;

    TestString("abcd\\U00010001xyz");
    TestString("");

    // Supplementary chars at start or end
    TestString("\\U00010001");
    TestString("abc\\U00010001");
    TestString("\\U00010001abc");

    // Test simple strings of lengths 1 to 60, looking for glitches at buffer boundaries
    UnicodeString s;
    for (i=1; i<60; i++) {
        s.truncate(0);
        for (j=0; j<i; j++) {
            if (j+0x30 == 0x5c) {
                // backslash.  Needs to be escaped
                s.append((char16_t)0x5c);
            }
            s.append(char16_t(j+0x30));
        }
        TestString(s);
    }

   // Test strings with odd-aligned supplementary chars,
   //    looking for glitches at buffer boundaries
    for (i=1; i<60; i++) {
        s.truncate(0);
        s.append((char16_t)0x41);
        for (j=0; j<i; j++) {
            s.append(UChar32(j+0x11000));
        }
        TestString(s);
    }

    // String of chars of randomly varying size in utf-8 representation.
    //   Exercise the mapping, and the varying sized buffer.
    //
    s.truncate(0);
    UChar32  c1 = 0;
    UChar32  c2 = 0x100;
    UChar32  c3 = 0xa000;
    UChar32  c4 = 0x11000;
    for (i=0; i<1000; i++) {
        int len8 = m_rand()%4 + 1;
        switch (len8) {
            case 1:
                c1 = (c1+1)%0x80;
                // don't put 0 into string (0 terminated strings for some tests)
                // don't put '\', will cause unescape() to fail.
                if (c1==0x5c || c1==0) {
                    c1++;
                }
                s.append(c1);
                break;
            case 2:
                s.append(c2++);
                break;
            case 3:
                s.append(c3++);
                break;
            case 4:
                s.append(c4++);
                break;
        }
    }
    TestString(s);
}


//
//  TestString()     Run a suite of UText tests on a string.
//                   The test string is unescaped before use.
//
void UTextTest::TestString(const UnicodeString &s) {
    int32_t       i;
    int32_t       j;
    UChar32       c;
    int32_t       cpCount = 0;
    UErrorCode    status  = U_ZERO_ERROR;
    UText        *ut      = nullptr;
    int32_t       saLen;

    UnicodeString sa = s.unescape();
    saLen = sa.length();

    //
    // Build up a mapping between code points and UTF-16 code unit indexes.
    //
    m *cpMap = new m[sa.length() + 1];
    j = 0;
    for (i=0; i<sa.length(); i=sa.moveIndex32(i, 1)) {
        c = sa.char32At(i);
        cpMap[j].nativeIdx = i;
        cpMap[j].cp = c;
        j++;
        cpCount++;
    }
    cpMap[j].nativeIdx = i;   // position following the last char in utf-16 string.


    // char16_t * test, null terminated
    status = U_ZERO_ERROR;
    char16_t *buf = new char16_t[saLen+1];
    sa.extract(buf, saLen+1, status);
    TEST_SUCCESS(status);
    ut = utext_openUChars(nullptr, buf, -1, &status);
    TEST_SUCCESS(status);
    TestAccess(sa, ut, cpCount, cpMap);
    utext_close(ut);
    delete [] buf;

    // char16_t * test, with length
    status = U_ZERO_ERROR;
    buf = new char16_t[saLen+1];
    sa.extract(buf, saLen+1, status);
    TEST_SUCCESS(status);
    ut = utext_openUChars(nullptr, buf, saLen, &status);
    TEST_SUCCESS(status);
    TestAccess(sa, ut, cpCount, cpMap);
    utext_close(ut);
    delete [] buf;


    // UnicodeString test
    status = U_ZERO_ERROR;
    ut = utext_openUnicodeString(nullptr, &sa, &status);
    TEST_SUCCESS(status);
    TestAccess(sa, ut, cpCount, cpMap);
    TestCMR(sa, ut, cpCount, cpMap, cpMap);
    utext_close(ut);


    // Const UnicodeString test
    status = U_ZERO_ERROR;
    ut = utext_openConstUnicodeString(nullptr, &sa, &status);
    TEST_SUCCESS(status);
    TestAccess(sa, ut, cpCount, cpMap);
    utext_close(ut);


    // Replaceable test.  (UnicodeString inherits Replaceable)
    status = U_ZERO_ERROR;
    ut = utext_openReplaceable(nullptr, &sa, &status);
    TEST_SUCCESS(status);
    TestAccess(sa, ut, cpCount, cpMap);
    TestCMR(sa, ut, cpCount, cpMap, cpMap);
    utext_close(ut);

    // Character Iterator Tests
    status = U_ZERO_ERROR;
    const char16_t *cbuf = sa.getBuffer();
    CharacterIterator *ci = new UCharCharacterIterator(cbuf, saLen, status);
    TEST_SUCCESS(status);
    ut = utext_openCharacterIterator(nullptr, ci, &status);
    TEST_SUCCESS(status);
    TestAccess(sa, ut, cpCount, cpMap);
    utext_close(ut);
    delete ci;


    // Fragmented UnicodeString  (Chunk size of one)
    //
    status = U_ZERO_ERROR;
    ut = openFragmentedUnicodeString(nullptr, &sa, &status);
    TEST_SUCCESS(status);
    TestAccess(sa, ut, cpCount, cpMap);
    utext_close(ut);

    //
    // UTF-8 test
    //

    // Convert the test string from UnicodeString to (char *) in utf-8 format
    int32_t u8Len = sa.extract(0, sa.length(), nullptr, 0, "utf-8");
    char *u8String = new char[u8Len + 1];
    sa.extract(0, sa.length(), u8String, u8Len+1, "utf-8");

    // Build up the map of code point indices in the utf-8 string
    m * u8Map = new m[sa.length() + 1];
    i = 0;   // native utf-8 index
    for (j=0; j<cpCount ; j++) {  // code point number
        u8Map[j].nativeIdx = i;
        U8_NEXT(u8String, i, u8Len, c);
        u8Map[j].cp = c;
    }
    u8Map[cpCount].nativeIdx = u8Len;   // position following the last char in utf-8 string.

    // Do the test itself
    status = U_ZERO_ERROR;
    ut = utext_openUTF8(nullptr, u8String, -1, &status);
    TEST_SUCCESS(status);
    TestAccess(sa, ut, cpCount, u8Map);
    utext_close(ut);



    delete []cpMap;
    delete []u8Map;
    delete []u8String;
}

//  TestCMR   test Copy, Move and Replace operations.
//              us         UnicodeString containing the test text.
//              ut         UText containing the same test text.
//              cpCount    number of code points in the test text.
//              nativeMap  Mapping from code points to native indexes for the UText.
//              u16Map     Mapping from code points to UTF-16 indexes, for use with the UnicodeString.
//
//     This function runs a whole series of operations on each incoming UText.
//     The UText is deep-cloned prior to each operation, so that the original UText remains unchanged.
//
void UTextTest::TestCMR(const UnicodeString &us, UText *ut, int cpCount, m *nativeMap, m *u16Map) {
    TEST_ASSERT(utext_isWritable(ut) == true);

    int  srcLengthType;       // Loop variables for selecting the position and length
    int  srcPosType;          //   of the block to operate on within the source text.
    int  destPosType;

    int  srcIndex  = 0;       // Code Point indexes of the block to operate on for
    int  srcLength = 0;       //   a specific test.

    int  destIndex = 0;       // Code point index of the destination for a copy/move test.

    int32_t  nativeStart = 0; // Native unit indexes for a test.
    int32_t  nativeLimit = 0;
    int32_t  nativeDest  = 0;

    int32_t  u16Start    = 0; // UTF-16 indexes for a test.
    int32_t  u16Limit    = 0; //   used when performing the same operation in a Unicode String
    int32_t  u16Dest     = 0;

    // Iterate over a whole series of source index, length and a target indexes.
    // This is done with code point indexes; these will be later translated to native
    //   indexes using the cpMap.
    for (srcLengthType=1; srcLengthType<=3; srcLengthType++) {
        switch (srcLengthType) {
            case 1: srcLength = 1; break;
            case 2: srcLength = 5; break;
            case 3: srcLength = cpCount / 3;
        }
        for (srcPosType=1; srcPosType<=5; srcPosType++) {
            switch (srcPosType) {
                case 1: srcIndex = 0; break;
                case 2: srcIndex = 1; break;
                case 3: srcIndex = cpCount - srcLength; break;
                case 4: srcIndex = cpCount - srcLength - 1; break;
                case 5: srcIndex = cpCount / 2; break;
            }
            if (srcIndex < 0 || srcIndex + srcLength > cpCount) {
                // filter out bogus test cases -
                //   those with a source range that falls of an edge of the string.
                continue;
            }

            //
            // Copy and move tests.
            //   iterate over a variety of destination positions.
            //
            for (destPosType=1; destPosType<=4; destPosType++) {
                switch (destPosType) {
                    case 1: destIndex = 0; break;
                    case 2: destIndex = 1; break;
                    case 3: destIndex = srcIndex - 1; break;
                    case 4: destIndex = srcIndex + srcLength + 1; break;
                    case 5: destIndex = cpCount-1; break;
                    case 6: destIndex = cpCount; break;
                }
                if (destIndex<0 || destIndex>cpCount) {
                    // filter out bogus test cases.
                    continue;
                }

                nativeStart = nativeMap[srcIndex].nativeIdx;
                nativeLimit = nativeMap[srcIndex+srcLength].nativeIdx;
                nativeDest  = nativeMap[destIndex].nativeIdx;

                u16Start    = u16Map[srcIndex].nativeIdx;
                u16Limit    = u16Map[srcIndex+srcLength].nativeIdx;
                u16Dest     = u16Map[destIndex].nativeIdx;

                gFailed = false;
                TestCopyMove(us, ut, false,
                    nativeStart, nativeLimit, nativeDest,
                    u16Start, u16Limit, u16Dest);

                TestCopyMove(us, ut, true,
                    nativeStart, nativeLimit, nativeDest,
                    u16Start, u16Limit, u16Dest);

                if (gFailed) {
                    return;
                }
            }

            //
            //  Replace tests.
            //
            UnicodeString fullRepString("This is an arbitrary string that will be used as replacement text");
            for (int32_t replStrLen=0; replStrLen<20; replStrLen++) {
                UnicodeString repStr(fullRepString, 0, replStrLen);
                TestReplace(us, ut,
                    nativeStart, nativeLimit,
                    u16Start, u16Limit,
                    repStr);
                if (gFailed) {
                    return;
                }
            }

        }
    }

}

//
//   TestCopyMove    run a single test case for utext_copy.
//                   Test cases are created in TestCMR and dispatched here for execution.
//
void UTextTest::TestCopyMove(const UnicodeString &us, UText *ut, UBool move,
                    int32_t nativeStart, int32_t nativeLimit, int32_t nativeDest,
                    int32_t u16Start, int32_t u16Limit, int32_t u16Dest)
{
    UErrorCode      status   = U_ZERO_ERROR;
    UText          *targetUT = nullptr;
    gTestNum++;
    gFailed = false;

    //
    //  clone the UText.  The test will be run in the cloned copy
    //  so that we don't alter the original.
    //
    targetUT = utext_clone(nullptr, ut, true, false, &status);
    TEST_SUCCESS(status);
    UnicodeString targetUS(us);    // And copy the reference string.

    // do the test operation first in the reference
    targetUS.copy(u16Start, u16Limit, u16Dest);
    if (move) {
        // delete out the source range.
        if (u16Limit < u16Dest) {
            targetUS.removeBetween(u16Start, u16Limit);
        } else {
            int32_t amtCopied = u16Limit - u16Start;
            targetUS.removeBetween(u16Start+amtCopied, u16Limit+amtCopied);
        }
    }

    // Do the same operation in the UText under test
    utext_copy(targetUT, nativeStart, nativeLimit, nativeDest, move, &status);
    if (nativeDest > nativeStart && nativeDest < nativeLimit) {
        TEST_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
    } else {
        TEST_SUCCESS(status);

        // Compare the results of the two parallel tests
        int32_t  usi = 0;    // UnicodeString position, utf-16 index.
        int64_t  uti = 0;    // UText position, native index.
        UChar32  usc;        // code point from Unicode String
        UChar32  utc;        // code point from UText
        utext_setNativeIndex(targetUT, 0);
        for (;;) {
            usc = targetUS.char32At(usi);
            utc = utext_next32(targetUT);
            if (utc < 0) {
                break;
            }
            TEST_ASSERT(uti == usi);
            TEST_ASSERT(utc == usc);
            usi = targetUS.moveIndex32(usi, 1);
            uti = utext_getNativeIndex(targetUT);
            if (gFailed) {
                goto cleanupAndReturn;
            }
        }
        int64_t expectedNativeLength = utext_nativeLength(ut);
        if (move == false) {
            expectedNativeLength += nativeLimit - nativeStart;
        }
        uti = utext_getNativeIndex(targetUT);
        TEST_ASSERT(uti == expectedNativeLength);
    }

cleanupAndReturn:
    utext_close(targetUT);
}


//
//  TestReplace   Test a single Replace operation.
//
void UTextTest::TestReplace(
            const UnicodeString &us,     // reference UnicodeString in which to do the replace
            UText         *ut,                // UnicodeText object under test.
            int32_t       nativeStart,        // Range to be replaced, in UText native units.
            int32_t       nativeLimit,
            int32_t       u16Start,           // Range to be replaced, in UTF-16 units
            int32_t       u16Limit,           //    for use in the reference UnicodeString.
            const UnicodeString &repStr)      // The replacement string
{
    UErrorCode      status   = U_ZERO_ERROR;
    UText          *targetUT = nullptr;
    gTestNum++;
    gFailed = false;

    //
    //  clone the target UText.  The test will be run in the cloned copy
    //  so that we don't alter the original.
    //
    targetUT = utext_clone(nullptr, ut, true, false, &status);
    TEST_SUCCESS(status);
    UnicodeString targetUS(us);    // And copy the reference string.

    //
    // Do the replace operation in the Unicode String, to
    //   produce a reference result.
    //
    targetUS.replace(u16Start, u16Limit-u16Start, repStr);

    //
    // Do the replace on the UText under test
    //
    const char16_t *rs = repStr.getBuffer();
    int32_t  rsLen = repStr.length();
    int32_t actualDelta = utext_replace(targetUT, nativeStart, nativeLimit, rs, rsLen, &status);
    int32_t expectedDelta = repStr.length() - (nativeLimit - nativeStart);
    TEST_ASSERT(actualDelta == expectedDelta);

    //
    // Compare the results
    //
    int32_t  usi = 0;    // UnicodeString position, utf-16 index.
    int64_t  uti = 0;    // UText position, native index.
    UChar32  usc;        // code point from Unicode String
    UChar32  utc;        // code point from UText
    int64_t  expectedNativeLength = 0;
    utext_setNativeIndex(targetUT, 0);
    for (;;) {
        usc = targetUS.char32At(usi);
        utc = utext_next32(targetUT);
        if (utc < 0) {
            break;
        }
        TEST_ASSERT(uti == usi);
        TEST_ASSERT(utc == usc);
        usi = targetUS.moveIndex32(usi, 1);
        uti = utext_getNativeIndex(targetUT);
        if (gFailed) {
            goto cleanupAndReturn;
        }
    }
    expectedNativeLength = utext_nativeLength(ut) + expectedDelta;
    uti = utext_getNativeIndex(targetUT);
    TEST_ASSERT(uti == expectedNativeLength);

cleanupAndReturn:
    utext_close(targetUT);
}

//
//  TestAccess      Test the read only access functions on a UText, including cloning.
//                  The text is accessed in a variety of ways, and compared with
//                  the reference UnicodeString.
//
void UTextTest::TestAccess(const UnicodeString &us, UText *ut, int cpCount, m *cpMap) {
    // Run the standard tests on the caller-supplied UText.
    TestAccessNoClone(us, ut, cpCount, cpMap);

    // Re-run tests on a shallow clone.
    utext_setNativeIndex(ut, 0);
    UErrorCode status = U_ZERO_ERROR;
    UText *shallowClone = utext_clone(nullptr, ut, false /*deep*/, false /*readOnly*/, &status);
    TEST_SUCCESS(status);
    TestAccessNoClone(us, shallowClone, cpCount, cpMap);

    //
    // Rerun again on a deep clone.
    // Note that text providers are not required to provide deep cloning,
    //   so unsupported errors are ignored.
    //
    status = U_ZERO_ERROR;
    utext_setNativeIndex(shallowClone, 0);
    UText *deepClone = utext_clone(nullptr, shallowClone, true, false, &status);
    utext_close(shallowClone);
    if (status != U_UNSUPPORTED_ERROR) {
        TEST_SUCCESS(status);
        TestAccessNoClone(us, deepClone, cpCount, cpMap);
    }
    utext_close(deepClone);
}


//
//  TestAccessNoClone()    Test the read only access functions on a UText.
//                         The text is accessed in a variety of ways, and compared with
//                         the reference UnicodeString.
//
void UTextTest::TestAccessNoClone(const UnicodeString &us, UText *ut, int cpCount, m *cpMap) {
    UErrorCode  status = U_ZERO_ERROR;
    gTestNum++;

    //
    //  Check the length from the UText
    //
    int64_t expectedLen = cpMap[cpCount].nativeIdx;
    int64_t utlen = utext_nativeLength(ut);
    TEST_ASSERT(expectedLen == utlen);

    //
    //  Iterate forwards, verify that we get the correct code points
    //   at the correct native offsets.
    //
    int         i = 0;
    int64_t     index;
    int64_t     expectedIndex = 0;
    int64_t     foundIndex = 0;
    UChar32     expectedC;
    UChar32     foundC;
    int64_t     len;

    for (i=0; i<cpCount; i++) {
        expectedIndex = cpMap[i].nativeIdx;
        foundIndex    = utext_getNativeIndex(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        expectedC     = cpMap[i].cp;
        foundC        = utext_next32(ut);
        TEST_ASSERT(expectedC == foundC);
        foundIndex    = utext_getPreviousNativeIndex(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        if (gFailed) {
            return;
        }
    }
    foundC = utext_next32(ut);
    TEST_ASSERT(foundC == U_SENTINEL);

    // Repeat above, using macros
    utext_setNativeIndex(ut, 0);
    for (i=0; i<cpCount; i++) {
        expectedIndex = cpMap[i].nativeIdx;
        foundIndex    = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        expectedC     = cpMap[i].cp;
        foundC        = UTEXT_NEXT32(ut);
        TEST_ASSERT(expectedC == foundC);
        if (gFailed) {
            return;
        }
    }
    foundC = UTEXT_NEXT32(ut);
    TEST_ASSERT(foundC == U_SENTINEL);

    //
    //  Forward iteration (above) should have left index at the
    //   end of the input, which should == length().
    //
    len = utext_nativeLength(ut);
    foundIndex  = utext_getNativeIndex(ut);
    TEST_ASSERT(len == foundIndex);

    //
    // Iterate backwards over entire test string
    //
    len = utext_getNativeIndex(ut);
    utext_setNativeIndex(ut, len);
    for (i=cpCount-1; i>=0; i--) {
        expectedC     = cpMap[i].cp;
        expectedIndex = cpMap[i].nativeIdx;
        int64_t prevIndex = utext_getPreviousNativeIndex(ut);
        foundC        = utext_previous32(ut);
        foundIndex    = utext_getNativeIndex(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        TEST_ASSERT(expectedC == foundC);
        TEST_ASSERT(prevIndex == foundIndex);
        if (gFailed) {
            return;
        }
    }

    //
    //  Backwards iteration, above, should have left our iterator
    //   position at zero, and continued backwards iterationshould fail.
    //
    foundIndex = utext_getNativeIndex(ut);
    TEST_ASSERT(foundIndex == 0);
    foundIndex = utext_getPreviousNativeIndex(ut);
    TEST_ASSERT(foundIndex == 0);


    foundC = utext_previous32(ut);
    TEST_ASSERT(foundC == U_SENTINEL);
    foundIndex = utext_getNativeIndex(ut);
    TEST_ASSERT(foundIndex == 0);
    foundIndex = utext_getPreviousNativeIndex(ut);
    TEST_ASSERT(foundIndex == 0);


    // And again, with the macros
    utext_setNativeIndex(ut, len);
    for (i=cpCount-1; i>=0; i--) {
        expectedC     = cpMap[i].cp;
        expectedIndex = cpMap[i].nativeIdx;
        foundC        = UTEXT_PREVIOUS32(ut);
        foundIndex    = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        TEST_ASSERT(expectedC == foundC);
        if (gFailed) {
            return;
        }
    }

    //
    //  Backwards iteration, above, should have left our iterator
    //   position at zero, and continued backwards iterationshould fail.
    //
    foundIndex = UTEXT_GETNATIVEINDEX(ut);
    TEST_ASSERT(foundIndex == 0);

    foundC = UTEXT_PREVIOUS32(ut);
    TEST_ASSERT(foundC == U_SENTINEL);
    foundIndex = UTEXT_GETNATIVEINDEX(ut);
    TEST_ASSERT(foundIndex == 0);
    if (gFailed) {
        return;
    }

    //
    //  next32From(), previous32From(), Iterate in a somewhat random order.
    //
    int  cpIndex = 0;
    for (i=0; i<cpCount; i++) {
        cpIndex = (cpIndex + 9973) % cpCount;
        index         = cpMap[cpIndex].nativeIdx;
        expectedC     = cpMap[cpIndex].cp;
        foundC        = utext_next32From(ut, index);
        TEST_ASSERT(expectedC == foundC);
        if (gFailed) {
            return;
        }
    }

    cpIndex = 0;
    for (i=0; i<cpCount; i++) {
        cpIndex = (cpIndex + 9973) % cpCount;
        index         = cpMap[cpIndex+1].nativeIdx;
        expectedC     = cpMap[cpIndex].cp;
        foundC        = utext_previous32From(ut, index);
        TEST_ASSERT(expectedC == foundC);
        if (gFailed) {
            return;
        }
    }


    //
    // moveIndex(int32_t delta);
    //

    // Walk through frontwards, incrementing by one
    utext_setNativeIndex(ut, 0);
    for (i=1; i<=cpCount; i++) {
        utext_moveIndex32(ut, 1);
        index = utext_getNativeIndex(ut);
        expectedIndex = cpMap[i].nativeIdx;
        TEST_ASSERT(expectedIndex == index);
        index = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == index);
    }

    // Walk through frontwards, incrementing by two
    utext_setNativeIndex(ut, 0);
    for (i=2; i<cpCount; i+=2) {
        utext_moveIndex32(ut, 2);
        index = utext_getNativeIndex(ut);
        expectedIndex = cpMap[i].nativeIdx;
        TEST_ASSERT(expectedIndex == index);
        index = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == index);
    }

    // walk through the string backwards, decrementing by one.
    i = cpMap[cpCount].nativeIdx;
    utext_setNativeIndex(ut, i);
    for (i=cpCount; i>=0; i--) {
        expectedIndex = cpMap[i].nativeIdx;
        index = utext_getNativeIndex(ut);
        TEST_ASSERT(expectedIndex == index);
        index = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == index);
        utext_moveIndex32(ut, -1);
    }


    // walk through backwards, decrementing by three
    i = cpMap[cpCount].nativeIdx;
    utext_setNativeIndex(ut, i);
    for (i=cpCount; i>=0; i-=3) {
        expectedIndex = cpMap[i].nativeIdx;
        index = utext_getNativeIndex(ut);
        TEST_ASSERT(expectedIndex == index);
        index = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == index);
        utext_moveIndex32(ut, -3);
    }


    //
    // Extract
    //
    int bufSize = us.length() + 10;
    char16_t *buf = new char16_t[bufSize];
    status = U_ZERO_ERROR;
    expectedLen = us.length();
    len = utext_extract(ut, 0, utlen, buf, bufSize, &status);
    TEST_SUCCESS(status);
    TEST_ASSERT(len == expectedLen);
    int compareResult = us.compare(buf, -1);
    TEST_ASSERT(compareResult == 0);

    status = U_ZERO_ERROR;
    len = utext_extract(ut, 0, utlen, nullptr, 0, &status);
    if (utlen == 0) {
        TEST_ASSERT(status == U_STRING_NOT_TERMINATED_WARNING);
    } else {
        TEST_ASSERT(status == U_BUFFER_OVERFLOW_ERROR);
    }
    TEST_ASSERT(len == expectedLen);

    status = U_ZERO_ERROR;
    u_memset(buf, 0x5555, bufSize);
    len = utext_extract(ut, 0, utlen, buf, 1, &status);
    if (us.length() == 0) {
        TEST_SUCCESS(status);
        TEST_ASSERT(buf[0] == 0);
    } else {
        // Buf len == 1, extracting a single 16 bit value.
        // If the data char is supplementary, it doesn't matter whether the buffer remains unchanged,
        //   or whether the lead surrogate of the pair is extracted.
        //   It's a buffer overflow error in either case.
        TEST_ASSERT(buf[0] == us.charAt(0) ||
                    (buf[0] == 0x5555 && U_IS_SUPPLEMENTARY(us.char32At(0))));
        TEST_ASSERT(buf[1] == 0x5555);
        if (us.length() == 1) {
            TEST_ASSERT(status == U_STRING_NOT_TERMINATED_WARNING);
        } else {
            TEST_ASSERT(status == U_BUFFER_OVERFLOW_ERROR);
        }
    }

    delete []buf;
}

//
//  ErrorTest()    Check various error and edge cases.
//
void UTextTest::ErrorTest()
{
    // Close of an uninitialized UText.  Shouldn't blow up.
    {
        UText  ut;
        memset(&ut, 0, sizeof(UText));
        utext_close(&ut);
        utext_close(nullptr);
    }

    // Double-close of a UText.  Shouldn't blow up.  UText should still be usable.
    {
        UErrorCode status = U_ZERO_ERROR;
        UText ut = UTEXT_INITIALIZER;
        UnicodeString s("Hello, World");
        UText *ut2 = utext_openUnicodeString(&ut, &s, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(ut2 == &ut);

        UText *ut3 = utext_close(&ut);
        TEST_ASSERT(ut3 == &ut);

        UText *ut4 = utext_close(&ut);
        TEST_ASSERT(ut4 == &ut);

        utext_openUnicodeString(&ut, &s, &status);
        TEST_SUCCESS(status);
        utext_close(&ut);
    }

    // Re-use of a UText, chaining through each of the types of UText
    //   (If it doesn't blow up, and doesn't leak, it's probably working fine)
    {
        UErrorCode status = U_ZERO_ERROR;
        UText ut = UTEXT_INITIALIZER;
        UText  *utp;
        UnicodeString s1("Hello, World");
        char16_t s2[] = {(char16_t)0x41, (char16_t)0x42, (char16_t)0};
        const char  *s3 = "\x66\x67\x68";

        utp = utext_openUnicodeString(&ut, &s1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);

        utp = utext_openConstUnicodeString(&ut, &s1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);

        utp = utext_openUTF8(&ut, s3, -1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);

        utp = utext_openUChars(&ut, s2, -1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);

        utp = utext_close(&ut);
        TEST_ASSERT(utp == &ut);

        utp = utext_openUnicodeString(&ut, &s1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);
    }

    // Invalid parameters on open
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        UText ut = UTEXT_INITIALIZER;

        utext_openUChars(&ut, nullptr, 5, &status);
        TEST_ASSERT(status == U_ILLEGAL_ARGUMENT_ERROR);

        status = U_ZERO_ERROR;
        utext_openUChars(&ut, nullptr, -1, &status);
        TEST_ASSERT(status == U_ILLEGAL_ARGUMENT_ERROR);

        status = U_ZERO_ERROR;
        utext_openUTF8(&ut, nullptr, 4, &status);
        TEST_ASSERT(status == U_ILLEGAL_ARGUMENT_ERROR);

        status = U_ZERO_ERROR;
        utext_openUTF8(&ut, nullptr, -1, &status);
        TEST_ASSERT(status == U_ILLEGAL_ARGUMENT_ERROR);
    }

    //
    //  UTF-8 with malformed sequences.
    //    These should come through as the Unicode replacement char, \ufffd
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        UText *ut = nullptr;
        const char *badUTF8 = "\x41\x81\x42\xf0\x81\x81\x43";
        UChar32  c;

        ut = utext_openUTF8(nullptr, badUTF8, -1, &status);
        TEST_SUCCESS(status);
        c = utext_char32At(ut, 1);
        TEST_ASSERT(c == 0xfffd);
        c = utext_char32At(ut, 3);
        TEST_ASSERT(c == 0xfffd);
        c = utext_char32At(ut, 5);
        TEST_ASSERT(c == 0xfffd);
        c = utext_char32At(ut, 6);
        TEST_ASSERT(c == 0x43);

        char16_t buf[10];
        int n = utext_extract(ut, 0, 9, buf, 10, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(n==7);
        TEST_ASSERT(buf[0] == 0x41);
        TEST_ASSERT(buf[1] == 0xfffd);
        TEST_ASSERT(buf[2] == 0x42);
        TEST_ASSERT(buf[3] == 0xfffd);
        TEST_ASSERT(buf[4] == 0xfffd);
        TEST_ASSERT(buf[5] == 0xfffd);
        TEST_ASSERT(buf[6] == 0x43);
        utext_close(ut);
    }


    //
    //  isLengthExpensive - does it make the expected transitions after
    //                      getting the length of a nul terminated string?
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString sa("Hello, this is a string");
        UBool  isExpensive;

        char16_t sb[100];
        memset(sb, 0x20, sizeof(sb));
        sb[99] = 0;

        UText *uta = utext_openUnicodeString(nullptr, &sa, &status);
        TEST_SUCCESS(status);
        isExpensive = utext_isLengthExpensive(uta);
        TEST_ASSERT(isExpensive == false);
        utext_close(uta);

        UText *utb = utext_openUChars(nullptr, sb, -1, &status);
        TEST_SUCCESS(status);
        isExpensive = utext_isLengthExpensive(utb);
        TEST_ASSERT(isExpensive == true);
        int64_t  len = utext_nativeLength(utb);
        TEST_ASSERT(len == 99);
        isExpensive = utext_isLengthExpensive(utb);
        TEST_ASSERT(isExpensive == false);
        utext_close(utb);
    }

    //
    // Index to positions not on code point boundaries.
    //
    {
        const char *u8str =         "\xc8\x81\xe1\x82\x83\xf1\x84\x85\x86";
        int32_t startMap[] =        {   0,  0,  2,  2,  2,  5,  5,  5,  5,  9,  9};
        int32_t nextMap[]  =        {   2,  2,  5,  5,  5,  9,  9,  9,  9,  9,  9};
        int32_t prevMap[]  =        {   0,  0,  0,  0,  0,  2,  2,  2,  2,  5,  5};
        UChar32  c32Map[] =    {0x201, 0x201, 0x1083, 0x1083, 0x1083, 0x044146, 0x044146, 0x044146, 0x044146, -1, -1};
        UChar32  pr32Map[] =   {    -1,   -1,  0x201,  0x201,  0x201,   0x1083,   0x1083,   0x1083,   0x1083, 0x044146, 0x044146};

        // extractLen is the size, in UChars, of what will be extracted between index and index+1.
        //  is zero when both index positions lie within the same code point.
        int32_t  exLen[] =          {   0,  1,   0,  0,  1,  0,  0,  0,  2,  0,  0};


        UErrorCode status = U_ZERO_ERROR;
        UText *ut = utext_openUTF8(nullptr, u8str, -1, &status);
        TEST_SUCCESS(status);

        // Check setIndex
        int32_t i;
        int32_t startMapLimit = UPRV_LENGTHOF(startMap);
        for (i=0; i<startMapLimit; i++) {
            utext_setNativeIndex(ut, i);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == startMap[i]);
            cpIndex = UTEXT_GETNATIVEINDEX(ut);
            TEST_ASSERT(cpIndex == startMap[i]);
        }

        // Check char32At
        for (i=0; i<startMapLimit; i++) {
            UChar32 c32 = utext_char32At(ut, i);
            TEST_ASSERT(c32 == c32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == startMap[i]);
        }

        // Check utext_next32From
        for (i=0; i<startMapLimit; i++) {
            UChar32 c32 = utext_next32From(ut, i);
            TEST_ASSERT(c32 == c32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == nextMap[i]);
        }

        // check utext_previous32From
        for (i=0; i<startMapLimit; i++) {
            gTestNum++;
            UChar32 c32 = utext_previous32From(ut, i);
            TEST_ASSERT(c32 == pr32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == prevMap[i]);
        }

        // check Extract
        //   Extract from i to i+1, which may be zero or one code points,
        //     depending on whether the indices straddle a cp boundary.
        for (i=0; i<startMapLimit; i++) {
            char16_t buf[3];
            status = U_ZERO_ERROR;
            int32_t  extractedLen = utext_extract(ut, i, i+1, buf, 3, &status);
            TEST_SUCCESS(status);
            TEST_ASSERT(extractedLen == exLen[i]);
            if (extractedLen > 0) {
                UChar32  c32;
                /* extractedLen-extractedLen == 0 is used to get around a compiler warning. */
                U16_GET(buf, 0, extractedLen-extractedLen, extractedLen, c32);
                TEST_ASSERT(c32 == c32Map[i]);
            }
        }

        utext_close(ut);
    }


    {    //  Similar test, with utf16 instead of utf8
         //  TODO:  merge the common parts of these tests.

        UnicodeString u16str("\\u1000\\U00011000\\u2000\\U00022000", -1, US_INV);
        int32_t startMap[]  ={ 0,     1,   1,    3,     4,  4,     6,  6};
        int32_t nextMap[]  = { 1,     3,   3,    4,     6,  6,     6,  6};
        int32_t prevMap[]  = { 0,     0,   0,    1,     3,  3,     4,  4};
        UChar32  c32Map[] =  {0x1000, 0x11000, 0x11000, 0x2000,  0x22000, 0x22000, -1, -1};
        UChar32  pr32Map[] = {    -1, 0x1000,  0x1000,  0x11000, 0x2000,  0x2000,   0x22000,   0x22000};
        int32_t  exLen[] =   {   1,  0,   2,  1,  0,  2,  0,  0,};

        u16str = u16str.unescape();
        UErrorCode status = U_ZERO_ERROR;
        UText *ut = utext_openUnicodeString(nullptr, &u16str, &status);
        TEST_SUCCESS(status);

        int32_t startMapLimit = UPRV_LENGTHOF(startMap);
        int i;
        for (i=0; i<startMapLimit; i++) {
            utext_setNativeIndex(ut, i);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == startMap[i]);
        }

        // Check char32At
        for (i=0; i<startMapLimit; i++) {
            UChar32 c32 = utext_char32At(ut, i);
            TEST_ASSERT(c32 == c32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == startMap[i]);
        }

        // Check utext_next32From
        for (i=0; i<startMapLimit; i++) {
            UChar32 c32 = utext_next32From(ut, i);
            TEST_ASSERT(c32 == c32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == nextMap[i]);
        }

        // check utext_previous32From
        for (i=0; i<startMapLimit; i++) {
            UChar32 c32 = utext_previous32From(ut, i);
            TEST_ASSERT(c32 == pr32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == prevMap[i]);
        }

        // check Extract
        //   Extract from i to i+1, which may be zero or one code points,
        //     depending on whether the indices straddle a cp boundary.
        for (i=0; i<startMapLimit; i++) {
            char16_t buf[3];
            status = U_ZERO_ERROR;
            int32_t  extractedLen = utext_extract(ut, i, i+1, buf, 3, &status);
            TEST_SUCCESS(status);
            TEST_ASSERT(extractedLen == exLen[i]);
            if (extractedLen > 0) {
                UChar32  c32;
                /* extractedLen-extractedLen == 0 is used to get around a compiler warning. */
                U16_GET(buf, 0, extractedLen-extractedLen, extractedLen, c32);
                TEST_ASSERT(c32 == c32Map[i]);
            }
        }

        utext_close(ut);
    }

    {    //  Similar test, with UText over Replaceable
         //  TODO:  merge the common parts of these tests.

        UnicodeString u16str("\\u1000\\U00011000\\u2000\\U00022000", -1, US_INV);
        int32_t startMap[]  ={ 0,     1,   1,    3,     4,  4,     6,  6};
        int32_t nextMap[]  = { 1,     3,   3,    4,     6,  6,     6,  6};
        int32_t prevMap[]  = { 0,     0,   0,    1,     3,  3,     4,  4};
        UChar32  c32Map[] =  {0x1000, 0x11000, 0x11000, 0x2000,  0x22000, 0x22000, -1, -1};
        UChar32  pr32Map[] = {    -1, 0x1000,  0x1000,  0x11000, 0x2000,  0x2000,   0x22000,   0x22000};
        int32_t  exLen[] =   {   1,  0,   2,  1,  0,  2,  0,  0,};

        u16str = u16str.unescape();
        UErrorCode status = U_ZERO_ERROR;
        UText *ut = utext_openReplaceable(nullptr, &u16str, &status);
        TEST_SUCCESS(status);

        int32_t startMapLimit = UPRV_LENGTHOF(startMap);
        int i;
        for (i=0; i<startMapLimit; i++) {
            utext_setNativeIndex(ut, i);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == startMap[i]);
        }

        // Check char32At
        for (i=0; i<startMapLimit; i++) {
            UChar32 c32 = utext_char32At(ut, i);
            TEST_ASSERT(c32 == c32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == startMap[i]);
        }

        // Check utext_next32From
        for (i=0; i<startMapLimit; i++) {
            UChar32 c32 = utext_next32From(ut, i);
            TEST_ASSERT(c32 == c32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == nextMap[i]);
        }

        // check utext_previous32From
        for (i=0; i<startMapLimit; i++) {
            UChar32 c32 = utext_previous32From(ut, i);
            TEST_ASSERT(c32 == pr32Map[i]);
            int64_t cpIndex = utext_getNativeIndex(ut);
            TEST_ASSERT(cpIndex == prevMap[i]);
        }

        // check Extract
        //   Extract from i to i+1, which may be zero or one code points,
        //     depending on whether the indices straddle a cp boundary.
        for (i=0; i<startMapLimit; i++) {
            char16_t buf[3];
            status = U_ZERO_ERROR;
            int32_t  extractedLen = utext_extract(ut, i, i+1, buf, 3, &status);
            TEST_SUCCESS(status);
            TEST_ASSERT(extractedLen == exLen[i]);
            if (extractedLen > 0) {
                UChar32  c32;
                /* extractedLen-extractedLen == 0 is used to get around a compiler warning. */
                U16_GET(buf, 0, extractedLen-extractedLen, extractedLen, c32);
                TEST_ASSERT(c32 == c32Map[i]);
            }
        }

        utext_close(ut);
    }
}


void UTextTest::FreezeTest() {
    // Check isWritable() and freeze() behavior.
    //

    UnicodeString  ustr("Hello, World.");
    const char u8str[] = {char(0x31), (char)0x32, (char)0x33, 0};
    const char16_t u16str[] = {(char16_t)0x31, (char16_t)0x32, (char16_t)0x44, 0};

    UErrorCode status = U_ZERO_ERROR;
    UText  *ut        = nullptr;
    UText  *ut2       = nullptr;

    ut = utext_openUTF8(ut, u8str, -1, &status);
    TEST_SUCCESS(status);
    UBool writable = utext_isWritable(ut);
    TEST_ASSERT(writable == false);
    utext_copy(ut, 1, 2, 0, true, &status);
    TEST_ASSERT(status == U_NO_WRITE_PERMISSION);

    status = U_ZERO_ERROR;
    ut = utext_openUChars(ut, u16str, -1, &status);
    TEST_SUCCESS(status);
    writable = utext_isWritable(ut);
    TEST_ASSERT(writable == false);
    utext_copy(ut, 1, 2, 0, true, &status);
    TEST_ASSERT(status == U_NO_WRITE_PERMISSION);

    status = U_ZERO_ERROR;
    ut = utext_openUnicodeString(ut, &ustr, &status);
    TEST_SUCCESS(status);
    writable = utext_isWritable(ut);
    TEST_ASSERT(writable == true);
    utext_freeze(ut);
    writable = utext_isWritable(ut);
    TEST_ASSERT(writable == false);
    utext_copy(ut, 1, 2, 0, true, &status);
    TEST_ASSERT(status == U_NO_WRITE_PERMISSION);

    status = U_ZERO_ERROR;
    ut = utext_openUnicodeString(ut, &ustr, &status);
    TEST_SUCCESS(status);
    ut2 = utext_clone(ut2, ut, false, false, &status);  // clone with readonly = false
    TEST_SUCCESS(status);
    writable = utext_isWritable(ut2);
    TEST_ASSERT(writable == true);
    ut2 = utext_clone(ut2, ut, false, true, &status);  // clone with readonly = true
    TEST_SUCCESS(status);
    writable = utext_isWritable(ut2);
    TEST_ASSERT(writable == false);
    utext_copy(ut2, 1, 2, 0, true, &status);
    TEST_ASSERT(status == U_NO_WRITE_PERMISSION);

    status = U_ZERO_ERROR;
    ut = utext_openConstUnicodeString(ut, &ustr, &status);
    TEST_SUCCESS(status);
    writable = utext_isWritable(ut);
    TEST_ASSERT(writable == false);
    utext_copy(ut, 1, 2, 0, true, &status);
    TEST_ASSERT(status == U_NO_WRITE_PERMISSION);

    // Deep Clone of a frozen UText should re-enable writing in the copy.
    status = U_ZERO_ERROR;
    ut = utext_openUnicodeString(ut, &ustr, &status);
    TEST_SUCCESS(status);
    utext_freeze(ut);
    ut2 = utext_clone(ut2, ut, true, false, &status);   // deep clone
    TEST_SUCCESS(status);
    writable = utext_isWritable(ut2);
    TEST_ASSERT(writable == true);


    // Deep clone of a frozen UText, where the base type is intrinsically non-writable,
    //  should NOT enable writing in the copy.
    status = U_ZERO_ERROR;
    ut = utext_openUChars(ut, u16str, -1, &status);
    TEST_SUCCESS(status);
    utext_freeze(ut);
    ut2 = utext_clone(ut2, ut, true, false, &status);   // deep clone
    TEST_SUCCESS(status);
    writable = utext_isWritable(ut2);
    TEST_ASSERT(writable == false);

    // cleanup
    utext_close(ut);
    utext_close(ut2);
}


//
//  Fragmented UText
//      A UText type that works with a chunk size of 1.
//      Intended to test for edge cases.
//      Input comes from a UnicodeString.
//
//       ut.b    the character.  Put into both halves.
//

U_CDECL_BEGIN
static UBool U_CALLCONV
fragTextAccess(UText *ut, int64_t index, UBool forward) {
    const UnicodeString *us = static_cast<const UnicodeString *>(ut->context);
    char16_t c;
    int32_t length = us->length();
    if (forward && index>=0 && index<length) {
        c = us->charAt((int32_t)index);
        ut->b = c | c<<16;
        ut->chunkOffset = 0;
        ut->chunkLength = 1;
        ut->chunkNativeStart = index;
        ut->chunkNativeLimit = index+1;
        return true;
    }
    if (!forward && index>0 && index <=length) {
        c = us->charAt((int32_t)index-1);
        ut->b = c | c<<16;
        ut->chunkOffset = 1;
        ut->chunkLength = 1;
        ut->chunkNativeStart = index-1;
        ut->chunkNativeLimit = index;
        return true;
    }
    ut->b = 0;
    ut->chunkOffset = 0;
    ut->chunkLength = 0;
    if (index <= 0) {
        ut->chunkNativeStart = 0;
        ut->chunkNativeLimit = 0;
    } else {
        ut->chunkNativeStart = length;
        ut->chunkNativeLimit = length;
    }
    return false;
}

// Function table to be used with this fragmented text provider.
//   Initialized in the open function.
static UTextFuncs  fragmentFuncs;

// Clone function for fragmented text provider.
//   Didn't really want to provide this, but it's easier to provide it than to keep it
//   out of the tests.
//
UText *
cloneFragmentedUnicodeString(UText *dest, const UText *src, UBool deep, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return nullptr;
    }
    if (deep) {
        *status = U_UNSUPPORTED_ERROR;
        return nullptr;
    }
    dest = utext_openUnicodeString(dest, static_cast<UnicodeString *>(const_cast<void*>(src->context)), status);
    utext_setNativeIndex(dest, utext_getNativeIndex(src));
    return dest;
}

U_CDECL_END

// Open function for the fragmented text provider.
UText *
openFragmentedUnicodeString(UText *ut, UnicodeString *s, UErrorCode *status) {
    ut = utext_openUnicodeString(ut, s, status);
    if (U_FAILURE(*status)) {
        return ut;
    }

    // Copy of the function table from the stock UnicodeString UText,
    //   and replace the entry for the access function.
    memcpy(&fragmentFuncs, ut->pFuncs, sizeof(fragmentFuncs));
    fragmentFuncs.access = fragTextAccess;
    fragmentFuncs.clone  = cloneFragmentedUnicodeString;
    ut->pFuncs = &fragmentFuncs;

    ut->chunkContents = (char16_t *)&ut->b;
    ut->pFuncs->access(ut, 0, true);
    return ut;
}

// Regression test for Ticket 5560
//   Clone fails to update chunkContentPointer in the cloned copy.
//   This is only an issue for UText types that work in a local buffer,
//      (UTF-8 wrapper, for example)
//
//   The test:
//     1.  Create an initial UText
//     2.  Deep clone it.  Contents should match original.
//     3.  Reset original to something different.
//     4.  Check that clone contents did not change.
//
void UTextTest::Ticket5560() {
    /* The following two strings are in UTF-8 even on EBCDIC platforms. */
    static const char s1[] = {0x41,0x42,0x43,0x44,0x45,0x46,0}; /* "ABCDEF" */
    static const char s2[] = {0x31,0x32,0x33,0x34,0x35,0x36,0}; /* "123456" */
	UErrorCode status = U_ZERO_ERROR;

	UText ut1 = UTEXT_INITIALIZER;
	UText ut2 = UTEXT_INITIALIZER;

	utext_openUTF8(&ut1, s1, -1, &status);
	char16_t c = utext_next32(&ut1);
	TEST_ASSERT(c == 0x41);  // c == 'A'

	utext_clone(&ut2, &ut1, true, false, &status);
	TEST_SUCCESS(status);
    c = utext_next32(&ut2);
	TEST_ASSERT(c == 0x42);  // c == 'B'
    c = utext_next32(&ut1);
	TEST_ASSERT(c == 0x42);  // c == 'B'

	utext_openUTF8(&ut1, s2, -1, &status);
	c = utext_next32(&ut1);
	TEST_ASSERT(c == 0x31);  // c == '1'
    c = utext_next32(&ut2);
	TEST_ASSERT(c == 0x43);  // c == 'C'

    utext_close(&ut1);
    utext_close(&ut2);
}


// Test for Ticket 6847
//
void UTextTest::Ticket6847() {
    const int STRLEN = 90;
    char16_t s[STRLEN+1];
    u_memset(s, 0x41, STRLEN);
    s[STRLEN] = 0;

    UErrorCode status = U_ZERO_ERROR;
    UText *ut = utext_openUChars(nullptr, s, -1, &status);

    utext_setNativeIndex(ut, 0);
    int32_t count = 0;
    UChar32 c = 0;
    int64_t nativeIndex = UTEXT_GETNATIVEINDEX(ut);
    TEST_ASSERT(nativeIndex == 0);
    while ((c = utext_next32(ut)) != U_SENTINEL) {
        TEST_ASSERT(c == 0x41);
        TEST_ASSERT(count < STRLEN);
        if (count >= STRLEN) {
            break;
        }
        count++;
        nativeIndex = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(nativeIndex == count);
    }
    TEST_ASSERT(count == STRLEN);
    nativeIndex = UTEXT_GETNATIVEINDEX(ut);
    TEST_ASSERT(nativeIndex == STRLEN);
    utext_close(ut);
}


void UTextTest::Ticket10562() {
    // Note: failures show as a heap error when the test is run under valgrind.
    UErrorCode status = U_ZERO_ERROR;

    const char *utf8_string = "\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41";
    UText *utf8Text = utext_openUTF8(nullptr, utf8_string, -1, &status);
    TEST_SUCCESS(status);
    UText *deepClone = utext_clone(nullptr, utf8Text, true, false, &status);
    TEST_SUCCESS(status);
    UText *shallowClone = utext_clone(nullptr, deepClone, false, false, &status);
    TEST_SUCCESS(status);
    utext_close(shallowClone);
    utext_close(deepClone);
    utext_close(utf8Text);

    status = U_ZERO_ERROR;
    UnicodeString usString("Hello, World.");
    UText *usText = utext_openUnicodeString(nullptr, &usString, &status);
    TEST_SUCCESS(status);
    UText *usDeepClone = utext_clone(nullptr, usText, true, false, &status);
    TEST_SUCCESS(status);
    UText *usShallowClone = utext_clone(nullptr, usDeepClone, false, false, &status);
    TEST_SUCCESS(status);
    utext_close(usShallowClone);
    utext_close(usDeepClone);
    utext_close(usText);
}


void UTextTest::Ticket10983() {
    // Note: failure shows as a seg fault when the defect is present.

    UErrorCode status = U_ZERO_ERROR;
    UnicodeString s("Hello, World");
    UText *ut = utext_openConstUnicodeString(nullptr, &s, &status);
    TEST_SUCCESS(status);

    status = U_INVALID_STATE_ERROR;
    UText *cloned = utext_clone(nullptr, ut, true, true, &status);
    TEST_ASSERT(cloned == nullptr);
    TEST_ASSERT(status == U_INVALID_STATE_ERROR);

    utext_close(ut);
}

// Ticket 12130 - extract on a UText wrapping a null terminated char16_t * string
//                leaves the iteration position set incorrectly when the
//                actual string length is not yet known.
//
//                The test text needs to be long enough that UText defers getting the length.

void UTextTest::Ticket12130() {
    UErrorCode status = U_ZERO_ERROR;
    
    const char *text8 =
        "Fundamentally, computers just deal with numbers. They store letters and other characters "
        "by assigning a number for each one. Before Unicode was invented, there were hundreds "
        "of different encoding systems for assigning these numbers. No single encoding could "
        "contain enough characters: for example, the European Union alone requires several "
        "different encodings to cover all its languages. Even for a single language like "
        "English no single encoding was adequate for all the letters, punctuation, and technical "
        "symbols in common use.";

    UnicodeString str(text8);
    const char16_t *ustr = str.getTerminatedBuffer();
    UText ut = UTEXT_INITIALIZER;
    utext_openUChars(&ut, ustr, -1, &status);
    char16_t extractBuffer[50];

    for (int32_t startIdx = 0; startIdx<str.length(); ++startIdx) {
        int32_t endIdx = startIdx + 20;

        u_memset(extractBuffer, 0, UPRV_LENGTHOF(extractBuffer));
        utext_extract(&ut, startIdx, endIdx, extractBuffer, UPRV_LENGTHOF(extractBuffer), &status);
        if (U_FAILURE(status)) {
            errln("%s:%d %s", __FILE__, __LINE__, u_errorName(status));
            return;
        }
        int64_t ni  = utext_getNativeIndex(&ut);
        int64_t expectedni = startIdx + 20;
        if (expectedni > str.length()) {
            expectedni = str.length();
        }
        if (expectedni != ni) {
            errln("%s:%d utext_getNativeIndex() expected %d, got %d", __FILE__, __LINE__, expectedni, ni);
        }
        if (0 != str.tempSubString(startIdx, 20).compare(extractBuffer)) { 
            errln("%s:%d utext_extract() failed. expected \"%s\", got \"%s\"",
                    __FILE__, __LINE__, CStr(str.tempSubString(startIdx, 20))(), CStr(UnicodeString(extractBuffer))());
        }
    }
    utext_close(&ut);

    // Similar utext extract, this time with the string length provided to the UText in advance,
    // and a buffer of larger than required capacity.
   
    utext_openUChars(&ut, ustr, str.length(), &status);
    for (int32_t startIdx = 0; startIdx<str.length(); ++startIdx) {
        int32_t endIdx = startIdx + 20;
        u_memset(extractBuffer, 0, UPRV_LENGTHOF(extractBuffer));
        utext_extract(&ut, startIdx, endIdx, extractBuffer, UPRV_LENGTHOF(extractBuffer), &status);
        if (U_FAILURE(status)) {
            errln("%s:%d %s", __FILE__, __LINE__, u_errorName(status));
            return;
        }
        int64_t ni  = utext_getNativeIndex(&ut);
        int64_t expectedni = startIdx + 20;
        if (expectedni > str.length()) {
            expectedni = str.length();
        }
        if (expectedni != ni) {
            errln("%s:%d utext_getNativeIndex() expected %d, got %d", __FILE__, __LINE__, expectedni, ni);
        }
        if (0 != str.tempSubString(startIdx, 20).compare(extractBuffer)) { 
            errln("%s:%d utext_extract() failed. expected \"%s\", got \"%s\"",
                    __FILE__, __LINE__, CStr(str.tempSubString(startIdx, 20))(), CStr(UnicodeString(extractBuffer))());
        }
    }
    utext_close(&ut);
}

// Ticket 13344 The macro form of UTEXT_SETNATIVEINDEX failed when target was a trail surrogate
//              of a supplementary character.

void UTextTest::Ticket13344() {
    UErrorCode status = U_ZERO_ERROR;
    const char16_t *str = u"abc\U0010abcd xyz";
    LocalUTextPointer ut(utext_openUChars(nullptr, str, -1, &status));

    assertSuccess("UTextTest::Ticket13344-status", status);
    UTEXT_SETNATIVEINDEX(ut.getAlias(), 3);
    assertEquals("UTextTest::Ticket13344-lead", (int64_t)3, utext_getNativeIndex(ut.getAlias()));
    UTEXT_SETNATIVEINDEX(ut.getAlias(), 4);
    assertEquals("UTextTest::Ticket13344-trail", (int64_t)3, utext_getNativeIndex(ut.getAlias()));
    UTEXT_SETNATIVEINDEX(ut.getAlias(), 5);
    assertEquals("UTextTest::Ticket13344-bmp", (int64_t)5, utext_getNativeIndex(ut.getAlias()));

    utext_setNativeIndex(ut.getAlias(), 3);
    assertEquals("UTextTest::Ticket13344-lead-2", (int64_t)3, utext_getNativeIndex(ut.getAlias()));
    utext_setNativeIndex(ut.getAlias(), 4);
    assertEquals("UTextTest::Ticket13344-trail-2", (int64_t)3, utext_getNativeIndex(ut.getAlias()));
    utext_setNativeIndex(ut.getAlias(), 5);
    assertEquals("UTextTest::Ticket13344-bmp-2", (int64_t)5, utext_getNativeIndex(ut.getAlias()));
}

// ICU-21653 UText does not handle access callback that changes chunk size

static const char16_t testAccessText[] = { // text with surrogates at chunk boundaries
    0xDC00,0xe001,0xe002,0xD83D,0xDE00,0xe005,0xe006,0xe007, 0xe008,0xe009,0xe00a,0xD83D,0xDE00,0xe00d,0xe00e,0xe00f, // 000-015, unpaired trail at 0
    0xE010,0xe011,0xe012,0xD83D,0xDE00,0xe015,0xe016,0xe017, 0xe018,0xe019,0xe01a,0xD83D,0xDE00,0xe01d,0xe01e,0xD800, // 016-031, paired lead at 31 with
    0xDC01,0xe021,0xe022,0xD83D,0xDE00,0xe025,0xe026,0xe027, 0xe028,0xe029,0xe02a,0xD83D,0xDE00,0xe02d,0xe02e,0xe02f, // 032-047, paired trail at 32
    0xe030,0xe031,0xe032,0xD83D,0xDE00,0xe035,0xe036,0xe037, 0xe038,0xe039,0xe03a,0xD83D,0xDE00,0xe03d,0xe03e,0xe03f, // 048-063
    0xDC02,0xe041,0xe042,0xD83D,0xDE00,0xe045,0xe046,0xe047, 0xe048,0xe049,0xe04a,0xD83D,0xDE00,0xe04d,0xe04e,0xe04f, // 064-079, unpaired trail at 64
    0xe050,0xe051,0xe052,0xD83D,0xDE00,0xe055,0xe056,0xe057, 0xe058,0xe059,0xe05a,0xD83D,0xDE00,0xe05d,0xe05e,0xD801, // 080-095, unpaired lead at 95
    0xe060,0xe061,0xe062,0xD83D,0xDE00,0xe065,0xe066,0xe067, 0xe068,0xe069,0xe06a,0xD83D,0xDE00,0xe06d,0xe06e,0xe06f, // 096-111
    0xE070,0xe071,0xe072,0xD83D,0xDE00,0xe075,0xe076,0xe077, 0xe078,0xe079,0xe07a,0xD83D,0xDE00,0xe07d,0xe07e,0xD802, // 112-127, unpaired lead at 127
};

static const UChar32 testAccess32Text[] = { // same as above in UTF32
    0xDC00,0xe001,0xe002,0x1F600,0xe005,0xe006,0xe007, 0xe008,0xe009,0xe00a,0x1F600,0xe00d,0xe00e,0xe00f, // 000-013, unpaired trail at 0
    0xE010,0xe011,0xe012,0x1F600,0xe015,0xe016,0xe017, 0xe018,0xe019,0xe01a,0x1F600,0xe01d,0xe01e,0x10001, // 014-027, nonBMP at 27, will split in chunks
           0xe021,0xe022,0x1F600,0xe025,0xe026,0xe027, 0xe028,0xe029,0xe02a,0x1F600,0xe02d,0xe02e,0xe02f, // 028-040
    0xe030,0xe031,0xe032,0x1F600,0xe035,0xe036,0xe037, 0xe038,0xe039,0xe03a,0x1F600,0xe03d,0xe03e,0xe03f, // 041-054
    0xDC02,0xe041,0xe042,0x1F600,0xe045,0xe046,0xe047, 0xe048,0xe049,0xe04a,0x1F600,0xe04d,0xe04e,0xe04f, // 055-068, unpaired trail at 55
    0xe050,0xe051,0xe052,0x1F600,0xe055,0xe056,0xe057, 0xe058,0xe059,0xe05a,0x1F600,0xe05d,0xe05e,0xD801, // 069-082, unpaired lead at 82
    0xe060,0xe061,0xe062,0x1F600,0xe065,0xe066,0xe067, 0xe068,0xe069,0xe06a,0x1F600,0xe06d,0xe06e,0xe06f, // 083-096
    0xE070,0xe071,0xe072,0x1F600,0xe075,0xe076,0xe077, 0xe078,0xe079,0xe07a,0x1F600,0xe07d,0xe07e,0xD802, // 097-110, unpaired lead at 110
};

enum {
    kTestAccessSmallChunkSize = 8,
    kTestAccessLargeChunkSize = 32,
    kTextAccessGapSize = 2
};

typedef struct {
    int64_t nativeOffset;
    UChar32 expectChar;
} OffsetAndChar;

static const OffsetAndChar testAccessEntries[] = { // sequence of offsets to test with expected UChar32
    // random access
    { 127,  0xD802 },
    { 16,   0xE010 },
    { 95,   0xD801 },
    { 31,   0x10001 },
    { 112,  0xE070 },
    { 0,    0xDC00 },
    { 64,   0xDC02 },
    { 32,   0x10001 },
    // sequential access
    { 0,    0xDC00 },
    { 16,   0xE010 },
    { 31,   0x10001 },
    { 32,   0x10001 },
    { 64,   0xDC02 },
    { 95,   0xD801 },
    { 112,  0xE070 },
    { 127,  0xD802 },
};

static const OffsetAndChar testAccess32Entries[] = { // sequence of offsets to test with expected UChar32
    // random access
    { 110,  0xD802 },   // 0 *
    { 14,   0xE010 },   // 1
    { 82,   0xD801 },   // 2 *
    { 27,   0x10001 },  // 3 *
    { 97,   0xE070 },   // 4
    { 0,    0xDC00 },   // 5
    { 55,   0xDC02 },   // 6
    // sequential access
    { 0,    0xDC00 },   // 7
    { 14,   0xE010 },   // 8
    { 27,   0x10001 },  // 9 *
    { 55,   0xDC02 },   // 10
    { 97,   0xE070 },   // 11
    { 82,   0xD801 },   // 12 *
    { 110,  0xD802 },   // 13 *
};
// modified UTextAccess function for char16_t string; a cross between
// UText ucstrTextAccess and a function that modifies chunk size
// 1. assumes native length is known and in ut->a
// 2. assumes that most fields may be 0 or nullptr, will fill out if index not in range
// 3. Will designate buffer of size kTestAccessSmallChunkSize or kTestAccessLargeChunkSize
//    depending on kTextAccessGapSize
static UBool
ustrTextAccessModChunks(UText *ut, int64_t index, UBool forward) {
    const char16_t *str = (const char16_t *)ut->context;
    int64_t length = ut->a;

    // pin the requested index to the bounds of the string
    if (index < 0) {
        index = 0;
    } else if (index > length) {
        index = length;
    }
    if (forward) {
        if (index < ut->chunkNativeLimit && index >= ut->chunkNativeStart) {
            /* Already inside the buffer. Set the new offset. */
            ut->chunkOffset = (int32_t)(index - ut->chunkNativeStart);
            return true;
        }
        if (index >= length && ut->chunkNativeLimit == length) {
            /* Off the end of the buffer, but we can't get it. */
            ut->chunkOffset = ut->chunkLength;
            return false;
        }
    }
    else {
        if (index <= ut->chunkNativeLimit && index > ut->chunkNativeStart) {
            /* Already inside the buffer. Set the new offset. */
            ut->chunkOffset = (int32_t)(index - ut->chunkNativeStart);
            return true;
        }
        if (index == 0 && ut->chunkNativeStart == 0) {
            /* Already at the beginning; can't go any farther */
            ut->chunkOffset = 0;
            return false;
        }
    }
    /* It's not inside the buffer. Start over from scratch. */
    // Assume large chunk size for first access
    int32_t chunkSize = kTestAccessLargeChunkSize;
    if (ut->chunkContents != nullptr && ut->chunkLength != 0) {
        // Subsequent access, set chunk size depending on gap (smaller chunk for large gap => random access)
        int64_t gap = forward ? (index-ut->chunkNativeLimit) : (ut->chunkNativeStart-index);
        if (gap < 0) {
            gap = -gap;
        }
        chunkSize = (gap > kTextAccessGapSize)? kTestAccessSmallChunkSize: kTestAccessLargeChunkSize;
    }
    ut->chunkLength = chunkSize;
    ut->chunkOffset = index % chunkSize;
    if (!forward && ut->chunkOffset == 0 && index >= chunkSize) {
        ut->chunkOffset = chunkSize;
    }
    ut->chunkNativeStart = index - ut->chunkOffset;
    ut->chunkNativeLimit = ut->chunkNativeStart + ut->chunkLength;
    ut->chunkContents = str + ut->chunkNativeStart;
    ut->nativeIndexingLimit = ut->chunkLength;
    return true;
}

// For testing UTF32 access (no native index does not match chunk offset/index

/**
 * @return the length, in the native units of the original text string.
 */
// 1. assumes native length is known and in ut->a
static int64_t
u32NativeLength(UText *ut) {
    return ut->a;
}

/**
 * Map from the current char16_t offset within the current text chunk to
 *  the corresponding native index in the original source text.
 * @return Absolute (native) index corresponding to chunkOffset in the current chunk.
 *         The returned native index should always be to a code point boundary.
 */
// 1. assumes native length is known and in ut->a
// 2. assumes that pointer to offset map is in
static int64_t
u32MapOffsetToNative(const UText *ut) {
    const int64_t* offsetMap = (const int64_t*)ut->p;
    int64_t u16Offset = offsetMap[ut->chunkNativeStart] + ut->chunkOffset;
    int64_t index = ut->a;
    while (u16Offset < offsetMap[index]) {
        index--;
    }
    return index;
}

/**
 * Map from a native index to a char16_t offset within a text chunk.
 * Behavior is undefined if the native index does not fall within the
 *   current chunk.
 * @param nativeIndex Absolute (native) text index, chunk->start<=index<=chunk->limit.
 * @return            Chunk-relative UTF-16 offset corresponding to the specified native
 *                    index.
 */
static int32_t
u32MapNativeIndexToUTF16(const UText *ut, int64_t index) {
    const int64_t* offsetMap = (const int64_t*)ut->p;
    if (index <= ut->chunkNativeStart) {
        return 0;
    } else if (index >= ut->chunkNativeLimit) {
        return ut->chunkLength;
    }
    return (offsetMap[index] - offsetMap[ut->chunkNativeStart]);
}

static void
u32Close(UText *ut) {
    uprv_free((void*)ut->p);
}

static UBool
u32Access(UText *ut, int64_t index, UBool forward) {
    int64_t length = ut->a;
    const int64_t* offsetMap = (const int64_t*)ut->p;
    const char16_t *u16 = (const char16_t *)ut->q;

    // pin the requested index to the bounds of the string
    if (index < 0) {
        index = 0;
    } else if (index > length) {
        index = length;
    }
    if (forward) {
        if (index < ut->chunkNativeLimit && index >= ut->chunkNativeStart) {
            /* Already inside the buffer. Set the new offset. */
            ut->chunkOffset = (int32_t)(index - ut->chunkNativeStart);
            return true;
        }
        if (index >= length && ut->chunkNativeLimit == length) {
            /* Off the end of the buffer, but we can't get it. */
            ut->chunkOffset = ut->chunkLength;
            return false;
        }
    }
    else {
        if (index <= ut->chunkNativeLimit && index > ut->chunkNativeStart) {
            /* Already inside the buffer. Set the new offset. */
            ut->chunkOffset = (int32_t)(index - ut->chunkNativeStart);
            return true;
        }
        if (index == 0 && ut->chunkNativeStart == 0) {
            /* Already at the beginning; can't go any farther */
            ut->chunkOffset = 0;
            return false;
        }
    }
    /* It's not inside the buffer. Start over from scratch. */
    // Assume large chunk size for first access
    int32_t chunkSize = kTestAccessLargeChunkSize;
    if (ut->chunkContents != nullptr && ut->chunkLength != 0) {
        // Subsequent access, set chunk size depending on gap (smaller chunk for large gap => random access)
        int64_t gap = forward ? (index-ut->chunkNativeLimit) : (ut->chunkNativeStart-index);
        if (gap < 0) {
            gap = -gap;
        }
        chunkSize = (gap > kTextAccessGapSize)? kTestAccessSmallChunkSize: kTestAccessLargeChunkSize;
    }
    int64_t u16Offset = offsetMap[index]; // guaranteed to be on code point boundary
    int64_t u16ChunkTryStart = (u16Offset/chunkSize) * chunkSize;
    int64_t u16ChunkTryEnd = u16ChunkTryStart + chunkSize;
    if (!forward && u16ChunkTryStart==u16Offset && u16ChunkTryStart>0) {
        u16ChunkTryEnd = u16ChunkTryStart;
        u16ChunkTryStart -= chunkSize;
    }
    int64_t nativeIndexEnd = length;
    while (u16ChunkTryEnd < offsetMap[nativeIndexEnd]) {
        nativeIndexEnd--;
    }
    int64_t nativeIndexStart = nativeIndexEnd;
    while (u16ChunkTryStart < offsetMap[nativeIndexStart]) {
        nativeIndexStart--;
    }
    if (forward && nativeIndexEnd < length && u16Offset >= offsetMap[nativeIndexEnd]) {
        // oops we need to be in the following chunk
        nativeIndexStart = nativeIndexEnd;
        u16ChunkTryEnd = ((offsetMap[nativeIndexStart + 1] + chunkSize)/chunkSize) * chunkSize;
        nativeIndexEnd = length;
        while (u16ChunkTryEnd < offsetMap[nativeIndexEnd]) {
            nativeIndexEnd--;
        }
    }
    ut->chunkNativeStart = nativeIndexStart;
    ut->chunkNativeLimit = nativeIndexEnd;
    ut->chunkLength = offsetMap[nativeIndexEnd] - offsetMap[nativeIndexStart];
    ut->chunkOffset = u16Offset - offsetMap[nativeIndexStart];
    ut->chunkContents = u16 + offsetMap[nativeIndexStart];
    ut->nativeIndexingLimit = 0 ;
    return true;
}

static const struct UTextFuncs u32Funcs =
{
    sizeof(UTextFuncs),
    0, 0, 0,              // Reserved alignment padding
    nullptr,              // Clone
    u32NativeLength,
    u32Access,
    nullptr,              // Extract
    nullptr,              // Replace
    nullptr,              // Copy
    u32MapOffsetToNative,
    u32MapNativeIndexToUTF16,
    u32Close,
    nullptr,              // spare 1
    nullptr,              // spare 2
    nullptr,              // spare 3
};

// A hack, this takes a pointer to both the UTF32 and UTF16 versions of the text
static UText *
utext_openUChar32s(UText *ut, const UChar32 *s, int64_t length, const char16_t *q, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return nullptr;
    }
    if (s==nullptr || length < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    ut = utext_setup(ut, 0, status);
    if (U_SUCCESS(*status)) {
        int64_t* offsetMap = (int64_t*)uprv_malloc((length+1)*sizeof(int64_t));
        if (offsetMap == nullptr) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        ut->pFuncs               = &u32Funcs;
        ut->context              = s;
        ut->providerProperties   = 0;
        ut->a                    = length;
        ut->chunkContents        = nullptr;
        ut->chunkNativeStart     = 0;
        ut->chunkNativeLimit     = 0;
        ut->chunkLength          = 0;
        ut->chunkOffset          = 0;
        ut->nativeIndexingLimit  = 0;
        ut->p                    = offsetMap;
        ut->q                    = q;
        int64_t u16Offset = 0;
        *offsetMap++ = 0;
        while (length-- > 0) {
            u16Offset += (*s++ < 0x10000)? 1: 2;
            *offsetMap++ = u16Offset;
        }
    }
    return ut;
}



void UTextTest::AccessChangesChunkSize() {
    UErrorCode status = U_ZERO_ERROR;
    UText ut = UTEXT_INITIALIZER;
    utext_openUChars(&ut, testAccessText, UPRV_LENGTHOF(testAccessText), &status);
    if (U_FAILURE(status)) {
        errln("utext_openUChars failed: %s", u_errorName(status));
        return;
    }
    // now reset many ut fields for this test
    ut.providerProperties = 0; // especially need to clear UTEXT_PROVIDER_STABLE_CHUNKS
    ut.chunkNativeLimit = 0;
    ut.nativeIndexingLimit = 0;
    ut.chunkNativeStart = 0;
    ut.chunkOffset = 0;
    ut.chunkLength = 0;
    ut.chunkContents = nullptr;
    UTextFuncs textFuncs = *ut.pFuncs;
    textFuncs.access = ustrTextAccessModChunks; // custom access that changes chunk size
    ut.pFuncs = &textFuncs;

    // do test
	const OffsetAndChar *testEntryPtr = testAccessEntries;
	int32_t testCount = UPRV_LENGTHOF(testAccessEntries);
	for (; testCount-- > 0; testEntryPtr++) {
	    utext_setNativeIndex(&ut, testEntryPtr->nativeOffset);
	    int64_t beforeOffset = utext_getNativeIndex(&ut);
	    UChar32 uchar = utext_current32(&ut);
	    int64_t afterOffset = utext_getNativeIndex(&ut);
	    if (uchar != testEntryPtr->expectChar || afterOffset != beforeOffset) {
	        errln("utext_current32 unexpected behavior for u16, test case %lld: expected char %04X at offset %lld, got %04X at %lld;\n"
	            "chunkNativeStart %lld chunkNativeLimit %lld nativeIndexingLimit %d chunkLength %d chunkOffset %d",
	            (int64_t)(testEntryPtr-testAccessEntries), testEntryPtr->expectChar, beforeOffset, uchar, afterOffset,
	            ut.chunkNativeStart, ut.chunkNativeLimit, ut.nativeIndexingLimit, ut.chunkLength, ut.chunkOffset);
	    }
	}
	utext_close(&ut);
	
	ut = UTEXT_INITIALIZER;
	utext_openUChar32s(&ut, testAccess32Text, UPRV_LENGTHOF(testAccess32Text), testAccessText, &status);
    if (U_FAILURE(status)) {
        errln("utext_openUChar32s failed: %s", u_errorName(status));
        return;
    }
    // do test
	testEntryPtr = testAccess32Entries;
	testCount = UPRV_LENGTHOF(testAccess32Entries);
	for (; testCount-- > 0; testEntryPtr++) {
	    utext_setNativeIndex(&ut, testEntryPtr->nativeOffset);
	    int64_t beforeOffset = utext_getNativeIndex(&ut);
	    UChar32 uchar = utext_current32(&ut);
	    int64_t afterOffset = utext_getNativeIndex(&ut);
	    if (uchar != testEntryPtr->expectChar || afterOffset != beforeOffset) {
	        errln("utext_current32 unexpected behavior for u32, test case %lld: expected char %04X at offset %lld, got %04X at %lld;\n"
	            "chunkNativeStart %lld chunkNativeLimit %lld nativeIndexingLimit %d chunkLength %d chunkOffset %d",
	            (int64_t)(testEntryPtr-testAccess32Entries), testEntryPtr->expectChar, beforeOffset, uchar, afterOffset,
	            ut.chunkNativeStart, ut.chunkNativeLimit, ut.nativeIndexingLimit, ut.chunkLength, ut.chunkOffset);
	    }
	}
	utext_close(&ut);
}

