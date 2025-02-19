// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2007-2014, International Business Machines Corporation and
* others. All Rights Reserved.
********************************************************************************

* File PLURULTS.cpp
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "unicode/localpointer.h"
#include "unicode/plurrule.h"
#include "unicode/stringpiece.h"
#include "unicode/numberformatter.h"

#include "cmemory.h"
#include "plurrule_impl.h"
#include "plurults.h"
#include "uhash.h"
#include "number_decimalquantity.h"

using icu::number::impl::DecimalQuantity;

void setupResult(const int32_t testSource[], char result[], int32_t* max);
UBool checkEqual(const PluralRules &test, char *result, int32_t max);
UBool testEquality(const PluralRules &test);

// This is an API test, not a unit test.  It doesn't test very many cases, and doesn't
// try to test the full functionality.  It just calls each function in the class and
// verifies that it works on a basic level.

void PluralRulesTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite PluralRulesAPI");
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testAPI);
    // TESTCASE_AUTO(testGetUniqueKeywordValue);
    TESTCASE_AUTO(testGetSamples);
    TESTCASE_AUTO(testWithin);
    TESTCASE_AUTO(testGetAllKeywordValues);
    TESTCASE_AUTO(testOrdinal);
    TESTCASE_AUTO(testSelect);
    TESTCASE_AUTO(testAvailbleLocales);
    TESTCASE_AUTO(testParseErrors);
    TESTCASE_AUTO(testFixedDecimal);
    TESTCASE_AUTO(testSelectTrailingZeros);
    TESTCASE_AUTO(testLocaleExtension);
    TESTCASE_AUTO_END;
}


// Quick and dirty class for putting UnicodeStrings in char * messages.
//   TODO: something like this should be generally available.
class US {
  private:
    char *buf;
  public:
    US(const UnicodeString &us) {
       int32_t bufLen = us.extract((int32_t)0, us.length(), (char *)NULL, (uint32_t)0) + 1;
       buf = (char *)uprv_malloc(bufLen);
       us.extract(0, us.length(), buf, bufLen); };
    const char *cstr() {return buf;};
    ~US() { uprv_free(buf);};
};





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
    LocalPointer<PluralRules> test(new PluralRules(status), status);
    if(U_FAILURE(status)) {
        dataerrln("ERROR: Could not create PluralRules (default) - exitting");
        return;
    }
    LocalPointer<PluralRules> newEnPlural(test->forLocale(Locale::getEnglish(), status), status);
    if(U_FAILURE(status)) {
        dataerrln("ERROR: Could not create PluralRules (English) - exitting");
        return;
    }

    // ======= Test clone, assignment operator && == operator.
    LocalPointer<PluralRules> dupRule(defRule.clone());
    if (dupRule==NULL) {
        errln("ERROR: clone plural rules test failed!");
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
    }

    // ======= Test empty plural rules
    logln("Testing Simple PluralRules");

    LocalPointer<PluralRules> empRule(test->createRules(UNICODE_STRING_SIMPLE("a:n"), status));
    UnicodeString key;
    for (int32_t i=0; i<10; ++i) {
        key = empRule->select(i);
        if ( key.charAt(0)!= 0x61 ) { // 'a'
            errln("ERROR:  empty plural rules test failed! - exitting");
        }
    }

    // ======= Test simple plural rules
    logln("Testing Simple PluralRules");

    char result[100];
    int32_t max;

    for (int32_t i=0; i<PLURAL_TEST_NUM-1; ++i) {
       LocalPointer<PluralRules> newRules(test->createRules(pluralTestData[i], status));
       setupResult(pluralTestResult[i], result, &max);
       if ( !checkEqual(*newRules, result, max) ) {
            errln("ERROR:  simple plural rules failed! - exitting");
            return;
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
    LocalPointer<PluralRules> newRules(test->createRules(complexRule, status));
    if ( !checkEqual(*newRules, cRuleResult, 12) ) {
         errln("ERROR:  complex plural rules failed! - exitting");
         return;
    }
    newRules.adoptInstead(test->createRules(complexRule2, status));
    if ( !checkEqual(*newRules, cRuleResult, 12) ) {
         errln("ERROR:  complex plural rules failed! - exitting");
         return;
    }

    // ======= Test decimal fractions plural rules
    UnicodeString decimalRule= UNICODE_STRING_SIMPLE("a: n not in 0..100;");
    UnicodeString KEYWORD_A = UNICODE_STRING_SIMPLE("a");
    status = U_ZERO_ERROR;
    newRules.adoptInstead(test->createRules(decimalRule, status));
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not create PluralRules for testing fractions - exitting");
        return;
    }
    double fData[] =     {-101, -100, -1,     -0.0,  0,     0.1,  1,     1.999,  2.0,   100,   100.001 };
    UBool isKeywordA[] = {TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE,   FALSE, FALSE, TRUE };
    for (int32_t i=0; i<UPRV_LENGTHOF(fData); i++) {
        if ((newRules->select(fData[i])== KEYWORD_A) != isKeywordA[i]) {
             errln("File %s, Line %d, ERROR: plural rules for decimal fractions test failed!\n"
                   "  number = %g, expected %s", __FILE__, __LINE__, fData[i], isKeywordA[i]?"TRUE":"FALSE");
        }
    }

    // ======= Test Equality
    logln("Testing Equality of PluralRules");

    if ( !testEquality(*test) ) {
         errln("ERROR:  complex plural rules failed! - exitting");
         return;
     }


    // ======= Test getStaticClassID()
    logln("Testing getStaticClassID()");

    if(test->getDynamicClassID() != PluralRules::getStaticClassID()) {
        errln("ERROR: getDynamicClassID() didn't return the expected value");
    }
    // ====== Test fallback to parent locale
    LocalPointer<PluralRules> en_UK(test->forLocale(Locale::getUK(), status));
    LocalPointer<PluralRules> en(test->forLocale(Locale::getEnglish(), status));
    if (en_UK.isValid() && en.isValid()) {
        if ( *en_UK != *en ) {
            errln("ERROR:  test locale fallback failed!");
        }
    }

    LocalPointer<PluralRules> zh_Hant(test->forLocale(Locale::getTaiwan(), status));
    LocalPointer<PluralRules> zh(test->forLocale(Locale::getChinese(), status));
    if (zh_Hant.isValid() && zh.isValid()) {
        if ( *zh_Hant != *zh ) {
            errln("ERROR:  test locale fallback failed!");
        }
    }
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


UBool checkEqual(const PluralRules &test, char *result, int32_t max) {
    UnicodeString key;
    UBool isEqual = TRUE;
    for (int32_t i=0; i<max; ++i) {
        key= test.select(i);
        if ( key.charAt(0)!=result[i] ) {
            isEqual = FALSE;
        }
    }
    return isEqual;
}



static const int32_t MAX_EQ_ROW = 2;
static const int32_t MAX_EQ_COL = 5;
UBool testEquality(const PluralRules &test) {
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
            rules[totalRules]=test.createRules(testEquRules[i][totalRules], status);
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

// TODO: UniqueKeywordValue() is not currently supported.
//       If it never will be, this test code should be removed.
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
  // TODO: fix samples, re-enable this test.

  // no get functional equivalent API in ICU4C, so just
  // test every locale...
  UErrorCode status = U_ZERO_ERROR;
  int32_t numLocales;
  const Locale* locales = Locale::getAvailableLocales(numLocales);

  double values[1000];
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
      int32_t count = rules->getSamples(*keyword, values, UPRV_LENGTHOF(values), status);
      if (U_FAILURE(status)) {
        errln(UNICODE_STRING_SIMPLE("getSamples() failed for locale ") +
              locales[i].getName() +
              UNICODE_STRING_SIMPLE(", keyword ") + *keyword);
        continue;
      }
      if (count == 0) {
        // TODO: Lots of these. 
        //   errln(UNICODE_STRING_SIMPLE("no samples for keyword ") + *keyword + UNICODE_STRING_SIMPLE(" in locale ") + locales[i].getName() );
      }
      if (count > UPRV_LENGTHOF(values)) {
        errln(UNICODE_STRING_SIMPLE("getSamples()=") + count +
              UNICODE_STRING_SIMPLE(", too many values, for locale ") +
              locales[i].getName() +
              UNICODE_STRING_SIMPLE(", keyword ") + *keyword);
        count = UPRV_LENGTHOF(values);
      }
      for (int32_t j = 0; j < count; ++j) {
        if (values[j] == UPLRULES_NO_UNIQUE_VALUE) {
          errln("got 'no unique value' among values");
        } else {
          UnicodeString resultKeyword = rules->select(values[j]);
          // if (strcmp(locales[i].getName(), "uk") == 0) {    // Debug only.
          //     std::cout << "  uk " << US(resultKeyword).cstr() << " " << values[j] << std::endl;
          // }
          if (*keyword != resultKeyword) {
            errln("file %s, line %d, Locale %s, sample for keyword \"%s\":  %g, select(%g) returns keyword \"%s\"",
                __FILE__, __LINE__, locales[i].getName(), US(*keyword).cstr(), values[j], values[j], US(resultKeyword).cstr());
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
        if (p == NULL || U_FAILURE(status)) {
            errln("file %s, line %d: could not create rules from '%s'\n"
                  "  ErrorCode: %s\n", 
                  __FILE__, __LINE__, data[i], u_errorName(status));
            continue;
        }

        // TODO: fix samples implementation, re-enable test.
        (void)result;
        #if 0

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
                    errln("file: %s, line %d, didn't get expected value: %s", __FILE__, __LINE__, ep);
                    ok = FALSE;
                }
            }

            while (*ep && *ep != ';') ++ep;
            if (*ep == ';') ++ep;
            rp = ep;
        }
    #endif
    delete p;
    }
}

void PluralRulesTest::testOrdinal() {
    IcuTestErrorCode errorCode(*this, "testOrdinal");
    LocalPointer<PluralRules> pr(PluralRules::forLocale("en", UPLURAL_TYPE_ORDINAL, errorCode));
    if (errorCode.errIfFailureAndReset("PluralRules::forLocale(en, UPLURAL_TYPE_ORDINAL) failed")) {
        return;
    }
    UnicodeString keyword = pr->select(2.);
    if (keyword != UNICODE_STRING("two", 3)) {
        dataerrln("PluralRules(en-ordinal).select(2) failed");
    }
}


static const char * END_MARK = "999.999";    // Mark end of varargs data.

void PluralRulesTest::checkSelect(const LocalPointer<PluralRules> &rules, UErrorCode &status, 
                                  int32_t line, const char *keyword, ...) {
    // The varargs parameters are a const char* strings, each being a decimal number.
    //   The formatting of the numbers as strings is significant, e.g.
    //     the difference between "2" and "2.0" can affect which rule matches (which keyword is selected).
    // Note: rules parameter is a LocalPointer reference rather than a PluralRules * to avoid having
    //       to write getAlias() at every (numerous) call site.

    if (U_FAILURE(status)) {
        errln("file %s, line %d, ICU error status: %s.", __FILE__, line, u_errorName(status));
        status = U_ZERO_ERROR;
        return;
    }

    if (rules == NULL) {
        errln("file %s, line %d: rules pointer is NULL", __FILE__, line);
        return;
    }
        
    va_list ap;
    va_start(ap, keyword);
    for (;;) {
        const char *num = va_arg(ap, const char *);
        if (strcmp(num, END_MARK) == 0) {
            break;
        }

        // DigitList is a convenient way to parse the decimal number string and get a double.
        DecimalQuantity  dl;
        dl.setToDecNumber(StringPiece(num), status);
        if (U_FAILURE(status)) {
            errln("file %s, line %d, ICU error status: %s.", __FILE__, line, u_errorName(status));
            status = U_ZERO_ERROR;
            continue;
        }
        double numDbl = dl.toDouble();
        const char *decimalPoint = strchr(num, '.');
        int fractionDigitCount = decimalPoint == NULL ? 0 : static_cast<int>((num + strlen(num) - 1) - decimalPoint);
        int fractionDigits = fractionDigitCount == 0 ? 0 : atoi(decimalPoint + 1);
        FixedDecimal ni(numDbl, fractionDigitCount, fractionDigits);
        
        UnicodeString actualKeyword = rules->select(ni);
        if (actualKeyword != UnicodeString(keyword)) {
            errln("file %s, line %d, select(%s) returned incorrect keyword. Expected %s, got %s",
                   __FILE__, line, num, keyword, US(actualKeyword).cstr());
        }
    }
    va_end(ap);
}

void PluralRulesTest::testSelect() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<PluralRules> pr(PluralRules::createRules("s: n in 1,3,4,6", status));
    checkSelect(pr, status, __LINE__, "s", "1.0", "3.0", "4.0", "6.0", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "0.0", "2.0", "3.1", "7.0", END_MARK);

    pr.adoptInstead(PluralRules::createRules("s: n not in 1,3,4,6", status));
    checkSelect(pr, status, __LINE__, "other", "1.0", "3.0", "4.0", "6.0", END_MARK);
    checkSelect(pr, status, __LINE__, "s", "0.0", "2.0", "3.1", "7.0", END_MARK);

    pr.adoptInstead(PluralRules::createRules("r: n in 1..4, 7..10, 14 .. 17;"
                                             "s: n is 29;", status));
    checkSelect(pr, status, __LINE__, "r", "1.0", "3.0", "7.0", "8.0", "10.0", "14.0", "17.0", END_MARK);
    checkSelect(pr, status, __LINE__, "s", "29.0", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "28.0", "29.1", END_MARK);

    pr.adoptInstead(PluralRules::createRules("a: n mod 10 is 1;  b: n mod 100 is 0 ", status));
    checkSelect(pr, status, __LINE__, "a", "1", "11", "41", "101", "301.00", END_MARK);
    checkSelect(pr, status, __LINE__, "b", "0", "100", "200.0", "300.", "1000", "1100", "110000", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "0.01", "1.01", "0.99", "2", "3", "99", "102", END_MARK);

    // Rules that end with or without a ';' and with or without trailing spaces.
    //    (There was a rule parser bug here with these.)
    pr.adoptInstead(PluralRules::createRules("a: n is 1", status));
    checkSelect(pr, status, __LINE__, "a", "1", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "2", END_MARK);

    pr.adoptInstead(PluralRules::createRules("a: n is 1 ", status));
    checkSelect(pr, status, __LINE__, "a", "1", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "2", END_MARK);

    pr.adoptInstead(PluralRules::createRules("a: n is 1;", status));
    checkSelect(pr, status, __LINE__, "a", "1", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "2", END_MARK);

    pr.adoptInstead(PluralRules::createRules("a: n is 1 ; ", status));
    checkSelect(pr, status, __LINE__, "a", "1", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "2", END_MARK);

    // First match when rules for different keywords are not disjoint.
    //   Also try spacing variations around ':' and '..'
    pr.adoptInstead(PluralRules::createRules("c: n in 5..15;  b : n in 1..10 ;a:n in 10 .. 20", status));
    checkSelect(pr, status, __LINE__, "a", "20", END_MARK);
    checkSelect(pr, status, __LINE__, "b", "1", END_MARK);
    checkSelect(pr, status, __LINE__, "c", "10", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "0", "21", "10.1", END_MARK);

    // in vs within
    pr.adoptInstead(PluralRules::createRules("a: n in 2..10; b: n within 8..15", status));
    checkSelect(pr, status, __LINE__, "a", "2", "8", "10", END_MARK);
    checkSelect(pr, status, __LINE__, "b", "8.01", "9.5", "11", "14.99", "15", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "1", "7.7", "15.01", "16", END_MARK);

    // OR and AND chains.
    pr.adoptInstead(PluralRules::createRules("a: n in 2..10 and n in 4..12 and n not in 5..7", status));
    checkSelect(pr, status, __LINE__, "a", "4", "8", "9", "10", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "2", "3", "5", "7", "11", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n is 2 or n is 5 or n in 7..11 and n in 11..13", status));
    checkSelect(pr, status, __LINE__, "a", "2", "5", "11", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "3", "4", "6", "8", "10", "12", "13", END_MARK);

    // Number attributes - 
    //   n: the number itself
    //   i: integer digits
    //   f: visible fraction digits
    //   t: f with trailing zeros removed.
    //   v: number of visible fraction digits
    //   j: = n if there are no visible fraction digits
    //      != anything if there are visible fraction digits

    pr.adoptInstead(PluralRules::createRules("a: i is 123", status));
    checkSelect(pr, status, __LINE__, "a", "123", "123.0", "123.1", "0123.99", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "124", "122.0", END_MARK);

    pr.adoptInstead(PluralRules::createRules("a: f is 120", status));
    checkSelect(pr, status, __LINE__, "a", "1.120", "0.120", "11123.120", "0123.120", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "1.121", "122.1200", "1.12", "120", END_MARK);

    pr.adoptInstead(PluralRules::createRules("a: t is 12", status));
    checkSelect(pr, status, __LINE__, "a", "1.120", "0.12", "11123.12000", "0123.1200000", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "1.121", "122.1200001", "1.11", "12", END_MARK);

    pr.adoptInstead(PluralRules::createRules("a: v is 3", status));
    checkSelect(pr, status, __LINE__, "a", "1.120", "0.000", "11123.100", "0123.124", ".666", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "1.1212", "122.12", "1.1", "122", "0.0000", END_MARK);

    pr.adoptInstead(PluralRules::createRules("a: v is 0 and i is 123", status));
    checkSelect(pr, status, __LINE__, "a", "123", "123.", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "123.0", "123.1", "123.123", "0.123", END_MARK);

    // The reserved words from the rule syntax will also function as keywords.
    pr.adoptInstead(PluralRules::createRules("a: n is 21; n: n is 22; i: n is 23; f: n is 24;"
                                             "t: n is 25; v: n is 26; w: n is 27; j: n is 28"
                                             , status));
    checkSelect(pr, status, __LINE__, "other", "20", "29", END_MARK);
    checkSelect(pr, status, __LINE__, "a", "21", END_MARK);
    checkSelect(pr, status, __LINE__, "n", "22", END_MARK);
    checkSelect(pr, status, __LINE__, "i", "23", END_MARK);
    checkSelect(pr, status, __LINE__, "f", "24", END_MARK);
    checkSelect(pr, status, __LINE__, "t", "25", END_MARK);
    checkSelect(pr, status, __LINE__, "v", "26", END_MARK);
    checkSelect(pr, status, __LINE__, "w", "27", END_MARK);
    checkSelect(pr, status, __LINE__, "j", "28", END_MARK);


    pr.adoptInstead(PluralRules::createRules("not: n=31; and: n=32; or: n=33; mod: n=34;"
                                             "in: n=35; within: n=36;is:n=37"
                                             , status));
    checkSelect(pr, status, __LINE__, "other",  "30", "39", END_MARK);
    checkSelect(pr, status, __LINE__, "not",    "31", END_MARK);
    checkSelect(pr, status, __LINE__, "and",    "32", END_MARK);
    checkSelect(pr, status, __LINE__, "or",     "33", END_MARK);
    checkSelect(pr, status, __LINE__, "mod",    "34", END_MARK);
    checkSelect(pr, status, __LINE__, "in",     "35", END_MARK);
    checkSelect(pr, status, __LINE__, "within", "36", END_MARK);
    checkSelect(pr, status, __LINE__, "is",     "37", END_MARK);

// Test cases from ICU4J PluralRulesTest.parseTestData

    pr.adoptInstead(PluralRules::createRules("a: n is 1", status));
    checkSelect(pr, status, __LINE__, "a", "1", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 10 is 2", status));
    checkSelect(pr, status, __LINE__, "a", "2", "12", "22", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n is not 1", status));
    checkSelect(pr, status, __LINE__, "a", "0", "2", "3", "4", "5", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 3 is not 1", status));
    checkSelect(pr, status, __LINE__, "a", "0", "2", "3", "5", "6", "8", "9", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n in 2..5", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "4", "5", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n within 2..5", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "4", "5", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n not in 2..5", status));
    checkSelect(pr, status, __LINE__, "a", "0", "1", "6", "7", "8", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n not within 2..5", status));
    checkSelect(pr, status, __LINE__, "a", "0", "1", "6", "7", "8", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 10 in 2..5", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "4", "5", "12", "13", "14", "15", "22", "23", "24", "25", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 10 within 2..5", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "4", "5", "12", "13", "14", "15", "22", "23", "24", "25", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 10 is 2 and n is not 12", status));
    checkSelect(pr, status, __LINE__, "a", "2", "22", "32", "42", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 10 in 2..3 or n mod 10 is 5", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "5", "12", "13", "15", "22", "23", "25", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 10 within 2..3 or n mod 10 is 5", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "5", "12", "13", "15", "22", "23", "25", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n is 1 or n is 4 or n is 23", status));
    checkSelect(pr, status, __LINE__, "a", "1", "4", "23", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 2 is 1 and n is not 3 and n in 1..11", status));
    checkSelect(pr, status, __LINE__, "a", "1", "5", "7", "9", "11", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 2 is 1 and n is not 3 and n within 1..11", status));
    checkSelect(pr, status, __LINE__, "a", "1", "5", "7", "9", "11", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 2 is 1 or n mod 5 is 1 and n is not 6", status));
    checkSelect(pr, status, __LINE__, "a", "1", "3", "5", "7", "9", "11", "13", "15", "16", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n in 2..5; b: n in 5..8; c: n mod 2 is 1", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "4", "5", END_MARK);
    checkSelect(pr, status, __LINE__, "b", "6", "7", "8", END_MARK);
    checkSelect(pr, status, __LINE__, "c", "1", "9", "11", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n within 2..5; b: n within 5..8; c: n mod 2 is 1", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "4", "5", END_MARK);
    checkSelect(pr, status, __LINE__, "b", "6", "7", "8", END_MARK);
    checkSelect(pr, status, __LINE__, "c", "1", "9", "11", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n in 2, 4..6; b: n within 7..9,11..12,20", status));
    checkSelect(pr, status, __LINE__, "a", "2", "4", "5", "6", END_MARK);
    checkSelect(pr, status, __LINE__, "b", "7", "8", "9", "11", "12", "20", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n in 2..8, 12 and n not in 4..6", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "7", "8", "12", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n mod 10 in 2, 3,5..7 and n is not 12", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "5", "6", "7", "13", "15", "16", "17", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a: n in 2..6, 3..7", status));
    checkSelect(pr, status, __LINE__, "a", "2", "3", "4", "5", "6", "7", END_MARK);

    // Extended Syntax, with '=', '!=' and '%' operators. 
    pr.adoptInstead(PluralRules::createRules("a: n = 1..8 and n!= 2,3,4,5", status));
    checkSelect(pr, status, __LINE__, "a", "1", "6", "7", "8", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "0", "2", "3", "4", "5", "9", END_MARK);
    pr.adoptInstead(PluralRules::createRules("a:n % 10 != 1", status));
    checkSelect(pr, status, __LINE__, "a", "2", "6", "7", "8", END_MARK);
    checkSelect(pr, status, __LINE__, "other", "1", "21", "211", "91", END_MARK);
}


void PluralRulesTest::testAvailbleLocales() {
    
    // Hash set of (char *) strings.
    UErrorCode status = U_ZERO_ERROR;
    UHashtable *localeSet = uhash_open(uhash_hashUnicodeString, uhash_compareUnicodeString, uhash_compareLong, &status);
    uhash_setKeyDeleter(localeSet, uprv_deleteUObject);
    if (U_FAILURE(status)) {
        errln("file %s,  line %d: Error status = %s", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    // Check that each locale returned by the iterator is unique.
    StringEnumeration *localesEnum = PluralRules::getAvailableLocales(status);
    int localeCount = 0;
    for (;;) {
        const char *locale = localesEnum->next(NULL, status);
        if (U_FAILURE(status)) {
            dataerrln("file %s,  line %d: Error status = %s", __FILE__, __LINE__, u_errorName(status));
            return;
        }
        if (locale == NULL) {
            break;
        }
        localeCount++;
        int32_t oldVal = uhash_puti(localeSet, new UnicodeString(locale), 1, &status);
        if (oldVal != 0) {
            errln("file %s,  line %d: locale %s was seen before.", __FILE__, __LINE__, locale);
        }
    }

    // Reset the iterator, verify that we get the same count.
    localesEnum->reset(status);
    int32_t localeCount2 = 0;
    while (localesEnum->next(NULL, status) != NULL) {
        if (U_FAILURE(status)) {
            errln("file %s,  line %d: Error status = %s", __FILE__, __LINE__, u_errorName(status));
            break;
        }
        localeCount2++;
    }
    if (localeCount != localeCount2) {
        errln("file %s,  line %d: locale counts differ. They are (%d, %d)", 
            __FILE__, __LINE__, localeCount, localeCount2);
    }

    // Instantiate plural rules for each available locale.
    localesEnum->reset(status);
    for (;;) {
        status = U_ZERO_ERROR;
        const char *localeName = localesEnum->next(NULL, status);
        if (U_FAILURE(status)) {
            errln("file %s,  line %d: Error status = %s, locale = %s",
                __FILE__, __LINE__, u_errorName(status), localeName);
            return;
        }
        if (localeName == NULL) {
            break;
        }
        Locale locale = Locale::createFromName(localeName);
        PluralRules *pr = PluralRules::forLocale(locale, status);
        if (U_FAILURE(status)) {
            errln("file %s,  line %d: Error %s creating plural rules for locale %s", 
                __FILE__, __LINE__, u_errorName(status), localeName);
            continue;
        }
        if (pr == NULL) {
            errln("file %s, line %d: Null plural rules for locale %s", __FILE__, __LINE__, localeName);
            continue;
        }

        // Pump some numbers through the plural rules.  Can't check for correct results, 
        // mostly this to tickle any asserts or crashes that may be lurking.
        for (double n=0; n<120.0; n+=0.5) {
            UnicodeString keyword = pr->select(n);
            if (keyword.length() == 0) {
                errln("file %s, line %d, empty keyword for n = %g, locale %s",
                    __FILE__, __LINE__, n, localeName);
            }
        }
        delete pr;
    }

    uhash_close(localeSet);
    delete localesEnum;

}


void PluralRulesTest::testParseErrors() {
    // Test rules with syntax errors.
    // Creation of PluralRules from them should fail.

    static const char *testCases[] = {
            "a: n mod 10, is 1",
            "a: q is 13",
            "a  n is 13",
            "a: n is 13,",
            "a: n is 13, 15,   b: n is 4",
            "a: n is 1, 3, 4.. ",
            "a: n within 5..4",
            "A: n is 13",          // Uppercase keywords not allowed.
            "a: n ! = 3",          // spaces in != operator
            "a: n = not 3",        // '=' not exact equivalent of 'is'
            "a: n ! in 3..4"       // '!' not exact equivalent of 'not'
            "a: n % 37 ! in 3..4"

            };
    for (int i=0; i<UPRV_LENGTHOF(testCases); i++) {
        const char *rules = testCases[i];
        UErrorCode status = U_ZERO_ERROR;
        PluralRules *pr = PluralRules::createRules(UnicodeString(rules), status);
        if (U_SUCCESS(status)) {
            errln("file %s, line %d, expected failure with \"%s\".", __FILE__, __LINE__, rules);
        }
        if (pr != NULL) {
            errln("file %s, line %d, expected NULL. Rules: \"%s\"", __FILE__, __LINE__, rules);
        }
    }
    return;
}


void PluralRulesTest::testFixedDecimal() {
    struct DoubleTestCase {
        double n;
        int32_t fractionDigitCount;
        int64_t fractionDigits;
    };

    // Check that the internal functions for extracting the decimal fraction digits from
    //   a double value are working.
    static DoubleTestCase testCases[] = {
        {1.0, 0, 0},
        {123456.0, 0, 0},
        {1.1, 1, 1},
        {1.23, 2, 23},
        {1.234, 3, 234},
        {1.2345, 4, 2345},
        {1.23456, 5, 23456},
        {.1234, 4, 1234},
        {.01234, 5, 1234},
        {.001234, 6, 1234},
        {.0001234, 7, 1234},
        {100.1234, 4, 1234},
        {100.01234, 5, 1234},
        {100.001234, 6, 1234},
        {100.0001234, 7, 1234}
    };

    for (int i=0; i<UPRV_LENGTHOF(testCases); ++i) {
        DoubleTestCase &tc = testCases[i];
        int32_t numFractionDigits = FixedDecimal::decimals(tc.n);
        if (numFractionDigits != tc.fractionDigitCount) {
            errln("file %s, line %d: decimals(%g) expected %d, actual %d",
                   __FILE__, __LINE__, tc.n, tc.fractionDigitCount, numFractionDigits);
            continue;
        }
        int64_t actualFractionDigits = FixedDecimal::getFractionalDigits(tc.n, numFractionDigits);
        if (actualFractionDigits != tc.fractionDigits) {
            errln("file %s, line %d: getFractionDigits(%g, %d): expected %ld, got %ld",
                  __FILE__, __LINE__, tc.n, numFractionDigits, tc.fractionDigits, actualFractionDigits);
        }
    }
}


void PluralRulesTest::testSelectTrailingZeros() {
    IcuTestErrorCode status(*this, "testSelectTrailingZeros");
    number::UnlocalizedNumberFormatter unf = number::NumberFormatter::with()
            .precision(number::Precision::fixedFraction(2));
    struct TestCase {
        const char* localeName;
        const char16_t* expectedDoubleKeyword;
        const char16_t* expectedFormattedKeyword;
        double number;
    } cases[] = {
        {"bs",  u"few",   u"other", 5.2},  // 5.2 => two, but 5.20 => other
        {"si",  u"one",   u"one",   0.0},
        {"si",  u"one",   u"one",   1.0},
        {"si",  u"one",   u"other", 0.1},  // 0.1 => one, but 0.10 => other
        {"si",  u"one",   u"one",   0.01}, // 0.01 => one
        {"hsb", u"few",   u"few",   1.03}, // (f % 100 == 3) => few
        {"hsb", u"few",   u"other", 1.3},  // 1.3 => few, but 1.30 => other
    };
    for (const auto& cas : cases) {
        UnicodeString message(UnicodeString(cas.localeName) + u" " + DoubleToUnicodeString(cas.number));
        status.setScope(message);
        Locale locale(cas.localeName);
        LocalPointer<PluralRules> rules(PluralRules::forLocale(locale, status));
        assertEquals(message, cas.expectedDoubleKeyword, rules->select(cas.number));
        number::FormattedNumber fn = unf.locale(locale).formatDouble(cas.number, status);
        assertEquals(message, cas.expectedFormattedKeyword, rules->select(fn, status));
        status.errIfFailureAndReset();
    }
}

void PluralRulesTest::testLocaleExtension() {
    IcuTestErrorCode errorCode(*this, "testLocaleExtension");
    LocalPointer<PluralRules> rules(PluralRules::forLocale("pt@calendar=gregorian", errorCode));
    if (errorCode.errIfFailureAndReset("PluralRules::forLocale()")) { return; }
    UnicodeString key = rules->select(1);
    assertEquals("pt@calendar=gregorian select(1)", u"one", key);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
