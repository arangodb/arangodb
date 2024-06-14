// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2015, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/


#ifndef MULTITHREADTEST_H
#define MULTITHREADTEST_H

#include "intltest.h"
#include "mutex.h"



/**
 * Tests actual threading
 **/
class MultithreadTest : public IntlTest
{
public:
    MultithreadTest();
    virtual ~MultithreadTest();
    
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL );

    /**
     * test that threads even work
     **/
    void TestThreads(void);

	/**
     * test that arabic shaping can work in threads
     **/
    void TestArabicShapingThreads(void);
	
    /**
     * test that mutexes work 
     **/
    void TestMutex(void);
#if !UCONFIG_NO_FORMATTING
    /**
     * test that intl functions work in a multithreaded context
     **/
    void TestThreadedIntl(void);
#endif
    void TestCollators(void);
    void TestString();
    void TestAnyTranslit();
    void TestConditionVariables();
    void TestUnifiedCache();
    void TestBreakTranslit();
    void TestIncDec();
    void Test20104();
};

#endif

