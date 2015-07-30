/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2007-2013, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/msgfmt.h"
#include "unicode/plurfmt.h"
#include "unicode/plurrule.h"
#include "cmemory.h"
#include "plurfmts.h"
#include "plurults.h"

#define PLURAL_PATTERN_DATA 4
#define PLURAL_TEST_ARRAY_SIZE 256

#define PLURAL_SYNTAX_DATA 8

// The value must be same as PLKeywordLookups[] order.
#define PFT_ZERO   0
#define PFT_ONE    1
#define PFT_TWO    2
#define PFT_FEW    3
#define PFT_MANY   4
#define PFT_OTHER  5

void PluralFormatTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite PluralFormat");
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(pluralFormatBasicTest);
    TESTCASE_AUTO(pluralFormatUnitTest);
    TESTCASE_AUTO(pluralFormatLocaleTest);
    TESTCASE_AUTO(pluralFormatExtendedTest);
    TESTCASE_AUTO(pluralFormatExtendedParseTest);
    TESTCASE_AUTO(ordinalFormatTest);
    TESTCASE_AUTO(TestDecimals);
    TESTCASE_AUTO_END;
}

/**
 * Test various generic API methods of PluralFormat for Basic usage.
 */
void PluralFormatTest::pluralFormatBasicTest(/*char *par*/)
{
    UErrorCode status[8];
    PluralFormat* plFmt[8];
    Locale        locale = Locale::getDefault();
    UnicodeString otherPattern = UnicodeString("other{#}");
    UnicodeString message=UnicodeString("ERROR: PluralFormat basic test");

    // ========= Test constructors
    logln(" Testing PluralFormat constructors ...");
    status[0] = U_ZERO_ERROR;
    PluralRules*  plRules = PluralRules::createDefaultRules(status[0]);
  
    status[0] = U_ZERO_ERROR;
    NumberFormat *numFmt = NumberFormat::createInstance(status[0]);
    if (U_FAILURE(status[0])) {
        dataerrln("ERROR: Could not create NumberFormat instance with default locale ");
    }
    
    for (int32_t i=0; i< 8; ++i) {
        status[i] = U_ZERO_ERROR;
    }
    plFmt[0] = new PluralFormat(status[0]);
    plFmt[1] = new PluralFormat(*plRules, status[1]);
    plFmt[2] = new PluralFormat(locale, status[2]);
    plFmt[3] = new PluralFormat(locale, *plRules, status[3]);
    plFmt[4] = new PluralFormat(otherPattern, status[4]);
    plFmt[5] = new PluralFormat(*plRules, otherPattern, status[5]);
    plFmt[6] = new PluralFormat(locale, otherPattern, status[6]);
    plFmt[7] = new PluralFormat(locale, *plRules, otherPattern, status[7]);
    
    for (int32_t i=0; i< 8; ++i) {
        if (U_SUCCESS(status[i])) {
            numberFormatTest(plFmt[i], numFmt, 1, 12, NULL, NULL, FALSE, &message);
            numberFormatTest(plFmt[i], numFmt, 100, 112, NULL, NULL, FALSE, &message);
        }
        else {
            dataerrln("ERROR: PluralFormat constructor failed!");
        }
       delete plFmt[i];
    }
    // ======= Test clone, assignment operator && == operator.
    plFmt[0]= new PluralFormat(status[0]);
    plFmt[0]->setNumberFormat(numFmt,status[0]);
    UnicodeString us = UnicodeString("");
    plFmt[0]->toPattern(us);
    plFmt[1]= new PluralFormat(locale, status[1]);
    if ( U_SUCCESS(status[0]) && U_SUCCESS(status[1]) ) {
        *plFmt[1] = *plFmt[0];
        if (plFmt[1]!=NULL) {
            if ( *plFmt[1] != *plFmt[0] ) {
                errln("ERROR:  clone plural format test failed!");
            }
        }
    }
    else {
         dataerrln("ERROR: PluralFormat constructor failed! - [0]%s [1]%s", u_errorName(status[0]), u_errorName(status[1]));
    }
    delete plFmt[0];

    status[0] = U_ZERO_ERROR;
    plFmt[0]= new PluralFormat(locale, status[0]);
    if ( U_SUCCESS(status[0]) ) {
        *plFmt[1] = *plFmt[0];
        if (plFmt[1]!=NULL) {
            if ( *plFmt[1] != *plFmt[0] ) {
                errln("ERROR:  assignment operator test failed!");
            }
        }
    }
    else {
         dataerrln("ERROR: PluralFormat constructor failed! - %s", u_errorName(status[1]));
    }

    if ( U_SUCCESS(status[1]) ) {
        plFmt[2] = (PluralFormat*) plFmt[1]->clone();

        if (plFmt[1]!=NULL) {
            if ( *plFmt[1] != *plFmt[2] ) {
                errln("ERROR:  clone function test failed!");
            }
        }
        delete plFmt[1];
        delete plFmt[2];
    }
    else {
         dataerrln("ERROR: PluralFormat clone failed! - %s", u_errorName(status[1]));
    }

    delete plFmt[0];
    delete numFmt;
    delete plRules;

    // Tests parseObject
    UErrorCode stat = U_ZERO_ERROR;
    PluralFormat *pf = new PluralFormat(stat);
    Formattable *f = new Formattable();
    ParsePosition *pp = new ParsePosition();
    pf->parseObject((UnicodeString)"",*f,*pp);
    if(U_FAILURE(stat)) {
        dataerrln("ERROR: PluralFormat::parseObject: %s", u_errorName(stat));
    }
    delete pf;
    delete f;
    delete pp;
}

