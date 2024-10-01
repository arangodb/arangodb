// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 1996-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 *******************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "itrbnf.h"

#include "unicode/umachine.h"

#include "unicode/tblcoll.h"
#include "unicode/coleitr.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "unicode/decimfmt.h"
#include "unicode/udata.h"
#include "cmemory.h"
#include "putilimp.h"
#include "testutil.h"

#include <string.h>

// import com.ibm.text.RuleBasedNumberFormat;
// import com.ibm.test.TestFmwk;

// import java.util.Locale;
// import java.text.NumberFormat;

// current macro not in icu1.8.1
#define TESTCASE(id,test)             \
    case id:                          \
        name = #test;                 \
        if (exec) {                   \
            logln(#test "---");       \
            logln();                  \
            test();                   \
        }                             \
        break

void IntlTestRBNF::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/)
{
    if (exec) logln("TestSuite RuleBasedNumberFormat");
    switch (index) {
#if U_HAVE_RBNF
        TESTCASE(0, TestEnglishSpellout);
        TESTCASE(1, TestOrdinalAbbreviations);
        TESTCASE(2, TestDurations);
        TESTCASE(3, TestSpanishSpellout);
        TESTCASE(4, TestFrenchSpellout);
        TESTCASE(5, TestSwissFrenchSpellout);
        TESTCASE(6, TestItalianSpellout);
        TESTCASE(7, TestGermanSpellout);
        TESTCASE(8, TestThaiSpellout);
        TESTCASE(9, TestAPI);
        TESTCASE(10, TestFractionalRuleSet);
        TESTCASE(11, TestSwedishSpellout);
        TESTCASE(12, TestBelgianFrenchSpellout);
        TESTCASE(13, TestSmallValues);
        TESTCASE(14, TestLocalizations);
        TESTCASE(15, TestAllLocales);
        TESTCASE(16, TestHebrewFraction);
        TESTCASE(17, TestPortugueseSpellout);
        TESTCASE(18, TestMultiplierSubstitution);
        TESTCASE(19, TestSetDecimalFormatSymbols);
        TESTCASE(20, TestPluralRules);
        TESTCASE(21, TestMultiplePluralRules);
        TESTCASE(22, TestInfinityNaN);
        TESTCASE(23, TestVariableDecimalPoint);
        TESTCASE(24, TestLargeNumbers);
        TESTCASE(25, TestCompactDecimalFormatStyle);
        TESTCASE(26, TestParseFailure);
        TESTCASE(27, TestMinMaxIntegerDigitsIgnored);
#else
        TESTCASE(0, TestRBNFDisabled);
#endif
    default:
        name = "";
        break;
    }
}

#if U_HAVE_RBNF

void IntlTestRBNF::TestHebrewFraction() {

    // this is the expected output for 123.45, with no '<' in it.
    UChar text1[] = { 
        0x05de, 0x05d0, 0x05d4, 0x0020, 
        0x05e2, 0x05e9, 0x05e8, 0x05d9, 0x05dd, 0x0020,
        0x05d5, 0x05e9, 0x05dc, 0x05d5, 0x05e9, 0x0020, 
        0x05e0, 0x05e7, 0x05d5, 0x05d3, 0x05d4, 0x0020,
        0x05d0, 0x05e8, 0x05d1, 0x05e2, 0x0020,
        0x05d7, 0x05de, 0x05e9, 0x0000,
    };
    UChar text2[] = { 
        0x05DE, 0x05D0, 0x05D4, 0x0020, 
        0x05E2, 0x05E9, 0x05E8, 0x05D9, 0x05DD, 0x0020, 
        0x05D5, 0x05E9, 0x05DC, 0x05D5, 0x05E9, 0x0020, 
        0x05E0, 0x05E7, 0x05D5, 0x05D3, 0x05D4, 0x0020, 
        0x05D0, 0x05E4, 0x05E1, 0x0020, 
        0x05D0, 0x05E4, 0x05E1, 0x0020, 
        0x05D0, 0x05E8, 0x05D1, 0x05E2, 0x0020, 
        0x05D7, 0x05DE, 0x05E9, 0x0000,
    };
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter = new RuleBasedNumberFormat(URBNF_SPELLOUT, "he_IL", status);
    if (status == U_MISSING_RESOURCE_ERROR || status == U_FILE_ACCESS_ERROR) {
        errcheckln(status, "Failed in constructing RuleBasedNumberFormat - %s", u_errorName(status));
        delete formatter;
        return;
    }
    UnicodeString result;
    Formattable parseResult;
    ParsePosition pp(0);
    {
        UnicodeString expected(text1);
        formatter->format(123.45, result);
        if (result != expected) {
            errln((UnicodeString)"expected '" + TestUtility::hex(expected) + "'\nbut got: '" + TestUtility::hex(result) + "'");
        } else {
//            formatter->parse(result, parseResult, pp);
//            if (parseResult.getDouble() != 123.45) {
//                errln("expected 123.45 but got: %g", parseResult.getDouble());
//            }
        }
    }
    {
        UnicodeString expected(text2);
        result.remove();
        formatter->format(123.0045, result);
        if (result != expected) {
            errln((UnicodeString)"expected '" + TestUtility::hex(expected) + "'\nbut got: '" + TestUtility::hex(result) + "'");
        } else {
            pp.setIndex(0);
//            formatter->parse(result, parseResult, pp);
//            if (parseResult.getDouble() != 123.0045) {
//                errln("expected 123.0045 but got: %g", parseResult.getDouble());
//            }
        }
    }
    delete formatter;
}

void 
IntlTestRBNF::TestAPI() {
  // This test goes through the APIs that were not tested before. 
  // These tests are too small to have separate test classes/functions

  UErrorCode status = U_ZERO_ERROR;
  RuleBasedNumberFormat* formatter
      = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale::getUS(), status);
  if (status == U_MISSING_RESOURCE_ERROR || status == U_FILE_ACCESS_ERROR) {
    dataerrln("Unable to create formatter. - %s", u_errorName(status));
    delete formatter;
    return;
  }

  logln("RBNF API test starting");
  // test clone
  {
    logln("Testing Clone");
    RuleBasedNumberFormat* rbnfClone = (RuleBasedNumberFormat *)formatter->clone();
    if(rbnfClone != NULL) {
      if(!(*rbnfClone == *formatter)) {
        errln("Clone should be semantically equivalent to the original!");
      }
      delete rbnfClone;
    } else {
      errln("Cloning failed!");
    }
  }

  // test assignment
  {
    logln("Testing assignment operator");
    RuleBasedNumberFormat assignResult(URBNF_SPELLOUT, Locale("es", "ES", ""), status);
    assignResult = *formatter;
    if(!(assignResult == *formatter)) {
      errln("Assignment result should be semantically equivalent to the original!");
    }
  }

  // test rule constructor
  {
    logln("Testing rule constructor");
    LocalUResourceBundlePointer en(ures_open(U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "rbnf", "en", &status));
    if(U_FAILURE(status)) {
      errln("Unable to access resource bundle with data!");
    } else {
      int32_t ruleLen = 0;
      int32_t len = 0;
      LocalUResourceBundlePointer rbnfRules(ures_getByKey(en.getAlias(), "RBNFRules", NULL, &status));
      LocalUResourceBundlePointer ruleSets(ures_getByKey(rbnfRules.getAlias(), "SpelloutRules", NULL, &status));
      UnicodeString desc;
      while (ures_hasNext(ruleSets.getAlias())) {
           const UChar* currentString = ures_getNextString(ruleSets.getAlias(), &len, NULL, &status);
           ruleLen += len;
           desc.append(currentString);
      }

      const UChar *spelloutRules = desc.getTerminatedBuffer();

      if(U_FAILURE(status) || ruleLen == 0 || spelloutRules == NULL) {
        errln("Unable to access the rules string!");
      } else {
        UParseError perror;
        RuleBasedNumberFormat ruleCtorResult(spelloutRules, Locale::getUS(), perror, status);
        if(!(ruleCtorResult == *formatter)) {
          errln("Formatter constructed from the original rules should be semantically equivalent to the original!");
        }
        
        // Jitterbug 4452, for coverage
        RuleBasedNumberFormat nf(spelloutRules, (UnicodeString)"", Locale::getUS(), perror, status);
        if(!(nf == *formatter)) {
          errln("Formatter constructed from the original rules should be semantically equivalent to the original!");
        }
      }
    }
  }

  // test getRules
  {
    logln("Testing getRules function");
    UnicodeString rules = formatter->getRules();
    UParseError perror;
    RuleBasedNumberFormat fromRulesResult(rules, Locale::getUS(), perror, status);

    if(!(fromRulesResult == *formatter)) {
      errln("Formatter constructed from rules obtained by getRules should be semantically equivalent to the original!");
    }
  }


  {
    logln("Testing copy constructor");
    RuleBasedNumberFormat copyCtorResult(*formatter);
    if(!(copyCtorResult == *formatter)) {
      errln("Copy constructor result result should be semantically equivalent to the original!");
    }
  }

#if !UCONFIG_NO_COLLATION
  // test ruleset names
  {
    logln("Testing getNumberOfRuleSetNames, getRuleSetName and format using rule set names");
    int32_t noOfRuleSetNames = formatter->getNumberOfRuleSetNames();
    if(noOfRuleSetNames == 0) {
      errln("Number of rule set names should be more than zero");
    }
    UnicodeString ruleSetName;
    int32_t i = 0;
    int32_t intFormatNum = 34567;
    double doubleFormatNum = 893411.234;
    logln("number of rule set names is %i", noOfRuleSetNames);
    for(i = 0; i < noOfRuleSetNames; i++) {
      FieldPosition pos1, pos2;
      UnicodeString intFormatResult, doubleFormatResult; 
      Formattable intParseResult, doubleParseResult;

      ruleSetName = formatter->getRuleSetName(i);
      log("Rule set name %i is ", i);
      log(ruleSetName);
      logln(". Format results are: ");
      intFormatResult = formatter->format(intFormatNum, ruleSetName, intFormatResult, pos1, status);
      doubleFormatResult = formatter->format(doubleFormatNum, ruleSetName, doubleFormatResult, pos2, status);
      if(U_FAILURE(status)) {
        errln("Format using a rule set failed");
        break;
      }
      logln(intFormatResult);
      logln(doubleFormatResult);
      formatter->setLenient(TRUE);
      formatter->parse(intFormatResult, intParseResult, status);
      formatter->parse(doubleFormatResult, doubleParseResult, status);

      logln("Parse results for lenient = TRUE, %i, %f", intParseResult.getLong(), doubleParseResult.getDouble());

      formatter->setLenient(FALSE);
      formatter->parse(intFormatResult, intParseResult, status);
      formatter->parse(doubleFormatResult, doubleParseResult, status);

      logln("Parse results for lenient = FALSE, %i, %f", intParseResult.getLong(), doubleParseResult.getDouble());

      if(U_FAILURE(status)) {
        errln("Error during parsing");
      }

      intFormatResult = formatter->format(intFormatNum, "BLABLA", intFormatResult, pos1, status);
      if(U_SUCCESS(status)) {
        errln("Using invalid rule set name should have failed");
        break;
      }
      status = U_ZERO_ERROR;
      doubleFormatResult = formatter->format(doubleFormatNum, "TRUC", doubleFormatResult, pos2, status);
      if(U_SUCCESS(status)) {
        errln("Using invalid rule set name should have failed");
        break;
      }
      status = U_ZERO_ERROR;
    }   
    status = U_ZERO_ERROR;
  }
#endif

  // test API
  UnicodeString expected("four point five","");
  logln("Testing format(double)");
  UnicodeString result;
  formatter->format(4.5,result);
  if(result != expected) {
      errln("Formatted 4.5, expected " + expected + " got " + result);
  } else {
      logln("Formatted 4.5, expected " + expected + " got " + result);
  }
  result.remove();
  expected = "four";
  formatter->format((int32_t)4,result);
  if(result != expected) {
      errln("Formatted 4, expected " + expected + " got " + result);
  } else {
      logln("Formatted 4, expected " + expected + " got " + result);
  }

  result.remove();
  FieldPosition pos;
  formatter->format((int64_t)4, result, pos, status = U_ZERO_ERROR);
  if(result != expected) {
      errln("Formatted 4 int64_t, expected " + expected + " got " + result);
  } else {
      logln("Formatted 4 int64_t, expected " + expected + " got " + result);
  }

  //Jitterbug 4452, for coverage
  result.remove();
  FieldPosition pos2;
  formatter->format((int64_t)4, formatter->getRuleSetName(0), result, pos2, status = U_ZERO_ERROR);
  if(result != expected) {
      errln("Formatted 4 int64_t, expected " + expected + " got " + result);
  } else {
      logln("Formatted 4 int64_t, expected " + expected + " got " + result);
  }

  // clean up
  logln("Cleaning up");
  delete formatter;
}

/**
 * Perform a simple spot check on the parsing going into an infinite loop for alternate rules.
 */
