// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include <utility>

#include "ustrtest.h"
#include "unicode/appendable.h"
#include "unicode/std_string.h"
#include "unicode/unistr.h"
#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/locid.h"
#include "unicode/strenum.h"
#include "unicode/ucnv.h"
#include "unicode/uenum.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "charstr.h"

#if 0
#include "unicode/ustream.h"

#include <iostream>
using namespace std;

#endif

UnicodeStringTest::~UnicodeStringTest() {}

extern IntlTest *createStringCaseTest();

void UnicodeStringTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char *par)
{
    if (exec) logln("TestSuite UnicodeStringTest: ");
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO_CREATE_CLASS(StringCaseTest);
    TESTCASE_AUTO(TestBasicManipulation);
    TESTCASE_AUTO(TestCompare);
    TESTCASE_AUTO(TestExtract);
    TESTCASE_AUTO(TestRemoveReplace);
    TESTCASE_AUTO(TestSearching);
    TESTCASE_AUTO(TestSpacePadding);
    TESTCASE_AUTO(TestPrefixAndSuffix);
    TESTCASE_AUTO(TestFindAndReplace);
    TESTCASE_AUTO(TestBogus);
    TESTCASE_AUTO(TestReverse);
    TESTCASE_AUTO(TestMiscellaneous);
    TESTCASE_AUTO(TestStackAllocation);
    TESTCASE_AUTO(TestUnescape);
    TESTCASE_AUTO(TestCountChar32);
    TESTCASE_AUTO(TestStringEnumeration);
    TESTCASE_AUTO(TestNameSpace);
    TESTCASE_AUTO(TestUTF32);
    TESTCASE_AUTO(TestUTF8);
    TESTCASE_AUTO(TestReadOnlyAlias);
    TESTCASE_AUTO(TestAppendable);
    TESTCASE_AUTO(TestUnicodeStringImplementsAppendable);
    TESTCASE_AUTO(TestSizeofUnicodeString);
    TESTCASE_AUTO(TestStartsWithAndEndsWithNulTerminated);
    TESTCASE_AUTO(TestMoveSwap);
    TESTCASE_AUTO(TestUInt16Pointers);
    TESTCASE_AUTO(TestWCharPointers);
    TESTCASE_AUTO(TestNullPointers);
    TESTCASE_AUTO(TestUnicodeStringInsertAppendToSelf);
    TESTCASE_AUTO(TestLargeAppend);
    TESTCASE_AUTO_END;
}

void
UnicodeStringTest::TestBasicManipulation()
{
    UnicodeString   test1("Now is the time for all men to come swiftly to the aid of the party.\n");
    UnicodeString   expectedValue;
    UnicodeString   *c;

    c=test1.clone();
    test1.insert(24, "good ");
    expectedValue = "Now is the time for all good men to come swiftly to the aid of the party.\n";
    if (test1 != expectedValue)
        errln("insert() failed:  expected \"" + expectedValue + "\"\n,got \"" + test1 + "\"");

    c->insert(24, "good ");
    if(*c != expectedValue) {
        errln("clone()->insert() failed:  expected \"" + expectedValue + "\"\n,got \"" + *c + "\"");
    }
    delete c;

    test1.remove(41, 8);
    expectedValue = "Now is the time for all good men to come to the aid of the party.\n";
    if (test1 != expectedValue)
        errln("remove() failed:  expected \"" + expectedValue + "\"\n,got \"" + test1 + "\"");
    
    test1.replace(58, 6, "ir country");
    expectedValue = "Now is the time for all good men to come to the aid of their country.\n";
    if (test1 != expectedValue)
        errln("replace() failed:  expected \"" + expectedValue + "\"\n,got \"" + test1 + "\"");
    
    char16_t  temp[80];
    test1.extract(0, 15, temp);
    
    UnicodeString       test2(temp, 15);
    
    expectedValue = "Now is the time";
    if (test2 != expectedValue)
        errln("extract() failed:  expected \"" + expectedValue + "\"\n,got \"" + test2 + "\"");
    
    test2 += " for me to go!\n";
    expectedValue = "Now is the time for me to go!\n";
    if (test2 != expectedValue)
        errln("operator+=() failed:  expected \"" + expectedValue + "\"\n,got \"" + test2 + "\"");
    
    if (test1.length() != 70)
        errln(UnicodeString("length() failed: expected 70, got ") + test1.length());
    if (test2.length() != 30)
        errln(UnicodeString("length() failed: expected 30, got ") + test2.length());

    UnicodeString test3;
    test3.append((UChar32)0x20402);
    if(test3 != CharsToUnicodeString("\\uD841\\uDC02")){
        errln((UnicodeString)"append failed for UChar32, expected \"\\\\ud841\\\\udc02\", got " + prettify(test3));
    }
    if(test3.length() != 2){
        errln(UnicodeString("append or length failed for UChar32, expected 2, got ") + test3.length());
    }
    test3.append((UChar32)0x0074);
    if(test3 != CharsToUnicodeString("\\uD841\\uDC02t")){
        errln((UnicodeString)"append failed for UChar32, expected \"\\\\uD841\\\\uDC02t\", got " + prettify(test3));
    }
    if(test3.length() != 3){
        errln((UnicodeString)"append or length failed for UChar32, expected 2, got " + test3.length());
    }

    // test some UChar32 overloads
    if( test3.setTo((UChar32)0x10330).length() != 2 ||
        test3.insert(0, (UChar32)0x20100).length() != 4 ||
        test3.replace(2, 2, (UChar32)0xe0061).length() != 4 ||
        (test3 = (UChar32)0x14001).length() != 2
    ) {
        errln((UnicodeString)"simple UChar32 overloads for replace, insert, setTo or = failed");
    }

    {
        // test moveIndex32()
        UnicodeString s=UNICODE_STRING("\\U0002f999\\U0001d15f\\u00c4\\u1ed0", 32).unescape();

        if(
            s.moveIndex32(2, -1)!=0 ||
            s.moveIndex32(2, 1)!=4 ||
            s.moveIndex32(2, 2)!=5 ||
            s.moveIndex32(5, -2)!=2 ||
            s.moveIndex32(0, -1)!=0 ||
            s.moveIndex32(6, 1)!=6
        ) {
            errln("UnicodeString::moveIndex32() failed");
        }

        if(s.getChar32Start(1)!=0 || s.getChar32Start(2)!=2) {
            errln("UnicodeString::getChar32Start() failed");
        }

        if(s.getChar32Limit(1)!=2 || s.getChar32Limit(2)!=2) {
            errln("UnicodeString::getChar32Limit() failed");
        }
    }

    {
        // test new 2.2 constructors and setTo function that parallel Java's substring function.
        UnicodeString src("Hello folks how are you?");
        UnicodeString target1("how are you?");
        if (target1 != UnicodeString(src, 12)) {
            errln("UnicodeString(const UnicodeString&, int32_t) failed");
        }
        UnicodeString target2("folks");
        if (target2 != UnicodeString(src, 6, 5)) {
            errln("UnicodeString(const UnicodeString&, int32_t, int32_t) failed");
        }
        if (target1 != target2.setTo(src, 12)) {
            errln("UnicodeString::setTo(const UnicodeString&, int32_t) failed");
        }
    }

    {
        // op+ is new in ICU 2.8
        UnicodeString s=UnicodeString("abc", "")+UnicodeString("def", "")+UnicodeString("ghi", "");
        if(s!=UnicodeString("abcdefghi", "")) {
            errln("operator+(UniStr, UniStr) failed");
        }
    }

    {
        // tests for Jitterbug 2360
        // verify that APIs with source pointer + length accept length == -1
        // mostly test only where modified, only few functions did not already do this
        if(UnicodeString("abc", -1, "")!=UnicodeString("abc", "")) {
            errln("UnicodeString(codepageData, dataLength, codepage) does not work with dataLength==-1");
        }

        char16_t buffer[10]={ 0x61, 0x62, 0x20ac, 0xd900, 0xdc05, 0,   0x62, 0xffff, 0xdbff, 0xdfff };
        UnicodeString s, t(buffer, -1, UPRV_LENGTHOF(buffer));

        if(s.setTo(buffer, -1, UPRV_LENGTHOF(buffer)).length()!=u_strlen(buffer)) {
            errln("UnicodeString.setTo(buffer, length, capacity) does not work with length==-1");
        }
        if(t.length()!=u_strlen(buffer)) {
            errln("UnicodeString(buffer, length, capacity) does not work with length==-1");
        }

        if(0!=s.caseCompare(buffer, -1, U_FOLD_CASE_DEFAULT)) {
            errln("UnicodeString.caseCompare(const char16_t *, length, options) does not work with length==-1");
        }
        if(0!=s.caseCompare(0, s.length(), buffer, U_FOLD_CASE_DEFAULT)) {
            errln("UnicodeString.caseCompare(start, _length, const char16_t *, options) does not work");
        }

        buffer[u_strlen(buffer)]=0xe4;
        UnicodeString u(buffer, -1, UPRV_LENGTHOF(buffer));
        if(s.setTo(buffer, -1, UPRV_LENGTHOF(buffer)).length()!=UPRV_LENGTHOF(buffer)) {
            errln("UnicodeString.setTo(buffer without NUL, length, capacity) does not work with length==-1");
        }
        if(u.length()!=UPRV_LENGTHOF(buffer)) {
            errln("UnicodeString(buffer without NUL, length, capacity) does not work with length==-1");
        }

        static const char cs[]={ 0x61, (char)0xe4, (char)0x85, 0 };
        UConverter *cnv;
        UErrorCode errorCode=U_ZERO_ERROR;

        cnv=ucnv_open("ISO-8859-1", &errorCode);
        UnicodeString v(cs, -1, cnv, errorCode);
        ucnv_close(cnv);
        if(v!=CharsToUnicodeString("a\\xe4\\x85")) {
            errln("UnicodeString(const char *, length, cnv, errorCode) does not work with length==-1");
        }
    }

#if U_CHARSET_IS_UTF8
    {
        // Test the hardcoded-UTF-8 UnicodeString optimizations.
        static const uint8_t utf8[]={ 0x61, 0xC3, 0xA4, 0xC3, 0x9F, 0xE4, 0xB8, 0x80, 0 };
        static const char16_t utf16[]={ 0x61, 0xE4, 0xDF, 0x4E00 };
        UnicodeString from8a = UnicodeString((const char *)utf8);
        UnicodeString from8b = UnicodeString((const char *)utf8, (int32_t)sizeof(utf8)-1);
        UnicodeString from16(false, utf16, UPRV_LENGTHOF(utf16));
        if(from8a != from16 || from8b != from16) {
            errln("UnicodeString(const char * U_CHARSET_IS_UTF8) failed");
        }
        char buffer[16];
        int32_t length8=from16.extract(0, 0x7fffffff, buffer, (uint32_t)sizeof(buffer));
        if(length8!=((int32_t)sizeof(utf8)-1) || 0!=uprv_memcmp(buffer, utf8, sizeof(utf8))) {
            errln("UnicodeString::extract(char * U_CHARSET_IS_UTF8) failed");
        }
        length8=from16.extract(1, 2, buffer, (uint32_t)sizeof(buffer));
        if(length8!=4 || buffer[length8]!=0 || 0!=uprv_memcmp(buffer, utf8+1, length8)) {
            errln("UnicodeString::extract(substring to char * U_CHARSET_IS_UTF8) failed");
        }
    }
#endif
}