/**
 * Unit tests of PluralFormat class.
 */
void PluralFormatTest::pluralFormatUnitTest(/*char *par*/)
{
    UnicodeString patternTestData[PLURAL_PATTERN_DATA] = {
        UNICODE_STRING_SIMPLE("odd {# is odd.} other{# is even.}"),
        UNICODE_STRING_SIMPLE("other{# is odd or even.}"),
        UNICODE_STRING_SIMPLE("odd{The number {0, number, #.#0} is odd.}other{The number {0, number, #.#0} is even.}"),
        UNICODE_STRING_SIMPLE("odd{The number {1, number, #} is odd.}other{The number {2, number, #} is even.}"),
    };
    UnicodeString patternOddTestResult[PLURAL_PATTERN_DATA] = {
        UNICODE_STRING_SIMPLE(" is odd."),
        UNICODE_STRING_SIMPLE(" is odd or even."),
        UNICODE_STRING_SIMPLE("The number {0, number, #.#0} is odd."),
        UNICODE_STRING_SIMPLE("The number {1, number, #} is odd."),
    };
    UnicodeString patternEvenTestResult[PLURAL_PATTERN_DATA] = {
        UNICODE_STRING_SIMPLE(" is even."),
        UNICODE_STRING_SIMPLE(" is odd or even."),
        UNICODE_STRING_SIMPLE("The number {0, number, #.#0} is even."),
        UNICODE_STRING_SIMPLE("The number {2, number, #} is even."),
    };
    UnicodeString checkSyntaxtData[PLURAL_SYNTAX_DATA] = {
        // ICU 4.8 does not check for duplicate keywords any more.
        //UNICODE_STRING_SIMPLE("odd{foo} odd{bar} other{foobar}"),
        //UNICODE_STRING_SIMPLE("odd{foo} other{bar} other{foobar}"),
        UNICODE_STRING_SIMPLE("odd{foo}"),
        // ICU 4.8 does not check for unknown keywords any more.
        //UNICODE_STRING_SIMPLE("otto{foo} other{bar}"),
        UNICODE_STRING_SIMPLE("*odd{foo} other{bar}"),
        UNICODE_STRING_SIMPLE("odd{foo},other{bar}"),
        UNICODE_STRING_SIMPLE("od d{foo} other{bar}"),
        UNICODE_STRING_SIMPLE("odd{foo}{foobar}other{foo}"),
    };

    UErrorCode status = U_ZERO_ERROR;
    UnicodeString oddAndEvenRule = UNICODE_STRING_SIMPLE("odd: n mod 2 is 1");
    PluralRules*  plRules = PluralRules::createRules(oddAndEvenRule, status);
    if (U_FAILURE(status)) {
        dataerrln("ERROR:  create PluralRules instance failed in unit tests.- exitting");
        return;
    }
    
    // ======= Test PluralRules pattern syntax.
    logln("Testing PluralRules pattern syntax.");
    for (int32_t i=0; i<PLURAL_SYNTAX_DATA; ++i) {
        status = U_ZERO_ERROR;
        
        PluralFormat plFmt=PluralFormat(*plRules, status);
        if (U_FAILURE(status)) {
            dataerrln("ERROR:  PluralFormat constructor failed in unit tests.- exitting");
            return;
        }
        plFmt.applyPattern(checkSyntaxtData[i], status);
        if (U_SUCCESS(status)) {
            errln("ERROR:  PluralFormat failed to detect syntax error with pattern: "+checkSyntaxtData[i]);
        }
    }
    


    // ======= Test applying various pattern
    logln("Testing various patterns");
    status = U_ZERO_ERROR;
    UBool overwrite[PLURAL_PATTERN_DATA] = {FALSE, FALSE, TRUE, TRUE};
    
    NumberFormat *numFmt = NumberFormat::createInstance(status);
    UnicodeString message=UnicodeString("ERROR: PluralFormat tests various pattern ...");
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not create NumberFormat instance with default locale ");
    }
    for(int32_t i=0; i<PLURAL_PATTERN_DATA; ++i) {
        status = U_ZERO_ERROR;
        PluralFormat plFmt=PluralFormat(*plRules, status);
        if (U_FAILURE(status)) {
            dataerrln("ERROR:  PluralFormat constructor failed in unit tests.- exitting");
            return;
        }
        plFmt.applyPattern(patternTestData[i], status);
        if (U_FAILURE(status)) {
            errln("ERROR:  PluralFormat failed to apply pattern- "+patternTestData[i]);
            continue;
        }
        numberFormatTest(&plFmt, numFmt, 1, 10, (UnicodeString *)&patternOddTestResult[i], 
                         (UnicodeString *)&patternEvenTestResult[i], overwrite[i], &message);
    }
    delete plRules;
    delete numFmt;
    
    // ======= Test set locale
    status = U_ZERO_ERROR;
    plRules = PluralRules::createRules(UNICODE_STRING_SIMPLE("odd: n mod 2 is 1"), status);  
    PluralFormat pluralFmt = PluralFormat(*plRules, status);
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not create PluralFormat instance in setLocale() test - exitting. ");
        delete plRules;
        return;
    }
    pluralFmt.applyPattern(UNICODE_STRING_SIMPLE("odd{odd} other{even}"), status);
    pluralFmt.setLocale(Locale::getEnglish(), status);
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not setLocale() with English locale ");
        delete plRules;
        return;
    }
    message = UNICODE_STRING_SIMPLE("Error set locale: pattern is not reset!");
    
    // Check that pattern gets deleted.
    logln("\n Test setLocale() ..\n");
    numFmt = NumberFormat::createInstance(Locale::getEnglish(), status);
    if (U_FAILURE(status)) {
        dataerrln("ERROR: Could not create NumberFormat instance with English locale ");
    }
    numberFormatTest(&pluralFmt, numFmt, 5, 5, NULL, NULL, FALSE, &message);
    pluralFmt.applyPattern(UNICODE_STRING_SIMPLE("odd__{odd} other{even}"), status);
    if (pluralFmt.format((int32_t)1, status) != UNICODE_STRING_SIMPLE("even")) {
        errln("SetLocale should reset rules but did not.");
    }
    status = U_ZERO_ERROR;
    pluralFmt.applyPattern(UNICODE_STRING_SIMPLE("one{one} other{not one}"), status);
    if (U_FAILURE(status)) {
        errln("SetLocale should reset rules but did not.");
    }
    UnicodeString one = UNICODE_STRING_SIMPLE("one");
    UnicodeString notOne = UNICODE_STRING_SIMPLE("not one");
    UnicodeString plResult, numResult;
    for (int32_t i=0; i<20; ++i) {
        plResult = pluralFmt.format(i, status);
        if ( i==1 ) {
            numResult = one;
        }
        else {
            numResult = notOne;
        }
        if ( numResult != plResult ) {
            errln("Wrong ruleset loaded by setLocale() - got:"+plResult+ UnicodeString("  expecting:")+numResult);
        }
    }
    
    // =========== Test copy constructor
    logln("Test copy constructor and == operator of PluralFormat");
    PluralFormat dupPFmt = PluralFormat(pluralFmt);
    if (pluralFmt != dupPFmt) {
        errln("Failed in PluralFormat copy constructor or == operator");
    }
    
    delete plRules;
    delete numFmt;
}