void IntlTestRBNF::TestMultiplePluralRules() {
    // This is trying to model the feminine form, but don't worry about the details too much.
    // We're trying to test the plural rules where there are different prefixes.
    UnicodeString rules("%spellout-cardinal-feminine-genitive:"
                "0: zero;"
                "1: ono;"
                "2: two;"
                "1000: << $(cardinal,one{thousand}few{thousanF}other{thousanO})$[ >>];"
                "%spellout-cardinal-feminine:"
                "x.x: [<< $(cardinal,one{singleton}other{plurality})$ ]>%%fractions>;"
                "0: zero;"
                "1: one;"
                "2: two;"
                "1000: << $(cardinal,one{thousand}few{thousanF}other{thousanO})$[ >>];"
                "%%fractions:"
                "10: <%spellout-cardinal-feminine< $(cardinal,one{oneth}other{tenth})$;"
                "100: <%spellout-cardinal-feminine< $(cardinal,one{1hundredth}other{hundredth})$;");
    UErrorCode status = U_ZERO_ERROR;
    UParseError pError;
    RuleBasedNumberFormat formatter(rules, Locale("ru"), pError, status);
    Formattable result;
    UnicodeString resultStr;
    FieldPosition pos;

    if (U_FAILURE(status)) {
        dataerrln("Unable to create formatter - %s", u_errorName(status));
        return;
    }

    formatter.parse(formatter.format(1000.0, resultStr, pos, status), result, status);
    if (1000 != result.getLong() || resultStr != UNICODE_STRING_SIMPLE("one thousand")) {
        errln("RuleBasedNumberFormat did not return the correct value. Got: %d", result.getLong());
        errln(resultStr);
    }
    resultStr.remove();
    formatter.parse(formatter.format(1000.0, UnicodeString("%spellout-cardinal-feminine-genitive"), resultStr, pos, status), result, status);
    if (1000 != result.getLong() || resultStr != UNICODE_STRING_SIMPLE("ono thousand")) {
        errln("RuleBasedNumberFormat(cardinal-feminine-genitive) did not return the correct value. Got: %d", result.getLong());
        errln(resultStr);
    }
    resultStr.remove();
    formatter.parse(formatter.format(1000.0, UnicodeString("%spellout-cardinal-feminine"), resultStr, pos, status), result, status);
    if (1000 != result.getLong() || resultStr != UNICODE_STRING_SIMPLE("one thousand")) {
        errln("RuleBasedNumberFormat(spellout-cardinal-feminine) did not return the correct value. Got: %d", result.getLong());
        errln(resultStr);
    }
    static const char* const testData[][2] = {
        { "0", "zero" },
        { "1", "one" },
        { "2", "two" },
        { "0.1", "one oneth" },
        { "0.2", "two tenth" },
        { "1.1", "one singleton one oneth" },
        { "1.2", "one singleton two tenth" },
        { "2.1", "two plurality one oneth" },
        { "2.2", "two plurality two tenth" },
        { "0.01", "one 1hundredth" },
        { "0.02", "two hundredth" },
        { NULL, NULL }
    };
    doTest(&formatter, testData, TRUE);
}

void IntlTestRBNF::TestFractionalRuleSet()
{
    UnicodeString fracRules(
        "%main:\n"
               // this rule formats the number if it's 1 or more.  It formats
               // the integral part using a DecimalFormat ("#,##0" puts
               // thousands separators in the right places) and the fractional
               // part using %%frac.  If there is no fractional part, it
               // just shows the integral part.
        "    x.0: <#,##0<[ >%%frac>];\n"
               // this rule formats the number if it's between 0 and 1.  It
               // shows only the fractional part (0.5 shows up as "1/2," not
               // "0 1/2")
        "    0.x: >%%frac>;\n"
        // the fraction rule set.  This works the same way as the one in the
        // preceding example: We multiply the fractional part of the number
        // being formatted by each rule's base value and use the rule that
        // produces the result closest to 0 (or the first rule that produces 0).
        // Since we only provide rules for the numbers from 2 to 10, we know
        // we'll get a fraction with a denominator between 2 and 10.
        // "<0<" causes the numerator of the fraction to be formatted
        // using numerals
        "%%frac:\n"
        "    2: 1/2;\n"
        "    3: <0</3;\n"
        "    4: <0</4;\n"
        "    5: <0</5;\n"
        "    6: <0</6;\n"
        "    7: <0</7;\n"
        "    8: <0</8;\n"
        "    9: <0</9;\n"
        "   10: <0</10;\n");

    // mondo hack
    int len = fracRules.length();
    int change = 2;
    for (int i = 0; i < len; ++i) {
        UChar ch = fracRules.charAt(i);
        if (ch == '\n') {
            change = 2; // change ok
        } else if (ch == ':') {
            change = 1; // change, but once we hit a non-space char, don't change
        } else if (ch == ' ') {
            if (change != 0) {
                fracRules.setCharAt(i, (UChar)0x200e);
            }
        } else {
            if (change == 1) {
                change = 0;
            }
        }
    }

    UErrorCode status = U_ZERO_ERROR;
    UParseError perror;
    RuleBasedNumberFormat formatter(fracRules, Locale::getEnglish(), perror, status);
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "0", "0" },
            { ".1", "1/10" },
            { ".11", "1/9" },
            { ".125", "1/8" },
            { ".1428", "1/7" },
            { ".1667", "1/6" },
            { ".2", "1/5" },
            { ".25", "1/4" },
            { ".333", "1/3" },
            { ".5", "1/2" },
            { "1.1", "1 1/10" },
            { "2.11", "2 1/9" },
            { "3.125", "3 1/8" },
            { "4.1428", "4 1/7" },
            { "5.1667", "5 1/6" },
            { "6.2", "6 1/5" },
            { "7.25", "7 1/4" },
            { "8.333", "8 1/3" },
            { "9.5", "9 1/2" },
            { ".2222", "2/9" },
            { ".4444", "4/9" },
            { ".5555", "5/9" },
            { "1.2856", "1 2/7" },
            { NULL, NULL }
        };
        doTest(&formatter, testData, FALSE); // exact values aren't parsable from fractions
    }
}

#if 0
#define LLAssert(a) \
  if (!(a)) errln("FAIL: " #a)

void IntlTestRBNF::TestLLongConstructors()
{
    logln("Testing constructors");

    // constant (shouldn't really be public)
    LLAssert(llong(llong::kD32).asDouble() == llong::kD32);

    // internal constructor (shouldn't really be public)
    LLAssert(llong(0, 1).asDouble() == 1);
    LLAssert(llong(1, 0).asDouble() == llong::kD32);
    LLAssert(llong((uint32_t)-1, (uint32_t)-1).asDouble() == -1);

    // public empty constructor
    LLAssert(llong().asDouble() == 0);
    
    // public int32_t constructor
    LLAssert(llong((int32_t)0).asInt() == (int32_t)0);
    LLAssert(llong((int32_t)1).asInt() == (int32_t)1);
    LLAssert(llong((int32_t)-1).asInt() == (int32_t)-1);
    LLAssert(llong((int32_t)0x7fffffff).asInt() == (int32_t)0x7fffffff);
    LLAssert(llong((int32_t)0xffffffff).asInt() == (int32_t)-1);
    LLAssert(llong((int32_t)0x80000000).asInt() == (int32_t)0x80000000);

    // public int16_t constructor
    LLAssert(llong((int16_t)0).asInt() == (int16_t)0);
    LLAssert(llong((int16_t)1).asInt() == (int16_t)1);
    LLAssert(llong((int16_t)-1).asInt() == (int16_t)-1);
    LLAssert(llong((int16_t)0x7fff).asInt() == (int16_t)0x7fff);
    LLAssert(llong((int16_t)0xffff).asInt() == (int16_t)0xffff);
    LLAssert(llong((int16_t)0x8000).asInt() == (int16_t)0x8000);

    // public int8_t constructor
    LLAssert(llong((int8_t)0).asInt() == (int8_t)0);
    LLAssert(llong((int8_t)1).asInt() == (int8_t)1);
    LLAssert(llong((int8_t)-1).asInt() == (int8_t)-1);
    LLAssert(llong((int8_t)0x7f).asInt() == (int8_t)0x7f);
    LLAssert(llong((int8_t)0xff).asInt() == (int8_t)0xff);
    LLAssert(llong((int8_t)0x80).asInt() == (int8_t)0x80);

    // public uint16_t constructor
    LLAssert(llong((uint16_t)0).asUInt() == (uint16_t)0);
    LLAssert(llong((uint16_t)1).asUInt() == (uint16_t)1);
    LLAssert(llong((uint16_t)-1).asUInt() == (uint16_t)-1);
    LLAssert(llong((uint16_t)0x7fff).asUInt() == (uint16_t)0x7fff);
    LLAssert(llong((uint16_t)0xffff).asUInt() == (uint16_t)0xffff);
    LLAssert(llong((uint16_t)0x8000).asUInt() == (uint16_t)0x8000);

    // public uint32_t constructor
    LLAssert(llong((uint32_t)0).asUInt() == (uint32_t)0);
    LLAssert(llong((uint32_t)1).asUInt() == (uint32_t)1);
    LLAssert(llong((uint32_t)-1).asUInt() == (uint32_t)-1);
    LLAssert(llong((uint32_t)0x7fffffff).asUInt() == (uint32_t)0x7fffffff);
    LLAssert(llong((uint32_t)0xffffffff).asUInt() == (uint32_t)-1);
    LLAssert(llong((uint32_t)0x80000000).asUInt() == (uint32_t)0x80000000);

    // public double constructor
    LLAssert(llong((double)0).asDouble() == (double)0);
    LLAssert(llong((double)1).asDouble() == (double)1);
    LLAssert(llong((double)0x7fffffff).asDouble() == (double)0x7fffffff);
    LLAssert(llong((double)0x80000000).asDouble() == (double)0x80000000);
    LLAssert(llong((double)0x80000001).asDouble() == (double)0x80000001);

    // can't access uprv_maxmantissa, so fake it
    double maxmantissa = (llong((int32_t)1) << 40).asDouble();
    LLAssert(llong(maxmantissa).asDouble() == maxmantissa);
    LLAssert(llong(-maxmantissa).asDouble() == -maxmantissa);

    // copy constructor
    LLAssert(llong(llong(0, 1)).asDouble() == 1);
    LLAssert(llong(llong(1, 0)).asDouble() == llong::kD32);
    LLAssert(llong(llong(-1, (uint32_t)-1)).asDouble() == -1);

    // asInt - test unsigned to signed narrowing conversion
    LLAssert(llong((uint32_t)-1).asInt() == (int32_t)0x7fffffff);
    LLAssert(llong(-1, 0).asInt() == (int32_t)0x80000000);

    // asUInt - test signed to unsigned narrowing conversion
    LLAssert(llong((int32_t)-1).asUInt() == (uint32_t)-1);
    LLAssert(llong((int32_t)0x80000000).asUInt() == (uint32_t)0x80000000);

    // asDouble already tested

}

void IntlTestRBNF::TestLLongSimpleOperators()
{
    logln("Testing simple operators");

    // operator==
    LLAssert(llong() == llong(0, 0));
    LLAssert(llong(1,0) == llong(1, 0));
    LLAssert(llong(0,1) == llong(0, 1));

    // operator!=
    LLAssert(llong(1,0) != llong(1,1));
    LLAssert(llong(0,1) != llong(1,1));
    LLAssert(llong(0xffffffff,0xffffffff) != llong(0x7fffffff, 0xffffffff));

    // unsigned >
    LLAssert(llong((int32_t)-1).ugt(llong(0x7fffffff, 0xffffffff)));

    // unsigned <
    LLAssert(llong(0x7fffffff, 0xffffffff).ult(llong((int32_t)-1)));

    // unsigned >=
    LLAssert(llong((int32_t)-1).uge(llong(0x7fffffff, 0xffffffff)));
    LLAssert(llong((int32_t)-1).uge(llong((int32_t)-1)));

    // unsigned <=
    LLAssert(llong(0x7fffffff, 0xffffffff).ule(llong((int32_t)-1)));
    LLAssert(llong((int32_t)-1).ule(llong((int32_t)-1)));

    // operator>
    LLAssert(llong(1, 1) > llong(1, 0));
    LLAssert(llong(0, 0x80000000) > llong(0, 0x7fffffff));
    LLAssert(llong(0x80000000, 1) > llong(0x80000000, 0));
    LLAssert(llong(1, 0) > llong(0, 0x7fffffff));
    LLAssert(llong(1, 0) > llong(0, 0xffffffff));
    LLAssert(llong(0, 0) > llong(0x80000000, 1));

    // operator<
    LLAssert(llong(1, 0) < llong(1, 1));
    LLAssert(llong(0, 0x7fffffff) < llong(0, 0x80000000));
    LLAssert(llong(0x80000000, 0) < llong(0x80000000, 1));
    LLAssert(llong(0, 0x7fffffff) < llong(1, 0));
    LLAssert(llong(0, 0xffffffff) < llong(1, 0));
    LLAssert(llong(0x80000000, 1) < llong(0, 0));

    // operator>=
    LLAssert(llong(1, 1) >= llong(1, 0));
    LLAssert(llong(0, 0x80000000) >= llong(0, 0x7fffffff));
    LLAssert(llong(0x80000000, 1) >= llong(0x80000000, 0));
    LLAssert(llong(1, 0) >= llong(0, 0x7fffffff));
    LLAssert(llong(1, 0) >= llong(0, 0xffffffff));
    LLAssert(llong(0, 0) >= llong(0x80000000, 1));
    LLAssert(llong() >= llong(0, 0));
    LLAssert(llong(1,0) >= llong(1, 0));
    LLAssert(llong(0,1) >= llong(0, 1));

    // operator<=
    LLAssert(llong(1, 0) <= llong(1, 1));
    LLAssert(llong(0, 0x7fffffff) <= llong(0, 0x80000000));
    LLAssert(llong(0x80000000, 0) <= llong(0x80000000, 1));
    LLAssert(llong(0, 0x7fffffff) <= llong(1, 0));
    LLAssert(llong(0, 0xffffffff) <= llong(1, 0));
    LLAssert(llong(0x80000000, 1) <= llong(0, 0));
    LLAssert(llong() <= llong(0, 0));
    LLAssert(llong(1,0) <= llong(1, 0));
    LLAssert(llong(0,1) <= llong(0, 1));

    // operator==(int32)
    LLAssert(llong() == (int32_t)0);
    LLAssert(llong(0,1) == (int32_t)1);

    // operator!=(int32)
    LLAssert(llong(1,0) != (int32_t)0);
    LLAssert(llong(0,1) != (int32_t)2);
    LLAssert(llong(0,0xffffffff) != (int32_t)-1);

    llong negOne(0xffffffff, 0xffffffff);

    // operator>(int32)
    LLAssert(llong(0, 0x80000000) > (int32_t)0x7fffffff);
    LLAssert(negOne > (int32_t)-2);
    LLAssert(llong(1, 0) > (int32_t)0x7fffffff);
    LLAssert(llong(0, 0) > (int32_t)-1);

    // operator<(int32)
    LLAssert(llong(0, 0x7ffffffe) < (int32_t)0x7fffffff);
    LLAssert(llong(0xffffffff, 0xfffffffe) < (int32_t)-1);

    // operator>=(int32)
    LLAssert(llong(0, 0x80000000) >= (int32_t)0x7fffffff);
    LLAssert(negOne >= (int32_t)-2);
    LLAssert(llong(1, 0) >= (int32_t)0x7fffffff);
    LLAssert(llong(0, 0) >= (int32_t)-1);
    LLAssert(llong() >= (int32_t)0);
    LLAssert(llong(0,1) >= (int32_t)1);

    // operator<=(int32)
    LLAssert(llong(0, 0x7ffffffe) <= (int32_t)0x7fffffff);
    LLAssert(llong(0xffffffff, 0xfffffffe) <= (int32_t)-1);
    LLAssert(llong() <= (int32_t)0);
    LLAssert(llong(0,1) <= (int32_t)1);

    // operator=
    LLAssert((llong(2,3) = llong((uint32_t)-1)).asUInt() == (uint32_t)-1);

    // operator <<=
    LLAssert((llong(1, 1) <<= 0) ==  llong(1, 1));
    LLAssert((llong(1, 1) <<= 31) == llong(0x80000000, 0x80000000));
    LLAssert((llong(1, 1) <<= 32) == llong(1, 0));
    LLAssert((llong(1, 1) <<= 63) == llong(0x80000000, 0));
    LLAssert((llong(1, 1) <<= 64) == llong(1, 1)); // only lower 6 bits are used
    LLAssert((llong(1, 1) <<= -1) == llong(0x80000000, 0)); // only lower 6 bits are used

    // operator <<
    LLAssert((llong((int32_t)1) << 5).asUInt() == 32);

    // operator >>= (sign extended)
    LLAssert((llong(0x7fffa0a0, 0xbcbcdfdf) >>= 16) == llong(0x7fff,0xa0a0bcbc));
    LLAssert((llong(0x8000789a, 0xbcde0000) >>= 16) == llong(0xffff8000,0x789abcde));
    LLAssert((llong(0x80000000, 0) >>= 63) == llong(0xffffffff, 0xffffffff));
    LLAssert((llong(0x80000000, 0) >>= 47) == llong(0xffffffff, 0xffff0000));
    LLAssert((llong(0x80000000, 0x80000000) >> 64) == llong(0x80000000, 0x80000000)); // only lower 6 bits are used
    LLAssert((llong(0x80000000, 0) >>= -1) == llong(0xffffffff, 0xffffffff)); // only lower 6 bits are used

    // operator >> sign extended)
    LLAssert((llong(0x8000789a, 0xbcde0000) >> 16) == llong(0xffff8000,0x789abcde));

    // ushr (right shift without sign extension)
    LLAssert(llong(0x7fffa0a0, 0xbcbcdfdf).ushr(16) == llong(0x7fff,0xa0a0bcbc));
    LLAssert(llong(0x8000789a, 0xbcde0000).ushr(16) == llong(0x00008000,0x789abcde));
    LLAssert(llong(0x80000000, 0).ushr(63) == llong(0, 1));
    LLAssert(llong(0x80000000, 0).ushr(47) == llong(0, 0x10000));
    LLAssert(llong(0x80000000, 0x80000000).ushr(64) == llong(0x80000000, 0x80000000)); // only lower 6 bits are used
    LLAssert(llong(0x80000000, 0).ushr(-1) == llong(0, 1)); // only lower 6 bits are used

    // operator&(llong)
    LLAssert((llong(0x55555555, 0x55555555) & llong(0xaaaaffff, 0xffffaaaa)) == llong(0x00005555, 0x55550000));

    // operator|(llong)
    LLAssert((llong(0x55555555, 0x55555555) | llong(0xaaaaffff, 0xffffaaaa)) == llong(0xffffffff, 0xffffffff));

    // operator^(llong)
    LLAssert((llong(0x55555555, 0x55555555) ^ llong(0xaaaaffff, 0xffffaaaa)) == llong(0xffffaaaa, 0xaaaaffff));

    // operator&(uint32)
    LLAssert((llong(0x55555555, 0x55555555) & (uint32_t)0xffffaaaa) == llong(0, 0x55550000));

    // operator|(uint32)
    LLAssert((llong(0x55555555, 0x55555555) | (uint32_t)0xffffaaaa) == llong(0x55555555, 0xffffffff));

    // operator^(uint32)
    LLAssert((llong(0x55555555, 0x55555555) ^ (uint32_t)0xffffaaaa) == llong(0x55555555, 0xaaaaffff));

    // operator~
    LLAssert(~llong(0x55555555, 0x55555555) == llong(0xaaaaaaaa, 0xaaaaaaaa));

    // operator&=(llong)
    LLAssert((llong(0x55555555, 0x55555555) &= llong(0xaaaaffff, 0xffffaaaa)) == llong(0x00005555, 0x55550000));

    // operator|=(llong)
    LLAssert((llong(0x55555555, 0x55555555) |= llong(0xaaaaffff, 0xffffaaaa)) == llong(0xffffffff, 0xffffffff));

    // operator^=(llong)
    LLAssert((llong(0x55555555, 0x55555555) ^= llong(0xaaaaffff, 0xffffaaaa)) == llong(0xffffaaaa, 0xaaaaffff));

    // operator&=(uint32)
    LLAssert((llong(0x55555555, 0x55555555) &= (uint32_t)0xffffaaaa) == llong(0, 0x55550000));

    // operator|=(uint32)
    LLAssert((llong(0x55555555, 0x55555555) |= (uint32_t)0xffffaaaa) == llong(0x55555555, 0xffffffff));

    // operator^=(uint32)
    LLAssert((llong(0x55555555, 0x55555555) ^= (uint32_t)0xffffaaaa) == llong(0x55555555, 0xaaaaffff));

    // prefix inc
    LLAssert(llong(1, 0) == ++llong(0,0xffffffff));

    // prefix dec
    LLAssert(llong(0,0xffffffff) == --llong(1, 0));

    // postfix inc
    {
        llong n(0, 0xffffffff);
        LLAssert(llong(0, 0xffffffff) == n++);
        LLAssert(llong(1, 0) == n);
    }

    // postfix dec
    {
        llong n(1, 0);
        LLAssert(llong(1, 0) == n--);
        LLAssert(llong(0, 0xffffffff) == n);
    }

    // unary minus
    LLAssert(llong(0, 0) == -llong(0, 0));
    LLAssert(llong(0xffffffff, 0xffffffff) == -llong(0, 1));
    LLAssert(llong(0, 1) == -llong(0xffffffff, 0xffffffff));
    LLAssert(llong(0x7fffffff, 0xffffffff) == -llong(0x80000000, 1));
    LLAssert(llong(0x80000000, 0) == -llong(0x80000000, 0)); // !!! we don't handle overflow

    // operator-=
    { 
        llong n;
        LLAssert((n -= llong(0, 1)) == llong(0xffffffff, 0xffffffff));
        LLAssert(n == llong(0xffffffff, 0xffffffff));

        n = llong(1, 0);
        LLAssert((n -= llong(0, 1)) == llong(0, 0xffffffff));
        LLAssert(n == llong(0, 0xffffffff));
    }

    // operator-
    {
        llong n;
        LLAssert((n - llong(0, 1)) == llong(0xffffffff, 0xffffffff));
        LLAssert(n == llong(0, 0));

        n = llong(1, 0);
        LLAssert((n - llong(0, 1)) == llong(0, 0xffffffff));
        LLAssert(n == llong(1, 0));
    }

    // operator+=
    {
        llong n(0xffffffff, 0xffffffff);
        LLAssert((n += llong(0, 1)) == llong(0, 0));
        LLAssert(n == llong(0, 0));

        n = llong(0, 0xffffffff);
        LLAssert((n += llong(0, 1)) == llong(1, 0));
        LLAssert(n == llong(1, 0));
    }

    // operator+
    {
        llong n(0xffffffff, 0xffffffff);
        LLAssert((n + llong(0, 1)) == llong(0, 0));
        LLAssert(n == llong(0xffffffff, 0xffffffff));

        n = llong(0, 0xffffffff);
        LLAssert((n + llong(0, 1)) == llong(1, 0));
        LLAssert(n == llong(0, 0xffffffff));
    }

}

void IntlTestRBNF::TestLLong()
{
    logln("Starting TestLLong");

    TestLLongConstructors();

    TestLLongSimpleOperators();

    logln("Testing operator*=, operator*");

    // operator*=, operator*
    // small and large values, positive, &NEGative, zero
    // also test commutivity
    {
        const llong ZERO;
        const llong ONE(0, 1);
        const llong NEG_ONE((int32_t)-1);
        const llong THREE(0, 3);
        const llong NEG_THREE((int32_t)-3);
        const llong TWO_TO_16(0, 0x10000);
        const llong NEG_TWO_TO_16 = -TWO_TO_16;
        const llong TWO_TO_32(1, 0);
        const llong NEG_TWO_TO_32 = -TWO_TO_32;

        const llong NINE(0, 9);
        const llong NEG_NINE = -NINE;

        const llong TWO_TO_16X3(0, 0x00030000);
        const llong NEG_TWO_TO_16X3 = -TWO_TO_16X3;

        const llong TWO_TO_32X3(3, 0);
        const llong NEG_TWO_TO_32X3 = -TWO_TO_32X3;

        const llong TWO_TO_48(0x10000, 0);
        const llong NEG_TWO_TO_48 = -TWO_TO_48;

        const int32_t VALUE_WIDTH = 9;
        const llong* values[VALUE_WIDTH] = {
            &ZERO, &ONE, &NEG_ONE, &THREE, &NEG_THREE, &TWO_TO_16, &NEG_TWO_TO_16, &TWO_TO_32, &NEG_TWO_TO_32
        };

        const llong* answers[VALUE_WIDTH*VALUE_WIDTH] = {
            &ZERO, &ZERO, &ZERO, &ZERO, &ZERO, &ZERO, &ZERO, &ZERO, &ZERO,
            &ZERO, &ONE,  &NEG_ONE, &THREE, &NEG_THREE,  &TWO_TO_16, &NEG_TWO_TO_16, &TWO_TO_32, &NEG_TWO_TO_32,
            &ZERO, &NEG_ONE, &ONE, &NEG_THREE, &THREE, &NEG_TWO_TO_16, &TWO_TO_16, &NEG_TWO_TO_32, &TWO_TO_32,
            &ZERO, &THREE, &NEG_THREE, &NINE, &NEG_NINE, &TWO_TO_16X3, &NEG_TWO_TO_16X3, &TWO_TO_32X3, &NEG_TWO_TO_32X3,
            &ZERO, &NEG_THREE, &THREE, &NEG_NINE, &NINE, &NEG_TWO_TO_16X3, &TWO_TO_16X3, &NEG_TWO_TO_32X3, &TWO_TO_32X3,
            &ZERO, &TWO_TO_16, &NEG_TWO_TO_16, &TWO_TO_16X3, &NEG_TWO_TO_16X3, &TWO_TO_32, &NEG_TWO_TO_32, &TWO_TO_48, &NEG_TWO_TO_48,
            &ZERO, &NEG_TWO_TO_16, &TWO_TO_16, &NEG_TWO_TO_16X3, &TWO_TO_16X3, &NEG_TWO_TO_32, &TWO_TO_32, &NEG_TWO_TO_48, &TWO_TO_48,
            &ZERO, &TWO_TO_32, &NEG_TWO_TO_32, &TWO_TO_32X3, &NEG_TWO_TO_32X3, &TWO_TO_48, &NEG_TWO_TO_48, &ZERO, &ZERO, 
            &ZERO, &NEG_TWO_TO_32, &TWO_TO_32, &NEG_TWO_TO_32X3, &TWO_TO_32X3, &NEG_TWO_TO_48, &TWO_TO_48, &ZERO, &ZERO
        };

        for (int i = 0; i < VALUE_WIDTH; ++i) {
            for (int j = 0; j < VALUE_WIDTH; ++j) {
                llong lhs = *values[i];
                llong rhs = *values[j];
                llong ans = *answers[i*VALUE_WIDTH + j];

                llong n = lhs;

                LLAssert((n *= rhs) == ans);
                LLAssert(n == ans);

                n = lhs;
                LLAssert((n * rhs) == ans);
                LLAssert(n == lhs);
            }
        }
    }

    logln("Testing operator/=, operator/");
    // operator/=, operator/
    // test num = 0, div = 0, pos/neg, > 2^32, div > num
    {
        const llong ZERO;
        const llong ONE(0, 1);
        const llong NEG_ONE = -ONE;
        const llong MAX(0x7fffffff, 0xffffffff);
        const llong MIN(0x80000000, 0);
        const llong TWO(0, 2);
        const llong NEG_TWO = -TWO;
        const llong FIVE(0, 5);
        const llong NEG_FIVE = -FIVE;
        const llong TWO_TO_32(1, 0);
        const llong NEG_TWO_TO_32 = -TWO_TO_32;
        const llong TWO_TO_32d5 = llong(TWO_TO_32.asDouble()/5.0);
        const llong NEG_TWO_TO_32d5 = -TWO_TO_32d5;
        const llong TWO_TO_32X5 = TWO_TO_32 * FIVE;
        const llong NEG_TWO_TO_32X5 = -TWO_TO_32X5;

        const llong* tuples[] = { // lhs, rhs, ans
            &ZERO, &ZERO, &ZERO,
            &ONE, &ZERO,&MAX,
            &NEG_ONE, &ZERO, &MIN,
            &ONE, &ONE, &ONE,
            &ONE, &NEG_ONE, &NEG_ONE,
            &NEG_ONE, &ONE, &NEG_ONE,
            &NEG_ONE, &NEG_ONE, &ONE,
            &FIVE, &TWO, &TWO,
            &FIVE, &NEG_TWO, &NEG_TWO,
            &NEG_FIVE, &TWO, &NEG_TWO,
            &NEG_FIVE, &NEG_TWO, &TWO,
            &TWO, &FIVE, &ZERO,
            &TWO, &NEG_FIVE, &ZERO,
            &NEG_TWO, &FIVE, &ZERO,
            &NEG_TWO, &NEG_FIVE, &ZERO,
            &TWO_TO_32, &TWO_TO_32, &ONE,
            &TWO_TO_32, &NEG_TWO_TO_32, &NEG_ONE,
            &NEG_TWO_TO_32, &TWO_TO_32, &NEG_ONE,
            &NEG_TWO_TO_32, &NEG_TWO_TO_32, &ONE,
            &TWO_TO_32, &FIVE, &TWO_TO_32d5,
            &TWO_TO_32, &NEG_FIVE, &NEG_TWO_TO_32d5,
            &NEG_TWO_TO_32, &FIVE, &NEG_TWO_TO_32d5,
            &NEG_TWO_TO_32, &NEG_FIVE, &TWO_TO_32d5,
            &TWO_TO_32X5, &FIVE, &TWO_TO_32,
            &TWO_TO_32X5, &NEG_FIVE, &NEG_TWO_TO_32,
            &NEG_TWO_TO_32X5, &FIVE, &NEG_TWO_TO_32,
            &NEG_TWO_TO_32X5, &NEG_FIVE, &TWO_TO_32,
            &TWO_TO_32X5, &TWO_TO_32, &FIVE,
            &TWO_TO_32X5, &NEG_TWO_TO_32, &NEG_FIVE,
            &NEG_TWO_TO_32X5, &NEG_TWO_TO_32, &FIVE,
            &NEG_TWO_TO_32X5, &TWO_TO_32, &NEG_FIVE
        };
        const int TUPLE_WIDTH = 3;
        const int TUPLE_COUNT = UPRV_LENGTHOF(tuples)/TUPLE_WIDTH;
        for (int i = 0; i < TUPLE_COUNT; ++i) {
            const llong lhs = *tuples[i*TUPLE_WIDTH+0];
            const llong rhs = *tuples[i*TUPLE_WIDTH+1];
            const llong ans = *tuples[i*TUPLE_WIDTH+2];

            llong n = lhs;
            if (!((n /= rhs) == ans)) {
                errln("fail: (n /= rhs) == ans");
            }
            LLAssert(n == ans);

            n = lhs;
            LLAssert((n / rhs) == ans);
            LLAssert(n == lhs);
        }
    }

    logln("Testing operator%%=, operator%%");
    //operator%=, operator%
    {
        const llong ZERO;
        const llong ONE(0, 1);
        const llong TWO(0, 2);
        const llong THREE(0,3);
        const llong FOUR(0, 4);
        const llong FIVE(0, 5);
        const llong SIX(0, 6);

        const llong NEG_ONE = -ONE;
        const llong NEG_TWO = -TWO;
        const llong NEG_THREE = -THREE;
        const llong NEG_FOUR = -FOUR;
        const llong NEG_FIVE = -FIVE;
        const llong NEG_SIX = -SIX;

        const llong NINETY_NINE(0, 99);
        const llong HUNDRED(0, 100);
        const llong HUNDRED_ONE(0, 101);

        const llong BIG(0x12345678, 0x9abcdef0);
        const llong BIG_FIVE(BIG * FIVE);
        const llong BIG_FIVEm1 = BIG_FIVE - ONE;
        const llong BIG_FIVEp1 = BIG_FIVE + ONE;

        const llong* tuples[] = {
            &ZERO, &FIVE, &ZERO,
            &ONE, &FIVE, &ONE,
            &TWO, &FIVE, &TWO,
            &THREE, &FIVE, &THREE,
            &FOUR, &FIVE, &FOUR,
            &FIVE, &FIVE, &ZERO,
            &SIX, &FIVE, &ONE,
            &ZERO, &NEG_FIVE, &ZERO,
            &ONE, &NEG_FIVE, &ONE,
            &TWO, &NEG_FIVE, &TWO,
            &THREE, &NEG_FIVE, &THREE,
            &FOUR, &NEG_FIVE, &FOUR,
            &FIVE, &NEG_FIVE, &ZERO,
            &SIX, &NEG_FIVE, &ONE,
            &NEG_ONE, &FIVE, &NEG_ONE,
            &NEG_TWO, &FIVE, &NEG_TWO,
            &NEG_THREE, &FIVE, &NEG_THREE,
            &NEG_FOUR, &FIVE, &NEG_FOUR,
            &NEG_FIVE, &FIVE, &ZERO,
            &NEG_SIX, &FIVE, &NEG_ONE,
            &NEG_ONE, &NEG_FIVE, &NEG_ONE,
            &NEG_TWO, &NEG_FIVE, &NEG_TWO,
            &NEG_THREE, &NEG_FIVE, &NEG_THREE,
            &NEG_FOUR, &NEG_FIVE, &NEG_FOUR,
            &NEG_FIVE, &NEG_FIVE, &ZERO,
            &NEG_SIX, &NEG_FIVE, &NEG_ONE,
            &NINETY_NINE, &FIVE, &FOUR,
            &HUNDRED, &FIVE, &ZERO,
            &HUNDRED_ONE, &FIVE, &ONE,
            &BIG_FIVEm1, &FIVE, &FOUR,
            &BIG_FIVE, &FIVE, &ZERO,
            &BIG_FIVEp1, &FIVE, &ONE
        };
        const int TUPLE_WIDTH = 3;
        const int TUPLE_COUNT = UPRV_LENGTHOF(tuples)/TUPLE_WIDTH;
        for (int i = 0; i < TUPLE_COUNT; ++i) {
            const llong lhs = *tuples[i*TUPLE_WIDTH+0];
            const llong rhs = *tuples[i*TUPLE_WIDTH+1];
            const llong ans = *tuples[i*TUPLE_WIDTH+2];

            llong n = lhs;
            if (!((n %= rhs) == ans)) {
                errln("fail: (n %= rhs) == ans");
            }
            LLAssert(n == ans);

            n = lhs;
            LLAssert((n % rhs) == ans);
            LLAssert(n == lhs);
        }
    }

    logln("Testing pow");
    // pow
    LLAssert(llong(0, 0).pow(0) == llong(0, 0));
    LLAssert(llong(0, 0).pow(2) == llong(0, 0));
    LLAssert(llong(0, 2).pow(0) == llong(0, 1));
    LLAssert(llong(0, 2).pow(2) == llong(0, 4));
    LLAssert(llong(0, 2).pow(32) == llong(1, 0));
    LLAssert(llong(0, 5).pow(10) == llong((double)5.0 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5 * 5));

    // absolute value
    {
        const llong n(0xffffffff,0xffffffff);
        LLAssert(n.abs() == llong(0, 1));
    }

#ifdef RBNF_DEBUG
    logln("Testing atoll");
    // atoll
    const char empty[] = "";
    const char zero[] = "0";
    const char neg_one[] = "-1";
    const char neg_12345[] = "-12345";
    const char big1[] = "123456789abcdef0";
    const char big2[] = "fFfFfFfFfFfFfFfF";
    LLAssert(llong::atoll(empty) == llong(0, 0));
    LLAssert(llong::atoll(zero) == llong(0, 0));
    LLAssert(llong::atoll(neg_one) == llong(0xffffffff, 0xffffffff));
    LLAssert(llong::atoll(neg_12345) == -llong(0, 12345));
    LLAssert(llong::atoll(big1, 16) == llong(0x12345678, 0x9abcdef0));
    LLAssert(llong::atoll(big2, 16) == llong(0xffffffff, 0xffffffff));
#endif

    // u_atoll
    const UChar uempty[] = { 0 };
    const UChar uzero[] = { 0x30, 0 };
    const UChar uneg_one[] = { 0x2d, 0x31, 0 };
    const UChar uneg_12345[] = { 0x2d, 0x31, 0x32, 0x33, 0x34, 0x35, 0 };
    const UChar ubig1[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x30, 0 };
    const UChar ubig2[] = { 0x66, 0x46, 0x66, 0x46, 0x66, 0x46, 0x66, 0x46, 0x66, 0x46, 0x66, 0x46, 0x66, 0x46, 0x66, 0x46, 0 };
    LLAssert(llong::utoll(uempty) == llong(0, 0));
    LLAssert(llong::utoll(uzero) == llong(0, 0));
    LLAssert(llong::utoll(uneg_one) == llong(0xffffffff, 0xffffffff));
    LLAssert(llong::utoll(uneg_12345) == -llong(0, 12345));
    LLAssert(llong::utoll(ubig1, 16) == llong(0x12345678, 0x9abcdef0));
    LLAssert(llong::utoll(ubig2, 16) == llong(0xffffffff, 0xffffffff));

#ifdef RBNF_DEBUG
    logln("Testing lltoa");
    // lltoa
    {
        char buf[64]; // ascii
        LLAssert((llong(0, 0).lltoa(buf, (uint32_t)sizeof(buf)) == 1) && (strcmp(buf, zero) == 0));
        LLAssert((llong(0xffffffff, 0xffffffff).lltoa(buf, (uint32_t)sizeof(buf)) == 2) && (strcmp(buf, neg_one) == 0));
        LLAssert(((-llong(0, 12345)).lltoa(buf, (uint32_t)sizeof(buf)) == 6) && (strcmp(buf, neg_12345) == 0));
        LLAssert((llong(0x12345678, 0x9abcdef0).lltoa(buf, (uint32_t)sizeof(buf), 16) == 16) && (strcmp(buf, big1) == 0));
    }
#endif

    logln("Testing u_lltoa");
    // u_lltoa
    {
        UChar buf[64];
        LLAssert((llong(0, 0).lltou(buf, (uint32_t)sizeof(buf)) == 1) && (u_strcmp(buf, uzero) == 0));
        LLAssert((llong(0xffffffff, 0xffffffff).lltou(buf, (uint32_t)sizeof(buf)) == 2) && (u_strcmp(buf, uneg_one) == 0));
        LLAssert(((-llong(0, 12345)).lltou(buf, (uint32_t)sizeof(buf)) == 6) && (u_strcmp(buf, uneg_12345) == 0));
        LLAssert((llong(0x12345678, 0x9abcdef0).lltou(buf, (uint32_t)sizeof(buf), 16) == 16) && (u_strcmp(buf, ubig1) == 0));
    }
}

/* if 0 */
#endif

void 
IntlTestRBNF::TestEnglishSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale::getUS(), status);
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "1", "one" },
            { "2", "two" },
            { "15", "fifteen" },
            { "20", "twenty" },
            { "23", "twenty-three" },
            { "73", "seventy-three" },
            { "88", "eighty-eight" },
            { "100", "one hundred" },
            { "106", "one hundred six" },
            { "127", "one hundred twenty-seven" },
            { "200", "two hundred" },
            { "579", "five hundred seventy-nine" },
            { "1,000", "one thousand" },
            { "2,000", "two thousand" },
            { "3,004", "three thousand four" },
            { "4,567", "four thousand five hundred sixty-seven" },
            { "15,943", "fifteen thousand nine hundred forty-three" },
            { "2,345,678", "two million three hundred forty-five thousand six hundred seventy-eight" },
            { "-36", "minus thirty-six" },
            { "234.567", "two hundred thirty-four point five six seven" },
            { NULL, NULL}
        };

        doTest(formatter, testData, TRUE);

#if !UCONFIG_NO_COLLATION
        formatter->setLenient(TRUE);
        static const char* lpTestData[][2] = {
            { "fifty-7", "57" },
            { " fifty-7", "57" },
            { "  fifty-7", "57" },
            { "2 thousand six    HUNDRED fifty-7", "2,657" },
            { "fifteen hundred and zero", "1,500" },
            { "FOurhundred     thiRTY six", "436" },
            { NULL, NULL}
        };
        doLenientParseTest(formatter, lpTestData);
#endif
    }
    delete formatter;
}

void 
IntlTestRBNF::TestOrdinalAbbreviations() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_ORDINAL, Locale::getUS(), status);
    
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "1", "1st" },
            { "2", "2nd" },
            { "3", "3rd" },
            { "4", "4th" },
            { "7", "7th" },
            { "10", "10th" },
            { "11", "11th" },
            { "13", "13th" },
            { "20", "20th" },
            { "21", "21st" },
            { "22", "22nd" },
            { "23", "23rd" },
            { "24", "24th" },
            { "33", "33rd" },
            { "102", "102nd" },
            { "312", "312th" },
            { "12,345", "12,345th" },
            { NULL, NULL}
        };
        
        doTest(formatter, testData, FALSE);
    }
    delete formatter;
}

