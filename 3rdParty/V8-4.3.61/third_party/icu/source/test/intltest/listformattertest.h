/*
*******************************************************************************
*
*   Copyright (C) 2012-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  listformattertest.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2012aug27
*   created by: Umesh P. Nair
*/

#ifndef __LISTFORMATTERTEST_H__
#define __LISTFORMATTERTEST_H__

#include "unicode/listformatter.h"
#include "intltest.h"

class ListFormatterTest : public IntlTest {
  public:
    ListFormatterTest();
    virtual ~ListFormatterTest() {}

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);

    void TestRoot();
    void TestBogus();
    void TestEnglish();
    void TestEnglishUS();
    void TestRussian();
    void TestMalayalam();
    void TestZulu();
    void TestOutOfOrderPatterns();
    void Test9946();

  private:
    void CheckFormatting(const ListFormatter* formatter, UnicodeString data[], int32_t data_size, const UnicodeString& expected_result);
    void CheckFourCases(
        const char* locale_string,
        UnicodeString one,
        UnicodeString two,
        UnicodeString three,
        UnicodeString four,
        UnicodeString results[4]);
    UBool RecordFourCases(
        const Locale& locale,
        UnicodeString one,
        UnicodeString two,
        UnicodeString three,
        UnicodeString four,
        UnicodeString results[4]);

  private:
    // Reused test data.
    const UnicodeString prefix;
    const UnicodeString one;
    const UnicodeString two;
    const UnicodeString three;
    const UnicodeString four;
};

#endif