/**
 * Test locale data used in PluralFormat class.
 */
void 
PluralFormatTest::pluralFormatLocaleTest(/*char *par*/)
{
    int8_t pluralResults[PLURAL_TEST_ARRAY_SIZE];  // 0: is for default

    // ======= Test DefaultRule
    logln("Testing PluralRules with no rule.");
    // for CLDR 24, here delete tr,
    // add id lo ms th zh
    const char* oneRuleLocales[8] = {"id", "ja", "ko", "lo", "ms", "th", "vi", "zh"};
    UnicodeString testPattern = UNICODE_STRING_SIMPLE("other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_OTHER; // other
    helperTestResults(oneRuleLocales, 8, testPattern, pluralResults);
    
    // ====== Test Singular1 locales.
    logln("Testing singular1 locales.");
    // for CLDR 24, here delete da de en et fi gl he it nl pt pt sv bn ca gu is mr pa sw ur zu
    // add hu tr others
    const char* singular1Locales[56] = {"af","asa","az","bem","bez","bg","brx","chr",
                    "ckb","dv","ee","el","eo","es","eu","fo","fur","fy","gsw","ha",
                    "haw","hu","jgo","ka","kk","kl","ks","ku","lb","ml","mn","nah",
                    "nb","ne","nn","no","nr","om","or","pap","ps","rm","rof","sn",
                    "so", "sq","ta","te","tk","tn","tr","ts","vo","wae","xh","xog"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_OTHER;
    pluralResults[1]= PFT_ONE;
    pluralResults[2]= PFT_OTHER;
    helperTestResults(singular1Locales, 56, testPattern, pluralResults);
    
    // ======== Test Singular01 locales.
    logln("Testing singular1 locales.");
    // for CLDR 24, here add hy
    const char* singular01Locales[4] = {"ff","fr","hy","kab"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_ONE;
    pluralResults[2]= PFT_OTHER;
    helperTestResults(singular01Locales, 4, testPattern, pluralResults);
    
    // ======== Test ZeroSingular locales.
    logln("Testing singular1 locales.");
    const char* zeroSingularLocales[1] = {"lv"};
    testPattern = UNICODE_STRING_SIMPLE("zero{zero} one{one} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_ZERO;
    pluralResults[1]= PFT_ONE;
    for (int32_t i=2; i<20; ++i) {
        pluralResults[i]= (i < 10)? PFT_OTHER: PFT_ZERO;
        pluralResults[i*10] = PFT_ZERO;
        pluralResults[i*10+1] = PFT_ONE; // note override after loop
        pluralResults[i*10+2] = PFT_OTHER; // note override after loop
    }
    pluralResults[111]= PFT_ZERO;
    pluralResults[112]= PFT_ZERO;
    helperTestResults(zeroSingularLocales, 1, testPattern, pluralResults);
    
    // ======== Test singular dual locales.
    logln("Testing singular1 locales.");
    const char* singularDualLocales[1] = {"ga"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} two{two} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_OTHER;
    pluralResults[1]= PFT_ONE;
    pluralResults[2]= PFT_TWO;
    pluralResults[3]= PFT_OTHER;
    helperTestResults(singularDualLocales, 1, testPattern, pluralResults);
    
    // ======== Test Singular Zero Some locales.
    logln("Testing singular1 locales.");
    const char* singularZeroSomeLocales[1] = {"ro"};
    testPattern = UNICODE_STRING_SIMPLE("few{few} one{one} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_FEW;
    for (int32_t i=1; i<20; ++i) {
        pluralResults[i] = PFT_FEW; // note override after loop
        pluralResults[100+i] = PFT_FEW;
    }
    pluralResults[1]= PFT_ONE;
    helperTestResults(singularZeroSomeLocales, 1, testPattern, pluralResults);
    
    // ======== Test Special 12/19.
    logln("Testing special 12 and 19.");
    const char* special12_19Locales[1] = {"lt"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} few{few} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_OTHER;
    pluralResults[1]= PFT_ONE;
    for (int32_t i=2; i<20; ++i) {
        pluralResults[i]= (i < 10)? PFT_FEW: PFT_OTHER;
        pluralResults[i*10] = PFT_OTHER;
        if (i==11)  continue;
        pluralResults[i*10+1] = PFT_ONE;
        pluralResults[i*10+2] = PFT_FEW;
    }
    helperTestResults(special12_19Locales, 1, testPattern, pluralResults);
    
    // ======== Test Paucal Except 11 14.
    logln("Testing Paucal Except 11 and 14, set A.");
    const char* paucal01LocalesA[2] = {"hr","sr"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} few{few} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_OTHER;
    pluralResults[1]= PFT_ONE;
    for (int32_t i=2; i<20; ++i) {
        pluralResults[i]= (i < 5)? PFT_FEW: PFT_OTHER;
        if (i==11)  continue;
        pluralResults[i*10+1] = PFT_ONE;
        pluralResults[i*10+2] = PFT_FEW;
        pluralResults[i*10+5] = PFT_OTHER;
        pluralResults[i*10+6] = PFT_OTHER;
        pluralResults[i*10+7] = PFT_OTHER;
        pluralResults[i*10+8] = PFT_OTHER;
        pluralResults[i*10+9] = PFT_OTHER;
    }
    helperTestResults(paucal01LocalesA, 2, testPattern, pluralResults);
    
    logln("Testing Paucal Except 11 and 14, set B.");
    const char* paucal01LocalesB[1] = {"ru"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} many{many} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_MANY;
    pluralResults[1]= PFT_ONE;
    for (int32_t i=2; i<20; ++i) {
        pluralResults[i]= (i < 5)? PFT_OTHER: PFT_MANY;
        if (i==11)  continue;
        pluralResults[i*10] = PFT_MANY;
        pluralResults[i*10+1] = PFT_ONE;
        pluralResults[i*10+2] = PFT_OTHER;
        pluralResults[i*10+5] = PFT_MANY;
        pluralResults[i*10+6] = PFT_MANY;
        pluralResults[i*10+7] = PFT_MANY;
        pluralResults[i*10+8] = PFT_MANY;
        pluralResults[i*10+9] = PFT_MANY;
    }
    helperTestResults(paucal01LocalesB, 1, testPattern, pluralResults);
    
    logln("Testing Paucal Except 11 and 14, set C.");
    const char* paucal01LocalesC[1] = {"uk"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} many{many} few{few} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_MANY;
    pluralResults[1]= PFT_ONE;
    for (int32_t i=2; i<20; ++i) {
        pluralResults[i]= (i < 5)? PFT_FEW: PFT_MANY;
        if (i==11)  continue;
        pluralResults[i*10] = PFT_MANY;
        pluralResults[i*10+1] = PFT_ONE;
        pluralResults[i*10+2] = PFT_FEW;
        pluralResults[i*10+5] = PFT_MANY;
        pluralResults[i*10+6] = PFT_MANY;
        pluralResults[i*10+7] = PFT_MANY;
        pluralResults[i*10+8] = PFT_MANY;
        pluralResults[i*10+9] = PFT_MANY;
    }
    helperTestResults(paucal01LocalesC, 1, testPattern, pluralResults);
    
    // ======== Test Singular Paucal.
    logln("Testing Singular Paucal.");
    const char* singularPaucalLocales[2] = {"cs","sk"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} few{few} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_OTHER;
    pluralResults[1]= PFT_ONE;
    pluralResults[2]= PFT_FEW;
    pluralResults[5]= PFT_OTHER;
    helperTestResults(singularPaucalLocales, 2, testPattern, pluralResults);

    // ======== Test Paucal (1), (2,3,4).
    logln("Testing Paucal (1), (2,3,4).");
    const char* paucal02Locales[1] = {"pl"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} many{many} few{few} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    for (int32_t i=0; i<20; ++i) {
        pluralResults[i*10+0] = PFT_MANY;
        pluralResults[i*10+1] = PFT_MANY; // note override after loop
        if ((i==1)||(i==11)) {
            pluralResults[i*10+2] = PFT_MANY;
            pluralResults[i*10+3] = PFT_MANY;
            pluralResults[i*10+4] = PFT_MANY;
        }
        else {
            pluralResults[i*10+2] = PFT_FEW;
            pluralResults[i*10+3] = PFT_FEW;
            pluralResults[i*10+4] = PFT_FEW;
        }
        pluralResults[i*10+5] = PFT_MANY;
    }
    pluralResults[1]= PFT_ONE;
    helperTestResults(paucal02Locales, 1, testPattern, pluralResults);
    
    // ======== Test Paucal (1), (2), (3,4).
    logln("Testing Paucal (1), (2), (3,4).");
    const char* paucal03Locales[1] = {"sl"};
    testPattern = UNICODE_STRING_SIMPLE("one{one} two{two} few{few} other{other}");
    uprv_memset(pluralResults, -1, sizeof(pluralResults));
    pluralResults[0]= PFT_OTHER;
    pluralResults[1]= PFT_ONE;
    pluralResults[2]= PFT_TWO;
    pluralResults[3]= PFT_FEW;
    pluralResults[5]= PFT_OTHER;
    pluralResults[101]= PFT_ONE;
    pluralResults[102]= PFT_TWO;
    pluralResults[103]= PFT_FEW;
    pluralResults[105]= PFT_OTHER;
    helperTestResults(paucal03Locales, 1, testPattern, pluralResults);
    
    // TODO: move this test to Unit Test after CLDR 1.6 is final and we support float
    // ======= Test French "WITHIN rule
    logln("Testing PluralRules with fr rule.");
    testPattern = UNICODE_STRING_SIMPLE("one{one} other{other}");
    Locale ulocale((const char *)"fr");
    UErrorCode status = U_ZERO_ERROR;
    PluralFormat plFmt(ulocale, testPattern, status);
    if (U_FAILURE(status)) {
        dataerrln("Failed to apply pattern to fr locale - %s", u_errorName(status));
    }
    else {
        status = U_ZERO_ERROR;
        UnicodeString plResult = plFmt.format(0.0, status);  // retrun ONE
        plResult = plFmt.format(0.5, status);  // retrun ONE
        plResult = plFmt.format(1.0, status);  // retrun ONE
        plResult = plFmt.format(1.9, status);  // retrun ONE
        plResult = plFmt.format(2.0, status);  // retrun OTHER
    }
}

void
PluralFormatTest::pluralFormatExtendedTest(void) {
  const char *targets[] = {
    "There are no widgets.",
    "There is one widget.",
    "There is a bling widget and one other widget.",
    "There is a bling widget and 2 other widgets.",
    "There is a bling widget and 3 other widgets.",
    "Widgets, five (5-1=4) there be.",
    "There is a bling widget and 5 other widgets.",
    "There is a bling widget and 6 other widgets.",
  };

  const char* fmt =
      "offset:1.0 "
      "=0 {There are no widgets.} "
      "=1.0 {There is one widget.} "
      "=5 {Widgets, five (5-1=#) there be.} "
      "one {There is a bling widget and one other widget.} "
      "other {There is a bling widget and # other widgets.}";

  UErrorCode status = U_ZERO_ERROR;
  UnicodeString fmtString(fmt, -1, US_INV);
  PluralFormat pf(Locale::getEnglish(), fmtString, status);
  MessageFormat mf(UNICODE_STRING_SIMPLE("{0,plural,").append(fmtString).append((UChar)0x7d /* '}' */),
                   Locale::getEnglish(), status);
  Formattable args;
  FieldPosition ignore;
  if (U_FAILURE(status)) {
    dataerrln("Failed to apply pattern - %s", u_errorName(status));
    return;
  }
  for (int32_t i = 0; i <= 7; ++i) {
    UnicodeString result = pf.format(i, status);
    if (U_FAILURE(status)) {
      errln("PluralFormat.format(value %d) failed - %s", i, u_errorName(status));
      return;
    }
    UnicodeString expected(targets[i], -1, US_INV);
    if (expected != result) {
      UnicodeString message("PluralFormat.format(): Expected '", -1, US_INV);
      message.append(expected);
      message.append(UnicodeString("' but got '", -1, US_INV));
      message.append(result);
      message.append("'", -1, US_INV);
      errln(message);
    }
    args.setLong(i);
    mf.format(&args, 1, result.remove(), ignore, status);
    if (U_FAILURE(status)) {
      errln("MessageFormat.format(value %d) failed - %s", i, u_errorName(status));
      return;
    }
    if (expected != result) {
      UnicodeString message("MessageFormat.format(): Expected '", -1, US_INV);
      message.append(expected);
      message.append(UnicodeString("' but got '", -1, US_INV));
      message.append(result);
      message.append("'", -1, US_INV);
      errln(message);
    }
  }
}

void
PluralFormatTest::pluralFormatExtendedParseTest(void) {
  const char *failures[] = {
    "offset:1..0 =0 {Foo}",
    "offset:1.0 {Foo}",
    "=0= {Foo}",
    "=0 {Foo} =0.0 {Bar}",
    " = {Foo}",
  };
  int len = sizeof(failures)/sizeof(failures[0]);

  for (int i = 0; i < len; ++i) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString fmt(failures[i], -1, US_INV);
    PluralFormat pf(fmt, status);
    if (U_SUCCESS(status)) {
      errln("expected failure when parsing '" + fmt + "'");
    }
  }
}

void
PluralFormatTest::ordinalFormatTest(void) {
    IcuTestErrorCode errorCode(*this, "ordinalFormatTest");
    UnicodeString pattern("one{#st file}two{#nd file}few{#rd file}other{#th file}");
    PluralFormat pf(Locale::getEnglish(), UPLURAL_TYPE_ORDINAL, pattern, errorCode);
    if (errorCode.logDataIfFailureAndReset("PluralFormat(en, UPLURAL_TYPE_ORDINAL, pattern) failed")) {
      return;
    }
    UnicodeString result = pf.format((int32_t)321, errorCode);
    if (!errorCode.logIfFailureAndReset("PluralFormat.format(321) failed") &&
        result != UNICODE_STRING_SIMPLE("321st file")) {
      errln(UnicodeString("PluralFormat.format(321) wrong result string: ") + result);
    }
    result = pf.format((int32_t)22, errorCode);
    if (!errorCode.logIfFailureAndReset("PluralFormat.format(22) failed") &&
        result != UNICODE_STRING_SIMPLE("22nd file")) {
      errln(UnicodeString("PluralFormat.format(22) wrong result string: ") + result);
    }
    result = pf.format((int32_t)3, errorCode);
    if (!errorCode.logIfFailureAndReset("PluralFormat.format(3) failed") &&
        result != UNICODE_STRING_SIMPLE("3rd file")) {
      errln(UnicodeString("PluralFormat.format(3) wrong result string: ") + result);
    }

    // Code coverage: Use the other new-for-UPluralType constructor as well.
    PluralFormat pf2(Locale::getEnglish(), UPLURAL_TYPE_ORDINAL, errorCode);
    pf2.applyPattern(pattern, errorCode);
    if (errorCode.logIfFailureAndReset("PluralFormat(en, UPLURAL_TYPE_ORDINAL, pattern) failed")) {
      return;
    }
    result = pf2.format((int32_t)456, errorCode);
    if (!errorCode.logIfFailureAndReset("PluralFormat.format(456) failed") &&
        result != UNICODE_STRING_SIMPLE("456th file")) {
      errln(UnicodeString("PluralFormat.format(456) wrong result string: ") + result);
    }
    result = pf2.format((int32_t)111, errorCode);
    if (!errorCode.logIfFailureAndReset("PluralFormat.format(111) failed") &&
        result != UNICODE_STRING_SIMPLE("111th file")) {
      errln(UnicodeString("PluralFormat.format(111) wrong result string: ") + result);
    }
}

void
PluralFormatTest::TestDecimals() {
    IcuTestErrorCode errorCode(*this, "TestDecimals");
    // Simple number replacement.
    PluralFormat pf(Locale::getEnglish(), "one{one meter}other{# meters}", errorCode);
    assertEquals("simple format(1)", "one meter", pf.format((int32_t)1, errorCode), TRUE);
    assertEquals("simple format(1.5)", "1.5 meters", pf.format(1.5, errorCode), TRUE);
    PluralFormat pf2(Locale::getEnglish(),
            "offset:1 one{another meter}other{another # meters}", errorCode);
    DecimalFormat df("0.0", new DecimalFormatSymbols(Locale::getEnglish(), errorCode), errorCode);
    pf2.setNumberFormat(&df, errorCode);
    assertEquals("offset-decimals format(1)", "another 0.0 meters", pf2.format((int32_t)1, errorCode), TRUE);
    assertEquals("offset-decimals format(2)", "another 1.0 meters", pf2.format((int32_t)2, errorCode), TRUE);
    assertEquals("offset-decimals format(2.5)", "another 1.5 meters", pf2.format(2.5, errorCode), TRUE);
    errorCode.reset();
}

void
PluralFormatTest::numberFormatTest(PluralFormat* plFmt, 
                                   NumberFormat *numFmt,
                                   int32_t start,
                                   int32_t end,
                                   UnicodeString *numOddAppendStr,
                                   UnicodeString *numEvenAppendStr,
                                   UBool overwrite,  // overwrite the numberFormat.format result
                                   UnicodeString *message) {
    UErrorCode status = U_ZERO_ERROR;
    
    if ( (plFmt==NULL) || (numFmt==NULL) ) {
        dataerrln("ERROR: Could not create PluralFormat or NumberFormat - exitting");
        return;
    }
    UnicodeString plResult, numResult ;
    
    for (int32_t i=start; i<= end; ++i ) {
        numResult.remove();
        numResult = numFmt->format(i, numResult);
        plResult = plFmt->format(i, status);
        if ((numOddAppendStr!= NULL)&&(numEvenAppendStr!=NULL)) {
            if (overwrite) {
                if (i&1) {
                    numResult = *numOddAppendStr;
                }
                else {
                    numResult = *numEvenAppendStr;
                }
            }
            else {  // Append the string
                if (i&1) {
                    numResult += *numOddAppendStr;
                }
                else{
                    numResult += *numEvenAppendStr;
                }
            }
        }
        if ( (numResult!=plResult) || U_FAILURE(status) ) {
            if ( message == NULL ) {
                errln("ERROR: Unexpected plural format - got:"+plResult+ UnicodeString("  expecting:")+numResult);
            }
            else {
                errln( *message+UnicodeString("  got:")+plResult+UnicodeString("  expecting:")+numResult);
            }
        }
    }
    return;
}


void
PluralFormatTest::helperTestResults(const char** localeArray, 
                                    int32_t capacityOfArray, 
                                    UnicodeString& testPattern,
                                    int8_t *expResults) {
    UErrorCode status;
    UnicodeString plResult;
    const UnicodeString PLKeywordLookups[6] = {
        UNICODE_STRING_SIMPLE("zero"),
        UNICODE_STRING_SIMPLE("one"),
        UNICODE_STRING_SIMPLE("two"),
        UNICODE_STRING_SIMPLE("few"),
        UNICODE_STRING_SIMPLE("many"),
        UNICODE_STRING_SIMPLE("other"),
    };
    
    for (int32_t i=0; i<capacityOfArray; ++i) {
        const char *locale = localeArray[i];
        Locale ulocale((const char *)locale);
        status = U_ZERO_ERROR;
        PluralFormat plFmt(ulocale, testPattern, status);
        if (U_FAILURE(status)) {
            dataerrln("Failed to apply pattern to locale:"+UnicodeString(localeArray[i]) + " - " + u_errorName(status));
            continue;
        }
        for (int32_t n=0; n<PLURAL_TEST_ARRAY_SIZE; ++n) {
            if (expResults[n]!=-1) {
                status = U_ZERO_ERROR;
                plResult = plFmt.format(n, status);
                if (U_FAILURE(status)) {
                    errln("ERROR: Failed to format number in locale data tests with locale: "+
                           UnicodeString(localeArray[i]));
                }
                if (plResult != PLKeywordLookups[expResults[n]]){
                    plResult = plFmt.format(n, status);
                    errln("ERROR: Unexpected format result in locale: "+UnicodeString(localeArray[i])+
                          UnicodeString(" for value: ")+n+
                          UnicodeString("  got:")+plResult+
                          UnicodeString("  expecting:")+ PLKeywordLookups[expResults[n]]);
                }
            }
        }
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
