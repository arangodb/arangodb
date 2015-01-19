/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2012-2013, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
//
//   file:  alphaindex.h
//          Alphabetic Index Tests.
//

#ifndef ALPHAINDEXTST_H
#define ALPHAINDEXTST_H

#include "intltest.h"

class AlphabeticIndexTest: public IntlTest {
public:
    AlphabeticIndexTest();
    virtual ~AlphabeticIndexTest();

    virtual void runIndexedTest(int32_t index, UBool exec, const char* &name, char* par = NULL );

    virtual void APITest();
    virtual void ManyLocalesTest();
    virtual void HackPinyinTest();
    virtual void TestBug9009();
    void TestIndexCharactersList();
    /**
     * Test AlphabeticIndex vs. root with script reordering.
     */
    void TestHaniFirst();
    /**
     * Test AlphabeticIndex vs. Pinyin with script reordering.
     */
    void TestPinyinFirst();
    /**
     * Test labels with multiple primary weights.
     */
    void TestSchSt();
    /**
     * With no real labels, there should be only the underflow label.
     */
    void TestNoLabels();
    /**
     * Test with the Bopomofo-phonetic tailoring.
     */
    void TestChineseZhuyin();
};

#endif
