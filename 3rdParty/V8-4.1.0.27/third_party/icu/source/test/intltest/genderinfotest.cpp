/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2012, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/* Modification History:
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <stdlib.h>
#include "unicode/gender.h"
#include "unicode/unum.h"
#include "intltest.h"

#define LENGTHOF(array) (int32_t)(sizeof(array) / sizeof((array)[0]))

static const UGender kSingleFemale[] = {UGENDER_FEMALE};
static const UGender kSingleMale[] = {UGENDER_MALE};
static const UGender kSingleOther[] = {UGENDER_OTHER};

static const UGender kAllFemale[] = {UGENDER_FEMALE, UGENDER_FEMALE};
static const UGender kAllMale[] = {UGENDER_MALE, UGENDER_MALE};
static const UGender kAllOther[] = {UGENDER_OTHER, UGENDER_OTHER};

static const UGender kFemaleMale[] = {UGENDER_FEMALE, UGENDER_MALE};
static const UGender kFemaleOther[] = {UGENDER_FEMALE, UGENDER_OTHER};
static const UGender kMaleOther[] = {UGENDER_MALE, UGENDER_OTHER};


class GenderInfoTest : public IntlTest {
public:
    GenderInfoTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestGetListGender();
    void TestFallback();
    void check(UGender expected_neutral, UGender expected_mixed, UGender expected_taints, const UGender* genderList, int32_t listLength);
    void checkLocale(const Locale& locale, UGender expected, const UGender* genderList, int32_t listLength);
};

void GenderInfoTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /* par */) {
  if (exec) {
    logln("TestSuite GenderInfoTest: ");
  }
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestGetListGender);
  TESTCASE_AUTO(TestFallback);
  TESTCASE_AUTO_END;
}

void GenderInfoTest::TestGetListGender() {
    check(UGENDER_OTHER, UGENDER_OTHER, UGENDER_OTHER, NULL, 0);
    check(UGENDER_FEMALE, UGENDER_FEMALE, UGENDER_FEMALE, kSingleFemale, LENGTHOF(kSingleFemale));
    check(UGENDER_MALE, UGENDER_MALE, UGENDER_MALE, kSingleMale, LENGTHOF(kSingleMale));
    check(UGENDER_OTHER, UGENDER_OTHER, UGENDER_OTHER, kSingleOther, LENGTHOF(kSingleOther));

    check(UGENDER_OTHER, UGENDER_FEMALE, UGENDER_FEMALE, kAllFemale, LENGTHOF(kAllFemale));
    check(UGENDER_OTHER, UGENDER_MALE, UGENDER_MALE, kAllMale, LENGTHOF(kAllMale));
    check(UGENDER_OTHER, UGENDER_OTHER, UGENDER_MALE, kAllOther, LENGTHOF(kAllOther));

    check(UGENDER_OTHER, UGENDER_OTHER, UGENDER_MALE, kFemaleMale, LENGTHOF(kFemaleMale));
    check(UGENDER_OTHER, UGENDER_OTHER, UGENDER_MALE, kFemaleOther, LENGTHOF(kFemaleOther));
    check(UGENDER_OTHER, UGENDER_OTHER, UGENDER_MALE, kMaleOther, LENGTHOF(kMaleOther));
}

void GenderInfoTest::TestFallback() {
  UErrorCode status = U_ZERO_ERROR;
  const GenderInfo* actual = GenderInfo::getInstance("xx", status);
  if (U_FAILURE(status)) {
    errcheckln(status, "Fail to create GenderInfo - %s", u_errorName(status));
    return;
  }
  const GenderInfo* expected = GenderInfo::getNeutralInstance();
  if (expected != actual) {
    errln("For Neutral, expected %d got %d", expected, actual);
  }
  actual = GenderInfo::getInstance("fr_CA", status);
  if (U_FAILURE(status)) {
    errcheckln(status, "Fail to create GenderInfo - %s", u_errorName(status));
    return;
  }
  expected = GenderInfo::getMaleTaintsInstance();
  if (expected != actual) {
    errln("For Male Taints, Expected %d got %d", expected, actual);
  }
}

void GenderInfoTest::check(
    UGender expected_neutral, UGender expected_mixed, UGender expected_taints, const UGender* genderList, int32_t listLength) {
  checkLocale(Locale::getUS(), expected_neutral, genderList, listLength);
  checkLocale("is", expected_mixed, genderList, listLength);
  checkLocale(Locale::getFrench(), expected_taints, genderList, listLength);
}

void GenderInfoTest::checkLocale(
    const Locale& locale, UGender expected, const UGender* genderList, int32_t listLength) {
  UErrorCode status = U_ZERO_ERROR;
  const GenderInfo* gi = GenderInfo::getInstance(locale, status);
  if (U_FAILURE(status)) {
    errcheckln(status, "Fail to create GenderInfo - %s", u_errorName(status));
    return;
  }
  UGender actual = gi->getListGender(genderList, listLength, status);
  if (U_FAILURE(status)) {
    errcheckln(status, "Fail to get gender of list - %s", u_errorName(status));
    return;
  }
  if (actual != expected) {
    errln("For locale: %s expected: %d got %d", locale.getName(), expected, actual);
  }
}

extern IntlTest *createGenderInfoTest() {
  return new GenderInfoTest();
}

#endif /* #if !UCONFIG_NO_FORMATTING */