void 
IntlTestRBNF::TestDurations() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_DURATION, Locale::getUS(), status);
    
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "3,600", "1:00:00" },     //move me and I fail
            { "0", "0 sec." },
            { "1", "1 sec." },
            { "24", "24 sec." },
            { "60", "1:00" },
            { "73", "1:13" },
            { "145", "2:25" },
            { "666", "11:06" },
            //            { "3,600", "1:00:00" },
            { "3,740", "1:02:20" },
            { "10,293", "2:51:33" },
            { NULL, NULL}
        };
        
        doTest(formatter, testData, TRUE);
        
#if !UCONFIG_NO_COLLATION
        formatter->setLenient(TRUE);
        static const char* lpTestData[][2] = {
            { "2-51-33", "10,293" },
            { NULL, NULL}
        };
        doLenientParseTest(formatter, lpTestData);
#endif
    }
    delete formatter;
}

void 
IntlTestRBNF::TestSpanishSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale("es", "ES", ""), status);
    
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "1", "uno" },
            { "6", "seis" },
            { "16", "diecis\\u00e9is" },
            { "20", "veinte" },
            { "24", "veinticuatro" },
            { "26", "veintis\\u00e9is" },
            { "73", "setenta y tres" },
            { "88", "ochenta y ocho" },
            { "100", "cien" },
            { "106", "ciento seis" },
            { "127", "ciento veintisiete" },
            { "200", "doscientos" },
            { "579", "quinientos setenta y nueve" },
            { "1,000", "mil" },
            { "2,000", "dos mil" },
            { "3,004", "tres mil cuatro" },
            { "4,567", "cuatro mil quinientos sesenta y siete" },
            { "15,943", "quince mil novecientos cuarenta y tres" },
            { "2,345,678", "dos millones trescientos cuarenta y cinco mil seiscientos setenta y ocho"},
            { "-36", "menos treinta y seis" },
            { "234.567", "doscientos treinta y cuatro coma cinco seis siete" },
            { NULL, NULL}
        };
        
        doTest(formatter, testData, TRUE);
    }
    delete formatter;
}

void 
IntlTestRBNF::TestFrenchSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale::getFrance(), status);
    
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "1", "un" },
            { "15", "quinze" },
            { "20", "vingt" },
            { "21", "vingt-et-un" },
            { "23", "vingt-trois" },
            { "62", "soixante-deux" },
            { "70", "soixante-dix" },
            { "71", "soixante-et-onze" },
            { "73", "soixante-treize" },
            { "80", "quatre-vingts" },
            { "88", "quatre-vingt-huit" },
            { "100", "cent" },
            { "106", "cent six" },
            { "127", "cent vingt-sept" },
            { "200", "deux cents" },
            { "579", "cinq cent soixante-dix-neuf" },
            { "1,000", "mille" },
            { "1,123", "mille cent vingt-trois" },
            { "1,594", "mille cinq cent quatre-vingt-quatorze" },
            { "2,000", "deux mille" },
            { "3,004", "trois mille quatre" },
            { "4,567", "quatre mille cinq cent soixante-sept" },
            { "15,943", "quinze mille neuf cent quarante-trois" },
            { "2,345,678", "deux millions trois cent quarante-cinq mille six cent soixante-dix-huit" },
            { "-36", "moins trente-six" },
            { "234.567", "deux cent trente-quatre virgule cinq six sept" },
            { NULL, NULL}
        };
        
        doTest(formatter, testData, TRUE);
        
#if !UCONFIG_NO_COLLATION
        formatter->setLenient(TRUE);
        static const char* lpTestData[][2] = {
            { "trente-et-un", "31" },
            { "un cent quatre vingt dix huit", "198" },
            { NULL, NULL}
        };
        doLenientParseTest(formatter, lpTestData);
#endif
    }
    delete formatter;
}

static const char* const swissFrenchTestData[][2] = {
    { "1", "un" },
    { "15", "quinze" },
    { "20", "vingt" },
    { "21", "vingt-et-un" },
    { "23", "vingt-trois" },
    { "62", "soixante-deux" },
    { "70", "septante" },
    { "71", "septante-et-un" },
    { "73", "septante-trois" },
    { "80", "huitante" },
    { "88", "huitante-huit" },
    { "100", "cent" },
    { "106", "cent six" },
    { "127", "cent vingt-sept" },
    { "200", "deux cents" },
    { "579", "cinq cent septante-neuf" },
    { "1,000", "mille" },
    { "1,123", "mille cent vingt-trois" },
    { "1,594", "mille cinq cent nonante-quatre" },
    { "2,000", "deux mille" },
    { "3,004", "trois mille quatre" },
    { "4,567", "quatre mille cinq cent soixante-sept" },
    { "15,943", "quinze mille neuf cent quarante-trois" },
    { "2,345,678", "deux millions trois cent quarante-cinq mille six cent septante-huit" },
    { "-36", "moins trente-six" },
    { "234.567", "deux cent trente-quatre virgule cinq six sept" },
    { NULL, NULL}
};

void 
IntlTestRBNF::TestSwissFrenchSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale("fr", "CH", ""), status);
    
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        doTest(formatter, swissFrenchTestData, TRUE);
    }
    delete formatter;
}

static const char* const belgianFrenchTestData[][2] = {
    { "1", "un" },
    { "15", "quinze" },
    { "20", "vingt" },
    { "21", "vingt-et-un" },
    { "23", "vingt-trois" },
    { "62", "soixante-deux" },
    { "70", "septante" },
    { "71", "septante-et-un" },
    { "73", "septante-trois" },
    { "80", "quatre-vingts" },
    { "88", "quatre-vingt huit" },
    { "90", "nonante" },
    { "91", "nonante-et-un" },
    { "95", "nonante-cinq" },
    { "100", "cent" },
    { "106", "cent six" },
    { "127", "cent vingt-sept" },
    { "200", "deux cents" },
    { "579", "cinq cent septante-neuf" },
    { "1,000", "mille" },
    { "1,123", "mille cent vingt-trois" },
    { "1,594", "mille cinq cent nonante-quatre" },
    { "2,000", "deux mille" },
    { "3,004", "trois mille quatre" },
    { "4,567", "quatre mille cinq cent soixante-sept" },
    { "15,943", "quinze mille neuf cent quarante-trois" },
    { "2,345,678", "deux millions trois cent quarante-cinq mille six cent septante-huit" },
    { "-36", "moins trente-six" },
    { "234.567", "deux cent trente-quatre virgule cinq six sept" },
    { NULL, NULL}
};


void 
IntlTestRBNF::TestBelgianFrenchSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale("fr", "BE", ""), status);
    
    if (U_FAILURE(status)) {
        errcheckln(status, "rbnf status: 0x%x (%s)\n", status, u_errorName(status));
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        // Belgian french should match Swiss french.
        doTest(formatter, belgianFrenchTestData, TRUE);
    }
    delete formatter;
}

void 
IntlTestRBNF::TestItalianSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale::getItalian(), status);

    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "1", "uno" },
            { "15", "quindici" },
            { "20", "venti" },
            { "23", "venti\\u00ADtr\\u00E9" },
            { "73", "settanta\\u00ADtr\\u00E9" },
            { "88", "ottant\\u00ADotto" },
            { "100", "cento" },
            { "101", "cento\\u00ADuno" },
            { "103", "cento\\u00ADtr\\u00E9" },
            { "106", "cento\\u00ADsei" },
            { "108", "cent\\u00ADotto" },
            { "127", "cento\\u00ADventi\\u00ADsette" },
            { "181", "cent\\u00ADottant\\u00ADuno" },
            { "200", "due\\u00ADcento" },
            { "579", "cinque\\u00ADcento\\u00ADsettanta\\u00ADnove" },
            { "1,000", "mille" },
            { "2,000", "due\\u00ADmila" },
            { "3,004", "tre\\u00ADmila\\u00ADquattro" },
            { "4,567", "quattro\\u00ADmila\\u00ADcinque\\u00ADcento\\u00ADsessanta\\u00ADsette" },
            { "15,943", "quindici\\u00ADmila\\u00ADnove\\u00ADcento\\u00ADquaranta\\u00ADtr\\u00E9" },
            { "-36", "meno trenta\\u00ADsei" },
            { "234.567", "due\\u00ADcento\\u00ADtrenta\\u00ADquattro virgola cinque sei sette" },
            { NULL, NULL}
        };
        
        doTest(formatter, testData, TRUE);
    }
    delete formatter;
}

void 
IntlTestRBNF::TestPortugueseSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale("pt","BR",""), status);

    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "1", "um" },
            { "15", "quinze" },
            { "20", "vinte" },
            { "23", "vinte e tr\\u00EAs" },
            { "73", "setenta e tr\\u00EAs" },
            { "88", "oitenta e oito" },
            { "100", "cem" },
            { "106", "cento e seis" },
            { "108", "cento e oito" },
            { "127", "cento e vinte e sete" },
            { "181", "cento e oitenta e um" },
            { "200", "duzentos" },
            { "579", "quinhentos e setenta e nove" },
            { "1,000", "mil" },
            { "2,000", "dois mil" },
            { "3,004", "tr\\u00EAs mil e quatro" },
            { "4,567", "quatro mil e quinhentos e sessenta e sete" },
            { "15,943", "quinze mil e novecentos e quarenta e tr\\u00EAs" },
            { "-36", "menos trinta e seis" },
            { "234.567", "duzentos e trinta e quatro v\\u00EDrgula cinco seis sete" },
            { NULL, NULL}
        };
        
        doTest(formatter, testData, TRUE);
    }
    delete formatter;
}
void 
IntlTestRBNF::TestGermanSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale::getGermany(), status);
    
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "1", "eins" },
            { "15", "f\\u00fcnfzehn" },
            { "20", "zwanzig" },
            { "23", "drei\\u00ADund\\u00ADzwanzig" },
            { "73", "drei\\u00ADund\\u00ADsiebzig" },
            { "88", "acht\\u00ADund\\u00ADachtzig" },
            { "100", "ein\\u00ADhundert" },
            { "106", "ein\\u00ADhundert\\u00ADsechs" },
            { "127", "ein\\u00ADhundert\\u00ADsieben\\u00ADund\\u00ADzwanzig" },
            { "200", "zwei\\u00ADhundert" },
            { "579", "f\\u00fcnf\\u00ADhundert\\u00ADneun\\u00ADund\\u00ADsiebzig" },
            { "1,000", "ein\\u00ADtausend" },
            { "2,000", "zwei\\u00ADtausend" },
            { "3,004", "drei\\u00ADtausend\\u00ADvier" },
            { "4,567", "vier\\u00ADtausend\\u00ADf\\u00fcnf\\u00ADhundert\\u00ADsieben\\u00ADund\\u00ADsechzig" },
            { "15,943", "f\\u00fcnfzehn\\u00ADtausend\\u00ADneun\\u00ADhundert\\u00ADdrei\\u00ADund\\u00ADvierzig" },
            { "2,345,678", "zwei Millionen drei\\u00ADhundert\\u00ADf\\u00fcnf\\u00ADund\\u00ADvierzig\\u00ADtausend\\u00ADsechs\\u00ADhundert\\u00ADacht\\u00ADund\\u00ADsiebzig" },
            { NULL, NULL}
        };
        
        doTest(formatter, testData, TRUE);
        
#if !UCONFIG_NO_COLLATION
        formatter->setLenient(TRUE);
        static const char* lpTestData[][2] = {
            { "ein Tausend sechs Hundert fuenfunddreissig", "1,635" },
            { NULL, NULL}
        };
        doLenientParseTest(formatter, lpTestData);
#endif
    }
    delete formatter;
}

void 
IntlTestRBNF::TestThaiSpellout() 
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale("th"), status);
    
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testData[][2] = {
            { "0", "\\u0e28\\u0e39\\u0e19\\u0e22\\u0e4c" },
            { "1", "\\u0e2b\\u0e19\\u0e36\\u0e48\\u0e07" },
            { "10", "\\u0e2a\\u0e34\\u0e1a" },
            { "11", "\\u0e2a\\u0e34\\u0e1a\\u200b\\u0e40\\u0e2d\\u0e47\\u0e14" },
            { "21", "\\u0e22\\u0e35\\u0e48\\u200b\\u0e2a\\u0e34\\u0e1a\\u200b\\u0e40\\u0e2d\\u0e47\\u0e14" },
            { "101", "\\u0e2b\\u0e19\\u0e36\\u0e48\\u0e07\\u200b\\u0e23\\u0e49\\u0e2d\\u0e22\\u200b\\u0e2b\\u0e19\\u0e36\\u0e48\\u0e07" },
            { "1.234", "\\u0e2b\\u0e19\\u0e36\\u0e48\\u0e07\\u200b\\u0e08\\u0e38\\u0e14\\u200b\\u0e2a\\u0e2d\\u0e07\\u0e2a\\u0e32\\u0e21\\u0e2a\\u0e35\\u0e48" },
            { NULL, NULL}
        };
        
        doTest(formatter, testData, TRUE);
    }
    delete formatter;
}

void
IntlTestRBNF::TestSwedishSpellout()
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale("sv"), status);

    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* testDataDefault[][2] = {
            { "101", "ett\\u00adhundra\\u00adett" },
            { "123", "ett\\u00adhundra\\u00adtjugo\\u00adtre" },
            { "1,001", "et\\u00adtusen ett" },
            { "1,100", "et\\u00adtusen ett\\u00adhundra" },
            { "1,101", "et\\u00adtusen ett\\u00adhundra\\u00adett" },
            { "1,234", "et\\u00adtusen tv\\u00e5\\u00adhundra\\u00adtrettio\\u00adfyra" },
            { "10,001", "tio\\u00adtusen ett" },
            { "11,000", "elva\\u00adtusen" },
            { "12,000", "tolv\\u00adtusen" },
            { "20,000", "tjugo\\u00adtusen" },
            { "21,000", "tjugo\\u00adet\\u00adtusen" },
            { "21,001", "tjugo\\u00adet\\u00adtusen ett" },
            { "200,000", "tv\\u00e5\\u00adhundra\\u00adtusen" },
            { "201,000", "tv\\u00e5\\u00adhundra\\u00adet\\u00adtusen" },
            { "200,200", "tv\\u00e5\\u00adhundra\\u00adtusen tv\\u00e5\\u00adhundra" },
            { "2,002,000", "tv\\u00e5 miljoner tv\\u00e5\\u00adtusen" },
            { "12,345,678", "tolv miljoner tre\\u00adhundra\\u00adfyrtio\\u00adfem\\u00adtusen sex\\u00adhundra\\u00adsjuttio\\u00ad\\u00e5tta" },
            { "123,456.789", "ett\\u00adhundra\\u00adtjugo\\u00adtre\\u00adtusen fyra\\u00adhundra\\u00adfemtio\\u00adsex komma sju \\u00e5tta nio" },
            { "-12,345.678", "minus tolv\\u00adtusen tre\\u00adhundra\\u00adfyrtio\\u00adfem komma sex sju \\u00e5tta" },
            { NULL, NULL }
        };
        doTest(formatter, testDataDefault, TRUE);

          static const char* testDataNeutrum[][2] = {
              { "101", "ett\\u00adhundra\\u00adett" },
              { "1,001", "et\\u00adtusen ett" },
              { "1,101", "et\\u00adtusen ett\\u00adhundra\\u00adett" },
              { "10,001", "tio\\u00adtusen ett" },
              { "21,001", "tjugo\\u00adet\\u00adtusen ett" },
              { NULL, NULL }
          };
  
          formatter->setDefaultRuleSet("%spellout-cardinal-neuter", status);
          if (U_SUCCESS(status)) {
          logln("        testing spellout-cardinal-neuter rules");
          doTest(formatter, testDataNeutrum, TRUE);
          }
          else {
          errln("Can't test spellout-cardinal-neuter rules");
          }

        static const char* testDataYear[][2] = {
            { "101", "ett\\u00adhundra\\u00adett" },
            { "900", "nio\\u00adhundra" },
            { "1,001", "et\\u00adtusen ett" },
            { "1,100", "elva\\u00adhundra" },
            { "1,101", "elva\\u00adhundra\\u00adett" },
            { "1,234", "tolv\\u00adhundra\\u00adtrettio\\u00adfyra" },
            { "2,001", "tjugo\\u00adhundra\\u00adett" },
            { "10,001", "tio\\u00adtusen ett" },
            { NULL, NULL }
        };

        status = U_ZERO_ERROR;
        formatter->setDefaultRuleSet("%spellout-numbering-year", status);
        if (U_SUCCESS(status)) {
            logln("testing year rules");
            doTest(formatter, testDataYear, TRUE);
        }
        else {
            errln("Can't test year rules");
        }

    }
    delete formatter;
}

void
IntlTestRBNF::TestSmallValues()
{
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat* formatter
        = new RuleBasedNumberFormat(URBNF_SPELLOUT, Locale("en_US"), status);

    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));
    } else {
        static const char* const testDataDefault[][2] = {
        { "0.001", "zero point zero zero one" },
        { "0.0001", "zero point zero zero zero one" },
        { "0.00001", "zero point zero zero zero zero one" },
        { "0.000001", "zero point zero zero zero zero zero one" },
        { "0.0000001", "zero point zero zero zero zero zero zero one" },
        { "0.00000001", "zero point zero zero zero zero zero zero zero one" },
        { "0.000000001", "zero point zero zero zero zero zero zero zero zero one" },
        { "0.0000000001", "zero point zero zero zero zero zero zero zero zero zero one" },
        { "0.00000000001", "zero point zero zero zero zero zero zero zero zero zero zero one" },
        { "0.000000000001", "zero point zero zero zero zero zero zero zero zero zero zero zero one" },
        { "0.0000000000001", "zero point zero zero zero zero zero zero zero zero zero zero zero zero one" },
        { "0.00000000000001", "zero point zero zero zero zero zero zero zero zero zero zero zero zero zero one" },
        { "0.000000000000001", "zero point zero zero zero zero zero zero zero zero zero zero zero zero zero zero one" },
        { "10,000,000.001", "ten million point zero zero one" },
        { "10,000,000.0001", "ten million point zero zero zero one" },
        { "10,000,000.00001", "ten million point zero zero zero zero one" },
        { "10,000,000.000001", "ten million point zero zero zero zero zero one" },
        { "10,000,000.0000001", "ten million point zero zero zero zero zero zero one" },
//        { "10,000,000.00000001", "ten million point zero zero zero zero zero zero zero one" },
//        { "10,000,000.000000002", "ten million point zero zero zero zero zero zero zero zero two" },
        { "10,000,000", "ten million" },
//        { "1,234,567,890.0987654", "one billion, two hundred and thirty-four million, five hundred and sixty-seven thousand, eight hundred and ninety point zero nine eight seven six five four" },
//        { "123,456,789.9876543", "one hundred and twenty-three million, four hundred and fifty-six thousand, seven hundred and eighty-nine point nine eight seven six five four three" },
//        { "12,345,678.87654321", "twelve million, three hundred and forty-five thousand, six hundred and seventy-eight point eight seven six five four three two one" },
        { "1,234,567.7654321", "one million two hundred thirty-four thousand five hundred sixty-seven point seven six five four three two one" },
        { "123,456.654321", "one hundred twenty-three thousand four hundred fifty-six point six five four three two one" },
        { "12,345.54321", "twelve thousand three hundred forty-five point five four three two one" },
        { "1,234.4321", "one thousand two hundred thirty-four point four three two one" },
        { "123.321", "one hundred twenty-three point three two one" },
        { "0.0000000011754944", "zero point zero zero zero zero zero zero zero zero one one seven five four nine four four" },
        { "0.000001175494351", "zero point zero zero zero zero zero one one seven five four nine four three five one" },
        { NULL, NULL }
        };

        doTest(formatter, testDataDefault, TRUE);

        delete formatter;
    }
}

void 
IntlTestRBNF::TestLocalizations(void)
{
    int i;
    UnicodeString rules("%main:0:no;1:some;100:a lot;1000:tons;\n"
        "%other:0:nada;1:yah, some;100:plenty;1000:more'n you'll ever need");

    UErrorCode status = U_ZERO_ERROR;
    UParseError perror;
    RuleBasedNumberFormat formatter(rules, perror, status);
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not construct formatter - %s", u_errorName(status));           
    } else {
        {
            static const char* const testData[][2] = {
                { "0", "nada" },
                { "5", "yah, some" },
                { "423", "plenty" },
                { "12345", "more'n you'll ever need" },
                { NULL, NULL }
            };
            doTest(&formatter, testData, FALSE);
        }

        {
            UnicodeString loc("<<%main, %other>,<en, Main, Other>,<fr, leMain, leOther>,<de, 'das Main', 'etwas anderes'>>");
            static const char* const testData[][2] = {
                { "0", "no" },
                { "5", "some" },
                { "423", "a lot" },
                { "12345", "tons" },
                { NULL, NULL }
            };
            RuleBasedNumberFormat formatter0(rules, loc, perror, status);
            if (U_FAILURE(status)) {
                errln("failed to build second formatter");
            } else {
                doTest(&formatter0, testData, FALSE);

                {
                // exercise localization info
                    Locale locale0("en__VALLEY@turkey=gobblegobble");
                    Locale locale1("de_DE_FOO");
                    Locale locale2("ja_JP");
                    UnicodeString name = formatter0.getRuleSetName(0);
                    if ( formatter0.getRuleSetDisplayName(0, locale0) == "Main"
                      && formatter0.getRuleSetDisplayName(0, locale1) == "das Main"
                      && formatter0.getRuleSetDisplayName(0, locale2) == "%main"
                      && formatter0.getRuleSetDisplayName(name, locale0) == "Main"
                      && formatter0.getRuleSetDisplayName(name, locale1) == "das Main"
                      && formatter0.getRuleSetDisplayName(name, locale2) == "%main"){
                          logln("getRuleSetDisplayName tested");
                    }else {
                        errln("failed to getRuleSetDisplayName");
                    }
                }

                for (i = 0; i < formatter0.getNumberOfRuleSetDisplayNameLocales(); ++i) {
                    Locale locale = formatter0.getRuleSetDisplayNameLocale(i, status);
                    if (U_SUCCESS(status)) {
                        for (int j = 0; j < formatter0.getNumberOfRuleSetNames(); ++j) {
                            UnicodeString name = formatter0.getRuleSetName(j);
                            UnicodeString lname = formatter0.getRuleSetDisplayName(j, locale);
                            UnicodeString msg = locale.getName();
                            msg.append(": ");
                            msg.append(name);
                            msg.append(" = ");
                            msg.append(lname);
                            logln(msg);
                        }
                    }
                }
            }
        }

        {
            static const char* goodLocs[] = {
                "", // zero-length ok, same as providing no localization data
                "<<>>", // no public rule sets ok
                "<<%main>>", // no localizations ok
                "<<%main,>,<en, Main,>>", // comma before close angle ok
                "<<%main>,<en, ',<>\" '>>", // quotes everything until next quote
                "<<%main>,<'en', \"it's ok\">>", // double quotes work too
                "  \n <\n  <\n  %main\n  >\n  , \t <\t   en\t  ,  \tfoo \t\t > \n\n >  \n ", // Pattern_White_Space ok
           }; 
            int32_t goodLocsLen = UPRV_LENGTHOF(goodLocs);

            static const char* badLocs[] = {
                " ", // non-zero length
                "<>", // empty array
                "<", // unclosed outer array
                "<<", // unclosed inner array
                "<<,>>", // unexpected comma
                "<<''>>", // empty string
                "  x<<%main>>", // first non space char not open angle bracket
                "<%main>", // missing inner array
                "<<%main %other>>", // elements missing separating commma (spaces must be quoted)
                "<<%main><en, Main>>", // arrays missing separating comma
                "<<%main>,<en, main, foo>>", // too many elements in locale data
                "<<%main>,<en>>", // too few elements in locale data
                "<<<%main>>>", // unexpected open angle
                "<<%main<>>>", // unexpected open angle
                "<<%main, %other>,<en,,>>", // implicit empty strings
                "<<%main>,<en,''>>", // empty string
                "<<%main>, < en, '>>", // unterminated quote
                "<<%main>, < en, \"<>>", // unterminated quote
                "<<%main\">>", // quote in string
                "<<%main'>>", // quote in string
                "<<%main<>>", // open angle in string
                "<<%main>> x", // extra non-space text at end

            };
            int32_t badLocsLen = UPRV_LENGTHOF(badLocs);

            for (i = 0; i < goodLocsLen; ++i) {
                logln("[%d] '%s'", i, goodLocs[i]);
                UErrorCode status = U_ZERO_ERROR;
                UnicodeString loc(goodLocs[i]);
                RuleBasedNumberFormat fmt(rules, loc, perror, status);
                if (U_FAILURE(status)) {
                    errln("Failed parse of good localization string: '%s'", goodLocs[i]);
                }
            }

            for (i = 0; i < badLocsLen; ++i) {
                logln("[%d] '%s'", i, badLocs[i]);
                UErrorCode status = U_ZERO_ERROR;
                UnicodeString loc(badLocs[i]);
                RuleBasedNumberFormat fmt(rules, loc, perror, status);
                if (U_SUCCESS(status)) {
                    errln("Successful parse of bad localization string: '%s'", badLocs[i]);
                }
            }
        }
    }
}

void
IntlTestRBNF::TestAllLocales()
{
    const char* names[] = {
        " (spellout) ",
        " (ordinal)  "
        // " (duration) " // This is English only, and it's not really supported in CLDR anymore.
    };
    double numbers[] = {45.678, 1, 2, 10, 11, 100, 110, 200, 1000, 1111, -1111};

    int32_t count = 0;
    const Locale* locales = Locale::getAvailableLocales(count);
    for (int i = 0; i < count; ++i) {
        const Locale* loc = &locales[i];

        for (int j = 0; j < 2; ++j) {
            UErrorCode status = U_ZERO_ERROR;
            RuleBasedNumberFormat* f = new RuleBasedNumberFormat((URBNFRuleSetTag)j, *loc, status);

            if (status == U_USING_DEFAULT_WARNING || status == U_USING_FALLBACK_WARNING) {
                // Skip it.
                delete f;
                break;
            }
            if (U_FAILURE(status)) {
                errln(UnicodeString(loc->getName()) + names[j]
                    + "ERROR could not instantiate -> " + u_errorName(status));
                continue;
            }
#if !UCONFIG_NO_COLLATION
            for (unsigned int numidx = 0; numidx < UPRV_LENGTHOF(numbers); numidx++) {
                double n = numbers[numidx];
                UnicodeString str;
                f->format(n, str);

                if (verbose) {
                    logln(UnicodeString(loc->getName()) + names[j]
                        + "success: " + n + " -> " + str);
                }

                // We do not validate the result in this test case,
                // because there are cases which do not round trip by design.
                Formattable num;

                // regular parse
                status = U_ZERO_ERROR;
                f->setLenient(FALSE);
                f->parse(str, num, status);
                if (U_FAILURE(status)) {
                    errln(UnicodeString(loc->getName()) + names[j]
                        + "ERROR could not parse '" + str + "' -> " + u_errorName(status));
                }
                // We only check the spellout. The behavior is undefined for numbers < 1 and fractional numbers.
                if (j == 0) {
                    if (num.getType() == Formattable::kLong && num.getLong() != n) {
                        errln(UnicodeString(loc->getName()) + names[j]
                            + UnicodeString("ERROR could not roundtrip ") + n
                            + UnicodeString(" -> ") + str + UnicodeString(" -> ") + num.getLong());
                    }
                    else if (num.getType() == Formattable::kDouble && (int64_t)(num.getDouble() * 1000) != (int64_t)(n*1000)) {
                        // The epsilon difference is too high.
                        errln(UnicodeString(loc->getName()) + names[j]
                            + UnicodeString("ERROR could not roundtrip ") + n
                            + UnicodeString(" -> ") + str + UnicodeString(" -> ") + num.getDouble());
                    }
                }
                if (!quick && !logKnownIssue("9503") ) {
                    // lenient parse
                    status = U_ZERO_ERROR;
                    f->setLenient(TRUE);
                    f->parse(str, num, status);
                    if (U_FAILURE(status)) {
                        errln(UnicodeString(loc->getName()) + names[j]
                            + "ERROR could not parse(lenient) '" + str + "' -> " + u_errorName(status));
                    }
                    // We only check the spellout. The behavior is undefined for numbers < 1 and fractional numbers.
                    if (j == 0) {
                        if (num.getType() == Formattable::kLong && num.getLong() != n) {
                            errln(UnicodeString(loc->getName()) + names[j]
                                + UnicodeString("ERROR could not roundtrip ") + n
                                + UnicodeString(" -> ") + str + UnicodeString(" -> ") + num.getLong());
                        }
                        else if (num.getType() == Formattable::kDouble && (int64_t)(num.getDouble() * 1000) != (int64_t)(n*1000)) {
                            // The epsilon difference is too high.
                            errln(UnicodeString(loc->getName()) + names[j]
                                + UnicodeString("ERROR could not roundtrip ") + n
                                + UnicodeString(" -> ") + str + UnicodeString(" -> ") + num.getDouble());
                        }
                    }
                }
            }
#endif
            delete f;
        }
    }
}

void 
IntlTestRBNF::TestMultiplierSubstitution(void) {
    UnicodeString rules("=#,##0=;1,000,000: <##0.###< million;");
    UErrorCode status = U_ZERO_ERROR;
    UParseError parse_error;
    RuleBasedNumberFormat *rbnf = 
        new RuleBasedNumberFormat(rules, Locale::getUS(), parse_error, status);
    if (U_SUCCESS(status)) {
        UnicodeString res;
        FieldPosition pos;
        double n = 1234000.0;
        rbnf->format(n, res, pos);
        delete rbnf;

        UnicodeString expected(UNICODE_STRING_SIMPLE("1.234 million"));
        if (expected != res) {
            UnicodeString msg = "Expected: ";
            msg.append(expected);
            msg.append(" but got ");
            msg.append(res);
            errln(msg);
        }
    }
}

void
IntlTestRBNF::TestSetDecimalFormatSymbols() {
    UErrorCode status = U_ZERO_ERROR;

    RuleBasedNumberFormat rbnf(URBNF_ORDINAL, Locale::getEnglish(), status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create RuleBasedNumberFormat - " + UnicodeString(u_errorName(status)));
        return;
    }

    DecimalFormatSymbols dfs(Locale::getEnglish(), status);
    if (U_FAILURE(status)) {
        errln("Unable to create DecimalFormatSymbols - " + UnicodeString(u_errorName(status)));
        return;
    }

    UnicodeString expected[] = {
            UnicodeString("1,001st"),
            UnicodeString("1&001st")
    };

    double number = 1001;

    UnicodeString result;

    rbnf.format(number, result);
    if (result != expected[0]) {
        errln("Format Error - Got: " + result + " Expected: " + expected[0]);
    }

    result.remove();

    /* Set new symbol for testing */
    dfs.setSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol, UnicodeString("&"), TRUE);
    rbnf.setDecimalFormatSymbols(dfs);

    rbnf.format(number, result);
    if (result != expected[1]) {
        errln("Format Error - Got: " + result + " Expected: " + expected[1]);
    }
}

void IntlTestRBNF::TestPluralRules() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString enRules("%digits-ordinal:-x: ->>;0: =#,##0=$(ordinal,one{st}two{nd}few{rd}other{th})$;");
    UParseError parseError;
    RuleBasedNumberFormat enFormatter(enRules, Locale::getEnglish(), parseError, status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create RuleBasedNumberFormat - " + UnicodeString(u_errorName(status)));
        return;
    }
    const char* const enTestData[][2] = {
            { "1", "1st" },
            { "2", "2nd" },
            { "3", "3rd" },
            { "4", "4th" },
            { "11", "11th" },
            { "12", "12th" },
            { "13", "13th" },
            { "14", "14th" },
            { "21", "21st" },
            { "22", "22nd" },
            { "23", "23rd" },
            { "24", "24th" },
            { NULL, NULL }
    };

    doTest(&enFormatter, enTestData, TRUE);

    // This is trying to model the feminine form, but don't worry about the details too much.
    // We're trying to test the plural rules.
    UnicodeString ruRules("%spellout-numbering:"
            "-x: minus >>;"
            "x.x: << point >>;"
            "0: zero;"
            "1: one;"
            "2: two;"
            "3: three;"
            "4: four;"
            "5: five;"
            "6: six;"
            "7: seven;"
            "8: eight;"
            "9: nine;"
            "10: ten;"
            "11: eleven;"
            "12: twelve;"
            "13: thirteen;"
            "14: fourteen;"
            "15: fifteen;"
            "16: sixteen;"
            "17: seventeen;"
            "18: eighteen;"
            "19: nineteen;"
            "20: twenty[->>];"
            "30: thirty[->>];"
            "40: forty[->>];"
            "50: fifty[->>];"
            "60: sixty[->>];"
            "70: seventy[->>];"
            "80: eighty[->>];"
            "90: ninety[->>];"
            "100: hundred[ >>];"
            "200: << hundred[ >>];"
            "300: << hundreds[ >>];"
            "500: << hundredss[ >>];"
            "1000: << $(cardinal,one{thousand}few{thousands}other{thousandss})$[ >>];"
            "1000000: << $(cardinal,one{million}few{millions}other{millionss})$[ >>];");
    RuleBasedNumberFormat ruFormatter(ruRules, Locale("ru"), parseError, status);
    const char* const ruTestData[][2] = {
            { "1", "one" },
            { "100", "hundred" },
            { "125", "hundred twenty-five" },
            { "399", "three hundreds ninety-nine" },
            { "1,000", "one thousand" },
            { "1,001", "one thousand one" },
            { "2,000", "two thousands" },
            { "2,001", "two thousands one" },
            { "2,002", "two thousands two" },
            { "3,333", "three thousands three hundreds thirty-three" },
            { "5,000", "five thousandss" },
            { "11,000", "eleven thousandss" },
            { "21,000", "twenty-one thousand" },
            { "22,000", "twenty-two thousands" },
            { "25,001", "twenty-five thousandss one" },
            { NULL, NULL }
    };

    if (U_FAILURE(status)) {
        errln("Unable to create RuleBasedNumberFormat - " + UnicodeString(u_errorName(status)));
        return;
    }
    doTest(&ruFormatter, ruTestData, TRUE);

    // Make sure there are no divide by 0 errors.
    UnicodeString result;
    RuleBasedNumberFormat(ruRules, Locale("ru"), parseError, status).format((int32_t)21000, result);
    if (result.compare(UNICODE_STRING_SIMPLE("twenty-one thousand")) != 0) {
        errln("Got " + result + " for 21000");
    }

}

void IntlTestRBNF::TestInfinityNaN() {
    UErrorCode status = U_ZERO_ERROR;
    UParseError parseError;
    UnicodeString enRules("%default:"
            "-x: minus >>;"
            "Inf: infinite;"
            "NaN: not a number;"
            "0: =#,##0=;");
    RuleBasedNumberFormat enFormatter(enRules, Locale::getEnglish(), parseError, status);
    const char * const enTestData[][2] = {
            {"1", "1"},
            {"\\u221E", "infinite"},
            {"-\\u221E", "minus infinite"},
            {"NaN", "not a number"},
            { NULL, NULL }
    };
    if (U_FAILURE(status)) {
        dataerrln("Unable to create RuleBasedNumberFormat - " + UnicodeString(u_errorName(status)));
        return;
    }

    doTest(&enFormatter, enTestData, true);

    // Test the default behavior when the rules are undefined.
    UnicodeString enRules2("%default:"
            "-x: ->>;"
            "0: =#,##0=;");
    RuleBasedNumberFormat enFormatter2(enRules2, Locale::getEnglish(), parseError, status);
    if (U_FAILURE(status)) {
        errln("Unable to create RuleBasedNumberFormat - " + UnicodeString(u_errorName(status)));
        return;
    }
    const char * const enDefaultTestData[][2] = {
            {"1", "1"},
            {"\\u221E", "\\u221E"},
            {"-\\u221E", "-\\u221E"},
            {"NaN", "NaN"},
            { NULL, NULL }
    };

    doTest(&enFormatter2, enDefaultTestData, true);
}

