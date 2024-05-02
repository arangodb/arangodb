// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2014, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CGENDTST.C
*********************************************************************************
*/

/* C API TEST FOR GENDER INFO */

#include "unicode/utypes.h"
#include "cmemory.h"

#if !UCONFIG_NO_FORMATTING

#include "cintltst.h"
#include "unicode/ugender.h"

static const UGender kAllFemale[] = {UGENDER_FEMALE, UGENDER_FEMALE};

void addGendInfoForTest(TestNode** root);
static void TestGenderInfo(void);

#define TESTCASE(x) addTest(root, &x, "tsformat/cgendtst/" #x)

void addGendInfoForTest(TestNode** root)
{
    TESTCASE(TestGenderInfo);
}

static void TestGenderInfo(void) {
  UErrorCode status = U_ZERO_ERROR;
  const UGenderInfo* actual_gi = ugender_getInstance("fr_CA", &status);
  UGender actual;
  if (U_FAILURE(status)) {
    log_err_status(status, "Fail to create UGenderInfo - %s (Are you missing data?)", u_errorName(status));
    return;
  }
  actual = ugender_getListGender(actual_gi, kAllFemale, UPRV_LENGTHOF(kAllFemale), &status);
  if (U_FAILURE(status)) {
    log_err("Fail to get gender of list - %s\n", u_errorName(status));
    return;
  }
  if (actual != UGENDER_FEMALE) {
    log_err("Expected UGENDER_FEMALE got %d\n", actual);
  }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
