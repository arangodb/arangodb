// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2008-2016 International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#ifndef _INTLTESTDATEINTERVALFORMAT
#define _INTLTESTDATEINTERVALFORMAT

#include "unicode/utypes.h"
#include "unicode/locid.h"

#if !UCONFIG_NO_FORMATTING

#include "intltest.h"
#include "itformat.h"

/**
 * Test basic functionality of various API functions
 **/
class DateIntervalFormatTest: public IntlTestWithFieldPosition {
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL );

public:
    /**
     * Performs tests on many API functions, see detailed comments in source code
     **/
    void testAPI();

    /**
     * Test formatting
     */
    void testFormat();

    /**
     * Test formatting using user defined DateIntervalInfo
     */
    void testFormatUserDII();

    /**
     * Test for no unwanted side effects when setting
     * interval patterns.
     */
    void testSetIntervalPatternNoSideEffect();

    /**
     * Tests different year formats.
     */
    void testYearFormats();

    /**
     * Stress test -- stress test formatting on 40 locales
     */
    void testStress();

    void testTicket11583_2();

    void testTicket11985();

    void testTicket11669();
    void threadFunc11669(int32_t threadNum);

    void testTicket12065();

    void testFormattedDateInterval();

private:
    /**
     * Test formatting against expected result
     */
    void expect(const char** data, int32_t data_length);

    /**
     * Test formatting against expected result using user defined
     * DateIntervalInfo
     */
    void expectUserDII(const char** data, int32_t data_length);

    /**
     * Stress test formatting
     */
    void stress(const char** data, int32_t data_length, const Locale& loc,
                const char* locName);
};

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif
