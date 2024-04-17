// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2015, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#ifndef UNICODESTRINGTEST_H
#define UNICODESTRINGTEST_H

#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "intltest.h"

U_NAMESPACE_BEGIN

class Appendable;

U_NAMESPACE_END

/**
 * Perform API and functionality tests for class UnicodeString
 **/
class UnicodeStringTest: public IntlTest {
public:
    UnicodeStringTest() {}
    virtual ~UnicodeStringTest();
    
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL );

    /**
     * Test some basic methods (insert, remove, replace, ...)
     **/
    void TestBasicManipulation(void);
    /**
     * Test the methods for comparison
     **/
    void TestCompare(void);
    /**
     * Test the methods for extracting
     **/
    void TestExtract(void);
    /**
     * More extensively test methods for removing and replacing
     **/
    void TestRemoveReplace(void);
    /**
     * Test language specific case conversions
     **/
    void TestSearching(void);
    /**
     * Test methods for padding, trimmimg and truncating
     **/
    void TestSpacePadding(void);
    /**
     * Test methods startsWith and endsWith
     **/
    void TestPrefixAndSuffix(void);
    void TestStartsWithAndEndsWithNulTerminated();
    /**
     * Test method findAndReplace
     **/
    void TestFindAndReplace(void);
    /**
     * Test method reverse
     **/
    void TestReverse(void);
    /**
     * Test a few miscellaneous methods (isBogus, hashCode,...)
     **/
    void TestMiscellaneous(void);
    /**
     * Test the functionality of allocating UnicodeStrings on the stack
     **/
    void TestStackAllocation(void);
    /**
     * Test the unescape() function.
     */
    void TestUnescape(void);

    void _testUnicodeStringHasMoreChar32Than(const UnicodeString &s, int32_t start, int32_t length, int32_t number);
    void TestCountChar32();
    void TestBogus();
    void TestStringEnumeration();
    void TestNameSpace();
    void TestUTF32();
    void TestUTF8();
    void TestReadOnlyAlias();
    void doTestAppendable(UnicodeString &dest, Appendable &app);
    void TestAppendable();
    void TestUnicodeStringImplementsAppendable();
    void TestSizeofUnicodeString();
    void TestMoveSwap();

    void TestUInt16Pointers();
    void TestWCharPointers();
    void TestNullPointers();
    void TestUnicodeStringInsertAppendToSelf();
};

#endif
