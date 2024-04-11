// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/************************************************************************
 * COPYRIGHT:
 * Copyright (c) 2013, International Business Machines Corporation
 * and others. All Rights Reserved.
 ************************************************************************/

#ifndef _REGIONTEST_
#define _REGIONTEST_

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/region.h"
#include "intltest.h"

/**
 * Performs various tests on Region APIs
 **/
class RegionTest: public IntlTest {
    // IntlTest override
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par );

public:
    RegionTest();
    virtual ~RegionTest();
    
    void TestKnownRegions(void);
    void TestGetInstanceString(void);
    void TestGetInstanceInt(void);
    void TestGetContainedRegions(void);
    void TestGetContainedRegionsWithType(void);
    void TestGetContainingRegion(void);
    void TestGetContainingRegionWithType(void);
    void TestGetPreferredValues(void);
    void TestContains(void);
    void TestAvailableTerritories(void);
    void TestNoContainedRegions(void);

private:

    UBool optionv; // TRUE if @v option is given on command line
};

#endif /* #if !UCONFIG_NO_FORMATTING */
 
#endif // _REGIONTEST_
//eof