void
UnicodeStringTest::TestCompare()
{
    UnicodeString   test1("this is a test");
    UnicodeString   test2("this is a test");
    UnicodeString   test3("this is a test of the emergency broadcast system");
    UnicodeString   test4("never say, \"this is a test\"!!");

    UnicodeString   test5((char16_t)0x5000);
    UnicodeString   test6((char16_t)0x5100);

    char16_t      uniChars[] = { 0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73,
                 0x20, 0x61, 0x20, 0x74, 0x65, 0x73, 0x74, 0 };
    char            chars[] = "this is a test";

    // test operator== and operator!=
    if (test1 != test2 || test1 == test3 || test1 == test4)
        errln("operator== or operator!= failed");

    // test operator> and operator<
    if (test1 > test2 || test1 < test2 || !(test1 < test3) || !(test1 > test4) ||
        !(test5 < test6)
    ) {
        errln("operator> or operator< failed");
    }

    // test operator>= and operator<=
    if (!(test1 >= test2) || !(test1 <= test2) || !(test1 <= test3) || !(test1 >= test4))
        errln("operator>= or operator<= failed");

    // test compare(UnicodeString)
    if (test1.compare(test2) != 0 || test1.compare(test3) >= 0 || test1.compare(test4) <= 0)
        errln("compare(UnicodeString) failed");

    //test compare(offset, length, UnicodeString)
    if(test1.compare(0, 14, test2) != 0 ||
        test3.compare(0, 14, test2) != 0 ||
        test4.compare(12, 14, test2) != 0 ||
        test3.compare(0, 18, test1) <=0  )
        errln("compare(offset, length, UnicodeString) fails");

    // test compare(char16_t*)
    if (test2.compare(uniChars) != 0 || test3.compare(uniChars) <= 0 || test4.compare(uniChars) >= 0)
        errln("compare(char16_t*) failed");

    // test compare(char*)
    if (test2.compare(chars) != 0 || test3.compare(chars) <= 0 || test4.compare(chars) >= 0)
        errln("compare(char*) failed");

    // test compare(char16_t*, length)
    if (test1.compare(uniChars, 4) <= 0 || test1.compare(uniChars, 4) <= 0)
        errln("compare(char16_t*, length) failed");

    // test compare(thisOffset, thisLength, that, thatOffset, thatLength)
    if (test1.compare(0, 14, test2, 0, 14) != 0 
    || test1.compare(0, 14, test3, 0, 14) != 0
    || test1.compare(0, 14, test4, 12, 14) != 0)
        errln("1. compare(thisOffset, thisLength, that, thatOffset, thatLength) failed");

    if (test1.compare(10, 4, test2, 0, 4) >= 0 
    || test1.compare(10, 4, test3, 22, 9) <= 0
    || test1.compare(10, 4, test4, 22, 4) != 0)
        errln("2. compare(thisOffset, thisLength, that, thatOffset, thatLength) failed");

    // test compareBetween
    if (test1.compareBetween(0, 14, test2, 0, 14) != 0 || test1.compareBetween(0, 14, test3, 0, 14) != 0
                    || test1.compareBetween(0, 14, test4, 12, 26) != 0)
        errln("compareBetween failed");

    if (test1.compareBetween(10, 14, test2, 0, 4) >= 0 || test1.compareBetween(10, 14, test3, 22, 31) <= 0
                    || test1.compareBetween(10, 14, test4, 22, 26) != 0)
        errln("compareBetween failed");

    // test compare() etc. with strings that share a buffer but are not equal
    test2=test1; // share the buffer, length() too large for the stackBuffer
    test2.truncate(1); // change only the length, not the buffer
    if( test1==test2 || test1<=test2 ||
        test1.compare(test2)<=0 ||
        test1.compareCodePointOrder(test2)<=0 ||
        test1.compareCodePointOrder(0, INT32_MAX, test2)<=0 ||
        test1.compareCodePointOrder(0, INT32_MAX, test2, 0, INT32_MAX)<=0 ||
        test1.compareCodePointOrderBetween(0, INT32_MAX, test2, 0, INT32_MAX)<=0 ||
        test1.caseCompare(test2, U_FOLD_CASE_DEFAULT)<=0
    ) {
        errln("UnicodeStrings that share a buffer but have different lengths compare as equal");
    }

    /* test compareCodePointOrder() */
    {
        /* these strings are in ascending order */
        static const char16_t strings[][4]={
            { 0x61, 0 },                    /* U+0061 */
            { 0x20ac, 0xd801, 0 },          /* U+20ac U+d801 */
            { 0x20ac, 0xd800, 0xdc00, 0 },  /* U+20ac U+10000 */
            { 0xd800, 0 },                  /* U+d800 */
            { 0xd800, 0xff61, 0 },          /* U+d800 U+ff61 */
            { 0xdfff, 0 },                  /* U+dfff */
            { 0xff61, 0xdfff, 0 },          /* U+ff61 U+dfff */
            { 0xff61, 0xd800, 0xdc02, 0 },  /* U+ff61 U+10002 */
            { 0xd800, 0xdc02, 0 },          /* U+10002 */
            { 0xd84d, 0xdc56, 0 }           /* U+23456 */
        };
        UnicodeString u[20]; // must be at least as long as strings[]
        int32_t i;

        for(i=0; i<UPRV_LENGTHOF(strings); ++i) {
            u[i]=UnicodeString(true, strings[i], -1);
        }

        for(i=0; i<UPRV_LENGTHOF(strings)-1; ++i) {
            if(u[i].compareCodePointOrder(u[i+1])>=0 || u[i].compareCodePointOrder(0, INT32_MAX, u[i+1].getBuffer())>=0) {
                errln("error: UnicodeString::compareCodePointOrder() fails for string %d and the following one\n", i);
            }
        }
    }

    /* test caseCompare() */
    {
        static const char16_t
        _mixed[]=               { 0x61, 0x42, 0x131, 0x3a3, 0xdf,       0x130,       0x49,  0xfb03,           0xd93f, 0xdfff, 0 },
        _otherDefault[]=        { 0x41, 0x62, 0x131, 0x3c3, 0x73, 0x53, 0x69, 0x307, 0x69,  0x46, 0x66, 0x49, 0xd93f, 0xdfff, 0 },
        _otherExcludeSpecialI[]={ 0x41, 0x62, 0x131, 0x3c3, 0x53, 0x73, 0x69,        0x131, 0x66, 0x46, 0x69, 0xd93f, 0xdfff, 0 },
        _different[]=           { 0x41, 0x62, 0x131, 0x3c3, 0x73, 0x53, 0x130,       0x49,  0x46, 0x66, 0x49, 0xd93f, 0xdffd, 0 };

        UnicodeString
            mixed(true, _mixed, -1),
            otherDefault(true, _otherDefault, -1),
            otherExcludeSpecialI(true, _otherExcludeSpecialI, -1),
            different(true, _different, -1);

        int8_t result;

        /* test caseCompare() */
        result=mixed.caseCompare(otherDefault, U_FOLD_CASE_DEFAULT);
        if(result!=0 || 0!=mixed.caseCompareBetween(0, INT32_MAX, otherDefault, 0, INT32_MAX, U_FOLD_CASE_DEFAULT)) {
            errln("error: mixed.caseCompare(other, default)=%ld instead of 0\n", result);
        }
        result=mixed.caseCompare(otherExcludeSpecialI, U_FOLD_CASE_EXCLUDE_SPECIAL_I);
        if(result!=0) {
            errln("error: mixed.caseCompare(otherExcludeSpecialI, U_FOLD_CASE_EXCLUDE_SPECIAL_I)=%ld instead of 0\n", result);
        }
        result=mixed.caseCompare(otherDefault, U_FOLD_CASE_EXCLUDE_SPECIAL_I);
        if(result==0 || 0==mixed.caseCompareBetween(0, INT32_MAX, otherDefault, 0, INT32_MAX, U_FOLD_CASE_EXCLUDE_SPECIAL_I)) {
            errln("error: mixed.caseCompare(other, U_FOLD_CASE_EXCLUDE_SPECIAL_I)=0 instead of !=0\n");
        }

        /* test caseCompare() */
        result=mixed.caseCompare(different, U_FOLD_CASE_DEFAULT);
        if(result<=0) {
            errln("error: mixed.caseCompare(different, default)=%ld instead of positive\n", result);
        }

        /* test caseCompare() - include the folded sharp s (U+00df) with different lengths */
        result=mixed.caseCompare(1, 4, different, 1, 5, U_FOLD_CASE_DEFAULT);
        if(result!=0 || 0!=mixed.caseCompareBetween(1, 5, different, 1, 6, U_FOLD_CASE_DEFAULT)) {
            errln("error: mixed.caseCompare(mixed, 1, 4, different, 1, 5, default)=%ld instead of 0\n", result);
        }

        /* test caseCompare() - stop in the middle of the sharp s (U+00df) */
        result=mixed.caseCompare(1, 4, different, 1, 4, U_FOLD_CASE_DEFAULT);
        if(result<=0) {
            errln("error: mixed.caseCompare(1, 4, different, 1, 4, default)=%ld instead of positive\n", result);
        }
    }

    // test that srcLength=-1 is handled in functions that
    // take input const char16_t */int32_t srcLength (j785)
    {
        static const char16_t u[]={ 0x61, 0x308, 0x62, 0 };
        UnicodeString s=UNICODE_STRING("a\\u0308b", 8).unescape();

        if(s.compare(u, -1)!=0 || s.compare(0, 999, u, 0, -1)!=0) {
            errln("error UnicodeString::compare(..., const char16_t *, srcLength=-1) does not work");
        }

        if(s.compareCodePointOrder(u, -1)!=0 || s.compareCodePointOrder(0, 999, u, 0, -1)!=0) {
            errln("error UnicodeString::compareCodePointOrder(..., const char16_t *, srcLength=-1, ...) does not work");
        }

        if(s.caseCompare(u, -1, U_FOLD_CASE_DEFAULT)!=0 || s.caseCompare(0, 999, u, 0, -1, U_FOLD_CASE_DEFAULT)!=0) {
            errln("error UnicodeString::caseCompare(..., const char16_t *, srcLength=-1, ...) does not work");
        }

        if(s.indexOf(u, 1, -1, 0, 999)!=1 || s.indexOf(u+1, -1, 0, 999)!=1 || s.indexOf(u+1, -1, 0)!=1) {
            errln("error UnicodeString::indexOf(const char16_t *, srcLength=-1, ...) does not work");
        }

        if(s.lastIndexOf(u, 1, -1, 0, 999)!=1 || s.lastIndexOf(u+1, -1, 0, 999)!=1 || s.lastIndexOf(u+1, -1, 0)!=1) {
            errln("error UnicodeString::lastIndexOf(const char16_t *, srcLength=-1, ...) does not work");
        }

        UnicodeString s2, s3;
        s2.replace(0, 0, u+1, -1);
        s3.replace(0, 0, u, 1, -1);
        if(s.compare(1, 999, s2)!=0 || s2!=s3) {
            errln("error UnicodeString::replace(..., const char16_t *, srcLength=-1, ...) does not work");
        }
    }
}

void
UnicodeStringTest::TestExtract()
{
    UnicodeString  test1("Now is the time for all good men to come to the aid of their country.", "");
    UnicodeString  test2;
    char16_t       test3[13] = {1, 2, 3, 4, 5, 6, 7, 8, 8, 10, 11, 12, 13};
    char           test4[13] = {1, 2, 3, 4, 5, 6, 7, 8, 8, 10, 11, 12, 13};
    UnicodeString  test5;
    char           test6[13] = {1, 2, 3, 4, 5, 6, 7, 8, 8, 10, 11, 12, 13};

    test1.extract(11, 12, test2);
    test1.extract(11, 12, test3);
    if (test1.extract(11, 12, test4) != 12 || test4[12] != 0) {
        errln("UnicodeString.extract(char *) failed to return the correct size of destination buffer.");
    }

    // test proper pinning in extractBetween()
    test1.extractBetween(-3, 7, test5);
    if(test5!=UNICODE_STRING("Now is ", 7)) {
        errln("UnicodeString.extractBetween(-3, 7) did not pin properly.");
    }

    test1.extractBetween(11, 23, test5);
    if (test1.extract(60, 71, test6) != 9) {
        errln("UnicodeString.extract() failed to return the correct size of destination buffer for end of buffer.");
    }
    if (test1.extract(11, 12, test6) != 12) {
        errln("UnicodeString.extract() failed to return the correct size of destination buffer.");
    }

    // convert test4 back to Unicode for comparison
    UnicodeString test4b(test4, 12);

    if (test1.extract(11, 12, (char *)nullptr) != 12) {
        errln("UnicodeString.extract(nullptr) failed to return the correct size of destination buffer.");
    }
    if (test1.extract(11, -1, test6) != 0) {
        errln("UnicodeString.extract(-1) failed to stop reading the string.");
    }

    for (int32_t i = 0; i < 12; i++) {
        if (test1.charAt((int32_t)(11 + i)) != test2.charAt(i)) {
            errln(UnicodeString("extracting into a UnicodeString failed at position ") + i);
            break;
        }
        if (test1.charAt((int32_t)(11 + i)) != test3[i]) {
            errln(UnicodeString("extracting into an array of char16_t failed at position ") + i);
            break;
        }
        if (((char)test1.charAt((int32_t)(11 + i))) != test4b.charAt(i)) {
            errln(UnicodeString("extracting into an array of char failed at position ") + i);
            break;
        }
        if (test1.charAt((int32_t)(11 + i)) != test5.charAt(i)) {
            errln(UnicodeString("extracting with extractBetween failed at position ") + i);
            break;
        }
    }

    // test preflighting and overflows with invariant conversion
    if (test1.extract(0, 10, (char *)nullptr, "") != 10) {
        errln("UnicodeString.extract(0, 10, (char *)nullptr, \"\") != 10");
    }

    test4[2] = (char)0xff;
    if (test1.extract(0, 10, test4, 2, "") != 10) {
        errln("UnicodeString.extract(0, 10, test4, 2, \"\") != 10");
    }
    if (test4[2] != (char)0xff) {
        errln("UnicodeString.extract(0, 10, test4, 2, \"\") overwrote test4[2]");
    }

    {
        // test new, NUL-terminating extract() function
        UnicodeString s("terminate", "");
        char16_t dest[20]={
            0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5,
            0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5
        };
        UErrorCode errorCode;
        int32_t length;

        errorCode=U_ZERO_ERROR;
        length=s.extract((char16_t *)nullptr, 0, errorCode);
        if(errorCode!=U_BUFFER_OVERFLOW_ERROR || length!=s.length()) {
            errln("UnicodeString.extract(nullptr, 0)==%d (%s) expected %d (U_BUFFER_OVERFLOW_ERROR)", length, s.length(), u_errorName(errorCode));
        }

        errorCode=U_ZERO_ERROR;
        length=s.extract(dest, s.length()-1, errorCode);
        if(errorCode!=U_BUFFER_OVERFLOW_ERROR || length!=s.length()) {
            errln("UnicodeString.extract(dest too short)==%d (%s) expected %d (U_BUFFER_OVERFLOW_ERROR)",
                length, u_errorName(errorCode), s.length());
        }

        errorCode=U_ZERO_ERROR;
        length=s.extract(dest, s.length(), errorCode);
        if(errorCode!=U_STRING_NOT_TERMINATED_WARNING || length!=s.length()) {
            errln("UnicodeString.extract(dest just right without NUL)==%d (%s) expected %d (U_STRING_NOT_TERMINATED_WARNING)",
                length, u_errorName(errorCode), s.length());
        }
        if(dest[length-1]!=s[length-1] || dest[length]!=0xa5) {
            errln("UnicodeString.extract(dest just right without NUL) did not extract the string correctly");
        }

        errorCode=U_ZERO_ERROR;
        length=s.extract(dest, s.length()+1, errorCode);
        if(errorCode!=U_ZERO_ERROR || length!=s.length()) {
            errln("UnicodeString.extract(dest large enough)==%d (%s) expected %d (U_ZERO_ERROR)",
                length, u_errorName(errorCode), s.length());
        }
        if(dest[length-1]!=s[length-1] || dest[length]!=0 || dest[length+1]!=0xa5) {
            errln("UnicodeString.extract(dest large enough) did not extract the string correctly");
        }
    }

    {
        // test new UConverter extract() and constructor
        UnicodeString s=UNICODE_STRING("\\U0002f999\\U0001d15f\\u00c4\\u1ed0", 32).unescape();
        char buffer[32];
        static const char expect[]={
            (char)0xf0, (char)0xaf, (char)0xa6, (char)0x99,
            (char)0xf0, (char)0x9d, (char)0x85, (char)0x9f,
            (char)0xc3, (char)0x84,
            (char)0xe1, (char)0xbb, (char)0x90
        };
        UErrorCode errorCode=U_ZERO_ERROR;
        UConverter *cnv=ucnv_open("UTF-8", &errorCode);
        int32_t length;

        if(U_SUCCESS(errorCode)) {
            // test preflighting
            if( (length=s.extract(nullptr, 0, cnv, errorCode))!=13 ||
                errorCode!=U_BUFFER_OVERFLOW_ERROR
            ) {
                errln("UnicodeString::extract(nullptr, UConverter) preflighting failed (length=%ld, %s)",
                      length, u_errorName(errorCode));
            }
            errorCode=U_ZERO_ERROR;
            if( (length=s.extract(buffer, 2, cnv, errorCode))!=13 ||
                errorCode!=U_BUFFER_OVERFLOW_ERROR
            ) {
                errln("UnicodeString::extract(too small, UConverter) preflighting failed (length=%ld, %s)",
                      length, u_errorName(errorCode));
            }

            // try error cases
            errorCode=U_ZERO_ERROR;
            if( s.extract(nullptr, 2, cnv, errorCode)==13 || U_SUCCESS(errorCode)) {
                errln("UnicodeString::extract(UConverter) succeeded with an illegal destination");
            }
            errorCode=U_ILLEGAL_ARGUMENT_ERROR;
            if( s.extract(nullptr, 0, cnv, errorCode)==13 || U_SUCCESS(errorCode)) {
                errln("UnicodeString::extract(UConverter) succeeded with a previous error code");
            }
            errorCode=U_ZERO_ERROR;

            // extract for real
            if( (length=s.extract(buffer, sizeof(buffer), cnv, errorCode))!=13 ||
                uprv_memcmp(buffer, expect, 13)!=0 ||
                buffer[13]!=0 ||
                U_FAILURE(errorCode)
            ) {
                errln("UnicodeString::extract(UConverter) conversion failed (length=%ld, %s)",
                      length, u_errorName(errorCode));
            }
            // Test again with just the converter name.
            if( (length=s.extract(0, s.length(), buffer, sizeof(buffer), "UTF-8"))!=13 ||
                uprv_memcmp(buffer, expect, 13)!=0 ||
                buffer[13]!=0 ||
                U_FAILURE(errorCode)
            ) {
                errln("UnicodeString::extract(\"UTF-8\") conversion failed (length=%ld, %s)",
                      length, u_errorName(errorCode));
            }

            // try the constructor
            UnicodeString t(expect, sizeof(expect), cnv, errorCode);
            if(U_FAILURE(errorCode) || s!=t) {
                errln("UnicodeString(UConverter) conversion failed (%s)",
                      u_errorName(errorCode));
            }

            ucnv_close(cnv);
        }
    }
}

void
UnicodeStringTest::TestRemoveReplace()
{
    UnicodeString   test1("The rain in Spain stays mainly on the plain");
    UnicodeString   test2("eat SPAMburgers!");
    char16_t      test3[] = { 0x53, 0x50, 0x41, 0x4d, 0x4d, 0 };
    char            test4[] = "SPAM";
    UnicodeString&  test5 = test1;

    test1.replace(4, 4, test2, 4, 4);
    test1.replace(12, 5, test3, 4);
    test3[4] = 0;
    test1.replace(17, 4, test3);
    test1.replace(23, 4, test4);
    test1.replaceBetween(37, 42, test2, 4, 8);

    if (test1 != "The SPAM in SPAM SPAMs SPAMly on the SPAM")
        errln("One of the replace methods failed:\n"
              "  expected \"The SPAM in SPAM SPAMs SPAMly on the SPAM\",\n"
              "  got \"" + test1 + "\"");

    test1.remove(21, 1);
    test1.removeBetween(26, 28);

    if (test1 != "The SPAM in SPAM SPAM SPAM on the SPAM")
        errln("One of the remove methods failed:\n"
              "  expected \"The SPAM in SPAM SPAM SPAM on the SPAM\",\n"
              "  got \"" + test1 + "\"");

    for (int32_t i = 0; i < test1.length(); i++) {
        if (test5[i] != 0x53 && test5[i] != 0x50 && test5[i] != 0x41 && test5[i] != 0x4d && test5[i] != 0x20) {
            test1.setCharAt(i, 0x78);
        }
    }

    if (test1 != "xxx SPAM xx SPAM SPAM SPAM xx xxx SPAM")
        errln("One of the remove methods failed:\n"
              "  expected \"xxx SPAM xx SPAM SPAM SPAM xx xxx SPAM\",\n"
              "  got \"" + test1 + "\"");

    test1.remove();
    if (test1.length() != 0)
        errln("Remove() failed: expected empty string, got \"" + test1 + "\"");
}

void
UnicodeStringTest::TestSearching()
{
    UnicodeString test1("test test ttest tetest testesteststt");
    UnicodeString test2("test");
    char16_t testChar = 0x74;
    
    UChar32 testChar32 = 0x20402;
    char16_t testData[]={
        //   0       1       2       3       4       5       6       7
        0xd841, 0xdc02, 0x0071, 0xdc02, 0xd841, 0x0071, 0xd841, 0xdc02,

        //   8       9      10      11      12      13      14      15
        0x0071, 0x0072, 0xd841, 0xdc02, 0x0071, 0xd841, 0xdc02, 0x0071,

        //  16      17      18      19
        0xdc02, 0xd841, 0x0073, 0x0000
    };
    UnicodeString test3(testData);
    UnicodeString test4(testChar32);

    uint16_t occurrences = 0;
    int32_t startPos = 0;
    for ( ;
          startPos != -1 && startPos < test1.length();
          (startPos = test1.indexOf(test2, startPos)) != -1 ? (++occurrences, startPos += 4) : 0)
        ;
    if (occurrences != 6)
        errln(UnicodeString("indexOf failed: expected to find 6 occurrences, found ") + occurrences);

    for ( occurrences = 0, startPos = 10;
          startPos != -1 && startPos < test1.length();
          (startPos = test1.indexOf(test2, startPos)) != -1 ? (++occurrences, startPos += 4) : 0)
        ;
    if (occurrences != 4)
        errln(UnicodeString("indexOf with starting offset failed: "
                            "expected to find 4 occurrences, found ") + occurrences);

    int32_t endPos = 28;
    for ( occurrences = 0, startPos = 5;
          startPos != -1 && startPos < test1.length();
          (startPos = test1.indexOf(test2, startPos, endPos - startPos)) != -1 ? (++occurrences, startPos += 4) : 0)
        ;
    if (occurrences != 4)
        errln(UnicodeString("indexOf with starting and ending offsets failed: "
                            "expected to find 4 occurrences, found ") + occurrences);

    //using UChar32 string
    for ( startPos=0, occurrences=0;
          startPos != -1 && startPos < test3.length();
          (startPos = test3.indexOf(test4, startPos)) != -1 ? (++occurrences, startPos += 2) : 0)
        ;
    if (occurrences != 4)
        errln((UnicodeString)"indexOf failed: expected to find 4 occurrences, found " + occurrences);

    for ( startPos=10, occurrences=0;
          startPos != -1 && startPos < test3.length();
          (startPos = test3.indexOf(test4, startPos)) != -1 ? (++occurrences, startPos += 2) : 0)
        ;
    if (occurrences != 2)
        errln(UnicodeString("indexOf failed: expected to find 2 occurrences, found ") + occurrences);
    //---

    for ( occurrences = 0, startPos = 0;
          startPos != -1 && startPos < test1.length();
          (startPos = test1.indexOf(testChar, startPos)) != -1 ? (++occurrences, startPos += 1) : 0)
        ;
    if (occurrences != 16)
        errln(UnicodeString("indexOf with character failed: "
                            "expected to find 16 occurrences, found ") + occurrences);

    for ( occurrences = 0, startPos = 10;
          startPos != -1 && startPos < test1.length();
          (startPos = test1.indexOf(testChar, startPos)) != -1 ? (++occurrences, startPos += 1) : 0)
        ;
    if (occurrences != 12)
        errln(UnicodeString("indexOf with character & start offset failed: "
                            "expected to find 12 occurrences, found ") + occurrences);

    for ( occurrences = 0, startPos = 5, endPos = 28;
          startPos != -1 && startPos < test1.length();
          (startPos = test1.indexOf(testChar, startPos, endPos - startPos)) != -1 ? (++occurrences, startPos += 1) : 0)
        ;
    if (occurrences != 10)
        errln(UnicodeString("indexOf with character & start & end offsets failed: "
                            "expected to find 10 occurrences, found ") + occurrences);

    //testing for UChar32
    UnicodeString subString;
    for( occurrences =0, startPos=0; startPos < test3.length(); startPos +=1){
        subString.append(test3, startPos, test3.length());
        if(subString.indexOf(testChar32) != -1 ){
             ++occurrences;
        }
        subString.remove();
    }
    if (occurrences != 14)
        errln((UnicodeString)"indexOf failed: expected to find 14 occurrences, found " + occurrences);

    for ( occurrences = 0, startPos = 0;
          startPos != -1 && startPos < test3.length();
          (startPos = test3.indexOf(testChar32, startPos)) != -1 ? (++occurrences, startPos += 1) : 0)
        ;
    if (occurrences != 4)
        errln((UnicodeString)"indexOf failed: expected to find 4 occurrences, found " + occurrences);
     
    endPos=test3.length();
    for ( occurrences = 0, startPos = 5;
          startPos != -1 && startPos < test3.length();
          (startPos = test3.indexOf(testChar32, startPos, endPos - startPos)) != -1 ? (++occurrences, startPos += 1) : 0)
        ;
    if (occurrences != 3)
        errln((UnicodeString)"indexOf with character & start & end offsets failed: expected to find 2 occurrences, found " + occurrences);
    //---

    if(test1.lastIndexOf(test2)!=29) {
        errln("test1.lastIndexOf(test2)!=29");
    }

    if(test1.lastIndexOf(test2, 15)!=29 || test1.lastIndexOf(test2, 29)!=29 || test1.lastIndexOf(test2, 30)!=-1) {
        errln("test1.lastIndexOf(test2, start) failed");
    }

    for ( occurrences = 0, startPos = 32;
          startPos != -1;
          (startPos = test1.lastIndexOf(test2, 5, startPos - 5)) != -1 ? ++occurrences : 0)
        ;
    if (occurrences != 4)
        errln(UnicodeString("lastIndexOf with starting and ending offsets failed: "
                            "expected to find 4 occurrences, found ") + occurrences);

    for ( occurrences = 0, startPos = 32;
          startPos != -1;
          (startPos = test1.lastIndexOf(testChar, 5, startPos - 5)) != -1 ? ++occurrences : 0)
        ;
    if (occurrences != 11)
        errln(UnicodeString("lastIndexOf with character & start & end offsets failed: "
                            "expected to find 11 occurrences, found ") + occurrences);

    //testing UChar32
    startPos=test3.length();
    for ( occurrences = 0;
          startPos != -1;
          (startPos = test3.lastIndexOf(testChar32, 5, startPos - 5)) != -1 ? ++occurrences : 0)
        ;
    if (occurrences != 3)
        errln((UnicodeString)"lastIndexOf with character & start & end offsets failed: expected to find 3 occurrences, found " + occurrences);


    for ( occurrences = 0, endPos = test3.length();  endPos > 0; endPos -= 1){
        subString.remove();
        subString.append(test3, 0, endPos);
        if(subString.lastIndexOf(testChar32) != -1 ){
            ++occurrences;
        }
    }
    if (occurrences != 18)
        errln((UnicodeString)"indexOf failed: expected to find 18 occurrences, found " + occurrences);
    //---

    // test that indexOf(UChar32) and lastIndexOf(UChar32)
    // do not find surrogate code points when they are part of matched pairs
    // (= part of supplementary code points)
    // Jitterbug 1542
    if(test3.indexOf((UChar32)0xd841) != 4 || test3.indexOf((UChar32)0xdc02) != 3) {
        errln("error: UnicodeString::indexOf(UChar32 surrogate) finds a partial supplementary code point");
    }
    if( UnicodeString(test3, 0, 17).lastIndexOf((char16_t)0xd841, 0) != 4 ||
        UnicodeString(test3, 0, 17).lastIndexOf((UChar32)0xd841, 2) != 4 ||
        test3.lastIndexOf((UChar32)0xd841, 0, 17) != 4 || test3.lastIndexOf((UChar32)0xdc02, 0, 17) != 16
    ) {
        errln("error: UnicodeString::lastIndexOf(UChar32 surrogate) finds a partial supplementary code point");
    }
}

