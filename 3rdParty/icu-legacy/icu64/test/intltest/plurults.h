// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2013, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#ifndef _PluralRulesTest
#define _PluralRulesTest

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "intltest.h"
#include "unicode/localpointer.h"
#include "unicode/plurrule.h"

/**
 * Test basic functionality of various API functions
 **/
class PluralRulesTest : public IntlTest {
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL );

private:
    /**
     * Performs tests on many API functions, see detailed comments in source code
     **/
    void testAPI();
    void testGetUniqueKeywordValue();
    void testGetSamples();
    void testWithin();
    void testGetAllKeywordValues();
    void testOrdinal();
    void testSelect();
    void testAvailbleLocales();
    void testParseErrors();
    void testFixedDecimal();
    void testSelectTrailingZeros();
    void testLocaleExtension();

    void assertRuleValue(const UnicodeString& rule, double expected);
    void assertRuleKeyValue(const UnicodeString& rule, const UnicodeString& key,
                            double expected);
    void checkSelect(const LocalPointer<PluralRules> &rules, UErrorCode &status, 
                                  int32_t line, const char *keyword, ...);
};

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif
