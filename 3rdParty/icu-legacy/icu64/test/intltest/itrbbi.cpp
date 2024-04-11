// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (C) 1998-2016, International Business Machines Corporation 
* and others.  All Rights Reserved.
**********************************************************************
*/
/***********************************************************************
*   Date        Name        Description
*   12/14/99    Madhu        Creation.
***********************************************************************/
/**
 * IntlTestRBBI is the medium level test class for RuleBasedBreakIterator
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_REGULAR_EXPRESSIONS

#include "intltest.h"
#include "itrbbi.h"
#include "lstmbetst.h"
#include "rbbiapts.h"
#include "rbbitst.h"
#include "rbbimonkeytest.h"


void IntlTestRBBI::runIndexedTest( int32_t index, UBool exec, const char* &name, char* par )
{
    if (exec) {
        logln("TestSuite RuleBasedBreakIterator: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO_CLASS(RBBIAPITest);
    TESTCASE_AUTO_CLASS(RBBITest);
#if !UCONFIG_NO_FORMATTING
    TESTCASE_AUTO_CLASS(RBBIMonkeyTest);
#endif
    TESTCASE_AUTO_CLASS(LSTMBETest);
    TESTCASE_AUTO_END;
}

#endif /* #if !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_REGULAR_EXPRESSIONS */