void
UnicodeStringTest::TestSpacePadding()
{
    UnicodeString test1("hello");
    UnicodeString test2("   there");
    UnicodeString test3("Hi!  How ya doin'?  Beautiful day, isn't it?");
    UnicodeString test4;
    UBool returnVal;
    UnicodeString expectedValue;

    returnVal = test1.padLeading(15);
    expectedValue = "          hello";
    if (returnVal == false || test1 != expectedValue)
        errln("padLeading() failed: expected \"" + expectedValue + "\", got \"" + test1 + "\".");

    returnVal = test2.padTrailing(15);
    expectedValue = "   there       ";
    if (returnVal == false || test2 != expectedValue)
        errln("padTrailing() failed: expected \"" + expectedValue + "\", got \"" + test2 + "\".");

    expectedValue = test3;
    returnVal = test3.padTrailing(15);
    if (returnVal == true || test3 != expectedValue)
        errln("padTrailing() failed: expected \"" + expectedValue + "\", got \"" + test3 + "\".");

    expectedValue = "hello";
    test4.setTo(test1).trim();

    if (test4 != expectedValue || test1 == expectedValue || test4 != expectedValue)
        errln("trim(UnicodeString&) failed");
    
    test1.trim();
    if (test1 != expectedValue)
        errln("trim() failed: expected \"" + expectedValue + "\", got \"" + test1 + "\".");

    test2.trim();
    expectedValue = "there";
    if (test2 != expectedValue)
        errln("trim() failed: expected \"" + expectedValue + "\", got \"" + test2 + "\".");

    test3.trim();
    expectedValue = "Hi!  How ya doin'?  Beautiful day, isn't it?";
    if (test3 != expectedValue)
        errln("trim() failed: expected \"" + expectedValue + "\", got \"" + test3 + "\".");

    returnVal = test1.truncate(15);
    expectedValue = "hello";
    if (returnVal == true || test1 != expectedValue)
        errln("truncate() failed: expected \"" + expectedValue + "\", got \"" + test1 + "\".");

    returnVal = test2.truncate(15);
    expectedValue = "there";
    if (returnVal == true || test2 != expectedValue)
        errln("truncate() failed: expected \"" + expectedValue + "\", got \"" + test2 + "\".");

    returnVal = test3.truncate(15);
    expectedValue = "Hi!  How ya doi";
    if (returnVal == false || test3 != expectedValue)
        errln("truncate() failed: expected \"" + expectedValue + "\", got \"" + test3 + "\".");
}

void
UnicodeStringTest::TestPrefixAndSuffix()
{
    UnicodeString test1("Now is the time for all good men to come to the aid of their country.");
    UnicodeString test2("Now");
    UnicodeString test3("country.");
    UnicodeString test4("count");

    if (!test1.startsWith(test2) || !test1.startsWith(test2, 0, test2.length())) {
        errln("startsWith() failed: \"" + test2 + "\" should be a prefix of \"" + test1 + "\".");
    }

    if (test1.startsWith(test3) ||
        test1.startsWith(test3.getBuffer(), test3.length()) ||
        test1.startsWith(test3.getTerminatedBuffer(), 0, -1)
    ) {
        errln("startsWith() failed: \"" + test3 + "\" shouldn't be a prefix of \"" + test1 + "\".");
    }

    if (test1.endsWith(test2)) {
        errln("endsWith() failed: \"" + test2 + "\" shouldn't be a suffix of \"" + test1 + "\".");
    }

    if (!test1.endsWith(test3)) { 
        errln("endsWith(test3) failed: \"" + test3 + "\" should be a suffix of \"" + test1 + "\".");
    }
    if (!test1.endsWith(test3, 0, INT32_MAX)) {
        errln("endsWith(test3, 0, INT32_MAX) failed: \"" + test3 + "\" should be a suffix of \"" + test1 + "\".");
    }

    if(!test1.endsWith(test3.getBuffer(), test3.length())) {
        errln("endsWith(test3.getBuffer(), test3.length()) failed: \"" + test3 + "\" should be a suffix of \"" + test1 + "\".");
    }
    if(!test1.endsWith(test3.getTerminatedBuffer(), 0, -1)) {
        errln("endsWith(test3.getTerminatedBuffer(), 0, -1) failed: \"" + test3 + "\" should be a suffix of \"" + test1 + "\".");
    }

    if (!test3.startsWith(test4)) {
        errln("endsWith(test4) failed: \"" + test4 + "\" should be a prefix of \"" + test3 + "\".");
    }

    if (test4.startsWith(test3)) {
        errln("startsWith(test3) failed: \"" + test3 + "\" shouldn't be a prefix of \"" + test4 + "\".");
    }
}

void
UnicodeStringTest::TestStartsWithAndEndsWithNulTerminated() {
    UnicodeString test("abcde");
    const char16_t ab[] = { 0x61, 0x62, 0 };
    const char16_t de[] = { 0x64, 0x65, 0 };
    assertTrue("abcde.startsWith(ab, -1)", test.startsWith(ab, -1));
    assertTrue("abcde.startsWith(ab, 0, -1)", test.startsWith(ab, 0, -1));
    assertTrue("abcde.endsWith(de, -1)", test.endsWith(de, -1));
    assertTrue("abcde.endsWith(de, 0, -1)", test.endsWith(de, 0, -1));
}

void
UnicodeStringTest::TestFindAndReplace()
{
    UnicodeString test1("One potato, two potato, three potato, four\n");
    UnicodeString test2("potato");
    UnicodeString test3("MISSISSIPPI");

    UnicodeString expectedValue;

    test1.findAndReplace(test2, test3);
    expectedValue = "One MISSISSIPPI, two MISSISSIPPI, three MISSISSIPPI, four\n";
    if (test1 != expectedValue)
        errln("findAndReplace failed: expected \"" + expectedValue + "\", got \"" + test1 + "\".");
    test1.findAndReplace(2, 32, test3, test2);
    expectedValue = "One potato, two potato, three MISSISSIPPI, four\n";
    if (test1 != expectedValue)
        errln("findAndReplace failed: expected \"" + expectedValue + "\", got \"" + test1 + "\".");
}

void
UnicodeStringTest::TestReverse()
{
    UnicodeString test("backwards words say to used I");

    test.reverse();
    test.reverse(2, 4);
    test.reverse(7, 2);
    test.reverse(10, 3);
    test.reverse(14, 5);
    test.reverse(20, 9);

    if (test != "I used to say words backwards")
        errln("reverse() failed:  Expected \"I used to say words backwards\",\n got \""
            + test + "\"");

    test=UNICODE_STRING("\\U0002f999\\U0001d15f\\u00c4\\u1ed0", 32).unescape();
    test.reverse();
    if(test.char32At(0)!=0x1ed0 || test.char32At(1)!=0xc4 || test.char32At(2)!=0x1d15f || test.char32At(4)!=0x2f999) {
        errln("reverse() failed with supplementary characters");
    }

    // Test case for ticket #8091:
    // UnicodeString::reverse() failed to see a lead surrogate in the middle of
    // an odd-length string that contains no other lead surrogates.
    test=UNICODE_STRING_SIMPLE("ab\\U0001F4A9e").unescape();
    UnicodeString expected=UNICODE_STRING_SIMPLE("e\\U0001F4A9ba").unescape();
    test.reverse();
    if(test!=expected) {
        errln("reverse() failed with only lead surrogate in the middle");
    }
}

void
UnicodeStringTest::TestMiscellaneous()
{
    UnicodeString   test1("This is a test");
    UnicodeString   test2("This is a test");
    UnicodeString   test3("Me too!");

    // test getBuffer(minCapacity) and releaseBuffer()
    test1=UnicodeString(); // make sure that it starts with its stackBuffer
    char16_t *p=test1.getBuffer(20);
    if(test1.getCapacity()<20) {
        errln("UnicodeString::getBuffer(20).getCapacity()<20");
    }

    test1.append((char16_t)7); // must not be able to modify the string here
    test1.setCharAt(3, 7);
    test1.reverse();
    if (test1.length() != 0 ||
        test1.charAt(0) != 0xffff || test1.charAt(3) != 0xffff ||
        test1.getBuffer(10) != nullptr || test1.getBuffer() != nullptr
    ) {
        errln("UnicodeString::getBuffer(minCapacity) allows read or write access to the UnicodeString");
    }

    p[0]=1;
    p[1]=2;
    p[2]=3;
    test1.releaseBuffer(3);
    test1.append((char16_t)4);

    if(test1.length()!=4 || test1.charAt(0)!=1 || test1.charAt(1)!=2 || test1.charAt(2)!=3 || test1.charAt(3)!=4) {
        errln("UnicodeString::releaseBuffer(newLength) does not properly reallow access to the UnicodeString");
    }

    // test releaseBuffer() without getBuffer(minCapacity) - must not have any effect
    test1.releaseBuffer(1);
    if(test1.length()!=4 || test1.charAt(0)!=1 || test1.charAt(1)!=2 || test1.charAt(2)!=3 || test1.charAt(3)!=4) {
        errln("UnicodeString::releaseBuffer(newLength) without getBuffer(minCapacity) changed the UnicodeString");
    }

    // test getBuffer(const)
    const char16_t *q=test1.getBuffer(), *r=test1.getBuffer();
    if( test1.length()!=4 ||
        q[0]!=1 || q[1]!=2 || q[2]!=3 || q[3]!=4 ||
        r[0]!=1 || r[1]!=2 || r[2]!=3 || r[3]!=4
    ) {
        errln("UnicodeString::getBuffer(const) does not return a usable buffer pointer");
    }

    // test releaseBuffer() with a NUL-terminated buffer
    test1.getBuffer(20)[2]=0;
    test1.releaseBuffer(); // implicit -1
    if(test1.length()!=2 || test1.charAt(0)!=1 || test1.charAt(1) !=2) {
        errln("UnicodeString::releaseBuffer(-1) does not properly set the length of the UnicodeString");
    }

    // test releaseBuffer() with a non-NUL-terminated buffer
    p=test1.getBuffer(256);
    for(int32_t i=0; i<test1.getCapacity(); ++i) {
        p[i]=(char16_t)1;      // fill the buffer with all non-NUL code units
    }
    test1.releaseBuffer();  // implicit -1
    if(test1.length()!=test1.getCapacity() || test1.charAt(1)!=1 || test1.charAt(100)!=1 || test1.charAt(test1.getCapacity()-1)!=1) {
        errln("UnicodeString::releaseBuffer(-1 but no NUL) does not properly set the length of the UnicodeString");
    }

    // test getTerminatedBuffer()
    test1=UnicodeString("This is another test.", "");
    test2=UnicodeString("This is another test.", "");
    q=test1.getTerminatedBuffer();
    if(q[test1.length()]!=0 || test1!=test2 || test2.compare(q, -1)!=0) {
        errln("getTerminatedBuffer()[length]!=0");
    }

    const char16_t u[]={ 5, 6, 7, 8, 0 };
    test1.setTo(false, u, 3);
    q=test1.getTerminatedBuffer();
    if(q==u || q[0]!=5 || q[1]!=6 || q[2]!=7 || q[3]!=0) {
        errln("UnicodeString(u[3]).getTerminatedBuffer() returns a bad buffer");
    }

    test1.setTo(true, u, -1);
    q=test1.getTerminatedBuffer();
    if(q!=u || test1.length()!=4 || q[3]!=8 || q[4]!=0) {
        errln("UnicodeString(u[-1]).getTerminatedBuffer() returns a bad buffer");
    }

    // NOTE: Some compilers will optimize u"la" to point to the same static memory
    // as u" lila", offset by 3 code units
    test1=UnicodeString(true, u"la", 2);
    test1.append(UnicodeString(true, u" lila", 5).getTerminatedBuffer(), 0, -1);
    assertEquals("UnicodeString::append(const char16_t *, start, length) failed",
        u"la lila", test1);

    test1.insert(3, UnicodeString(true, u"dudum ", 6), 0, INT32_MAX);
    assertEquals("UnicodeString::insert(start, const UniStr &, start, length) failed",
        u"la dudum lila", test1);

    static const char16_t ucs[]={ 0x68, 0x6d, 0x20, 0 };
    test1.insert(9, ucs, -1);
    assertEquals("UnicodeString::insert(start, const char16_t *, length) failed",
        u"la dudum hm lila", test1);

    test1.replace(9, 2, (char16_t)0x2b);
    assertEquals("UnicodeString::replace(start, length, char16_t) failed",
        u"la dudum + lila", test1);

    if(test1.hasMetaData() || UnicodeString().hasMetaData()) {
        errln("UnicodeString::hasMetaData() returns true");
    }

    // test getTerminatedBuffer() on a truncated, shared, heap-allocated string
    test1=UNICODE_STRING_SIMPLE("abcdefghijklmnopqrstuvwxyz0123456789.");
    test1.truncate(36);  // ensure length()<getCapacity()
    test2=test1;  // share the buffer
    test1.truncate(5);
    if(test1.length()!=5 || test1.getTerminatedBuffer()[5]!=0) {
        errln("UnicodeString(shared buffer).truncate() failed");
    }
    if(test2.length()!=36 || test2[5]!=0x66 || u_strlen(test2.getTerminatedBuffer())!=36) {
        errln("UnicodeString(shared buffer).truncate().getTerminatedBuffer() "
              "modified another copy of the string!");
    }
    test1=UNICODE_STRING_SIMPLE("abcdefghijklmnopqrstuvwxyz0123456789.");
    test1.truncate(36);  // ensure length()<getCapacity()
    test2=test1;  // share the buffer
    test1.remove();
    if(test1.length()!=0 || test1.getTerminatedBuffer()[0]!=0) {
        errln("UnicodeString(shared buffer).remove() failed");
    }
    if(test2.length()!=36 || test2[0]!=0x61 || u_strlen(test2.getTerminatedBuffer())!=36) {
        errln("UnicodeString(shared buffer).remove().getTerminatedBuffer() "
              "modified another copy of the string!");
    }

    // ticket #9740
    test1.setTo(true, ucs, 3);
    assertEquals("length of read-only alias", 3, test1.length());
    test1.trim();
    assertEquals("length of read-only alias after trim()", 2, test1.length());
    assertEquals("length of terminated buffer of read-only alias + trim()",
                 2, u_strlen(test1.getTerminatedBuffer()));
}

void
UnicodeStringTest::TestStackAllocation()
{
    char16_t        testString[] ={
        0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x63, 0x72, 0x61, 0x7a, 0x79, 0x20, 0x74, 0x65, 0x73, 0x74, 0x2e, 0 };
    char16_t        guardWord = 0x4DED;
    UnicodeString* test = nullptr;

    test = new  UnicodeString(testString);
    if (*test != "This is a crazy test.")
        errln("Test string failed to initialize properly.");
    if (guardWord != 0x04DED)
        errln("Test string initialization overwrote guard word!");

    test->insert(8, "only ");
    test->remove(15, 6);
    if (*test != "This is only a test.")
        errln("Manipulation of test string failed to work right.");
    if (guardWord != 0x4DED)
        errln("Manipulation of test string overwrote guard word!");

    // we have to deinitialize and release the backing store by calling the destructor
    // explicitly, since we can't overload operator delete
    delete test;

    char16_t workingBuffer[] = {
        0x4e, 0x6f, 0x77, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68, 0x65, 0x20, 0x74, 0x69, 0x6d, 0x65, 0x20,
        0x66, 0x6f, 0x72, 0x20, 0x61, 0x6c, 0x6c, 0x20, 0x6d, 0x65, 0x6e, 0x20, 0x74, 0x6f, 0x20,
        0x63, 0x6f, 0x6d, 0x65, 0xffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    char16_t guardWord2 = 0x4DED;

    test = new UnicodeString(workingBuffer, 35, 100);
    if (*test != "Now is the time for all men to come")
        errln("Stack-allocated backing store failed to initialize correctly.");
    if (guardWord2 != 0x4DED)
        errln("Stack-allocated backing store overwrote guard word!");

    test->insert(24, "good ");
    if (*test != "Now is the time for all good men to come")
        errln("insert() on stack-allocated UnicodeString didn't work right");
    if (guardWord2 != 0x4DED)
        errln("insert() on stack-allocated UnicodeString overwrote guard word!");

    if (workingBuffer[24] != 0x67)
        errln("insert() on stack-allocated UnicodeString didn't affect backing store");

    *test += " to the aid of their country.";
    if (*test != "Now is the time for all good men to come to the aid of their country.")
        errln("Stack-allocated UnicodeString overflow didn't work");
    if (guardWord2 != 0x4DED)
        errln("Stack-allocated UnicodeString overflow overwrote guard word!");

    *test = "ha!";
    if (*test != "ha!")
        errln("Assignment to stack-allocated UnicodeString didn't work");
    if (workingBuffer[0] != 0x4e)
        errln("Change to UnicodeString after overflow are still affecting original buffer");
    if (guardWord2 != 0x4DED)
        errln("Change to UnicodeString after overflow overwrote guard word!");

    // test read-only aliasing with setTo()
    workingBuffer[0] = 0x20ac;
    workingBuffer[1] = 0x125;
    workingBuffer[2] = 0;
    test->setTo(true, workingBuffer, 2);
    if(test->length() != 2 || test->charAt(0) != 0x20ac || test->charAt(1) != 0x125) {
        errln("UnicodeString.setTo(readonly alias) does not alias correctly");
    }

    UnicodeString *c=test->clone();

    workingBuffer[1] = 0x109;
    if(test->charAt(1) != 0x109) {
        errln("UnicodeString.setTo(readonly alias) made a copy: did not see change in buffer");
    }

    if(c->length() != 2 || c->charAt(1) != 0x125) {
        errln("clone(alias) did not copy the buffer");
    }
    delete c;

    test->setTo(true, workingBuffer, -1);
    if(test->length() != 2 || test->charAt(0) != 0x20ac || test->charAt(1) != 0x109) {
        errln("UnicodeString.setTo(readonly alias, length -1) does not alias correctly");
    }

    test->setTo(false, workingBuffer, -1);
    if(!test->isBogus()) {
        errln("UnicodeString.setTo(unterminated readonly alias, length -1) does not result in isBogus()");
    }
    
    delete test;
     
    test=new UnicodeString();
    char16_t buffer[]={0x0061, 0x0062, 0x20ac, 0x0043, 0x0042, 0x0000};
    test->setTo(buffer, 4, 10);
    if(test->length() !=4 || test->charAt(0) != 0x0061 || test->charAt(1) != 0x0062 ||
        test->charAt(2) != 0x20ac || test->charAt(3) != 0x0043){
        errln((UnicodeString)"UnicodeString.setTo(char16_t*, length, capacity) does not work correctly\n" + prettify(*test));
    }
    delete test;


    // test the UChar32 constructor
    UnicodeString c32Test((UChar32)0x10ff2a);
    if( c32Test.length() != U16_LENGTH(0x10ff2a) ||
        c32Test.char32At(c32Test.length() - 1) != 0x10ff2a
    ) {
        errln("The UnicodeString(UChar32) constructor does not work with a 0x10ff2a filler");
    }

    // test the (new) capacity constructor
    UnicodeString capTest(5, (UChar32)0x2a, 5);
    if( capTest.length() != 5 * U16_LENGTH(0x2a) ||
        capTest.char32At(0) != 0x2a ||
        capTest.char32At(4) != 0x2a
    ) {
        errln("The UnicodeString capacity constructor does not work with an ASCII filler");
    }

    capTest = UnicodeString(5, (UChar32)0x10ff2a, 5);
    if( capTest.length() != 5 * U16_LENGTH(0x10ff2a) ||
        capTest.char32At(0) != 0x10ff2a ||
        capTest.char32At(4) != 0x10ff2a
    ) {
        errln("The UnicodeString capacity constructor does not work with a 0x10ff2a filler");
    }

    capTest = UnicodeString(5, (UChar32)0, 0);
    if(capTest.length() != 0) {
        errln("The UnicodeString capacity constructor does not work with a 0x10ff2a filler");
    }
}

/**
 * Test the unescape() function.
 */
void UnicodeStringTest::TestUnescape() {
    UnicodeString IN("abc\\u4567 \\n\\r \\U00101234xyz\\x1\\x{5289}\\x1b", -1, US_INV);
    UnicodeString OUT("abc");
    OUT.append((char16_t)0x4567);
    OUT.append(" ");
    OUT.append((char16_t)0xA);
    OUT.append((char16_t)0xD);
    OUT.append(" ");
    OUT.append((UChar32)0x00101234);
    OUT.append("xyz");
    OUT.append((UChar32)1).append((UChar32)0x5289).append((char16_t)0x1b);
    UnicodeString result = IN.unescape();
    if (result != OUT) {
        errln("FAIL: " + prettify(IN) + ".unescape() -> " +
              prettify(result) + ", expected " +
              prettify(OUT));
    }

    // test that an empty string is returned in case of an error
    if (!UNICODE_STRING("wrong \\u sequence", 17).unescape().isEmpty()) {
        errln("FAIL: unescaping of a string with an illegal escape sequence did not return an empty string");
    }

    // ICU-21648 limit backslash-uhhhh escapes to ASCII hex digits
    UnicodeString euro = UnicodeString(u"\\u20aC").unescape();
    assertEquals("ASCII Euro", u"€", euro);
    UnicodeString nonASCIIEuro = UnicodeString(u"\\u୨෦ａＣ").unescape();
    assertTrue("unescape() accepted non-ASCII digits", nonASCIIEuro.isEmpty());
}

/* test code point counting functions --------------------------------------- */

/* reference implementation of UnicodeString::hasMoreChar32Than() */
static int32_t
_refUnicodeStringHasMoreChar32Than(const UnicodeString &s, int32_t start, int32_t length, int32_t number) {
    int32_t count=s.countChar32(start, length);
    return count>number;
}

/* compare the real function against the reference */
void
UnicodeStringTest::_testUnicodeStringHasMoreChar32Than(const UnicodeString &s, int32_t start, int32_t length, int32_t number) {
    if(s.hasMoreChar32Than(start, length, number)!=_refUnicodeStringHasMoreChar32Than(s, start, length, number)) {
        errln("hasMoreChar32Than(%d, %d, %d)=%hd is wrong\n",
                start, length, number, s.hasMoreChar32Than(start, length, number));
    }
}

void
UnicodeStringTest::TestCountChar32() {
    {
        UnicodeString s=UNICODE_STRING("\\U0002f999\\U0001d15f\\u00c4\\u1ed0", 32).unescape();

        // test countChar32()
        // note that this also calls and tests u_countChar32(length>=0)
        if(
            s.countChar32()!=4 ||
            s.countChar32(1)!=4 ||
            s.countChar32(2)!=3 ||
            s.countChar32(2, 3)!=2 ||
            s.countChar32(2, 0)!=0
        ) {
            errln("UnicodeString::countChar32() failed");
        }

        // NUL-terminate the string buffer and test u_countChar32(length=-1)
        const char16_t *buffer=s.getTerminatedBuffer();
        if(
            u_countChar32(buffer, -1)!=4 ||
            u_countChar32(buffer+1, -1)!=4 ||
            u_countChar32(buffer+2, -1)!=3 ||
            u_countChar32(buffer+3, -1)!=3 ||
            u_countChar32(buffer+4, -1)!=2 ||
            u_countChar32(buffer+5, -1)!=1 ||
            u_countChar32(buffer+6, -1)!=0
        ) {
            errln("u_countChar32(length=-1) failed");
        }

        // test u_countChar32() with bad input
        if(u_countChar32(nullptr, 5)!=0 || u_countChar32(buffer, -2)!=0) {
            errln("u_countChar32(bad input) failed (returned non-zero counts)");
        }
    }

    /* test data and variables for hasMoreChar32Than() */
    static const char16_t str[]={
        0x61, 0x62, 0xd800, 0xdc00,
        0xd801, 0xdc01, 0x63, 0xd802,
        0x64, 0xdc03, 0x65, 0x66,
        0xd804, 0xdc04, 0xd805, 0xdc05,
        0x67
    };
    UnicodeString string(str, UPRV_LENGTHOF(str));
    int32_t start, length, number;

    /* test hasMoreChar32Than() */
    for(length=string.length(); length>=0; --length) {
        for(start=0; start<=length; ++start) {
            for(number=-1; number<=((length-start)+2); ++number) {
                _testUnicodeStringHasMoreChar32Than(string, start, length-start, number);
            }
        }
    }

    /* test hasMoreChar32Than() with pinning */
    for(start=-1; start<=string.length()+1; ++start) {
        for(number=-1; number<=((string.length()-start)+2); ++number) {
            _testUnicodeStringHasMoreChar32Than(string, start, 0x7fffffff, number);
        }
    }

    /* test hasMoreChar32Than() with a bogus string */
    string.setToBogus();
    for(length=-1; length<=1; ++length) {
        for(start=-1; start<=length; ++start) {
            for(number=-1; number<=((length-start)+2); ++number) {
                _testUnicodeStringHasMoreChar32Than(string, start, length-start, number);
            }
        }
    }
}

void
UnicodeStringTest::TestBogus() {
    UnicodeString   test1("This is a test");
    UnicodeString   test2("This is a test");
    UnicodeString   test3("Me too!");

    // test isBogus() and setToBogus()
    if (test1.isBogus() || test2.isBogus() || test3.isBogus()) {
        errln("A string returned true for isBogus()!");
    }

    // nullptr pointers are treated like empty strings
    // use other illegal arguments to make a bogus string
    test3.setTo(false, test1.getBuffer(), -2);
    if(!test3.isBogus()) {
        errln("A bogus string returned false for isBogus()!");
    }
    if (test1.hashCode() != test2.hashCode() || test1.hashCode() == test3.hashCode()) {
        errln("hashCode() failed");
    }
    if (test3.getBuffer() != nullptr || test3.getBuffer(20) != nullptr || test3.getTerminatedBuffer() != nullptr) {
        errln("bogus.getBuffer()!=0");
    }
    if (test1.indexOf(test3) != -1) {
        errln("bogus.indexOf() != -1");
    }
    if (test1.lastIndexOf(test3) != -1) {
        errln("bogus.lastIndexOf() != -1");
    }
    if (test1.caseCompare(test3, U_FOLD_CASE_DEFAULT) != 1 || test3.caseCompare(test1, U_FOLD_CASE_DEFAULT) != -1) {
        errln("caseCompare() doesn't work with bogus strings");
    }
    if (test1.compareCodePointOrder(test3) != 1 || test3.compareCodePointOrder(test1) != -1) {
        errln("compareCodePointOrder() doesn't work with bogus strings");
    }

    // verify that non-assignment modifications fail and do not revive a bogus string
    test3.setToBogus();
    test3.append((char16_t)0x61);
    if (!test3.isBogus() || test3.getBuffer() != nullptr) {
        errln("bogus.append('a') worked but must not");
    }

    test3.setToBogus();
    test3.findAndReplace(UnicodeString((char16_t)0x61), test2);
    if (!test3.isBogus() || test3.getBuffer() != nullptr) {
        errln("bogus.findAndReplace() worked but must not");
    }

    test3.setToBogus();
    test3.trim();
    if (!test3.isBogus() || test3.getBuffer() != nullptr) {
        errln("bogus.trim() revived bogus but must not");
    }

    test3.setToBogus();
    test3.remove(1);
    if (!test3.isBogus() || test3.getBuffer() != nullptr) {
        errln("bogus.remove(1) revived bogus but must not");
    }

    test3.setToBogus();
    if(!test3.setCharAt(0, 0x62).isBogus() || !test3.isEmpty()) {
        errln("bogus.setCharAt(0, 'b') worked but must not");
    }

    test3.setToBogus();
    if(test3.truncate(1) || !test3.isBogus() || !test3.isEmpty()) {
        errln("bogus.truncate(1) revived bogus but must not");
    }

    // verify that assignments revive a bogus string
    test3.setToBogus();
    if(!test3.isBogus() || (test3=test1).isBogus() || test3!=test1) {
        errln("bogus.operator=() failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.fastCopyFrom(test1).isBogus() || test3!=test1) {
        errln("bogus.fastCopyFrom() failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo(test1).isBogus() || test3!=test1) {
        errln("bogus.setTo(UniStr) failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo(test1, 0).isBogus() || test3!=test1) {
        errln("bogus.setTo(UniStr, 0) failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo(test1, 0, 0x7fffffff).isBogus() || test3!=test1) {
        errln("bogus.setTo(UniStr, 0, len) failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo(test1.getBuffer(), test1.length()).isBogus() || test3!=test1) {
        errln("bogus.setTo(const char16_t *, len) failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo((char16_t)0x2028).isBogus() || test3!=UnicodeString((char16_t)0x2028)) {
        errln("bogus.setTo(char16_t) failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo((UChar32)0x1d157).isBogus() || test3!=UnicodeString((UChar32)0x1d157)) {
        errln("bogus.setTo(UChar32) failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo(false, test1.getBuffer(), test1.length()).isBogus() || test3!=test1) {
        errln("bogus.setTo(readonly alias) failed");
    }

    // writable alias to another string's buffer: very bad idea, just convenient for this test
    test3.setToBogus();
    if(!test3.isBogus() ||
            test3.setTo(const_cast<char16_t *>(test1.getBuffer()),
                        test1.length(), test1.getCapacity()).isBogus() ||
            test3!=test1) {
        errln("bogus.setTo(writable alias) failed");
    }

    // verify simple, documented ways to turn a bogus string into an empty one
    test3.setToBogus();
    if(!test3.isBogus() || (test3=UnicodeString()).isBogus() || !test3.isEmpty()) {
        errln("bogus.operator=(UnicodeString()) failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo(UnicodeString()).isBogus() || !test3.isEmpty()) {
        errln("bogus.setTo(UnicodeString()) failed");
    }

    test3.setToBogus();
    if (test3.remove().isBogus() || test3.getBuffer() == nullptr || !test3.isEmpty()) {
        errln("bogus.remove() failed");
    }

    test3.setToBogus();
    if (test3.remove(0, INT32_MAX).isBogus() || test3.getBuffer() == nullptr || !test3.isEmpty()) {
        errln("bogus.remove(0, INT32_MAX) failed");
    }

    test3.setToBogus();
    if(test3.truncate(0) || test3.isBogus() || !test3.isEmpty()) {
        errln("bogus.truncate(0) failed");
    }

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo((UChar32)-1).isBogus() || !test3.isEmpty()) {
        errln("bogus.setTo((UChar32)-1) failed");
    }

    static const char16_t nul=0;

    test3.setToBogus();
    if(!test3.isBogus() || test3.setTo(&nul, 0).isBogus() || !test3.isEmpty()) {
        errln("bogus.setTo(&nul, 0) failed");
    }

    test3.setToBogus();
    if (!test3.isBogus() || test3.getBuffer() != nullptr) {
        errln("setToBogus() failed to make a string bogus");
    }

    test3.setToBogus();
    if(test1.isBogus() || !(test1=test3).isBogus()) {
        errln("normal=bogus failed to make the left string bogus");
    }

    // test that nullptr primitive input string values are treated like
    // empty strings, not errors (bogus)
    test2.setTo((UChar32)0x10005);
    if(test2.insert(1, nullptr, 1).length()!=2) {
        errln("UniStr.insert(...nullptr...) should not modify the string but does");
    }

    UErrorCode errorCode=U_ZERO_ERROR;
    UnicodeString
        test4((const char16_t *)nullptr),
        test5(true, (const char16_t *)nullptr, 1),
        test6((char16_t *)nullptr, 5, 5),
        test7((const char *)nullptr, 3, nullptr, errorCode);
    if(test4.isBogus() || test5.isBogus() || test6.isBogus() || test7.isBogus()) {
        errln("a constructor set to bogus for a nullptr input string, should be empty");
    }

    test4.setTo(nullptr, 3);
    test5.setTo(true, (const char16_t *)nullptr, 1);
    test6.setTo((char16_t *)nullptr, 5, 5);
    if(test4.isBogus() || test5.isBogus() || test6.isBogus()) {
        errln("a setTo() set to bogus for a nullptr input string, should be empty");
    }

    // test that bogus==bogus<any
    if(test1!=test3 || test1.compare(test3)!=0) {
        errln("bogus==bogus failed");
    }

    test2.remove();
    if(test1>=test2 || !(test2>test1) || test1.compare(test2)>=0 || !(test2.compare(test1)>0)) {
        errln("bogus<empty failed");
    }

    // test that copy constructor of bogus is bogus & clone of bogus is nullptr
    {
        test3.setToBogus();
        UnicodeString test3Copy(test3);
        UnicodeString *test3Clone = test3.clone();
        assertTrue(WHERE, test3.isBogus());
        assertTrue(WHERE, test3Copy.isBogus());
        assertTrue(WHERE, test3Clone == nullptr);
    }
}

// StringEnumeration ------------------------------------------------------- ***
// most of StringEnumeration is tested elsewhere
// this test improves code coverage

static const char *const
testEnumStrings[]={
    "a",
    "b",
    "c",
    "this is a long string which helps us test some buffer limits",
    "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
};

class TestEnumeration : public StringEnumeration {
public:
    TestEnumeration() : i(0) {}

    virtual int32_t count(UErrorCode& /*status*/) const override {
        return UPRV_LENGTHOF(testEnumStrings);
    }

    virtual const UnicodeString *snext(UErrorCode &status) override {
        if(U_SUCCESS(status) && i<UPRV_LENGTHOF(testEnumStrings)) {
            unistr=UnicodeString(testEnumStrings[i++], "");
            return &unistr;
        }

        return nullptr;
    }

    virtual void reset(UErrorCode& /*status*/) override {
        i=0;
    }

    static inline UClassID getStaticClassID() {
        return (UClassID)&fgClassID;
    }
    virtual UClassID getDynamicClassID() const override {
        return getStaticClassID();
    }

private:
    static const char fgClassID;

    int32_t i;
};

const char TestEnumeration::fgClassID=0;

void
UnicodeStringTest::TestStringEnumeration() {
    UnicodeString s;
    TestEnumeration ten;
    int32_t i, length;
    UErrorCode status;

    const char16_t *pu;
    const char *pc;

    // test the next() default implementation and ensureCharsCapacity()
    for(i=0; i<UPRV_LENGTHOF(testEnumStrings); ++i) {
        status=U_ZERO_ERROR;
        pc=ten.next(&length, status);
        s=UnicodeString(testEnumStrings[i], "");
        if(U_FAILURE(status) || pc==nullptr || length!=s.length() || UnicodeString(pc, length, "")!=s) {
            errln("StringEnumeration.next(%d) failed", i);
        }
    }
    status=U_ZERO_ERROR;
    if(ten.next(&length, status)!=nullptr) {
        errln("StringEnumeration.next(done)!=nullptr");
    }

    // test the unext() default implementation
    ten.reset(status);
    for(i=0; i<UPRV_LENGTHOF(testEnumStrings); ++i) {
        status=U_ZERO_ERROR;
        pu=ten.unext(&length, status);
        s=UnicodeString(testEnumStrings[i], "");
        if(U_FAILURE(status) || pu==nullptr || length!=s.length() || UnicodeString(true, pu, length)!=s) {
            errln("StringEnumeration.unext(%d) failed", i);
        }
    }
    status=U_ZERO_ERROR;
    if(ten.unext(&length, status)!=nullptr) {
        errln("StringEnumeration.unext(done)!=nullptr");
    }

    // test that the default clone() implementation works, and returns nullptr
    if(ten.clone()!=nullptr) {
        errln("StringEnumeration.clone()!=nullptr");
    }

    // test that uenum_openFromStringEnumeration() works
    // Need a heap allocated string enumeration because it is adopted by the UEnumeration.
    StringEnumeration *newTen = new TestEnumeration;
    status=U_ZERO_ERROR;
    UEnumeration *uten = uenum_openFromStringEnumeration(newTen, &status);
    if (uten==nullptr || U_FAILURE(status)) {
        errln("fail at file %s, line %d, UErrorCode is %s\n", __FILE__, __LINE__, u_errorName(status));
        return;
    }
    
    // test  uenum_next()
    for(i=0; i<UPRV_LENGTHOF(testEnumStrings); ++i) {
        status=U_ZERO_ERROR;
        pc=uenum_next(uten, &length, &status);
        if(U_FAILURE(status) || pc==nullptr || strcmp(pc, testEnumStrings[i]) != 0) {
            errln("File %s, line %d, StringEnumeration.next(%d) failed", __FILE__, __LINE__, i);
        }
    }
    status=U_ZERO_ERROR;
    if(uenum_next(uten, &length, &status)!=nullptr) {
        errln("File %s, line %d, uenum_next(done)!=nullptr");
    }

    // test the uenum_unext()
    uenum_reset(uten, &status);
    for(i=0; i<UPRV_LENGTHOF(testEnumStrings); ++i) {
        status=U_ZERO_ERROR;
        pu=uenum_unext(uten, &length, &status);
        s=UnicodeString(testEnumStrings[i], "");
        if(U_FAILURE(status) || pu==nullptr || length!=s.length() || UnicodeString(true, pu, length)!=s) {
            errln("File %s, Line %d, uenum_unext(%d) failed", __FILE__, __LINE__, i);
        }
    }
    status=U_ZERO_ERROR;
    if(uenum_unext(uten, &length, &status)!=nullptr) {
        errln("File %s, Line %d, uenum_unext(done)!=nullptr" __FILE__, __LINE__);
    }

    uenum_close(uten);
}

/*
 * Namespace test, to make sure that macros like UNICODE_STRING include the
 * namespace qualifier.
 *
 * Define a (bogus) UnicodeString class in another namespace and check for ambiguity.
 */
namespace bogus {
    class UnicodeString {
    public:
        enum EInvariant { kInvariant };
        UnicodeString() : i(1) {}
        UnicodeString(UBool /*isTerminated*/, const char16_t * /*text*/, int32_t textLength) : i(textLength) {(void)i;}
        UnicodeString(const char * /*src*/, int32_t length, enum EInvariant /*inv*/
) : i(length) {}
    private:
        int32_t i;
    };
}

void
UnicodeStringTest::TestNameSpace() {
    // Provoke name collision unless the UnicodeString macros properly
    // qualify the icu::UnicodeString class.
    using namespace bogus;

    // Use all UnicodeString macros from unistr.h.
    icu::UnicodeString s1=icu::UnicodeString("abc", 3, US_INV);
    icu::UnicodeString s2=UNICODE_STRING("def", 3);
    icu::UnicodeString s3=UNICODE_STRING_SIMPLE("ghi");

    // Make sure the compiler does not optimize away instantiation of s1, s2, s3.
    icu::UnicodeString s4=s1+s2+s3;
    if(s4.length()!=9) {
        errln("Something wrong with UnicodeString::operator+().");
    }
}

void
UnicodeStringTest::TestUTF32() {
    // Input string length US_STACKBUF_SIZE to cause overflow of the
    // initially chosen fStackBuffer due to supplementary characters.
    static const UChar32 utf32[] = {
        0x41, 0xd900, 0x61, 0xdc00, -1, 0x110000, 0x5a, 0x50000, 0x7a,
        0x10000, 0x20000, 0xe0000, 0x10ffff
    };
    static const char16_t expected_utf16[] = {
        0x41, 0xfffd, 0x61, 0xfffd, 0xfffd, 0xfffd, 0x5a, 0xd900, 0xdc00, 0x7a,
        0xd800, 0xdc00, 0xd840, 0xdc00, 0xdb40, 0xdc00, 0xdbff, 0xdfff
    };
    UnicodeString from32 = UnicodeString::fromUTF32(utf32, UPRV_LENGTHOF(utf32));
    UnicodeString expected(false, expected_utf16, UPRV_LENGTHOF(expected_utf16));
    if(from32 != expected) {
        errln("UnicodeString::fromUTF32() did not create the expected string.");
    }

    static const char16_t utf16[] = {
        0x41, 0xd900, 0x61, 0xdc00, 0x5a, 0xd900, 0xdc00, 0x7a, 0xd800, 0xdc00, 0xdbff, 0xdfff
    };
    static const UChar32 expected_utf32[] = {
        0x41, 0xfffd, 0x61, 0xfffd, 0x5a, 0x50000, 0x7a, 0x10000, 0x10ffff
    };
    UChar32 result32[16];
    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t length32 =
        UnicodeString(false, utf16, UPRV_LENGTHOF(utf16)).
        toUTF32(result32, UPRV_LENGTHOF(result32), errorCode);
    if( length32 != UPRV_LENGTHOF(expected_utf32) ||
        0 != uprv_memcmp(result32, expected_utf32, length32*4) ||
        result32[length32] != 0
    ) {
        errln("UnicodeString::toUTF32() did not create the expected string.");
    }
}

class TestCheckedArrayByteSink : public CheckedArrayByteSink {
public:
    TestCheckedArrayByteSink(char* outbuf, int32_t capacity)
            : CheckedArrayByteSink(outbuf, capacity), calledFlush(false) {}
    virtual void Flush() override { calledFlush = true; }
    UBool calledFlush;
};

void
UnicodeStringTest::TestUTF8() {
    static const uint8_t utf8[] = {
        // Code points:
        // 0x41, 0xd900,
        // 0x61, 0xdc00,
        // 0x110000, 0x5a,
        // 0x50000, 0x7a,
        // 0x10000, 0x20000,
        // 0xe0000, 0x10ffff
        0x41, 0xed, 0xa4, 0x80,
        0x61, 0xed, 0xb0, 0x80,
        0xf4, 0x90, 0x80, 0x80, 0x5a,
        0xf1, 0x90, 0x80, 0x80, 0x7a,
        0xf0, 0x90, 0x80, 0x80, 0xf0, 0xa0, 0x80, 0x80,
        0xf3, 0xa0, 0x80, 0x80, 0xf4, 0x8f, 0xbf, 0xbf
    };
    static const char16_t expected_utf16[] = {
        0x41, 0xfffd, 0xfffd, 0xfffd,
        0x61, 0xfffd, 0xfffd, 0xfffd,
        0xfffd,  0xfffd, 0xfffd, 0xfffd,0x5a,
        0xd900, 0xdc00, 0x7a,
        0xd800, 0xdc00, 0xd840, 0xdc00,
        0xdb40, 0xdc00, 0xdbff, 0xdfff
    };
    UnicodeString from8 = UnicodeString::fromUTF8(StringPiece((const char *)utf8, (int32_t)sizeof(utf8)));
    UnicodeString expected(false, expected_utf16, UPRV_LENGTHOF(expected_utf16));

    if(from8 != expected) {
        errln("UnicodeString::fromUTF8(StringPiece) did not create the expected string.");
    }
    std::string utf8_string((const char *)utf8, sizeof(utf8));
    UnicodeString from8b = UnicodeString::fromUTF8(utf8_string);
    if(from8b != expected) {
        errln("UnicodeString::fromUTF8(std::string) did not create the expected string.");
    }

    static const char16_t utf16[] = {
        0x41, 0xd900, 0x61, 0xdc00, 0x5a, 0xd900, 0xdc00, 0x7a, 0xd800, 0xdc00, 0xdbff, 0xdfff
    };
    static const uint8_t expected_utf8[] = {
        0x41, 0xef, 0xbf, 0xbd, 0x61, 0xef, 0xbf, 0xbd, 0x5a, 0xf1, 0x90, 0x80, 0x80, 0x7a,
        0xf0, 0x90, 0x80, 0x80, 0xf4, 0x8f, 0xbf, 0xbf
    };
    UnicodeString us(false, utf16, UPRV_LENGTHOF(utf16));

    char buffer[64];
    TestCheckedArrayByteSink sink(buffer, (int32_t)sizeof(buffer));
    us.toUTF8(sink);
    if( sink.NumberOfBytesWritten() != (int32_t)sizeof(expected_utf8) ||
        0 != uprv_memcmp(buffer, expected_utf8, sizeof(expected_utf8))
    ) {
        errln("UnicodeString::toUTF8() did not create the expected string.");
    }
    if(!sink.calledFlush) {
        errln("UnicodeString::toUTF8(sink) did not sink.Flush().");
    }
    // Initial contents for testing that toUTF8String() appends.
    std::string result8 = "-->";
    std::string expected8 = "-->" + std::string((const char *)expected_utf8, sizeof(expected_utf8));
    // Use the return value just for testing.
    std::string &result8r = us.toUTF8String(result8);
    if(result8r != expected8 || &result8r != &result8) {
        errln("UnicodeString::toUTF8String() did not create the expected string.");
    }
}

// Test if this compiler supports Return Value Optimization of unnamed temporary objects.
static UnicodeString wrapUChars(const char16_t *uchars) {
    return UnicodeString(true, uchars, -1);
}

void
UnicodeStringTest::TestReadOnlyAlias() {
    char16_t uchars[]={ 0x61, 0x62, 0 };
    UnicodeString alias(true, uchars, 2);
    if(alias.length()!=2 || alias.getBuffer()!=uchars || alias.getTerminatedBuffer()!=uchars) {
        errln("UnicodeString read-only-aliasing constructor does not behave as expected.");
        return;
    }
    alias.truncate(1);
    if(alias.length()!=1 || alias.getBuffer()!=uchars) {
        errln("UnicodeString(read-only-alias).truncate() did not preserve aliasing as expected.");
    }
    if(alias.getTerminatedBuffer()==uchars) {
        errln("UnicodeString(read-only-alias).truncate().getTerminatedBuffer() "
              "did not allocate and copy as expected.");
    }
    if(uchars[1]!=0x62) {
        errln("UnicodeString(read-only-alias).truncate().getTerminatedBuffer() "
              "modified the original buffer.");
    }
    if(1!=u_strlen(alias.getTerminatedBuffer())) {
        errln("UnicodeString(read-only-alias).truncate().getTerminatedBuffer() "
              "does not return a buffer terminated at the proper length.");
    }

    alias.setTo(true, uchars, 2);
    if(alias.length()!=2 || alias.getBuffer()!=uchars || alias.getTerminatedBuffer()!=uchars) {
        errln("UnicodeString read-only-aliasing setTo() does not behave as expected.");
        return;
    }
    alias.remove();
    if(alias.length()!=0) {
        errln("UnicodeString(read-only-alias).remove() did not work.");
    }
    if(alias.getTerminatedBuffer()==uchars) {
        errln("UnicodeString(read-only-alias).remove().getTerminatedBuffer() "
              "did not un-alias as expected.");
    }
    if(uchars[0]!=0x61) {
        errln("UnicodeString(read-only-alias).remove().getTerminatedBuffer() "
              "modified the original buffer.");
    }
    if(0!=u_strlen(alias.getTerminatedBuffer())) {
        errln("UnicodeString.setTo(read-only-alias).remove().getTerminatedBuffer() "
              "does not return a buffer terminated at length 0.");
    }

    UnicodeString longString=UNICODE_STRING_SIMPLE("abcdefghijklmnopqrstuvwxyz0123456789");
    alias.setTo(false, longString.getBuffer(), longString.length());
    alias.remove(0, 10);
    if(longString.compare(10, INT32_MAX, alias)!=0 || alias.getBuffer()!=longString.getBuffer()+10) {
        errln("UnicodeString.setTo(read-only-alias).remove(0, 10) did not preserve aliasing as expected.");
    }
    alias.setTo(false, longString.getBuffer(), longString.length());
    alias.remove(27, 99);
    if(longString.compare(0, 27, alias)!=0 || alias.getBuffer()!=longString.getBuffer()) {
        errln("UnicodeString.setTo(read-only-alias).remove(27, 99) did not preserve aliasing as expected.");
    }
    alias.setTo(false, longString.getBuffer(), longString.length());
    alias.retainBetween(6, 30);
    if(longString.compare(6, 24, alias)!=0 || alias.getBuffer()!=longString.getBuffer()+6) {
        errln("UnicodeString.setTo(read-only-alias).retainBetween(6, 30) did not preserve aliasing as expected.");
    }

    char16_t abc[]={ 0x61, 0x62, 0x63, 0 };
    UBool hasRVO= wrapUChars(abc).getBuffer()==abc;

    UnicodeString temp;
    temp.fastCopyFrom(longString.tempSubString());
    if(temp!=longString || (hasRVO && temp.getBuffer()!=longString.getBuffer())) {
        errln("UnicodeString.tempSubString() failed");
    }
    temp.fastCopyFrom(longString.tempSubString(-3, 5));
    if(longString.compare(0, 5, temp)!=0 || (hasRVO && temp.getBuffer()!=longString.getBuffer())) {
        errln("UnicodeString.tempSubString(-3, 5) failed");
    }
    temp.fastCopyFrom(longString.tempSubString(17));
    if(longString.compare(17, INT32_MAX, temp)!=0 || (hasRVO && temp.getBuffer()!=longString.getBuffer()+17)) {
        errln("UnicodeString.tempSubString(17) failed");
    }
    temp.fastCopyFrom(longString.tempSubString(99));
    if(!temp.isEmpty()) {
        errln("UnicodeString.tempSubString(99) failed");
    }
    temp.fastCopyFrom(longString.tempSubStringBetween(6));
    if(longString.compare(6, INT32_MAX, temp)!=0 || (hasRVO && temp.getBuffer()!=longString.getBuffer()+6)) {
        errln("UnicodeString.tempSubStringBetween(6) failed");
    }
    temp.fastCopyFrom(longString.tempSubStringBetween(8, 18));
    if(longString.compare(8, 10, temp)!=0 || (hasRVO && temp.getBuffer()!=longString.getBuffer()+8)) {
        errln("UnicodeString.tempSubStringBetween(8, 18) failed");
    }
    UnicodeString bogusString;
    bogusString.setToBogus();
    temp.fastCopyFrom(bogusString.tempSubStringBetween(8, 18));
    if(!temp.isBogus()) {
        errln("UnicodeString.setToBogus().tempSubStringBetween(8, 18) failed");
    }
}

void
UnicodeStringTest::doTestAppendable(UnicodeString &dest, Appendable &app) {
    static const char16_t cde[3]={ 0x63, 0x64, 0x65 };
    static const char16_t fg[3]={ 0x66, 0x67, 0 };
    if(!app.reserveAppendCapacity(12)) {
        errln("Appendable.reserve(12) failed");
    }
    app.appendCodeUnit(0x61);
    app.appendCodePoint(0x62);
    app.appendCodePoint(0x50000);
    app.appendString(cde, 3);
    app.appendString(fg, -1);
    char16_t scratch[3];
    int32_t capacity=-1;
    char16_t *buffer=app.getAppendBuffer(3, 3, scratch, 3, &capacity);
    if(capacity<3) {
        errln("Appendable.getAppendBuffer(min=3) returned capacity=%d<3", (int)capacity);
        return;
    }
    static const char16_t hij[3]={ 0x68, 0x69, 0x6a };
    u_memcpy(buffer, hij, 3);
    app.appendString(buffer, 3);
    if(dest!=UNICODE_STRING_SIMPLE("ab\\U00050000cdefghij").unescape()) {
        errln("Appendable.append(...) failed");
    }
    buffer=app.getAppendBuffer(0, 3, scratch, 3, &capacity);
    if(buffer!=nullptr || capacity!=0) {
        errln("Appendable.getAppendBuffer(min=0) failed");
    }
    capacity=1;
    buffer=app.getAppendBuffer(3, 3, scratch, 2, &capacity);
    if(buffer!=nullptr || capacity!=0) {
        errln("Appendable.getAppendBuffer(scratch<min) failed");
    }
}

class SimpleAppendable : public Appendable {
public:
    explicit SimpleAppendable(UnicodeString &dest) : str(dest) {}
    virtual UBool appendCodeUnit(char16_t c) override { str.append(c); return true; }
    SimpleAppendable &reset() { str.remove(); return *this; }
private:
    UnicodeString &str;
};

void
UnicodeStringTest::TestAppendable() {
    UnicodeString dest;
    SimpleAppendable app(dest);
    doTestAppendable(dest, app);
}

void
UnicodeStringTest::TestUnicodeStringImplementsAppendable() {
    UnicodeString dest;
    UnicodeStringAppendable app(dest);
    doTestAppendable(dest, app);
}

void
UnicodeStringTest::TestSizeofUnicodeString() {
    // See the comments in unistr.h near the declaration of UnicodeString's fields.
    // See the API comments for UNISTR_OBJECT_SIZE.
    size_t sizeofUniStr=sizeof(UnicodeString);
    size_t expected=UNISTR_OBJECT_SIZE;
    if(expected!=sizeofUniStr) {
        // Possible cause: UNISTR_OBJECT_SIZE may not be a multiple of sizeof(pointer),
        // of the compiler might add more internal padding than expected.
        errln("sizeof(UnicodeString)=%d, expected UNISTR_OBJECT_SIZE=%d",
              (int)sizeofUniStr, (int)expected);
    }
    if(sizeofUniStr<32) {
        errln("sizeof(UnicodeString)=%d < 32, probably too small", (int)sizeofUniStr);
    }
    // We assume that the entire UnicodeString object,
    // minus the vtable pointer and 2 bytes for flags and short length,
    // is available for internal storage of UChars.
    int32_t expectedStackBufferLength=((int32_t)UNISTR_OBJECT_SIZE-sizeof(void *)-2)/U_SIZEOF_UCHAR;
    UnicodeString s;
    const char16_t *emptyBuffer=s.getBuffer();
    for(int32_t i=0; i<expectedStackBufferLength; ++i) {
        s.append((char16_t)0x2e);
    }
    const char16_t *fullBuffer=s.getBuffer();
    if(fullBuffer!=emptyBuffer) {
        errln("unexpected reallocation when filling with assumed stack buffer size of %d",
              expectedStackBufferLength);
    }
    const char16_t *terminatedBuffer=s.getTerminatedBuffer();
    if(terminatedBuffer==emptyBuffer) {
        errln("unexpected keeping stack buffer when overfilling assumed stack buffer size of %d",
              expectedStackBufferLength);
    }
}

// Try to avoid clang -Wself-move warnings from s1 = std::move(s1);
void moveFrom(UnicodeString &dest, UnicodeString &src) {
    dest = std::move(src);
}

void
UnicodeStringTest::TestMoveSwap() {
    static const char16_t abc[3] = { 0x61, 0x62, 0x63 };  // "abc"
    UnicodeString s1(false, abc, UPRV_LENGTHOF(abc));  // read-only alias
    UnicodeString s2(100, 0x7a, 100);  // 100 * 'z' should be on the heap
    UnicodeString s3("defg", 4, US_INV);  // in stack buffer
    const char16_t *p = s2.getBuffer();
    s1.swap(s2);
    if(s1.getBuffer() != p || s1.length() != 100 || s2.getBuffer() != abc || s2.length() != 3) {
        errln("UnicodeString.swap() did not swap");
    }
    swap(s2, s3);
    if(s2 != UNICODE_STRING_SIMPLE("defg") || s3.getBuffer() != abc || s3.length() != 3) {
        errln("swap(UnicodeString) did not swap back");
    }
    UnicodeString s4;
    s4 = std::move(s1);
    if(s4.getBuffer() != p || s4.length() != 100 || !s1.isBogus()) {
        errln("UnicodeString = std::move(heap) did not move");
    }
    UnicodeString s5;
    s5 = std::move(s2);
    if(s5 != UNICODE_STRING_SIMPLE("defg")) {
        errln("UnicodeString = std::move(stack) did not move");
    }
    UnicodeString s6;
    s6 = std::move(s3);
    if(s6.getBuffer() != abc || s6.length() != 3) {
        errln("UnicodeString = std::move(alias) did not move");
    }
    infoln("TestMoveSwap() with rvalue references");
    s1 = static_cast<UnicodeString &&>(s6);
    if(s1.getBuffer() != abc || s1.length() != 3) {
        errln("UnicodeString move assignment operator did not move");
    }
    UnicodeString s7(static_cast<UnicodeString &&>(s4));
    if(s7.getBuffer() != p || s7.length() != 100 || !s4.isBogus()) {
        errln("UnicodeString move constructor did not move");
    }

    // Move self assignment leaves the object valid but in an undefined state.
    // Do it to make sure there is no crash,
    // but do not check for any particular resulting value.
    moveFrom(s1, s1);
    moveFrom(s2, s2);
    moveFrom(s3, s3);
    moveFrom(s4, s4);
    moveFrom(s5, s5);
    moveFrom(s6, s6);
    moveFrom(s7, s7);
    // Simple copy assignment must work.
    UnicodeString simple = UNICODE_STRING_SIMPLE("simple");
    s1 = s6 = s4 = s7 = simple;
    if(s1 != simple || s4 != simple || s6 != simple || s7 != simple) {
        errln("UnicodeString copy after self-move did not work");
    }
}

void
UnicodeStringTest::TestUInt16Pointers() {
    static const uint16_t carr[] = { 0x61, 0x62, 0x63, 0 };
    uint16_t arr[4];

    UnicodeString expected(u"abc");
    assertEquals("abc from pointer", expected, UnicodeString(carr));
    assertEquals("abc from pointer+length", expected, UnicodeString(carr, 3));
    assertEquals("abc from read-only-alias pointer", expected, UnicodeString(true, carr, 3));

    UnicodeString alias(arr, 0, 4);
    alias.append(u'a').append(u'b').append(u'c');
    assertEquals("abc from writable alias", expected, alias);
    assertEquals("buffer=abc from writable alias", expected, UnicodeString(arr, 3));

    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t length = UnicodeString(u"def").extract(arr, 4, errorCode);
    assertSuccess(WHERE, errorCode);
    assertEquals("def from extract()", UnicodeString(u"def"), UnicodeString(arr, length));
}

void
UnicodeStringTest::TestWCharPointers() {
#if U_SIZEOF_WCHAR_T==2
    static const wchar_t carr[] = { 0x61, 0x62, 0x63, 0 };
    wchar_t arr[4];

    UnicodeString expected(u"abc");
    assertEquals("abc from pointer", expected, UnicodeString(carr));
    assertEquals("abc from pointer+length", expected, UnicodeString(carr, 3));
    assertEquals("abc from read-only-alias pointer", expected, UnicodeString(true, carr, 3));

    UnicodeString alias(arr, 0, 4);
    alias.append(u'a').append(u'b').append(u'c');
    assertEquals("abc from writable alias", expected, alias);
    assertEquals("buffer=abc from writable alias", expected, UnicodeString(arr, 3));

    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t length = UnicodeString(u"def").extract(arr, 4, errorCode);
    assertSuccess(WHERE, errorCode);
    assertEquals("def from extract()", UnicodeString(u"def"), UnicodeString(arr, length));
#endif
}

void
UnicodeStringTest::TestNullPointers() {
    assertTrue("empty from nullptr", UnicodeString(nullptr).isEmpty());
    assertTrue("empty from nullptr+length", UnicodeString(nullptr, 2).isEmpty());
    assertTrue("empty from read-only-alias nullptr", UnicodeString(true, nullptr, 3).isEmpty());

    UnicodeString alias(nullptr, 4, 4);  // empty, no alias
    assertTrue("empty from writable alias", alias.isEmpty());
    alias.append(u'a').append(u'b').append(u'c');
    UnicodeString expected(u"abc");
    assertEquals("abc from writable alias", expected, alias);

    UErrorCode errorCode = U_ZERO_ERROR;
    UnicodeString(u"def").extract(nullptr, 0, errorCode);
    assertEquals("buffer overflow extracting to nullptr", U_BUFFER_OVERFLOW_ERROR, errorCode);
}

void UnicodeStringTest::TestUnicodeStringInsertAppendToSelf() {
    IcuTestErrorCode status(*this, "TestUnicodeStringAppendToSelf");

    // Test append operation
    UnicodeString str(u"foo ");
    str.append(str);
    str.append(str);
    str.append(str);
    assertEquals("", u"foo foo foo foo foo foo foo foo ", str);

    // Test append operation with readonly alias to start
    str = UnicodeString(true, u"foo ", 4);
    str.append(str);
    str.append(str);
    str.append(str);
    assertEquals("", u"foo foo foo foo foo foo foo foo ", str);

    // Test append operation with aliased substring
    str = u"abcde";
    UnicodeString sub = str.tempSubString(1, 2);
    str.append(sub);
    assertEquals("", u"abcdebc", str);

    // Test append operation with double-aliased substring
    str = UnicodeString(true, u"abcde", 5);
    sub = str.tempSubString(1, 2);
    str.append(sub);
    assertEquals("", u"abcdebc", str);

    // Test insert operation
    str = u"a-*b";
    str.insert(2, str);
    str.insert(4, str);
    str.insert(8, str);
    assertEquals("", u"a-a-a-a-a-a-a-a-*b*b*b*b*b*b*b*b", str);

    // Test insert operation with readonly alias to start
    str = UnicodeString(true, u"a-*b", 4);
    str.insert(2, str);
    str.insert(4, str);
    str.insert(8, str);
    assertEquals("", u"a-a-a-a-a-a-a-a-*b*b*b*b*b*b*b*b", str);

    // Test insert operation with aliased substring
    str = u"abcde";
    sub = str.tempSubString(1, 3);
    str.insert(2, sub);
    assertEquals("", u"abbcdcde", str);

    // Test insert operation with double-aliased substring
    str = UnicodeString(true, u"abcde", 5);
    sub = str.tempSubString(1, 3);
    str.insert(2, sub);
    assertEquals("", u"abbcdcde", str);
}

void UnicodeStringTest::TestLargeAppend() {
    if(quick) return;

    IcuTestErrorCode status(*this, "TestLargeAppend");
    // Make a large UnicodeString
    int32_t len = 0xAFFFFFF;
    UnicodeString str;
    char16_t *buf = str.getBuffer(len);
    // A fast way to set buffer to valid Unicode.
    // 4E4E is a valid unicode character
    uprv_memset(buf, 0x4e, len * 2);
    str.releaseBuffer(len);
    UnicodeString dest;
    // Append it 16 times
    // 0xAFFFFFF times 16 is 0xA4FFFFF1,
    // which is greater than INT32_MAX, which is 0x7FFFFFFF.
    int64_t total = 0;
    for (int32_t i = 0; i < 16; i++) {
        dest.append(str);
        total += len;
        if (total <= INT32_MAX) {
            assertFalse("dest is not bogus", dest.isBogus());
        } else {
            assertTrue("dest should be bogus", dest.isBogus());
        }
    }
    dest.remove();
    total = 0;
    for (int32_t i = 0; i < 16; i++) {
        dest.append(str);
        total += len;
        if (total + len <= INT32_MAX) {
            assertFalse("dest is not bogus", dest.isBogus());
        } else if (total <= INT32_MAX) {
            // Check that a string of exactly the maximum size works
            UnicodeString str2;
            int32_t remain = static_cast<int32_t>(INT32_MAX - total);
            char16_t *buf2 = str2.getBuffer(remain);
            if (buf2 == nullptr) {
                // if somehow memory allocation fail, return the test
                return;
            }
            uprv_memset(buf2, 0x4e, remain * 2);
            str2.releaseBuffer(remain);
            dest.append(str2);
            total += remain;
            assertEquals("When a string of exactly the maximum size works", (int64_t)INT32_MAX, total);
            assertEquals("When a string of exactly the maximum size works", INT32_MAX, dest.length());
            assertFalse("dest is not bogus", dest.isBogus());

            // Check that a string size+1 goes bogus
            str2.truncate(1);
            dest.append(str2);
            total++;
            assertTrue("dest should be bogus", dest.isBogus());
        } else {
            assertTrue("dest should be bogus", dest.isBogus());
        }
    }
}
