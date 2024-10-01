// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2007-2015, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef _TIMEZONEFORMATTEST_
#define _TIMEZONEFORMATTEST_

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "intltest.h"

class TimeZoneFormatTest : public IntlTest {
  public:
    // IntlTest override
    void runIndexedTest(int32_t index, UBool exec, const char*& name, char* par);

    void TestTimeZoneRoundTrip(void);
    void TestTimeRoundTrip(void);
    void TestParse(void);
    void TestISOFormat(void);
    void TestFormat(void);
    void TestFormatTZDBNames(void);
    void TestFormatCustomZone(void);
    void TestFormatTZDBNamesAllZoneCoverage(void);

    void RunTimeRoundTripTests(int32_t threadNumber);
};

#endif /* #if !UCONFIG_NO_FORMATTING */
 
#endif // _TIMEZONEFORMATTEST_