void IntlTestRBNF::TestVariableDecimalPoint() {
    UErrorCode status = U_ZERO_ERROR;
    UParseError parseError;
    UnicodeString enRules("%spellout-numbering:"
            "-x: minus >>;"
            "x.x: << point >>;"
            "x,x: << comma >>;"
            "0.x: xpoint >>;"
            "0,x: xcomma >>;"
            "0: zero;"
            "1: one;"
            "2: two;"
            "3: three;"
            "4: four;"
            "5: five;"
            "6: six;"
            "7: seven;"
            "8: eight;"
            "9: nine;");
    RuleBasedNumberFormat enFormatter(enRules, Locale::getEnglish(), parseError, status);
    const char * const enTestPointData[][2] = {
            {"1.1", "one point one"},
            {"1.23", "one point two three"},
            {"0.4", "xpoint four"},
            { NULL, NULL }
    };
    if (U_FAILURE(status)) {
        dataerrln("Unable to create RuleBasedNumberFormat - " + UnicodeString(u_errorName(status)));
        return;
    }
    doTest(&enFormatter, enTestPointData, true);

    DecimalFormatSymbols decimalFormatSymbols(Locale::getEnglish(), status);
    decimalFormatSymbols.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, UNICODE_STRING_SIMPLE(","));
    enFormatter.setDecimalFormatSymbols(decimalFormatSymbols);
    const char * const enTestCommaData[][2] = {
            {"1.1", "one comma one"},
            {"1.23", "one comma two three"},
            {"0.4", "xcomma four"},
            { NULL, NULL }
    };
    doTest(&enFormatter, enTestCommaData, true);
}

void IntlTestRBNF::TestLargeNumbers() {
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat rbnf(URBNF_SPELLOUT, Locale::getEnglish(), status);

    const char * const enTestFullData[][2] = {
            {"-9007199254740991", "minus nine quadrillion seven trillion one hundred ninety-nine billion two hundred fifty-four million seven hundred forty thousand nine hundred ninety-one"}, // Maximum precision in both a double and a long
            {"9007199254740991", "nine quadrillion seven trillion one hundred ninety-nine billion two hundred fifty-four million seven hundred forty thousand nine hundred ninety-one"}, // Maximum precision in both a double and a long
            {"-9007199254740992", "minus nine quadrillion seven trillion one hundred ninety-nine billion two hundred fifty-four million seven hundred forty thousand nine hundred ninety-two"}, // Only precisely contained in a long
            {"9007199254740992", "nine quadrillion seven trillion one hundred ninety-nine billion two hundred fifty-four million seven hundred forty thousand nine hundred ninety-two"}, // Only precisely contained in a long
            {"9999999999999998", "nine quadrillion nine hundred ninety-nine trillion nine hundred ninety-nine billion nine hundred ninety-nine million nine hundred ninety-nine thousand nine hundred ninety-eight"},
            {"9999999999999999", "nine quadrillion nine hundred ninety-nine trillion nine hundred ninety-nine billion nine hundred ninety-nine million nine hundred ninety-nine thousand nine hundred ninety-nine"},
            {"999999999999999999", "nine hundred ninety-nine quadrillion nine hundred ninety-nine trillion nine hundred ninety-nine billion nine hundred ninety-nine million nine hundred ninety-nine thousand nine hundred ninety-nine"},
            {"1000000000000000000", "1,000,000,000,000,000,000"}, // The rules don't go to 1 quintillion yet
            {"-9223372036854775809", "-9,223,372,036,854,775,809"}, // We've gone beyond 64-bit precision
            {"-9223372036854775808", "-9,223,372,036,854,775,808"}, // We've gone beyond +64-bit precision
            {"-9223372036854775807", "minus 9,223,372,036,854,775,807"}, // Minimum 64-bit precision
            {"-9223372036854775806", "minus 9,223,372,036,854,775,806"}, // Minimum 64-bit precision + 1
            {"9223372036854774111", "9,223,372,036,854,774,111"}, // Below 64-bit precision
            {"9223372036854774999", "9,223,372,036,854,774,999"}, // Below 64-bit precision
            {"9223372036854775000", "9,223,372,036,854,775,000"}, // Below 64-bit precision
            {"9223372036854775806", "9,223,372,036,854,775,806"}, // Maximum 64-bit precision - 1
            {"9223372036854775807", "9,223,372,036,854,775,807"}, // Maximum 64-bit precision
            {"9223372036854775808", "9,223,372,036,854,775,808"}, // We've gone beyond 64-bit precision. This can only be represented with BigDecimal.
            { NULL, NULL }
    };
    doTest(&rbnf, enTestFullData, false);
}

void IntlTestRBNF::TestCompactDecimalFormatStyle() {
    UErrorCode status = U_ZERO_ERROR;
    UParseError parseError;
    // This is not a common use case, but we're testing it anyway.
    UnicodeString numberPattern("=###0.#####=;"
            "1000: <###0.00< K;"
            "1000000: <###0.00< M;"
            "1000000000: <###0.00< B;"
            "1000000000000: <###0.00< T;"
            "1000000000000000: <###0.00< Q;");
    RuleBasedNumberFormat rbnf(numberPattern, UnicodeString(), Locale::getEnglish(), parseError, status);

    const char * const enTestFullData[][2] = {
            {"1000", "1.00 K"},
            {"1234", "1.23 K"},
            {"999994", "999.99 K"},
            {"999995", "1000.00 K"},
            {"1000000", "1.00 M"},
            {"1200000", "1.20 M"},
            {"1200000000", "1.20 B"},
            {"1200000000000", "1.20 T"},
            {"1200000000000000", "1.20 Q"},
            {"4503599627370495", "4.50 Q"},
            {"4503599627370496", "4.50 Q"},
            {"8990000000000000", "8.99 Q"},
            {"9008000000000000", "9.00 Q"}, // Number doesn't precisely fit into a double
            {"9456000000000000", "9.00 Q"},  // Number doesn't precisely fit into a double
            {"10000000000000000", "10.00 Q"},  // Number doesn't precisely fit into a double
            {"9223372036854775807", "9223.00 Q"}, // Maximum 64-bit precision
            {"9223372036854775808", "9,223,372,036,854,775,808"}, // We've gone beyond 64-bit precision. This can only be represented with BigDecimal.
            { NULL, NULL }
    };
    doTest(&rbnf, enTestFullData, false);
}

void IntlTestRBNF::TestParseFailure() {
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedNumberFormat rbnf(URBNF_SPELLOUT, Locale::getJapanese(), status);
    static const UChar* testData[] = {
        u"・・・・・・・・・・・・・・・・・・・・・・・・"
    };
    if (assertSuccess("", status, true, __FILE__, __LINE__)) {
        for (int i = 0; i < UPRV_LENGTHOF(testData); ++i) {
            UnicodeString spelledNumberString(testData[i]);
            Formattable actualNumber;
            rbnf.parse(spelledNumberString, actualNumber, status);
            if (status != U_INVALID_FORMAT_ERROR) { // I would have expected U_PARSE_ERROR, but NumberFormat::parse gives U_INVALID_FORMAT_ERROR
                errln("FAIL: string should be unparseable index=%d %s", i, u_errorName(status));
            }
        }
    }
}

void IntlTestRBNF::TestMinMaxIntegerDigitsIgnored() {
    IcuTestErrorCode status(*this, "TestMinMaxIntegerDigitsIgnored");

    // NOTE: SimpleDateFormat has an optimization that depends on the fact that min/max integer digits
    // do not affect RBNF (see SimpleDateFormat#zeroPaddingNumber).
    RuleBasedNumberFormat rbnf(URBNF_SPELLOUT, "en", status);
    if (status.isSuccess()) {
        rbnf.setMinimumIntegerDigits(2);
        rbnf.setMaximumIntegerDigits(3);
        UnicodeString result;
        rbnf.format(3, result.remove(), status);
        assertEquals("Min integer digits are ignored", u"three", result);
        rbnf.format(1012, result.remove(), status);
        assertEquals("Max integer digits are ignored", u"one thousand twelve", result);
    }
}

void 
IntlTestRBNF::doTest(RuleBasedNumberFormat* formatter, const char* const testData[][2], UBool testParsing) 
{
  // man, error reporting would be easier with printf-style syntax for unicode string and formattable

    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols dfs("en", status);
    // NumberFormat* decFmt = NumberFormat::createInstance(Locale::getUS(), status);
    DecimalFormat decFmt("#,###.################", dfs, status);
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not create NumberFormat - %s", u_errorName(status));
    } else {
        for (int i = 0; testData[i][0]; ++i) {
            const char* numString = testData[i][0];
            const char* expectedWords = testData[i][1];

            log("[%i] %s = ", i, numString);
            Formattable expectedNumber;
            UnicodeString escapedNumString = UnicodeString(numString, -1, US_INV).unescape();
            decFmt.parse(escapedNumString, expectedNumber, status);
            if (U_FAILURE(status)) {
                errln("FAIL: decFmt could not parse %s", numString);
                break;
            } else {
                UnicodeString actualString;
                FieldPosition pos;
                formatter->format(expectedNumber, actualString/* , pos*/, status);
                if (U_FAILURE(status)) {
                    UnicodeString msg = "Fail: formatter could not format ";
                    decFmt.format(expectedNumber, msg, status);
                    errln(msg);
                    break;
                } else {
                    UnicodeString expectedString = UnicodeString(expectedWords, -1, US_INV).unescape();
                    if (actualString != expectedString) {
                        UnicodeString msg = "FAIL: check failed for ";
                        decFmt.format(expectedNumber, msg, status);
                        msg.append(", expected ");
                        msg.append(expectedString);
                        msg.append(" but got ");
                        msg.append(actualString);
                        errln(msg);
                        break;
                    } else {
                        logln(actualString);
                        if (testParsing) {
                            Formattable parsedNumber;
                            formatter->parse(actualString, parsedNumber, status);
                            if (U_FAILURE(status)) {
                                UnicodeString msg = "FAIL: formatter could not parse ";
                                msg.append(actualString);
                                msg.append(" status code: " );
                                msg.append(u_errorName(status));
                                errln(msg);
                                break;
                            } else {
                                if (parsedNumber != expectedNumber
                                    && (!uprv_isNaN(parsedNumber.getDouble()) || !uprv_isNaN(expectedNumber.getDouble())))
                                {
                                    UnicodeString msg = "FAIL: parse failed for ";
                                    msg.append(actualString);
                                    msg.append(", expected ");
                                    decFmt.format(expectedNumber, msg, status);
                                    msg.append(", but got ");
                                    decFmt.format(parsedNumber, msg, status);
                                    errln(msg);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void 
IntlTestRBNF::doLenientParseTest(RuleBasedNumberFormat* formatter, const char* testData[][2]) 
{
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat* decFmt = NumberFormat::createInstance(Locale::getUS(), status);
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: could not create NumberFormat - %s", u_errorName(status));
    } else {
        for (int i = 0; testData[i][0]; ++i) {
            const char* spelledNumber = testData[i][0]; // spelled-out number
            const char* asciiUSNumber = testData[i][1]; // number as ascii digits formatted for US locale
            
            UnicodeString spelledNumberString = UnicodeString(spelledNumber).unescape();
            Formattable actualNumber;
            formatter->parse(spelledNumberString, actualNumber, status);
            if (U_FAILURE(status)) {
                UnicodeString msg = "FAIL: formatter could not parse ";
                msg.append(spelledNumberString);
                errln(msg);
                break;
            } else {
                // I changed the logic of this test somewhat from Java-- instead of comparing the
                // strings, I compare the Formattables.  Hmmm, but the Formattables don't compare,
                // so change it back.

                UnicodeString asciiUSNumberString = asciiUSNumber;
                Formattable expectedNumber;
                decFmt->parse(asciiUSNumberString, expectedNumber, status);
                if (U_FAILURE(status)) {
                    UnicodeString msg = "FAIL: decFmt could not parse ";
                    msg.append(asciiUSNumberString);
                    errln(msg);
                    break;
                } else {
                    UnicodeString actualNumberString;
                    UnicodeString expectedNumberString;
                    decFmt->format(actualNumber, actualNumberString, status);
                    decFmt->format(expectedNumber, expectedNumberString, status);
                    if (actualNumberString != expectedNumberString) {
                        UnicodeString msg = "FAIL: parsing";
                        msg.append(asciiUSNumberString);
                        msg.append("\n");
                        msg.append("  lenient parse failed for ");
                        msg.append(spelledNumberString);
                        msg.append(", expected ");
                        msg.append(expectedNumberString);
                        msg.append(", but got ");
                        msg.append(actualNumberString);
                        errln(msg);
                        break;
                    }
                }
            }
        }
        delete decFmt;
    }
}

/* U_HAVE_RBNF */
#else

void
IntlTestRBNF::TestRBNFDisabled() {
    errln("*** RBNF currently disabled on this platform ***\n");
}

/* U_HAVE_RBNF */
#endif

#endif /* #if !UCONFIG_NO_FORMATTING */
