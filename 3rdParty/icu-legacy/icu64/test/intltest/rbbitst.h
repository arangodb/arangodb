// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*************************************************************************
 * Copyright (c) 1999-2016, International Business Machines
 * Corporation and others. All Rights Reserved.
 *************************************************************************
 *   Date        Name        Description
 *   12/15/99    Madhu        Creation.
 *   01/12/2000  Madhu        Updated for changed API and added new tests
 ************************************************************************/


#ifndef RBBITEST_H
#define RBBITEST_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include <memory>

#include "intltest.h"
#include "unicode/brkiter.h"
#include "unicode/rbbi.h"

class  Enumeration;
class  BITestData;
struct TestParams;
class  RBBIMonkeyKind;

U_NAMESPACE_BEGIN
class  UVector32;
U_NAMESPACE_END

/**
 * Test the RuleBasedBreakIterator class giving different rules
 */
class RBBITest: public IntlTest {
public:

    RBBITest();
    virtual ~RBBITest();

    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL );

    void TestGetAvailableLocales();
    void TestGetDisplayName();
    void TestEndBehaviour();
    void TestBug4153072();
    void TestJapaneseLineBreak();
    void TestThaiLineBreak();
    void TestMixedThaiLineBreak();
    void TestMaiyamok();
    void TestMonkey();

    void TestExtended();
    UChar *ReadAndConvertFile(const char *fileName, int &ulen, const char *encoding, UErrorCode &status);
    void executeTest(TestParams *, UErrorCode &status);

    void TestWordBreaks();
    void TestWordBoundary();
    void TestLineBreaks();
    void TestSentBreaks();
    void TestBug3818();
    void TestJapaneseWordBreak();
    void TestTrieDict();
    void TestUnicodeFiles();
    void TestBug5775();
    void TestTailoredBreaks();
    void TestDictRules();
    void TestBug5532();
    void TestBug9983();
    void TestBug7547();
    void TestBug12797();
    void TestBug12918();
    void TestBug12932();
    void TestEmoji();
    void TestBug12519();
    void TestBug12677();
    void TestTableRedundancies();
    void TestBug13447();
    void TestReverse();
    void TestReverse(std::unique_ptr<RuleBasedBreakIterator>bi);
    void TestBug13692();

    void TestDebug();
    void TestProperties();

/***********************/
private:
    /**
     * internal methods to prepare test data
     **/

    void RunMonkey(BreakIterator *bi, RBBIMonkeyKind &mk, const char *name, uint32_t  seed,
        int32_t loopCount, UBool useUText);

    // Run one of the Unicode Consortium boundary test data files.
    void runUnicodeTestData(const char *fileName, RuleBasedBreakIterator *bi);

    // Run a single test case from one of the Unicode Consortium test files.
    void checkUnicodeTestCase(const char *testFileName, int lineNumber,
                         const UnicodeString &testString,
                         UVector32 *breakPositions,
                         RuleBasedBreakIterator *bi);

    // Run the actual tests for TestTailoredBreaks()
    void TBTest(BreakIterator* brkitr, int type, const char *locale, const char* escapedText,
                const int32_t *expectOffsets, int32_t expectOffsetsCount);

    /** Filter for test cases from the Unicode test data files.
     *  Some need to be skipped because ICU is unable to fully implement the
     *  Unicode boundary specifications.
     *  @param testCase the test data string.
     *  @param fileName the Unicode test data file name.
     *  @return FALSE if the test case should be run, TRUE if it should be skipped.
     */
    UBool testCaseIsKnownIssue(const UnicodeString &testCase, const char *fileName);

    // Test parameters, from the test framework and test invocation.
    const char* fTestParams;
};

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

#endif
