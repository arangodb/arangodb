/*
*******************************************************************************
* Copyright (C) 2007-2011, International Business Machines Corporation and
* others. All Rights Reserved.
********************************************************************************

* File PLURRULTS.cpp
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <stdlib.h> // for strtod
#include "plurults.h"
#include "unicode/plurrule.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof(array[0]))

void setupResult(const int32_t testSource[], char result[], int32_t* max);
UBool checkEqual(PluralRules *test, char *result, int32_t max);
UBool testEquality(PluralRules *test);

// This is an API test, not a unit test.  It doesn't test very many cases, and doesn't
// try to test the full functionality.  It just calls each function in the class and
// verifies that it works on a basic level.

void PluralRulesTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite PluralRulesAPI");
    switch (index) {
        TESTCASE(0, testAPI);
        TESTCASE(1, testGetUniqueKeywordValue);
        TESTCASE(2, testGetSamples);
        TESTCASE(3, testWithin);
        TESTCASE(4, testGetAllKeywordValues);
        default: name = ""; break;
    }
}

#define PLURAL_TEST_NUM    18
/**
 * Test various generic API methods of PluralRules for API coverage.
 */
void PluralRulesTest::testAPI(/*char *par*/)
{
    UnicodeString pluralTestData[PLURAL_TEST_NUM] = {
            UNICODE_STRING_SIMPLE("a: n is 1"),
            UNICODE_STRING_SIMPLE("a: n mod 10 is 2"),
            UNICODE_STRING_SIMPLE("a: n is not 1"),
            UNICODE_STRING_SIMPLE("a: n mod 3 is not 1"),
            UNICODE_STRING_SIMPLE("a: n in 2..5"),
            UNICODE_STRING_SIMPLE("a: n within 2..5"),
            UNICODE_STRING_SIMPLE("a: n not in 2..5"),
            UNICODE_STRING_SIMPLE("a: n not within 2..5"),
            UNICODE_STRING_SIMPLE("a: n mod 10 in 2..5"),
            UNICODE_STRING_SIMPLE("a: n mod 10 within 2..5"),
            UNICODE_STRING_SIMPLE("a: n mod 10 is 2 and n is not 12"),
            UNICODE_STRING_SIMPLE("a: n mod 10 in 2..3 or n mod 10 is 5"),
            UNICODE_STRING_SIMPLE("a: n mod 10 within 2..3 or n mod 10 is 5"),
            UNICODE_STRING_SIMPLE("a: n is 1 or n is 4 or n is 23"),
            UNICODE_STRING_SIMPLE("a: n mod 2 is 1 and n is not 3 and n in 1..11"),
            UNICODE_STRING_SIMPLE("a: n mod 2 is 1 and n is not 3 and n within 1..11"),
            UNICODE_STRING_SIMPLE("a: n mod 2 is 1 or n mod 5 is 1 and n is not 6"),
            "",
    };
    static const int32_t pluralTestResult[PLURAL_TEST_NUM][30] = {
        {1, 0},
        {2,12,22, 0},
        {0,2,3,4,5,0},
        {0,2,3,5,6,8,9,0},
        {2,3,4,5,0},
        {2,3,4,5,0},
        {0,1,6,7,8, 0},
        {0,1,6,7,8, 0},
        {2,3,4,5,12,13,14,15,22,23,24,25,0},
        {2,3,4,5,12,13,14,15,22,23,24,25,0},
        {2,22,32,42,0},
        {2,3,5,12,13,15,22,23,25,0},
        {2,3,5,12,13,15,22,23,25,0},
        {1,4,23,0},
        {1,5,7,9,11,0},
        {1,5,7,9,11,0},
        {1,3,5,7,9,11,13,15,16,0},
    };
    UErrorCode status = U_ZERO_ERROR;

    // ======= Test constructors
    logln("Testing PluralRules constructors");


    logln("\n start default locale test case ..\n");

    PluralRules defRule(status);
    PluralRules* test=new PluralRules(status);
    PluralRules* newEnPlural= test->forLocale(Locale::getEnglish(), status);
    if(U_FAILURE(status)) {
        dataerrln("ERROR: Could not create PluralRules (default) - exitting");
        delete test;
        return;
    }

    // ======= Test clone, assignment operator && == operator.
    PluralRules *dupRule = defRule.clone();
    if (dupRule==NULL) {
        errln("ERROR: clone plural rules test failed!");
        delete test;
        return;
    } else {
        if ( *dupRule != defRule ) {
            errln("ERROR:  clone plural rules test failed!");
        }
    }
    *dupRule = *newEnPlural;
    if (dupRule!=NULL) {
        if ( *dupRule != *newEnPlural ) {
            errln("ERROR:  clone plural rules test failed!");
        }
        delete dupRule;
    }

    delete newEnPlural;

    // ======= Test empty plural rules
    logln("Testing Simple PluralRules");

    PluralRules* empRule = test->createRules(UNICODE_STRING_SIMPLE("a:n"), status);
    UnicodeString key;
    for (int32_t i=0; i<10; ++i) {
        key = empRule->select(i);
        if ( key.charAt(0)!= 0x61 ) { // 'a'
            errln("ERROR:  empty plural rules test failed! - exitting");
        }
    }
    if (empRule!=NULL) {
        delete empRule;
    }

    // ======= Test simple plural rules
    logln("Testing Simple PluralRules");

    char result[100];
    int32_t max;

    for (int32_t i=0; i<PLURAL_TEST_NUM-1; ++i) {
       PluralRules *newRules = test->createRules(pluralTestData[i], status);
       setupResult(pluralTestResult[i], result, &max);
       if ( !checkEqual(newRules, result, max) ) {
            errln("ERROR:  simple plural rules failed! - exitting");
            delete test;
            return;
        }
       if (newRules!=NULL) {
           delete newRules;
       }
    }


    // ======= Test complex plural rules
    logln("Testing Complex PluralRules");
    // TODO: the complex test data is hard coded. It's better to implement
    // a parser to parse the test data.
    UnicodeString complexRule = UNICODE_STRING_SIMPLE("a: n in 2..5; b: n in 5..8; c: n mod 2 is 1");
    UnicodeString complexRule2 = UNICODE_STRING_SIMPLE("a: n within 2..5; b: n within 5..8; c: n mod 2 is 1");
    char cRuleResult[] =
    {
       0x6F, // 'o'
       0x63, // 'c'
       0x61, // 'a'
       0x61, // 'a'
       0x61, // 'a'
       0x61, // 'a'
       0x62, // 'b'
       0x62, // 'b'
       0x62, // 'b'
       0x63, // 'c'
       0x6F, // 'o'
       0x63  // 'c'
    };
    PluralRules *newRules = test->createRules(complexRule, status);
    if ( !checkEqual(newRules, cRuleResult, 12) ) {
         errln("ERROR:  complex plural rules failed! - exitting");
         delete test;
         return;
     }
    if (newRules!=NULL) {
        delete newRules;
        newRules=NULL;
    }
    newRules = test->createRules(complexRule2, status);
    if ( !checkEqual(newRules, cRuleResult, 12) ) {
         errln("ERROR:  complex plural rules failed! - exitting");
         delete test;
         return;
     }
    if (newRules!=NULL) {
        delete newRules;
        newRules=NULL;
    }

    // ======= Test decimal fractions plural rules
    UnicodeString decimalRule= UNICODE_STRING_SIMPLE("a: n not in 0..100;");
    UnicodeString KEYWORD_A = UNICODE_STRING_SIMPLE("a");
    status = U_ZERO_ERROR;
    newRules = test->createRules(decimalRule, status);
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not create PluralRules for testing fractions - exitting");
        delete test;
        return;
    }
    double fData[10] = {-100, -1, -0.0, 0, 0.1, 1, 1.999, 2.0, 100, 100.001 };
    UBool isKeywordA[10] = {
           TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE, FALSE, TRUE };
    for (int32_t i=0; i<10; i++) {
        if ((newRules->select(fData[i])== KEYWORD_A) != isKeywordA[i]) {
             errln("ERROR: plural rules for decimal fractions test failed!");
        }
    }
    if (newRules!=NULL) {
        delete newRules;
        newRules=NULL;
    }



    // ======= Test Equality
    logln("Testing Equality of PluralRules");


    if ( !testEquality(test) ) {
         errln("ERROR:  complex plural rules failed! - exitting");
         delete test;
         return;
     }


    // ======= Test getStaticClassID()
    logln("Testing getStaticClassID()");

    if(test->getDynamicClassID() != PluralRules::getStaticClassID()) {
        errln("ERROR: getDynamicClassID() didn't return the expected value");
    }
    // ====== Test fallback to parent locale
    PluralRules *en_UK = test->forLocale(Locale::getUK(), status);
    PluralRules *en = test->forLocale(Locale::getEnglish(), status);
    if (en_UK != NULL && en != NULL) {
        if ( *en_UK != *en ) {
            errln("ERROR:  test locale fallback failed!");
        }
    }
    delete en;
    delete en_UK;

    PluralRules *zh_Hant = test->forLocale(Locale::getTaiwan(), status);
    PluralRules *zh = test->forLocale(Locale::getChinese(), status);
    if (zh_Hant != NULL && zh != NULL) {
        if ( *zh_Hant != *zh ) {
            errln("ERROR:  test locale fallback failed!");
        }
    }
    delete zh_Hant;
    delete zh;
    delete test;
}

void setupResult(const int32_t testSource[], char result[], int32_t* max) {
    int32_t i=0;
    int32_t curIndex=0;

    do {
        while (curIndex < testSource[i]) {
            result[curIndex++]=0x6F; //'o' other
        }
        result[curIndex++]=0x61; // 'a'

    } while(testSource[++i]>0);
    *max=curIndex;
}


UBool checkEqual(PluralRules *test, char *result, int32_t max) {
    UnicodeString key;
    UBool isEqual = TRUE;
    for (int32_t i=0; i<max; ++i) {
        key= test->select(i);
        if ( key.charAt(0)!=result[i] ) {
            isEqual = FALSE;
        }
    }
    return isEqual;
}

#define MAX_EQ_ROW  2
#define MAX_EQ_COL  5
UBool testEquality(PluralRules *test) {
    UnicodeString testEquRules[MAX_EQ_ROW][MAX_EQ_COL] = {
        {   UNICODE_STRING_SIMPLE("a: n in 2..3"),
            UNICODE_STRING_SIMPLE("a: n is 2 or n is 3"),
            UNICODE_STRING_SIMPLE( "a:n is 3 and n in 2..5 or n is 2"),
            "",
        },
        {   UNICODE_STRING_SIMPLE("a: n is 12; b:n mod 10 in 2..3"),
            UNICODE_STRING_SIMPLE("b: n mod 10 in 2..3 and n is not 12; a: n in 12..12"),
            UNICODE_STRING_SIMPLE("b: n is 13; a: n in 12..13; b: n mod 10 is 2 or n mod 10 is 3"),
            "",
        }
    };
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString key[MAX_EQ_COL];
    UBool ret=TRUE;
    for (int32_t i=0; i<MAX_EQ_ROW; ++i) {
        PluralRules* rules[MAX_EQ_COL];

        for (int32_t j=0; j<MAX_EQ_COL; ++j) {
            rules[j]=NULL;
        }
        int32_t totalRules=0;
        while((totalRules<MAX_EQ_COL) && (testEquRules[i][totalRules].length()>0) ) {
            rules[totalRules]=test->createRules(testEquRules[i][totalRules], status);
            totalRules++;
        }
        for (int32_t n=0; n<300 && ret ; ++n) {
            for(int32_t j=0; j<totalRules;++j) {
                key[j] = rules[j]->select(n);
            }
            for(int32_t j=0; j<totalRules-1;++j) {
                if (key[j]!=key[j+1]) {
                    ret= FALSE;
                    break;
                }
            }

        }
        for (int32_t j=0; j<MAX_EQ_COL; ++j) {
            if (rules[j]!=NULL) {
                delete rules[j];
            }
        }
    }

    return ret;
}

void
PluralRulesTest::assertRuleValue(const UnicodeString& rule, double expected) {
  assertRuleKeyValue("a:" + rule, "a", expected);
}

void
PluralRulesTest::assertRuleKeyValue(const UnicodeString& rule,
                                    const UnicodeString& key, double expected) {
  UErrorCode status = U_ZERO_ERROR;
  PluralRules *pr = PluralRules::createRules(rule, status);
  double result = pr->getUniqueKeywordValue(key);
  delete pr;
  if (expected != result) {
    errln("expected %g but got %g", expected, result);
  }
}

void PluralRulesTest::testGetUniqueKeywordValue() {
  assertRuleValue("n is 1", 1);
  assertRuleValue("n in 2..2", 2);
  assertRuleValue("n within 2..2", 2);
  assertRuleValue("n in 3..4", UPLRULES_NO_UNIQUE_VALUE);
  assertRuleValue("n within 3..4", UPLRULES_NO_UNIQUE_VALUE);
  assertRuleValue("n is 2 or n is 2", 2);
  assertRuleValue("n is 2 and n is 2", 2);
  assertRuleValue("n is 2 or n is 3", UPLRULES_NO_UNIQUE_VALUE);
  assertRuleValue("n is 2 and n is 3", UPLRULES_NO_UNIQUE_VALUE);
  assertRuleValue("n is 2 or n in 2..3", UPLRULES_NO_UNIQUE_VALUE);
  assertRuleValue("n is 2 and n in 2..3", 2);
  assertRuleKeyValue("a: n is 1", "not_defined", UPLRULES_NO_UNIQUE_VALUE); // key not defined
  assertRuleKeyValue("a: n is 1", "other", UPLRULES_NO_UNIQUE_VALUE); // key matches default rule
}

void PluralRulesTest::testGetSamples() {
  // no get functional equivalent API in ICU4C, so just
  // test every locale...
  UErrorCode status = U_ZERO_ERROR;
  int32_t numLocales;
  const Locale* locales = Locale::getAvailableLocales(numLocales);

  double values[4];
  for (int32_t i = 0; U_SUCCESS(status) && i < numLocales; ++i) {
    PluralRules *rules = PluralRules::forLocale(locales[i], status);
    if (U_FAILURE(status)) {
      break;
    }
    StringEnumeration *keywords = rules->getKeywords(status);
    if (U_FAILURE(status)) {
      delete rules;
      break;
    }
    const UnicodeString* keyword;
    while (NULL != (keyword = keywords->snext(status))) {
      int32_t count = rules->getSamples(*keyword, values, 4, status);
      if (U_FAILURE(status)) {
        errln(UNICODE_STRING_SIMPLE("getSamples() failed for locale ") +
              locales[i].getName() +
              UNICODE_STRING_SIMPLE(", keyword ") + *keyword);
        continue;
      }
      if (count == 0) {
        errln("no samples for keyword");
      }
      if (count > LENGTHOF(values)) {
        errln(UNICODE_STRING_SIMPLE("getSamples()=") + count +
              UNICODE_STRING_SIMPLE(", too many values, for locale ") +
              locales[i].getName() +
              UNICODE_STRING_SIMPLE(", keyword ") + *keyword);
        count = LENGTHOF(values);
      }
      for (int32_t j = 0; j < count; ++j) {
        if (values[j] == UPLRULES_NO_UNIQUE_VALUE) {
          errln("got 'no unique value' among values");
        } else {
          UnicodeString resultKeyword = rules->select(values[j]);
          if (*keyword != resultKeyword) {
            errln("keywords don't match");
          }
        }
      }
    }
    delete keywords;
    delete rules;
  }
}

void PluralRulesTest::testWithin() {
  // goes to show you what lack of testing will do.
  // of course, this has been broken for two years and no one has noticed...
  UErrorCode status = U_ZERO_ERROR;
  PluralRules *rules = PluralRules::createRules("a: n mod 10 in 5..8", status);
  if (!rules) {
    errln("couldn't instantiate rules");
    return;
  }

  UnicodeString keyword = rules->select((int32_t)26);
  if (keyword != "a") {
    errln("expected 'a' for 26 but didn't get it.");
  }

  keyword = rules->select(26.5);
  if (keyword != "other") {
    errln("expected 'other' for 26.5 but didn't get it.");
  }

  delete rules;
}

void
PluralRulesTest::testGetAllKeywordValues() {
    const char* data[] = {
        "a: n in 2..5", "a: 2,3,4,5; other: null; b:",
        "a: n not in 2..5", "a: null; other: null",
        "a: n within 2..5", "a: null; other: null",
        "a: n not within 2..5", "a: null; other: null",
        "a: n in 2..5 or n within 6..8", "a: null", // ignore 'other' here on out, always null
        "a: n in 2..5 and n within 6..8", "a:",
        "a: n in 2..5 and n within 5..8", "a: 5",
        "a: n within 2..5 and n within 6..8", "a:", // our sampling catches these
        "a: n within 2..5 and n within 5..8", "a: 5", // ''
        "a: n within 1..2 and n within 2..3 or n within 3..4 and n within 4..5", "a: 2,4",
        "a: n within 1..2 and n within 2..3 or n within 3..4 and n within 4..5 "
          "or n within 5..6 and n within 6..7", "a: null", // but not this...
        "a: n mod 3 is 0", "a: null",
        "a: n mod 3 is 0 and n within 1..2", "a:",
        "a: n mod 3 is 0 and n within 0..5", "a: 0,3",
        "a: n mod 3 is 0 and n within 0..6", "a: null", // similarly with mod, we don't catch...
        "a: n mod 3 is 0 and n in 3..12", "a: 3,6,9,12",
        NULL
    };

    for (int i = 0; data[i] != NULL; i += 2) {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString ruleDescription(data[i], -1, US_INV);
        const char* result = data[i+1];

        logln("[%d] %s", i >> 1, data[i]);

        PluralRules *p = PluralRules::createRules(ruleDescription, status);
        if (U_FAILURE(status)) {
            logln("could not create rules from '%s'\n", data[i]);
            continue;
        }

        const char* rp = result;
        while (*rp) {
            while (*rp == ' ') ++rp;
            if (!rp) {
                break;
            }

            const char* ep = rp;
            while (*ep && *ep != ':') ++ep;

            status = U_ZERO_ERROR;
            UnicodeString keyword(rp, ep - rp, US_INV);
            double samples[4]; // no test above should have more samples than 4
            int32_t count = p->getAllKeywordValues(keyword, &samples[0], 4, status);
            if (U_FAILURE(status)) {
                errln("error getting samples for %s", rp);
                break;
            }

            if (count > 4) {
              errln("count > 4 for keyword %s", rp);
              count = 4;
            }

            if (*ep) {
                ++ep; // skip colon
                while (*ep && *ep == ' ') ++ep; // and spaces
            }

            UBool ok = TRUE;
            if (count == -1) {
                if (*ep != 'n') {
                    errln("expected values for keyword %s but got -1 (%s)", rp, ep);
                    ok = FALSE;
                }
            } else if (*ep == 'n') {
                errln("expected count of -1, got %d, for keyword %s (%s)", count, rp, ep);
                ok = FALSE;
            }

            // We'll cheat a bit here.  The samples happend to be in order and so are our
            // expected values, so we'll just test in order until a failure.  If the
            // implementation changes to return samples in an arbitrary order, this test
            // must change.  There's no actual restriction on the order of the samples.

            for (int j = 0; ok && j < count; ++j ) { // we've verified count < 4
                double val = samples[j];
                if (*ep == 0 || *ep == ';') {
                    errln("got unexpected value[%d]: %g", j, val);
                    ok = FALSE;
                    break;
                }
                char* xp;
                double expectedVal = strtod(ep, &xp);
                if (xp == ep) {
                    // internal error
                    errln("yikes!");
                    ok = FALSE;
                    break;
                }
                ep = xp;
                if (expectedVal != val) {
                    errln("expected %g but got %g", expectedVal, val);
                    ok = FALSE;
                    break;
                }
                if (*ep == ',') ++ep;
            }

            if (ok && count != -1) {
                if (!(*ep == 0 || *ep == ';')) {
                    errln("didn't get expected value: %s", ep);
                    ok = FALSE;
                }
            }

            while (*ep && *ep != ';') ++ep;
            if (*ep == ';') ++ep;
            rp = ep;
        }
        delete p;
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
