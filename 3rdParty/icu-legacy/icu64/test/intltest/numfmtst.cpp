// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/* Modification History:
*   Date        Name        Description
*   07/15/99    helena      Ported to HPUX 10/11 CC.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numfmtst.h"
#include "unicode/currpinf.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/localpointer.h"
#include "unicode/ucurr.h"
#include "unicode/ustring.h"
#include "unicode/measfmt.h"
#include "unicode/curramt.h"
#include "unicode/strenum.h"
#include "textfile.h"
#include "tokiter.h"
#include "charstr.h"
#include "cstr.h"
#include "putilimp.h"
#include "winnmtst.h"
#include <cmath>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include "cmemory.h"
#include "cstring.h"
#include "unicode/numsys.h"
#include "fmtableimp.h"
#include "numberformattesttuple.h"
#include "unicode/msgfmt.h"
#include "number_decimalquantity.h"
#include "unicode/numberformatter.h"

#if (U_PLATFORM == U_PF_AIX) || (U_PLATFORM == U_PF_OS390)
// These should not be macros. If they are,
// replace them with std::isnan and std::isinf
#if defined(isnan)
#undef isnan
namespace std {
 bool isnan(double x) {
   return _isnan(x);
 }
}
#endif
#if defined(isinf)
#undef isinf
namespace std {
 bool isinf(double x) {
   return _isinf(x);
 }
}
#endif
#endif

using icu::number::impl::DecimalQuantity;
using namespace icu::number;

//#define NUMFMTST_CACHE_DEBUG 1
#include "stdio.h" /* for sprintf */
// #include "iostream"   // for cout

//#define NUMFMTST_DEBUG 1

static const UChar EUR[] = {69,85,82,0}; // "EUR"
static const UChar ISO_CURRENCY_USD[] = {0x55, 0x53, 0x44, 0}; // "USD"


// *****************************************************************************
// class NumberFormatTest
// *****************************************************************************

#define CHECK(status,str) if (U_FAILURE(status)) { errcheckln(status, UnicodeString("FAIL: ") + str + " - " + u_errorName(status)); return; }
#define CHECK_DATA(status,str) if (U_FAILURE(status)) { dataerrln(UnicodeString("FAIL: ") + str + " - " + u_errorName(status)); return; }

void NumberFormatTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestCurrencySign);
  TESTCASE_AUTO(TestCurrency);
  TESTCASE_AUTO(TestParse);
  TESTCASE_AUTO(TestRounding487);
  TESTCASE_AUTO(TestQuotes);
  TESTCASE_AUTO(TestExponential);
  TESTCASE_AUTO(TestPatterns);
  TESTCASE_AUTO(Test20186_SpacesAroundSemicolon);

  // Upgrade to alphaWorks - liu 5/99
  TESTCASE_AUTO(TestExponent);
  TESTCASE_AUTO(TestScientific);
  TESTCASE_AUTO(TestPad);
  TESTCASE_AUTO(TestPatterns2);
  TESTCASE_AUTO(TestSecondaryGrouping);
  TESTCASE_AUTO(TestSurrogateSupport);
  TESTCASE_AUTO(TestAPI);

  TESTCASE_AUTO(TestCurrencyObject);
  TESTCASE_AUTO(TestCurrencyPatterns);
  //TESTCASE_AUTO(TestDigitList);
  TESTCASE_AUTO(TestWhiteSpaceParsing);
  TESTCASE_AUTO(TestComplexCurrency);  // This test removed because CLDR no longer uses choice formats in currency symbols.
  TESTCASE_AUTO(TestRegCurrency);
  TESTCASE_AUTO(TestSymbolsWithBadLocale);
  TESTCASE_AUTO(TestAdoptDecimalFormatSymbols);

  TESTCASE_AUTO(TestScientific2);
  TESTCASE_AUTO(TestScientificGrouping);
  TESTCASE_AUTO(TestInt64);

  TESTCASE_AUTO(TestPerMill);
  TESTCASE_AUTO(TestIllegalPatterns);
  TESTCASE_AUTO(TestCases);

  TESTCASE_AUTO(TestCurrencyNames);
  TESTCASE_AUTO(Test20484_NarrowSymbolFallback);
  TESTCASE_AUTO(TestCurrencyAmount);
  TESTCASE_AUTO(TestCurrencyUnit);
  TESTCASE_AUTO(TestCoverage);
  TESTCASE_AUTO(TestLocalizedPatternSymbolCoverage);
  TESTCASE_AUTO(TestJB3832);
  TESTCASE_AUTO(TestHost);
  TESTCASE_AUTO(TestHostClone);
  TESTCASE_AUTO(TestCurrencyFormat);
  TESTCASE_AUTO(TestRounding);
  TESTCASE_AUTO(TestNonpositiveMultiplier);
  TESTCASE_AUTO(TestNumberingSystems);
  TESTCASE_AUTO(TestSpaceParsing);
  TESTCASE_AUTO(TestMultiCurrencySign);
  TESTCASE_AUTO(TestCurrencyFormatForMixParsing);
  TESTCASE_AUTO(TestMismatchedCurrencyFormatFail);
  TESTCASE_AUTO(TestDecimalFormatCurrencyParse);
  TESTCASE_AUTO(TestCurrencyIsoPluralFormat);
  TESTCASE_AUTO(TestCurrencyParsing);
  TESTCASE_AUTO(TestParseCurrencyInUCurr);
  TESTCASE_AUTO(TestFormatAttributes);
  TESTCASE_AUTO(TestFieldPositionIterator);
  TESTCASE_AUTO(TestDecimal);
  TESTCASE_AUTO(TestCurrencyFractionDigits);
  TESTCASE_AUTO(TestExponentParse);
  TESTCASE_AUTO(TestExplicitParents);
  TESTCASE_AUTO(TestLenientParse);
  TESTCASE_AUTO(TestAvailableNumberingSystems);
  TESTCASE_AUTO(TestRoundingPattern);
  TESTCASE_AUTO(Test9087);
  TESTCASE_AUTO(TestFormatFastpaths);
  TESTCASE_AUTO(TestFormattableSize);
  TESTCASE_AUTO(TestUFormattable);
  TESTCASE_AUTO(TestSignificantDigits);
  TESTCASE_AUTO(TestShowZero);
  TESTCASE_AUTO(TestCompatibleCurrencies);
  TESTCASE_AUTO(TestBug9936);
  TESTCASE_AUTO(TestParseNegativeWithFaLocale);
  TESTCASE_AUTO(TestParseNegativeWithAlternateMinusSign);
  TESTCASE_AUTO(TestCustomCurrencySignAndSeparator);
  TESTCASE_AUTO(TestParseSignsAndMarks);
  TESTCASE_AUTO(Test10419RoundingWith0FractionDigits);
  TESTCASE_AUTO(Test10468ApplyPattern);
  TESTCASE_AUTO(TestRoundingScientific10542);
  TESTCASE_AUTO(TestZeroScientific10547);
  TESTCASE_AUTO(TestAccountingCurrency);
  TESTCASE_AUTO(TestEquality);
  TESTCASE_AUTO(TestCurrencyUsage);
  TESTCASE_AUTO(TestDoubleLimit11439);
  TESTCASE_AUTO(TestGetAffixes);
  TESTCASE_AUTO(TestToPatternScientific11648);
  TESTCASE_AUTO(TestBenchmark);
  TESTCASE_AUTO(TestCtorApplyPatternDifference);
  TESTCASE_AUTO(TestFractionalDigitsForCurrency);
  TESTCASE_AUTO(TestFormatCurrencyPlural);
  TESTCASE_AUTO(Test11868);
  TESTCASE_AUTO(Test11739_ParseLongCurrency);
  TESTCASE_AUTO(Test13035_MultiCodePointPaddingInPattern);
  TESTCASE_AUTO(Test13737_ParseScientificStrict);
  TESTCASE_AUTO(Test10727_RoundingZero);
  TESTCASE_AUTO(Test11376_getAndSetPositivePrefix);
  TESTCASE_AUTO(Test11475_signRecognition);
  TESTCASE_AUTO(Test11640_getAffixes);
  TESTCASE_AUTO(Test11649_toPatternWithMultiCurrency);
  TESTCASE_AUTO(Test13327_numberingSystemBufferOverflow);
  TESTCASE_AUTO(Test13391_chakmaParsing);
  TESTCASE_AUTO(Test11735_ExceptionIssue);
  TESTCASE_AUTO(Test11035_FormatCurrencyAmount);
  TESTCASE_AUTO(Test11318_DoubleConversion);
  TESTCASE_AUTO(TestParsePercentRegression);
  TESTCASE_AUTO(TestMultiplierWithScale);
  TESTCASE_AUTO(TestFastFormatInt32);
  TESTCASE_AUTO(Test11646_Equality);
  TESTCASE_AUTO(TestParseNaN);
  TESTCASE_AUTO(TestFormatFailIfMoreThanMaxDigits);
  TESTCASE_AUTO(TestParseCaseSensitive);
  TESTCASE_AUTO(TestParseNoExponent);
  TESTCASE_AUTO(TestSignAlwaysShown);
  TESTCASE_AUTO(TestMinimumGroupingDigits);
  TESTCASE_AUTO(Test11897_LocalizedPatternSeparator);
  TESTCASE_AUTO(Test13055_PercentageRounding);
  TESTCASE_AUTO(Test11839);
  TESTCASE_AUTO(Test10354);
  TESTCASE_AUTO(Test11645_ApplyPatternEquality);
  TESTCASE_AUTO(Test12567);
  TESTCASE_AUTO(Test11626_CustomizeCurrencyPluralInfo);
  TESTCASE_AUTO(Test20073_StrictPercentParseErrorIndex);
  TESTCASE_AUTO(Test13056_GroupingSize);
  TESTCASE_AUTO(Test11025_CurrencyPadding);
  TESTCASE_AUTO(Test11648_ExpDecFormatMalPattern);
  TESTCASE_AUTO(Test11649_DecFmtCurrencies);
  TESTCASE_AUTO(Test13148_ParseGroupingSeparators);
  TESTCASE_AUTO(Test12753_PatternDecimalPoint);
  TESTCASE_AUTO(Test11647_PatternCurrencySymbols);
  TESTCASE_AUTO(Test11913_BigDecimal);
  TESTCASE_AUTO(Test11020_RoundingInScientificNotation);
  TESTCASE_AUTO(Test11640_TripleCurrencySymbol);
  TESTCASE_AUTO(Test13763_FieldPositionIteratorOffset);
  TESTCASE_AUTO(Test13777_ParseLongNameNonCurrencyMode);
  TESTCASE_AUTO(Test13804_EmptyStringsWhenParsing);
  TESTCASE_AUTO(Test20037_ScientificIntegerOverflow);
  TESTCASE_AUTO(Test13840_ParseLongStringCrash);
  TESTCASE_AUTO(Test13850_EmptyStringCurrency);
  TESTCASE_AUTO(Test20348_CurrencyPrefixOverride);
  TESTCASE_AUTO(Test20358_GroupingInPattern);
  TESTCASE_AUTO(Test13731_DefaultCurrency);
  TESTCASE_AUTO(Test20499_CurrencyVisibleDigitsPlural);
  TESTCASE_AUTO_END;
}

// -------------------------------------

// Test API (increase code coverage)
void
NumberFormatTest::TestAPI(void)
{
  logln("Test API");
  UErrorCode status = U_ZERO_ERROR;
  NumberFormat *test = NumberFormat::createInstance("root", status);
  if(U_FAILURE(status)) {
    dataerrln("unable to create format object - %s", u_errorName(status));
  }
  if(test != NULL) {
    test->setMinimumIntegerDigits(10);
    test->setMaximumIntegerDigits(1);

    test->setMinimumFractionDigits(10);
    test->setMaximumFractionDigits(1);

    UnicodeString result;
    FieldPosition pos;
    Formattable bla("Paja Patak"); // Donald Duck for non Serbian speakers
    test->format(bla, result, pos, status);
    if(U_SUCCESS(status)) {
      errln("Yuck... Formatted a duck... As a number!");
    } else {
      status = U_ZERO_ERROR;
    }

    result.remove();
    int64_t ll = 12;
    test->format(ll, result);
    assertEquals("format int64_t error", u"2.0", result);

    test->setMinimumIntegerDigits(4);
    test->setMinimumFractionDigits(4);

    result.remove();
    test->format(ll, result);
    assertEquals("format int64_t error", u"0,012.0000", result);

    ParsePosition ppos;
    LocalPointer<CurrencyAmount> currAmt(test->parseCurrency("",ppos));
    // old test for (U_FAILURE(status)) was bogus here, method does not set status!
    if (ppos.getIndex()) {
        errln("Parsed empty string as currency");
    }

    delete test;
  }
}

class StubNumberFormat :public NumberFormat{
public:
    StubNumberFormat(){};
    virtual UnicodeString& format(double ,UnicodeString& appendTo,FieldPosition& ) const {
        return appendTo;
    }
    virtual UnicodeString& format(int32_t ,UnicodeString& appendTo,FieldPosition& ) const {
        return appendTo.append((UChar)0x0033);
    }
    virtual UnicodeString& format(int64_t number,UnicodeString& appendTo,FieldPosition& pos) const {
        return NumberFormat::format(number, appendTo, pos);
    }
    virtual UnicodeString& format(const Formattable& , UnicodeString& appendTo, FieldPosition& , UErrorCode& ) const {
        return appendTo;
    }
    virtual void parse(const UnicodeString& ,
                    Formattable& ,
                    ParsePosition& ) const {}
    virtual void parse( const UnicodeString& ,
                        Formattable& ,
                        UErrorCode& ) const {}
    virtual UClassID getDynamicClassID(void) const {
        static char classID = 0;
        return (UClassID)&classID;
    }
    virtual Format* clone() const {return NULL;}
};

void
NumberFormatTest::TestCoverage(void){
    StubNumberFormat stub;
    UnicodeString agent("agent");
    FieldPosition pos;
    int64_t num = 4;
    if (stub.format(num, agent, pos) != UnicodeString("agent3")){
        errln("NumberFormat::format(int64, UnicodString&, FieldPosition&) should delegate to (int32, ,)");
    };
}

void NumberFormatTest::TestLocalizedPatternSymbolCoverage() {
    IcuTestErrorCode errorCode(*this, "TestLocalizedPatternSymbolCoverage");
    // Ticket #12961: DecimalFormat::toLocalizedPattern() is not working as designed.
    DecimalFormatSymbols dfs(errorCode);
    dfs.setSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol, u'⁖');
    dfs.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, u'⁘');
    dfs.setSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol, u'⁙');
    dfs.setSymbol(DecimalFormatSymbols::kDigitSymbol, u'▰');
    dfs.setSymbol(DecimalFormatSymbols::kZeroDigitSymbol, u'໐');
    dfs.setSymbol(DecimalFormatSymbols::kSignificantDigitSymbol, u'⁕');
    dfs.setSymbol(DecimalFormatSymbols::kPlusSignSymbol, u'†');
    dfs.setSymbol(DecimalFormatSymbols::kMinusSignSymbol, u'‡');
    dfs.setSymbol(DecimalFormatSymbols::kPercentSymbol, u'⁜');
    dfs.setSymbol(DecimalFormatSymbols::kPerMillSymbol, u'‱');
    dfs.setSymbol(DecimalFormatSymbols::kExponentialSymbol, u"⁑⁑"); // tests multi-char sequence
    dfs.setSymbol(DecimalFormatSymbols::kPadEscapeSymbol, u'⁂');

    {
        UnicodeString standardPattern(u"#,##0.05+%;#,##0.05-%");
        UnicodeString localizedPattern(u"▰⁖▰▰໐⁘໐໕†⁜⁙▰⁖▰▰໐⁘໐໕‡⁜");

        DecimalFormat df1("#", new DecimalFormatSymbols(dfs), errorCode);
        df1.applyPattern(standardPattern, errorCode);
        DecimalFormat df2("#", new DecimalFormatSymbols(dfs), errorCode);
        df2.applyLocalizedPattern(localizedPattern, errorCode);
        assertTrue("DecimalFormat instances should be equal", df1 == df2);
        UnicodeString p2;
        assertEquals("toPattern should match on localizedPattern instance",
                standardPattern, df2.toPattern(p2));
        UnicodeString lp1;
        assertEquals("toLocalizedPattern should match on standardPattern instance",
                localizedPattern, df1.toLocalizedPattern(lp1));
    }

    {
        UnicodeString standardPattern(u"* @@@E0‰");
        UnicodeString localizedPattern(u"⁂ ⁕⁕⁕⁑⁑໐‱");

        DecimalFormat df1("#", new DecimalFormatSymbols(dfs), errorCode);
        df1.applyPattern(standardPattern, errorCode);
        DecimalFormat df2("#", new DecimalFormatSymbols(dfs), errorCode);
        df2.applyLocalizedPattern(localizedPattern, errorCode);
        assertTrue("DecimalFormat instances should be equal", df1 == df2);
        UnicodeString p2;
        assertEquals("toPattern should match on localizedPattern instance",
                standardPattern, df2.toPattern(p2));
        UnicodeString lp1;
        assertEquals("toLocalizedPattern should match on standardPattern instance",
                localizedPattern, df1.toLocalizedPattern(lp1));
    }
}

// Test various patterns
void
NumberFormatTest::TestPatterns(void)
{
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols sym(Locale::getUS(), status);
    if (U_FAILURE(status)) { errcheckln(status, "FAIL: Could not construct DecimalFormatSymbols - %s", u_errorName(status)); return; }

    const char* pat[]    = { "#.#", "#.", ".#", "#" };
    int32_t pat_length = UPRV_LENGTHOF(pat);
    const char* newpat[] = { "0.#", "0.", "#.0", "0" };
    const char* num[]    = { "0",   "0.", ".0", "0" };
    for (int32_t i=0; i<pat_length; ++i)
    {
        status = U_ZERO_ERROR;
        DecimalFormat fmt(pat[i], sym, status);
        if (U_FAILURE(status)) { errln((UnicodeString)"FAIL: DecimalFormat constructor failed for " + pat[i]); continue; }
        UnicodeString newp; fmt.toPattern(newp);
        if (!(newp == newpat[i]))
            errln((UnicodeString)"FAIL: Pattern " + pat[i] + " should transmute to " + newpat[i] +
                  "; " + newp + " seen instead");

        UnicodeString s; (*(NumberFormat*)&fmt).format((int32_t)0, s);
        if (!(s == num[i]))
        {
            errln((UnicodeString)"FAIL: Pattern " + pat[i] + " should format zero as " + num[i] +
                  "; " + s + " seen instead");
            logln((UnicodeString)"Min integer digits = " + fmt.getMinimumIntegerDigits());
        }
    }
}

void NumberFormatTest::Test20186_SpacesAroundSemicolon() {
    IcuTestErrorCode status(*this, "Test20186_SpacesAroundSemicolon");
    DecimalFormat df(u"0.00 ; -0.00", {"en-us", status}, status);
    expect2(df, 1, u"1.00 ");
    expect2(df, -1, u" -1.00");

    df = DecimalFormat(u"0.00;", {"en-us", status}, status);
    expect2(df, 1, u"1.00");
    expect2(df, -1, u"-1.00");

    df = DecimalFormat(u"0.00;0.00", {"en-us", status}, status);
    expect2(df, 1, u"1.00");
    expect(df, -1, u"1.00");  // parses as 1, not -1

    df = DecimalFormat(u" 0.00 ; -0.00 ", {"en-us", status}, status);
    expect2(df, 1, u" 1.00 ");
    expect2(df, -1, u" -1.00 ");
}

/*
icu_2_4::DigitList::operator== 0 0 2 icuuc24d.dll digitlst.cpp Doug
icu_2_4::DigitList::append 0 0 4 icuin24d.dll digitlst.h Doug
icu_2_4::DigitList::operator!= 0 0 1 icuuc24d.dll digitlst.h Doug
*/
/*
void
NumberFormatTest::TestDigitList(void)
{
  // API coverage for DigitList
  DigitList list1;
  list1.append('1');
  list1.fDecimalAt = 1;
  DigitList list2;
  list2.set((int32_t)1);
  if (list1 != list2) {
    errln("digitlist append, operator!= or set failed ");
  }
  if (!(list1 == list2)) {
    errln("digitlist append, operator== or set failed ");
  }
}
*/

// -------------------------------------

// Test exponential pattern
void
NumberFormatTest::TestExponential(void)
{
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols sym(Locale::getUS(), status);
    if (U_FAILURE(status)) { errcheckln(status, "FAIL: Bad status returned by DecimalFormatSymbols ct - %s", u_errorName(status)); return; }
    const char* pat[] = { "0.####E0", "00.000E00", "##0.######E000", "0.###E0;[0.###E0]"  };
    int32_t pat_length = UPRV_LENGTHOF(pat);

// The following #if statements allow this test to be built and run on
// platforms that do not have standard IEEE numerics.  For example,
// S/390 doubles have an exponent range of -78 to +75.  For the
// following #if statements to work, float.h must define
// DBL_MAX_10_EXP to be a compile-time constant.

// This section may be expanded as needed.

#if DBL_MAX_10_EXP > 300
    double val[] = { 0.01234, 123456789, 1.23e300, -3.141592653e-271 };
    int32_t val_length = UPRV_LENGTHOF(val);
    const char* valFormat[] =
    {
        // 0.####E0
        "1.234E-2", "1.2346E8", "1.23E300", "-3.1416E-271",
        // 00.000E00
        "12.340E-03", "12.346E07", "12.300E299", "-31.416E-272",
        // ##0.######E000
        "12.34E-003", "123.4568E006", "1.23E300", "-314.1593E-273",
        // 0.###E0;[0.###E0]
        "1.234E-2", "1.235E8", "1.23E300", "[3.142E-271]"
    };
    double valParse[] =
    {
        0.01234, 123460000, 1.23E300, -3.1416E-271,
        0.01234, 123460000, 1.23E300, -3.1416E-271,
        0.01234, 123456800, 1.23E300, -3.141593E-271,
        0.01234, 123500000, 1.23E300, -3.142E-271,
    };
#elif DBL_MAX_10_EXP > 70
    double val[] = { 0.01234, 123456789, 1.23e70, -3.141592653e-71 };
    int32_t val_length = UPRV_LENGTHOF(val);
    char* valFormat[] =
    {
        // 0.####E0
        "1.234E-2", "1.2346E8", "1.23E70", "-3.1416E-71",
        // 00.000E00
        "12.340E-03", "12.346E07", "12.300E69", "-31.416E-72",
        // ##0.######E000
        "12.34E-003", "123.4568E006", "12.3E069", "-31.41593E-072",
        // 0.###E0;[0.###E0]
        "1.234E-2", "1.235E8", "1.23E70", "[3.142E-71]"
    };
    double valParse[] =
    {
        0.01234, 123460000, 1.23E70, -3.1416E-71,
        0.01234, 123460000, 1.23E70, -3.1416E-71,
        0.01234, 123456800, 1.23E70, -3.141593E-71,
        0.01234, 123500000, 1.23E70, -3.142E-71,
    };
#else
    // Don't test double conversion
    double* val = 0;
    int32_t val_length = 0;
    char** valFormat = 0;
    double* valParse = 0;
    logln("Warning: Skipping double conversion tests");
#endif

    int32_t lval[] = { 0, -1, 1, 123456789 };
    int32_t lval_length = UPRV_LENGTHOF(lval);
    const char* lvalFormat[] =
    {
        // 0.####E0
        "0E0", "-1E0", "1E0", "1.2346E8",
        // 00.000E00
        "00.000E00", "-10.000E-01", "10.000E-01", "12.346E07",
        // ##0.######E000
        "0E000", "-1E000", "1E000", "123.4568E006",
        // 0.###E0;[0.###E0]
        "0E0", "[1E0]", "1E0", "1.235E8"
    };
    int32_t lvalParse[] =
    {
        0, -1, 1, 123460000,
        0, -1, 1, 123460000,
        0, -1, 1, 123456800,
        0, -1, 1, 123500000,
    };
    int32_t ival = 0, ilval = 0;
    for (int32_t p=0; p<pat_length; ++p)
    {
        DecimalFormat fmt(pat[p], sym, status);
        if (U_FAILURE(status)) { errln("FAIL: Bad status returned by DecimalFormat ct"); continue; }
        UnicodeString pattern;
        logln((UnicodeString)"Pattern \"" + pat[p] + "\" -toPattern-> \"" +
          fmt.toPattern(pattern) + "\"");
        int32_t v;
        for (v=0; v<val_length; ++v)
        {
            UnicodeString s; (*(NumberFormat*)&fmt).format(val[v], s);
            logln((UnicodeString)" " + val[v] + " -format-> " + s);
            if (s != valFormat[v+ival])
                errln((UnicodeString)"FAIL: Expected " + valFormat[v+ival]);

            ParsePosition pos(0);
            Formattable af;
            fmt.parse(s, af, pos);
            double a;
            UBool useEpsilon = FALSE;
            if (af.getType() == Formattable::kLong)
                a = af.getLong();
            else if (af.getType() == Formattable::kDouble) {
                a = af.getDouble();
#if U_PF_OS390 <= U_PLATFORM && U_PLATFORM <= U_PF_OS400
                // S/390 will show a failure like this:
                //| -3.141592652999999e-271 -format-> -3.1416E-271
                //|                          -parse-> -3.1416e-271
                //| FAIL: Expected -3.141599999999999e-271
                // To compensate, we use an epsilon-based equality
                // test on S/390 only.  We don't want to do this in
                // general because it's less exacting.
                useEpsilon = TRUE;
#endif
            }
            else {
                errln(UnicodeString("FAIL: Non-numeric Formattable returned: ") + pattern + " " + s);
                continue;
            }
            if (pos.getIndex() == s.length())
            {
                logln((UnicodeString)"  -parse-> " + a);
                // Use epsilon comparison as necessary
                if ((useEpsilon &&
                    (uprv_fabs(a - valParse[v+ival]) / a > (2*DBL_EPSILON))) ||
                    (!useEpsilon && a != valParse[v+ival]))
                {
                    errln((UnicodeString)"FAIL: Expected " + valParse[v+ival] + " but got " + a
                        + " on input " + s);
                }
            }
            else {
                errln((UnicodeString)"FAIL: Partial parse (" + pos.getIndex() + " chars) -> " + a);
                errln((UnicodeString)"  should be (" + s.length() + " chars) -> " + valParse[v+ival]);
            }
        }
        for (v=0; v<lval_length; ++v)
        {
            UnicodeString s;
            (*(NumberFormat*)&fmt).format(lval[v], s);
            logln((UnicodeString)" " + lval[v] + "L -format-> " + s);
            if (s != lvalFormat[v+ilval])
                errln((UnicodeString)"ERROR: Expected " + lvalFormat[v+ilval] + " Got: " + s);

            ParsePosition pos(0);
            Formattable af;
            fmt.parse(s, af, pos);
            if (af.getType() == Formattable::kLong ||
                af.getType() == Formattable::kInt64) {
                UErrorCode status = U_ZERO_ERROR;
                int32_t a = af.getLong(status);
                if (pos.getIndex() == s.length())
                {
                    logln((UnicodeString)"  -parse-> " + a);
                    if (a != lvalParse[v+ilval])
                        errln((UnicodeString)"FAIL: Expected " + lvalParse[v+ilval] + " but got " + a);
                }
                else
                    errln((UnicodeString)"FAIL: Partial parse (" + pos.getIndex() + " chars) -> " + a);
            }
            else
                errln((UnicodeString)"FAIL: Non-long Formattable returned for " + s
                    + " Double: " + af.getDouble()
                    + ", Long: " + af.getLong());
        }
        ival += val_length;
        ilval += lval_length;
    }
}

void
NumberFormatTest::TestScientific2() {
    // jb 2552
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat* fmt = (DecimalFormat*)NumberFormat::createCurrencyInstance("en_US", status);
    if (U_SUCCESS(status)) {
        double num = 12.34;
        expect(*fmt, num, "$12.34");
        fmt->setScientificNotation(TRUE);
        expect(*fmt, num, "$1.23E1");
        fmt->setScientificNotation(FALSE);
        expect(*fmt, num, "$12.34");
    }
    delete fmt;
}

void
NumberFormatTest::TestScientificGrouping() {
    // jb 2552
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat fmt("##0.00E0",status);
    if (assertSuccess("", status, true, __FILE__, __LINE__)) {
        expect(fmt, .01234, "12.3E-3");
        expect(fmt, .1234, "123E-3");
        expect(fmt, 1.234, "1.23E0");
        expect(fmt, 12.34, "12.3E0");
        expect(fmt, 123.4, "123E0");
        expect(fmt, 1234., "1.23E3");
    }
}

/*static void setFromString(DigitList& dl, const char* str) {
    char c;
    UBool decimalSet = FALSE;
    dl.clear();
    while ((c = *str++)) {
        if (c == '-') {
            dl.fIsPositive = FALSE;
        } else if (c == '+') {
            dl.fIsPositive = TRUE;
        } else if (c == '.') {
            dl.fDecimalAt = dl.fCount;
            decimalSet = TRUE;
        } else {
            dl.append(c);
        }
    }
    if (!decimalSet) {
        dl.fDecimalAt = dl.fCount;
    }
}*/

void
NumberFormatTest::TestInt64() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat fmt("#.#E0",status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        return;
    }
    fmt.setMaximumFractionDigits(20);
    if (U_SUCCESS(status)) {
        expect(fmt, (Formattable)(int64_t)0, "0E0");
        expect(fmt, (Formattable)(int64_t)-1, "-1E0");
        expect(fmt, (Formattable)(int64_t)1, "1E0");
        expect(fmt, (Formattable)(int64_t)2147483647, "2.147483647E9");
        expect(fmt, (Formattable)((int64_t)-2147483647-1), "-2.147483648E9");
        expect(fmt, (Formattable)(int64_t)U_INT64_MAX, "9.223372036854775807E18");
        expect(fmt, (Formattable)(int64_t)U_INT64_MIN, "-9.223372036854775808E18");
    }

    // also test digitlist
/*    int64_t int64max = U_INT64_MAX;
    int64_t int64min = U_INT64_MIN;
    const char* int64maxstr = "9223372036854775807";
    const char* int64minstr = "-9223372036854775808";
    UnicodeString fail("fail: ");

    // test max int64 value
    DigitList dl;
    setFromString(dl, int64maxstr);
    {
        if (!dl.fitsIntoInt64(FALSE)) {
            errln(fail + int64maxstr + " didn't fit");
        }
        int64_t int64Value = dl.getInt64();
        if (int64Value != int64max) {
            errln(fail + int64maxstr);
        }
        dl.set(int64Value);
        int64Value = dl.getInt64();
        if (int64Value != int64max) {
            errln(fail + int64maxstr);
        }
    }
    // test negative of max int64 value (1 shy of min int64 value)
    dl.fIsPositive = FALSE;
    {
        if (!dl.fitsIntoInt64(FALSE)) {
            errln(fail + "-" + int64maxstr + " didn't fit");
        }
        int64_t int64Value = dl.getInt64();
        if (int64Value != -int64max) {
            errln(fail + "-" + int64maxstr);
        }
        dl.set(int64Value);
        int64Value = dl.getInt64();
        if (int64Value != -int64max) {
            errln(fail + "-" + int64maxstr);
        }
    }
    // test min int64 value
    setFromString(dl, int64minstr);
    {
        if (!dl.fitsIntoInt64(FALSE)) {
            errln(fail + "-" + int64minstr + " didn't fit");
        }
        int64_t int64Value = dl.getInt64();
        if (int64Value != int64min) {
            errln(fail + int64minstr);
        }
        dl.set(int64Value);
        int64Value = dl.getInt64();
        if (int64Value != int64min) {
            errln(fail + int64minstr);
        }
    }
    // test negative of min int 64 value (1 more than max int64 value)
    dl.fIsPositive = TRUE; // won't fit
    {
        if (dl.fitsIntoInt64(FALSE)) {
            errln(fail + "-(" + int64minstr + ") didn't fit");
        }
    }*/
}

// -------------------------------------

// Test the handling of quotes
void
NumberFormatTest::TestQuotes(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString *pat;
    DecimalFormatSymbols *sym = new DecimalFormatSymbols(Locale::getUS(), status);
    if (U_FAILURE(status)) {
        errcheckln(status, "Fail to create DecimalFormatSymbols - %s", u_errorName(status));
        delete sym;
        return;
    }
    pat = new UnicodeString("a'fo''o'b#");
    DecimalFormat *fmt = new DecimalFormat(*pat, *sym, status);
    UnicodeString s;
    ((NumberFormat*)fmt)->format((int32_t)123, s);
    logln((UnicodeString)"Pattern \"" + *pat + "\"");
    logln((UnicodeString)" Format 123 -> " + escape(s));
    if (!(s=="afo'ob123"))
        errln((UnicodeString)"FAIL: Expected afo'ob123");

    s.truncate(0);
    delete fmt;
    delete pat;

    pat = new UnicodeString("a''b#");
    fmt = new DecimalFormat(*pat, *sym, status);
    ((NumberFormat*)fmt)->format((int32_t)123, s);
    logln((UnicodeString)"Pattern \"" + *pat + "\"");
    logln((UnicodeString)" Format 123 -> " + escape(s));
    if (!(s=="a'b123"))
        errln((UnicodeString)"FAIL: Expected a'b123");
    delete fmt;
    delete pat;
    delete sym;
}

/**
 * Test the handling of the currency symbol in patterns.
 */
void
NumberFormatTest::TestCurrencySign(void)
{
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols* sym = new DecimalFormatSymbols(Locale::getUS(), status);
    UnicodeString pat;
    UChar currency = 0x00A4;
    if (U_FAILURE(status)) {
        errcheckln(status, "Fail to create DecimalFormatSymbols - %s", u_errorName(status));
        delete sym;
        return;
    }
    // "\xA4#,##0.00;-\xA4#,##0.00"
    pat.append(currency).append("#,##0.00;-").
        append(currency).append("#,##0.00");
    DecimalFormat *fmt = new DecimalFormat(pat, *sym, status);
    UnicodeString s; ((NumberFormat*)fmt)->format(1234.56, s);
    pat.truncate(0);
    logln((UnicodeString)"Pattern \"" + fmt->toPattern(pat) + "\"");
    logln((UnicodeString)" Format " + 1234.56 + " -> " + escape(s));
    if (s != "$1,234.56") dataerrln((UnicodeString)"FAIL: Expected $1,234.56");
    s.truncate(0);
    ((NumberFormat*)fmt)->format(- 1234.56, s);
    logln((UnicodeString)" Format " + (-1234.56) + " -> " + escape(s));
    if (s != "-$1,234.56") dataerrln((UnicodeString)"FAIL: Expected -$1,234.56");
    delete fmt;
    pat.truncate(0);
    // "\xA4\xA4 #,##0.00;\xA4\xA4 -#,##0.00"
    pat.append(currency).append(currency).
        append(" #,##0.00;").
        append(currency).append(currency).
        append(" -#,##0.00");
    fmt = new DecimalFormat(pat, *sym, status);
    s.truncate(0);
    ((NumberFormat*)fmt)->format(1234.56, s);
    logln((UnicodeString)"Pattern \"" + fmt->toPattern(pat) + "\"");
    logln((UnicodeString)" Format " + 1234.56 + " -> " + escape(s));
    if (s != "USD 1,234.56") dataerrln((UnicodeString)"FAIL: Expected USD 1,234.56");
    s.truncate(0);
    ((NumberFormat*)fmt)->format(-1234.56, s);
    logln((UnicodeString)" Format " + (-1234.56) + " -> " + escape(s));
    if (s != "USD -1,234.56") dataerrln((UnicodeString)"FAIL: Expected USD -1,234.56");
    delete fmt;
    delete sym;
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: Status " + u_errorName(status));
}

// -------------------------------------

static UChar toHexString(int32_t i) { return (UChar)(i + (i < 10 ? 0x30 : (0x41 - 10))); }

UnicodeString&
NumberFormatTest::escape(UnicodeString& s)
{
    UnicodeString buf;
    for (int32_t i=0; i<s.length(); ++i)
    {
        UChar c = s[(int32_t)i];
        if (c <= (UChar)0x7F) buf += c;
        else {
            buf += (UChar)0x5c; buf += (UChar)0x55;
            buf += toHexString((c & 0xF000) >> 12);
            buf += toHexString((c & 0x0F00) >> 8);
            buf += toHexString((c & 0x00F0) >> 4);
            buf += toHexString(c & 0x000F);
        }
    }
    return (s = buf);
}


// -------------------------------------
static const char* testCases[][2]= {
     /* locale ID */  /* expected */
    {"ca_ES@currency=ESP", "\\u20A7\\u00A01.150" },
    {"de_LU@currency=LUF", "1,150\\u00A0F" },
    {"el_GR@currency=GRD", "1.150,50\\u00A0\\u0394\\u03C1\\u03C7" },
    {"en_BE@currency=BEF", "1.150,50\\u00A0BEF" },
    {"es_ES@currency=ESP", "1.150\\u00A0\\u20A7" },
    {"eu_ES@currency=ESP", "\\u20A7\\u00A01.150" },
    {"gl_ES@currency=ESP", "1.150\\u00A0\\u20A7" },
    {"it_IT@currency=ITL", "ITL\\u00A01.150" },
    {"pt_PT@currency=PTE", "1,150$50\\u00A0\\u200B"}, // per cldrbug 7670
    {"en_US@currency=JPY", "\\u00A51,150"},
    {"en_US@currency=jpy", "\\u00A51,150"},
    {"en-US-u-cu-jpy", "\\u00A51,150"}
};
/**
 * Test localized currency patterns.
 */
void
NumberFormatTest::TestCurrency(void)
{
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat* currencyFmt = NumberFormat::createCurrencyInstance(Locale::getCanadaFrench(), status);
    if (U_FAILURE(status)) {
        dataerrln("Error calling NumberFormat::createCurrencyInstance()");
        return;
    }

    UnicodeString s; currencyFmt->format(1.50, s);
    logln((UnicodeString)"Un pauvre ici a..........." + s);
    if (!(s==CharsToUnicodeString("1,50\\u00A0$")))
        errln((UnicodeString)"FAIL: Expected 1,50<nbsp>$ but got " + s);
    delete currencyFmt;
    s.truncate(0);
    char loc[256]={0};
    int len = uloc_canonicalize("de_DE@currency=DEM", loc, 256, &status);
    (void)len;  // Suppress unused variable warning.
    currencyFmt = NumberFormat::createCurrencyInstance(Locale(loc),status);
    currencyFmt->format(1.50, s);
    logln((UnicodeString)"Un pauvre en Allemagne a.." + s);
    if (!(s==CharsToUnicodeString("1,50\\u00A0DM")))
        errln((UnicodeString)"FAIL: Expected 1,50<nbsp>DM but got " + s);
    delete currencyFmt;
    s.truncate(0);
    len = uloc_canonicalize("fr_FR@currency=FRF", loc, 256, &status);
    currencyFmt = NumberFormat::createCurrencyInstance(Locale(loc), status);
    currencyFmt->format(1.50, s);
    logln((UnicodeString)"Un pauvre en France a....." + s);
    if (!(s==CharsToUnicodeString("1,50\\u00A0F")))
        errln((UnicodeString)"FAIL: Expected 1,50<nbsp>F");
    delete currencyFmt;
    if (U_FAILURE(status))
        errln((UnicodeString)"FAIL: Status " + (int32_t)status);

    for(int i=0; i < UPRV_LENGTHOF(testCases); i++){
        status = U_ZERO_ERROR;
        const char *localeID = testCases[i][0];
        UnicodeString expected(testCases[i][1], -1, US_INV);
        expected = expected.unescape();
        s.truncate(0);
        char loc[256]={0};
        uloc_canonicalize(localeID, loc, 256, &status);
        currencyFmt = NumberFormat::createCurrencyInstance(Locale(loc), status);
        if(U_FAILURE(status)){
            errln("Could not create currency formatter for locale %s",localeID);
            continue;
        }
        currencyFmt->format(1150.50, s);
        if(s!=expected){
            errln(UnicodeString("FAIL: Expected: ")+expected
                    + UnicodeString(" Got: ") + s
                    + UnicodeString( " for locale: ")+ UnicodeString(localeID) );
        }
        if (U_FAILURE(status)){
            errln((UnicodeString)"FAIL: Status " + (int32_t)status);
        }
        delete currencyFmt;
    }
}

// -------------------------------------

/**
 * Test the Currency object handling, new as of ICU 2.2.
 */
void NumberFormatTest::TestCurrencyObject() {
    UErrorCode ec = U_ZERO_ERROR;
    NumberFormat* fmt =
        NumberFormat::createCurrencyInstance(Locale::getUS(), ec);

    if (U_FAILURE(ec)) {
        dataerrln("FAIL: getCurrencyInstance(US) - %s", u_errorName(ec));
        delete fmt;
        return;
    }

    Locale null("", "", "");

    expectCurrency(*fmt, null, 1234.56, "$1,234.56");

    expectCurrency(*fmt, Locale::getFrance(),
                   1234.56, CharsToUnicodeString("\\u20AC1,234.56")); // Euro

    expectCurrency(*fmt, Locale::getJapan(),
                   1234.56, CharsToUnicodeString("\\u00A51,235")); // Yen

    expectCurrency(*fmt, Locale("fr", "CH", ""),
                   1234.56, "CHF 1,234.56"); // no more 0.05 rounding here, see cldrbug 5548

    expectCurrency(*fmt, Locale::getUS(),
                   1234.56, "$1,234.56");

    delete fmt;
    fmt = NumberFormat::createCurrencyInstance(Locale::getFrance(), ec);

    if (U_FAILURE(ec)) {
        errln("FAIL: getCurrencyInstance(FRANCE)");
        delete fmt;
        return;
    }

    expectCurrency(*fmt, null, 1234.56, CharsToUnicodeString("1\\u202F234,56 \\u20AC"));

    expectCurrency(*fmt, Locale::getJapan(),
                   1234.56, CharsToUnicodeString("1\\u202F235 JPY")); // Yen

    expectCurrency(*fmt, Locale("fr", "CH", ""),
                   1234.56, CharsToUnicodeString("1\\u202F234,56 CHF")); // no more 0.05 rounding here, see cldrbug 5548

    expectCurrency(*fmt, Locale::getUS(),
                   1234.56, CharsToUnicodeString("1\\u202F234,56 $US"));

    expectCurrency(*fmt, Locale::getFrance(),
                   1234.56, CharsToUnicodeString("1\\u202F234,56 \\u20AC")); // Euro

    delete fmt;
}

// -------------------------------------

/**
 * Do rudimentary testing of parsing.
 */
void
NumberFormatTest::TestParse(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString arg("0");
    DecimalFormat* format = new DecimalFormat("00", status);
    //try {
        Formattable n; format->parse(arg, n, status);
        logln((UnicodeString)"parse(" + arg + ") = " + n.getLong());
        if (n.getType() != Formattable::kLong ||
            n.getLong() != 0) errln((UnicodeString)"FAIL: Expected 0");
    delete format;
    if (U_FAILURE(status)) errcheckln(status, (UnicodeString)"FAIL: Status " + u_errorName(status));
    //}
    //catch(Exception e) {
    //    errln((UnicodeString)"Exception caught: " + e);
    //}
}

// -------------------------------------

static const char *lenientAffixTestCases[] = {
        "(1)",
        "( 1)",
        "(1 )",
        "( 1 )"
};

static const char *lenientMinusTestCases[] = {
    "-5",
    "\\u22125",
    "\\u27965"
};

static const char *lenientCurrencyTestCases[] = {
        "$1,000",
        "$ 1,000",
        "$1000",
        "$ 1000",
        "$1 000.00",
        "$ 1 000.00",
        "$ 1\\u00A0000.00",
        "1000.00"
};

// changed from () to - per cldrbug 5674
static const char *lenientNegativeCurrencyTestCases[] = {
        "-$1,000",
        "-$ 1,000",
        "-$1000",
        "-$ 1000",
        "-$1 000.00",
        "-$ 1 000.00",
        "- $ 1,000.00 ",
        "-$ 1\\u00A0000.00",
        "-1000.00"
};

static const char *lenientPercentTestCases[] = {
        "25%",
        " 25%",
        " 25 %",
    	"25 %",
		"25\\u00A0%",
		"25"
};

static const char *lenientNegativePercentTestCases[] = {
		"-25%",
		" -25%",
		" - 25%",
		"- 25 %",
		" - 25 %",
		"-25 %",
		"-25\\u00A0%",
		"-25",
		"- 25"
};

static const char *strictFailureTestCases[] = {
		" 1000",
		"10,00",
		"1,000,.0"
};

/**
 * Test lenient parsing.
 */
void
NumberFormatTest::TestLenientParse(void)
{
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat *format = new DecimalFormat("(#,##0)", status);
    Formattable n;

    if (format == NULL || U_FAILURE(status)) {
        dataerrln("Unable to create DecimalFormat (#,##0) - %s", u_errorName(status));
    } else {
        format->setLenient(TRUE);
        for (int32_t t = 0; t < UPRV_LENGTHOF (lenientAffixTestCases); t += 1) {
        	UnicodeString testCase = ctou(lenientAffixTestCases[t]);

            format->parse(testCase, n, status);
            logln((UnicodeString)"parse(" + testCase + ") = " + n.getLong());

            if (U_FAILURE(status) || n.getType() != Formattable::kLong ||
            	n.getLong() != 1) {
            	dataerrln((UnicodeString)"Lenient parse failed for \"" + (UnicodeString) lenientAffixTestCases[t]
                      + (UnicodeString) "\"; error code = " + u_errorName(status));
            	status = U_ZERO_ERROR;
            }
       }
       delete format;
    }

    Locale en_US("en_US");
    Locale sv_SE("sv_SE");

    NumberFormat *mFormat = NumberFormat::createInstance(sv_SE, UNUM_DECIMAL, status);

    if (mFormat == NULL || U_FAILURE(status)) {
        dataerrln("Unable to create NumberFormat (sv_SE, UNUM_DECIMAL) - %s", u_errorName(status));
    } else {
        mFormat->setLenient(TRUE);
        for (int32_t t = 0; t < UPRV_LENGTHOF(lenientMinusTestCases); t += 1) {
            UnicodeString testCase = ctou(lenientMinusTestCases[t]);

            mFormat->parse(testCase, n, status);
            logln((UnicodeString)"parse(" + testCase + ") = " + n.getLong());

            if (U_FAILURE(status) || n.getType() != Formattable::kLong || n.getLong() != -5) {
                errln((UnicodeString)"Lenient parse failed for \"" + (UnicodeString) lenientMinusTestCases[t]
                      + (UnicodeString) "\"; error code = " + u_errorName(status));
                status = U_ZERO_ERROR;
            }
        }
        delete mFormat;
    }

    mFormat = NumberFormat::createInstance(en_US, UNUM_DECIMAL, status);

    if (mFormat == NULL || U_FAILURE(status)) {
        dataerrln("Unable to create NumberFormat (en_US, UNUM_DECIMAL) - %s", u_errorName(status));
    } else {
        mFormat->setLenient(TRUE);
        for (int32_t t = 0; t < UPRV_LENGTHOF(lenientMinusTestCases); t += 1) {
            UnicodeString testCase = ctou(lenientMinusTestCases[t]);

            mFormat->parse(testCase, n, status);
            logln((UnicodeString)"parse(" + testCase + ") = " + n.getLong());

            if (U_FAILURE(status) || n.getType() != Formattable::kLong || n.getLong() != -5) {
                errln((UnicodeString)"Lenient parse failed for \"" + (UnicodeString) lenientMinusTestCases[t]
                      + (UnicodeString) "\"; error code = " + u_errorName(status));
                status = U_ZERO_ERROR;
            }
        }
        delete mFormat;
    }

    NumberFormat *cFormat = NumberFormat::createInstance(en_US, UNUM_CURRENCY, status);

    if (cFormat == NULL || U_FAILURE(status)) {
        dataerrln("Unable to create NumberFormat (en_US, UNUM_CURRENCY) - %s", u_errorName(status));
    } else {
        cFormat->setLenient(TRUE);
        for (int32_t t = 0; t < UPRV_LENGTHOF (lenientCurrencyTestCases); t += 1) {
        	UnicodeString testCase = ctou(lenientCurrencyTestCases[t]);

            cFormat->parse(testCase, n, status);
            logln((UnicodeString)"parse(" + testCase + ") = " + n.getLong());

            if (U_FAILURE(status) ||n.getType() != Formattable::kLong ||
            	n.getLong() != 1000) {
            	errln((UnicodeString)"Lenient parse failed for \"" + (UnicodeString) lenientCurrencyTestCases[t]
                      + (UnicodeString) "\"; error code = " + u_errorName(status));
            	status = U_ZERO_ERROR;
            }
        }

        for (int32_t t = 0; t < UPRV_LENGTHOF (lenientNegativeCurrencyTestCases); t += 1) {
        	UnicodeString testCase = ctou(lenientNegativeCurrencyTestCases[t]);

            cFormat->parse(testCase, n, status);
            logln((UnicodeString)"parse(" + testCase + ") = " + n.getLong());

            if (U_FAILURE(status) ||n.getType() != Formattable::kLong ||
            	n.getLong() != -1000) {
            	errln((UnicodeString)"Lenient parse failed for \"" + (UnicodeString) lenientNegativeCurrencyTestCases[t]
                      + (UnicodeString) "\"; error code = " + u_errorName(status));
            	status = U_ZERO_ERROR;
            }
        }

        delete cFormat;
    }

    NumberFormat *pFormat = NumberFormat::createPercentInstance(en_US, status);

    if (pFormat == NULL || U_FAILURE(status)) {
        dataerrln("Unable to create NumberFormat::createPercentInstance (en_US) - %s", u_errorName(status));
    } else {
        pFormat->setLenient(TRUE);
        for (int32_t t = 0; t < UPRV_LENGTHOF (lenientPercentTestCases); t += 1) {
        	UnicodeString testCase = ctou(lenientPercentTestCases[t]);

        	pFormat->parse(testCase, n, status);
            logln((UnicodeString)"parse(" + testCase + ") = " + n.getDouble());

            if (U_FAILURE(status) ||n.getType() != Formattable::kDouble ||
            	n.getDouble() != 0.25) {
            	errln((UnicodeString)"Lenient parse failed for \"" + (UnicodeString) lenientPercentTestCases[t]
                      + (UnicodeString) "\"; error code = " + u_errorName(status)
                      + "; got: " + n.getDouble(status));
            	status = U_ZERO_ERROR;
            }
        }

        for (int32_t t = 0; t < UPRV_LENGTHOF (lenientNegativePercentTestCases); t += 1) {
        	UnicodeString testCase = ctou(lenientNegativePercentTestCases[t]);

        	pFormat->parse(testCase, n, status);
            logln((UnicodeString)"parse(" + testCase + ") = " + n.getDouble());

            if (U_FAILURE(status) ||n.getType() != Formattable::kDouble ||
            	n.getDouble() != -0.25) {
            	errln((UnicodeString)"Lenient parse failed for \"" + (UnicodeString) lenientNegativePercentTestCases[t]
                      + (UnicodeString) "\"; error code = " + u_errorName(status)
                      + "; got: " + n.getDouble(status));
            	status = U_ZERO_ERROR;
            }
        }

        delete pFormat;
    }

   // Test cases that should fail with a strict parse and pass with a
   // lenient parse.
   NumberFormat *nFormat = NumberFormat::createInstance(en_US, status);

   if (nFormat == NULL || U_FAILURE(status)) {
       dataerrln("Unable to create NumberFormat (en_US) - %s", u_errorName(status));
   } else {
       // first, make sure that they fail with a strict parse
       for (int32_t t = 0; t < UPRV_LENGTHOF(strictFailureTestCases); t += 1) {
	       UnicodeString testCase = ctou(strictFailureTestCases[t]);

	       nFormat->parse(testCase, n, status);
	       logln((UnicodeString)"parse(" + testCase + ") = " + n.getLong());

	       if (! U_FAILURE(status)) {
		       errln((UnicodeString)"Strict Parse succeeded for \"" + (UnicodeString) strictFailureTestCases[t]
                     + (UnicodeString) "\"; error code = " + u_errorName(status));
	       }

	       status = U_ZERO_ERROR;
       }

       // then, make sure that they pass with a lenient parse
       nFormat->setLenient(TRUE);
       for (int32_t t = 0; t < UPRV_LENGTHOF(strictFailureTestCases); t += 1) {
	       UnicodeString testCase = ctou(strictFailureTestCases[t]);

	       nFormat->parse(testCase, n, status);
	       logln((UnicodeString)"parse(" + testCase + ") = " + n.getLong());

	       if (U_FAILURE(status) ||n.getType() != Formattable::kLong ||
	            	n.getLong() != 1000) {
		       errln((UnicodeString)"Lenient parse failed for \"" + (UnicodeString) strictFailureTestCases[t]
                     + (UnicodeString) "\"; error code = " + u_errorName(status));
		       status = U_ZERO_ERROR;
	       }
       }

       delete nFormat;
   }
}

// -------------------------------------

/**
 * Test proper rounding by the format method.
 */
void
NumberFormatTest::TestRounding487(void)
{
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat *nf = NumberFormat::createInstance(status);
    if (U_FAILURE(status)) {
        dataerrln("Error calling NumberFormat::createInstance()");
        return;
    }

    roundingTest(*nf, 0.00159999, 4, "0.0016");
    roundingTest(*nf, 0.00995, 4, "0.01");

    roundingTest(*nf, 12.3995, 3, "12.4");

    roundingTest(*nf, 12.4999, 0, "12");
    roundingTest(*nf, - 19.5, 0, "-20");
    delete nf;
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: Status " + (int32_t)status);
}

/**
 * Test the functioning of the secondary grouping value.
 */
void NumberFormatTest::TestSecondaryGrouping(void) {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols US(Locale::getUS(), status);
    CHECK(status, "DecimalFormatSymbols ct");

    DecimalFormat f("#,##,###", US, status);
    CHECK(status, "DecimalFormat ct");

    expect2(f, (int32_t)123456789L, "12,34,56,789");
    expectPat(f, "#,##,##0");
    f.applyPattern("#,###", status);
    CHECK(status, "applyPattern");

    f.setSecondaryGroupingSize(4);
    expect2(f, (int32_t)123456789L, "12,3456,789");
    expectPat(f, "#,####,##0");
    NumberFormat *g = NumberFormat::createInstance(Locale("hi", "IN"), status);
    CHECK_DATA(status, "createInstance(hi_IN)");

    UnicodeString out;
    int32_t l = (int32_t)1876543210L;
    g->format(l, out);
    delete g;
    // expect "1,87,65,43,210", but with Hindi digits
    //         01234567890123
    UBool ok = TRUE;
    if (out.length() != 14) {
        ok = FALSE;
    } else {
        for (int32_t i=0; i<out.length(); ++i) {
            UBool expectGroup = FALSE;
            switch (i) {
            case 1:
            case 4:
            case 7:
            case 10:
                expectGroup = TRUE;
                break;
            }
            // Later -- fix this to get the actual grouping
            // character from the resource bundle.
            UBool isGroup = (out.charAt(i) == 0x002C);
            if (isGroup != expectGroup) {
                ok = FALSE;
                break;
            }
        }
    }
    if (!ok) {
        errln((UnicodeString)"FAIL  Expected " + l +
              " x hi_IN -> \"1,87,65,43,210\" (with Hindi digits), got \"" +
              escape(out) + "\"");
    } else {
        logln((UnicodeString)"Ok    " + l +
              " x hi_IN -> \"" +
              escape(out) + "\"");
    }
}

void NumberFormatTest::TestWhiteSpaceParsing(void) {
    UErrorCode ec = U_ZERO_ERROR;
    DecimalFormatSymbols US(Locale::getUS(), ec);
    DecimalFormat fmt("a  b#0c  ", US, ec);
    if (U_FAILURE(ec)) {
        errcheckln(ec, "FAIL: Constructor - %s", u_errorName(ec));
        return;
    }
    // From ICU 62, flexible whitespace needs lenient mode
    fmt.setLenient(TRUE);
    int32_t n = 1234;
    expect(fmt, "a b1234c ", n);
    expect(fmt, "a   b1234c   ", n);
}

/**
 * Test currencies whose display name is a ChoiceFormat.
 */
void NumberFormatTest::TestComplexCurrency() {

//    UErrorCode ec = U_ZERO_ERROR;
//    Locale loc("kn", "IN", "");
//    NumberFormat* fmt = NumberFormat::createCurrencyInstance(loc, ec);
//    if (U_SUCCESS(ec)) {
//        expect2(*fmt, 1.0, CharsToUnicodeString("Re.\\u00A01.00"));
//        Use .00392625 because that's 2^-8.  Any value less than 0.005 is fine.
//        expect(*fmt, 1.00390625, CharsToUnicodeString("Re.\\u00A01.00")); // tricky
//        expect2(*fmt, 12345678.0, CharsToUnicodeString("Rs.\\u00A01,23,45,678.00"));
//        expect2(*fmt, 0.5, CharsToUnicodeString("Rs.\\u00A00.50"));
//        expect2(*fmt, -1.0, CharsToUnicodeString("-Re.\\u00A01.00"));
//        expect2(*fmt, -10.0, CharsToUnicodeString("-Rs.\\u00A010.00"));
//    } else {
//        errln("FAIL: getCurrencyInstance(kn_IN)");
//    }
//    delete fmt;

}

// -------------------------------------

void
NumberFormatTest::roundingTest(NumberFormat& nf, double x, int32_t maxFractionDigits, const char* expected)
{
    nf.setMaximumFractionDigits(maxFractionDigits);
    UnicodeString out; nf.format(x, out);
    logln((UnicodeString)"" + x + " formats with " + maxFractionDigits + " fractional digits to " + out);
    if (!(out==expected)) errln((UnicodeString)"FAIL: Expected " + expected);
}

/**
 * Upgrade to alphaWorks
 */
void NumberFormatTest::TestExponent(void) {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols US(Locale::getUS(), status);
    CHECK(status, "DecimalFormatSymbols constructor");
    DecimalFormat fmt1(UnicodeString("0.###E0"), US, status);
    CHECK(status, "DecimalFormat(0.###E0)");
    DecimalFormat fmt2(UnicodeString("0.###E+0"), US, status);
    CHECK(status, "DecimalFormat(0.###E+0)");
    int32_t n = 1234;
    expect2(fmt1, n, "1.234E3");
    expect2(fmt2, n, "1.234E+3");
    expect(fmt1, "1.234E+3", n); // Either format should parse "E+3"
}

/**
 * Upgrade to alphaWorks
 */
void NumberFormatTest::TestScientific(void) {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols US(Locale::getUS(), status);
    CHECK(status, "DecimalFormatSymbols constructor");

    // Test pattern round-trip
    const char* PAT[] = { "#E0", "0.####E0", "00.000E00", "##0.####E000",
                          "0.###E0;[0.###E0]" };
    int32_t PAT_length = UPRV_LENGTHOF(PAT);
    int32_t DIGITS[] = {
        // min int, max int, min frac, max frac
        1, 1, 0, 0, // "#E0"
        1, 1, 0, 4, // "0.####E0"
        2, 2, 3, 3, // "00.000E00"
        1, 3, 0, 4, // "##0.####E000"
        1, 1, 0, 3, // "0.###E0;[0.###E0]"
    };
    for (int32_t i=0; i<PAT_length; ++i) {
        UnicodeString pat(PAT[i]);
        DecimalFormat df(pat, US, status);
        CHECK(status, "DecimalFormat constructor");
        UnicodeString pat2;
        df.toPattern(pat2);
        if (pat == pat2) {
            logln(UnicodeString("Ok   Pattern rt \"") +
                  pat + "\" -> \"" +
                  pat2 + "\"");
        } else {
            errln(UnicodeString("FAIL Pattern rt \"") +
                  pat + "\" -> \"" +
                  pat2 + "\"");
        }
        // Make sure digit counts match what we expect
        if (df.getMinimumIntegerDigits() != DIGITS[4*i] ||
            df.getMaximumIntegerDigits() != DIGITS[4*i+1] ||
            df.getMinimumFractionDigits() != DIGITS[4*i+2] ||
            df.getMaximumFractionDigits() != DIGITS[4*i+3]) {
            errln(UnicodeString("FAIL \"" + pat +
                                "\" min/max int; min/max frac = ") +
                  df.getMinimumIntegerDigits() + "/" +
                  df.getMaximumIntegerDigits() + ";" +
                  df.getMinimumFractionDigits() + "/" +
                  df.getMaximumFractionDigits() + ", expect " +
                  DIGITS[4*i] + "/" +
                  DIGITS[4*i+1] + ";" +
                  DIGITS[4*i+2] + "/" +
                  DIGITS[4*i+3]);
        }
    }


    // Test the constructor for default locale. We have to
    // manually set the default locale, as there is no
    // guarantee that the default locale has the same
    // scientific format.
    Locale def = Locale::getDefault();
    Locale::setDefault(Locale::getUS(), status);
    expect2(NumberFormat::createScientificInstance(status),
           12345.678901,
           "1.2345678901E4", status);
    Locale::setDefault(def, status);

    expect2(new DecimalFormat("#E0", US, status),
           12345.0,
           "1.2345E4", status);
    expect(new DecimalFormat("0E0", US, status),
           12345.0,
           "1E4", status);
    expect2(NumberFormat::createScientificInstance(Locale::getUS(), status),
           12345.678901,
           "1.2345678901E4", status);
    expect(new DecimalFormat("##0.###E0", US, status),
           12345.0,
           "12.34E3", status);
    expect(new DecimalFormat("##0.###E0", US, status),
           12345.00001,
           "12.35E3", status);
    expect2(new DecimalFormat("##0.####E0", US, status),
           (int32_t) 12345,
           "12.345E3", status);
    expect2(NumberFormat::createScientificInstance(Locale::getFrance(), status),
           12345.678901,
           "1,2345678901E4", status);
    expect(new DecimalFormat("##0.####E0", US, status),
           789.12345e-9,
           "789.12E-9", status);
    expect2(new DecimalFormat("##0.####E0", US, status),
           780.e-9,
           "780E-9", status);
    expect(new DecimalFormat(".###E0", US, status),
           45678.0,
           ".457E5", status);
    expect2(new DecimalFormat(".###E0", US, status),
           (int32_t) 0,
           ".0E0", status);
    /*
    expect(new DecimalFormat[] { new DecimalFormat("#E0", US),
                                 new DecimalFormat("##E0", US),
                                 new DecimalFormat("####E0", US),
                                 new DecimalFormat("0E0", US),
                                 new DecimalFormat("00E0", US),
                                 new DecimalFormat("000E0", US),
                               },
           new Long(45678000),
           new String[] { "4.5678E7",
                          "45.678E6",
                          "4567.8E4",
                          "5E7",
                          "46E6",
                          "457E5",
                        }
           );
    !
    ! Unroll this test into individual tests below...
    !
    */
    expect2(new DecimalFormat("#E0", US, status),
           (int32_t) 45678000, "4.5678E7", status);
    expect2(new DecimalFormat("##E0", US, status),
           (int32_t) 45678000, "45.678E6", status);
    expect2(new DecimalFormat("####E0", US, status),
           (int32_t) 45678000, "4567.8E4", status);
    expect(new DecimalFormat("0E0", US, status),
           (int32_t) 45678000, "5E7", status);
    expect(new DecimalFormat("00E0", US, status),
           (int32_t) 45678000, "46E6", status);
    expect(new DecimalFormat("000E0", US, status),
           (int32_t) 45678000, "457E5", status);
    /*
    expect(new DecimalFormat("###E0", US, status),
           new Object[] { new Double(0.0000123), "12.3E-6",
                          new Double(0.000123), "123E-6",
                          new Double(0.00123), "1.23E-3",
                          new Double(0.0123), "12.3E-3",
                          new Double(0.123), "123E-3",
                          new Double(1.23), "1.23E0",
                          new Double(12.3), "12.3E0",
                          new Double(123), "123E0",
                          new Double(1230), "1.23E3",
                         });
    !
    ! Unroll this test into individual tests below...
    !
    */
    expect2(new DecimalFormat("###E0", US, status),
           0.0000123, "12.3E-6", status);
    expect2(new DecimalFormat("###E0", US, status),
           0.000123, "123E-6", status);
    expect2(new DecimalFormat("###E0", US, status),
           0.00123, "1.23E-3", status);
    expect2(new DecimalFormat("###E0", US, status),
           0.0123, "12.3E-3", status);
    expect2(new DecimalFormat("###E0", US, status),
           0.123, "123E-3", status);
    expect2(new DecimalFormat("###E0", US, status),
           1.23, "1.23E0", status);
    expect2(new DecimalFormat("###E0", US, status),
           12.3, "12.3E0", status);
    expect2(new DecimalFormat("###E0", US, status),
           123.0, "123E0", status);
    expect2(new DecimalFormat("###E0", US, status),
           1230.0, "1.23E3", status);
    /*
    expect(new DecimalFormat("0.#E+00", US, status),
           new Object[] { new Double(0.00012), "1.2E-04",
                          new Long(12000),     "1.2E+04",
                         });
    !
    ! Unroll this test into individual tests below...
    !
    */
    expect2(new DecimalFormat("0.#E+00", US, status),
           0.00012, "1.2E-04", status);
    expect2(new DecimalFormat("0.#E+00", US, status),
           (int32_t) 12000, "1.2E+04", status);
}

/**
 * Upgrade to alphaWorks
 */
void NumberFormatTest::TestPad(void) {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols US(Locale::getUS(), status);
    CHECK(status, "DecimalFormatSymbols constructor");

    expect2(new DecimalFormat("*^##.##", US, status),
           int32_t(0), "^^^^0", status);
    expect2(new DecimalFormat("*^##.##", US, status),
           -1.3, "^-1.3", status);
    expect2(new DecimalFormat("##0.0####E0*_ 'g-m/s^2'", US, status),
           int32_t(0), "0.0E0______ g-m/s^2", status);
    expect(new DecimalFormat("##0.0####E0*_ 'g-m/s^2'", US, status),
           1.0/3, "333.333E-3_ g-m/s^2", status);
    expect2(new DecimalFormat("##0.0####*_ 'g-m/s^2'", US, status),
           int32_t(0), "0.0______ g-m/s^2", status);
    expect(new DecimalFormat("##0.0####*_ 'g-m/s^2'", US, status),
           1.0/3, "0.33333__ g-m/s^2", status);

    // Test padding before a sign
    const char *formatStr = "*x#,###,###,##0.0#;*x(###,###,##0.0#)";
    expect2(new DecimalFormat(formatStr, US, status),
           int32_t(-10),  "xxxxxxxxxx(10.0)", status);
    expect2(new DecimalFormat(formatStr, US, status),
           int32_t(-1000),"xxxxxxx(1,000.0)", status);
    expect2(new DecimalFormat(formatStr, US, status),
           int32_t(-1000000),"xxx(1,000,000.0)", status);
    expect2(new DecimalFormat(formatStr, US, status),
           -100.37,       "xxxxxxxx(100.37)", status);
    expect2(new DecimalFormat(formatStr, US, status),
           -10456.37,     "xxxxx(10,456.37)", status);
    expect2(new DecimalFormat(formatStr, US, status),
           -1120456.37,   "xx(1,120,456.37)", status);
    expect2(new DecimalFormat(formatStr, US, status),
           -112045600.37, "(112,045,600.37)", status);
    expect2(new DecimalFormat(formatStr, US, status),
           -1252045600.37,"(1,252,045,600.37)", status);

    expect2(new DecimalFormat(formatStr, US, status),
           int32_t(10),  "xxxxxxxxxxxx10.0", status);
    expect2(new DecimalFormat(formatStr, US, status),
           int32_t(1000),"xxxxxxxxx1,000.0", status);
    expect2(new DecimalFormat(formatStr, US, status),
           int32_t(1000000),"xxxxx1,000,000.0", status);
    expect2(new DecimalFormat(formatStr, US, status),
           100.37,       "xxxxxxxxxx100.37", status);
    expect2(new DecimalFormat(formatStr, US, status),
           10456.37,     "xxxxxxx10,456.37", status);
    expect2(new DecimalFormat(formatStr, US, status),
           1120456.37,   "xxxx1,120,456.37", status);
    expect2(new DecimalFormat(formatStr, US, status),
           112045600.37, "xx112,045,600.37", status);
    expect2(new DecimalFormat(formatStr, US, status),
           10252045600.37,"10,252,045,600.37", status);


    // Test padding between a sign and a number
    const char *formatStr2 = "#,###,###,##0.0#*x;(###,###,##0.0#*x)";
    expect2(new DecimalFormat(formatStr2, US, status),
           int32_t(-10),  "(10.0xxxxxxxxxx)", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           int32_t(-1000),"(1,000.0xxxxxxx)", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           int32_t(-1000000),"(1,000,000.0xxx)", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           -100.37,       "(100.37xxxxxxxx)", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           -10456.37,     "(10,456.37xxxxx)", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           -1120456.37,   "(1,120,456.37xx)", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           -112045600.37, "(112,045,600.37)", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           -1252045600.37,"(1,252,045,600.37)", status);

    expect2(new DecimalFormat(formatStr2, US, status),
           int32_t(10),  "10.0xxxxxxxxxxxx", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           int32_t(1000),"1,000.0xxxxxxxxx", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           int32_t(1000000),"1,000,000.0xxxxx", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           100.37,       "100.37xxxxxxxxxx", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           10456.37,     "10,456.37xxxxxxx", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           1120456.37,   "1,120,456.37xxxx", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           112045600.37, "112,045,600.37xx", status);
    expect2(new DecimalFormat(formatStr2, US, status),
           10252045600.37,"10,252,045,600.37", status);

    //testing the setPadCharacter(UnicodeString) and getPadCharacterString()
    DecimalFormat fmt("#", US, status);
    CHECK(status, "DecimalFormat constructor");
    UnicodeString padString("P");
    fmt.setPadCharacter(padString);
    expectPad(fmt, "*P##.##", DecimalFormat::kPadBeforePrefix, 5, padString);
    fmt.setPadCharacter((UnicodeString)"^");
    expectPad(fmt, "*^#", DecimalFormat::kPadBeforePrefix, 1, (UnicodeString)"^");
    //commented untill implementation is complete
  /*  fmt.setPadCharacter((UnicodeString)"^^^");
    expectPad(fmt, "*^^^#", DecimalFormat::kPadBeforePrefix, 3, (UnicodeString)"^^^");
    padString.remove();
    padString.append((UChar)0x0061);
    padString.append((UChar)0x0302);
    fmt.setPadCharacter(padString);
    UChar patternChars[]={0x002a, 0x0061, 0x0302, 0x0061, 0x0302, 0x0023, 0x0000};
    UnicodeString pattern(patternChars);
    expectPad(fmt, pattern , DecimalFormat::kPadBeforePrefix, 4, padString);
 */

}

/**
 * Upgrade to alphaWorks
 */
void NumberFormatTest::TestPatterns2(void) {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols US(Locale::getUS(), status);
    CHECK(status, "DecimalFormatSymbols constructor");

    DecimalFormat fmt("#", US, status);
    CHECK(status, "DecimalFormat constructor");

    UChar hat = 0x005E; /*^*/

    expectPad(fmt, "*^#", DecimalFormat::kPadBeforePrefix, 1, hat);
    expectPad(fmt, "$*^#", DecimalFormat::kPadAfterPrefix, 2, hat);
    expectPad(fmt, "#*^", DecimalFormat::kPadBeforeSuffix, 1, hat);
    expectPad(fmt, "#$*^", DecimalFormat::kPadAfterSuffix, 2, hat);
    expectPad(fmt, "$*^$#", ILLEGAL);
    expectPad(fmt, "#$*^$", ILLEGAL);
    expectPad(fmt, "'pre'#,##0*x'post'", DecimalFormat::kPadBeforeSuffix,
              12, (UChar)0x0078 /*x*/);
    expectPad(fmt, "''#0*x", DecimalFormat::kPadBeforeSuffix,
              3, (UChar)0x0078 /*x*/);
    expectPad(fmt, "'I''ll'*a###.##", DecimalFormat::kPadAfterPrefix,
              10, (UChar)0x0061 /*a*/);

    fmt.applyPattern("AA#,##0.00ZZ", status);
    CHECK(status, "applyPattern");
    fmt.setPadCharacter(hat);

    fmt.setFormatWidth(10);

    fmt.setPadPosition(DecimalFormat::kPadBeforePrefix);
    expectPat(fmt, "*^AA#,##0.00ZZ");

    fmt.setPadPosition(DecimalFormat::kPadBeforeSuffix);
    expectPat(fmt, "AA#,##0.00*^ZZ");

    fmt.setPadPosition(DecimalFormat::kPadAfterSuffix);
    expectPat(fmt, "AA#,##0.00ZZ*^");

    //            12  3456789012
    UnicodeString exp("AA*^#,##0.00ZZ", "");
    fmt.setFormatWidth(12);
    fmt.setPadPosition(DecimalFormat::kPadAfterPrefix);
    expectPat(fmt, exp);

    fmt.setFormatWidth(13);
    //              12  34567890123
    expectPat(fmt, "AA*^##,##0.00ZZ");

    fmt.setFormatWidth(14);
    //              12  345678901234
    expectPat(fmt, "AA*^###,##0.00ZZ");

    fmt.setFormatWidth(15);
    //              12  3456789012345
    expectPat(fmt, "AA*^####,##0.00ZZ"); // This is the interesting case

    fmt.setFormatWidth(16);
    //              12  34567890123456
    expectPat(fmt, "AA*^#####,##0.00ZZ");
}

void NumberFormatTest::TestSurrogateSupport(void) {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols custom(Locale::getUS(), status);
    CHECK(status, "DecimalFormatSymbols constructor");

    custom.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, "decimal");
    custom.setSymbol(DecimalFormatSymbols::kPlusSignSymbol, "plus");
    custom.setSymbol(DecimalFormatSymbols::kMinusSignSymbol, " minus ");
    custom.setSymbol(DecimalFormatSymbols::kExponentialSymbol, "exponent");

    UnicodeString patternStr("*\\U00010000##.##", "");
    patternStr = patternStr.unescape();
    UnicodeString expStr("\\U00010000\\U00010000\\U00010000\\U000100000", "");
    expStr = expStr.unescape();
    expect2(new DecimalFormat(patternStr, custom, status),
           int32_t(0), expStr, status);

    status = U_ZERO_ERROR;
    expect2(new DecimalFormat("*^##.##", custom, status),
           int32_t(0), "^^^^0", status);
    status = U_ZERO_ERROR;
    expect2(new DecimalFormat("##.##", custom, status),
           -1.3, " minus 1decimal3", status);
    status = U_ZERO_ERROR;
    expect2(new DecimalFormat("##0.0####E0 'g-m/s^2'", custom, status),
           int32_t(0), "0decimal0exponent0 g-m/s^2", status);
    status = U_ZERO_ERROR;
    expect(new DecimalFormat("##0.0####E0 'g-m/s^2'", custom, status),
           1.0/3, "333decimal333exponent minus 3 g-m/s^2", status);
    status = U_ZERO_ERROR;
    expect2(new DecimalFormat("##0.0#### 'g-m/s^2'", custom, status),
           int32_t(0), "0decimal0 g-m/s^2", status);
    status = U_ZERO_ERROR;
    expect(new DecimalFormat("##0.0#### 'g-m/s^2'", custom, status),
           1.0/3, "0decimal33333 g-m/s^2", status);

    UnicodeString zero((UChar32)0x10000);
    UnicodeString one((UChar32)0x10001);
    UnicodeString two((UChar32)0x10002);
    UnicodeString five((UChar32)0x10005);
    custom.setSymbol(DecimalFormatSymbols::kZeroDigitSymbol, zero);
    custom.setSymbol(DecimalFormatSymbols::kOneDigitSymbol, one);
    custom.setSymbol(DecimalFormatSymbols::kTwoDigitSymbol, two);
    custom.setSymbol(DecimalFormatSymbols::kFiveDigitSymbol, five);
    expStr = UnicodeString("\\U00010001decimal\\U00010002\\U00010005\\U00010000", "");
    expStr = expStr.unescape();
    status = U_ZERO_ERROR;
    expect2(new DecimalFormat("##0.000", custom, status),
           1.25, expStr, status);

    custom.setSymbol(DecimalFormatSymbols::kZeroDigitSymbol, (UChar)0x30);
    custom.setSymbol(DecimalFormatSymbols::kCurrencySymbol, "units of money");
    custom.setSymbol(DecimalFormatSymbols::kMonetarySeparatorSymbol, "money separator");
    patternStr = UNICODE_STRING_SIMPLE("0.00 \\u00A4' in your bank account'");
    patternStr = patternStr.unescape();
    expStr = UnicodeString(" minus 20money separator00 units of money in your bank account", "");
    status = U_ZERO_ERROR;
    expect2(new DecimalFormat(patternStr, custom, status),
           int32_t(-20), expStr, status);

    custom.setSymbol(DecimalFormatSymbols::kPercentSymbol, "percent");
    patternStr = "'You''ve lost ' -0.00 %' of your money today'";
    patternStr = patternStr.unescape();
    expStr = UnicodeString(" minus You've lost   minus 2000decimal00 percent of your money today", "");
    status = U_ZERO_ERROR;
    expect2(new DecimalFormat(patternStr, custom, status),
           int32_t(-20), expStr, status);
}

void NumberFormatTest::TestCurrencyPatterns(void) {
    int32_t i, locCount;
    const Locale* locs = NumberFormat::getAvailableLocales(locCount);
    for (i=0; i<locCount; ++i) {
        UErrorCode ec = U_ZERO_ERROR;
        NumberFormat* nf = NumberFormat::createCurrencyInstance(locs[i], ec);
        if (U_FAILURE(ec)) {
            errln("FAIL: Can't create NumberFormat(%s) - %s", locs[i].getName(), u_errorName(ec));
        } else {
            // Make sure currency formats do not have a variable number
            // of fraction digits
            int32_t min = nf->getMinimumFractionDigits();
            int32_t max = nf->getMaximumFractionDigits();
            if (min != max) {
                UnicodeString a, b;
                nf->format(1.0, a);
                nf->format(1.125, b);
                errln((UnicodeString)"FAIL: " + locs[i].getName() +
                      " min fraction digits != max fraction digits; "
                      "x 1.0 => " + escape(a) +
                      "; x 1.125 => " + escape(b));
            }

            // Make sure EURO currency formats have exactly 2 fraction digits
            DecimalFormat* df = dynamic_cast<DecimalFormat*>(nf);
            if (df != NULL) {
                if (u_strcmp(EUR, df->getCurrency()) == 0) {
                    if (min != 2 || max != 2) {
                        UnicodeString a;
                        nf->format(1.0, a);
                        errln((UnicodeString)"FAIL: " + locs[i].getName() +
                              " is a EURO format but it does not have 2 fraction digits; "
                              "x 1.0 => " +
                              escape(a));
                    }
                }
            }
        }
        delete nf;
    }
}

void NumberFormatTest::TestRegCurrency(void) {
#if !UCONFIG_NO_SERVICE
    UErrorCode status = U_ZERO_ERROR;
    UChar USD[4];
    ucurr_forLocale("en_US", USD, 4, &status);
    UChar YEN[4];
    ucurr_forLocale("ja_JP", YEN, 4, &status);
    UChar TMP[4];

    if(U_FAILURE(status)) {
        errcheckln(status, "Unable to get currency for locale, error %s", u_errorName(status));
        return;
    }

    UCurrRegistryKey enkey = ucurr_register(YEN, "en_US", &status);

    ucurr_forLocale("en_US", TMP, 4, &status);
    if (u_strcmp(YEN, TMP) != 0) {
        errln("FAIL: didn't return YEN registered for en_US");
    }

    int32_t fallbackLen = ucurr_forLocale("en_XX_BAR", TMP, 4, &status);
    if (fallbackLen) {
        errln("FAIL: tried to fallback en_XX_BAR");
    }
    status = U_ZERO_ERROR; // reset

    if (!ucurr_unregister(enkey, &status)) {
        errln("FAIL: couldn't unregister enkey");
    }

    ucurr_forLocale("en_US", TMP, 4, &status);
    if (u_strcmp(USD, TMP) != 0) {
        errln("FAIL: didn't return USD for en_US after unregister of en_US");
    }
    status = U_ZERO_ERROR; // reset

    ucurr_forLocale("en_US_BLAH", TMP, 4, &status);
    if (u_strcmp(USD, TMP) != 0) {
        errln("FAIL: could not find USD for en_US_BLAH after unregister of en");
    }
    status = U_ZERO_ERROR; // reset
#endif
}

void NumberFormatTest::TestCurrencyNames(void) {
    // Do a basic check of getName()
    // USD { "US$", "US Dollar"            } // 04/04/1792-
    UErrorCode ec = U_ZERO_ERROR;
    static const UChar USD[] = {0x55, 0x53, 0x44, 0}; /*USD*/
    static const UChar USX[] = {0x55, 0x53, 0x58, 0}; /*USX*/
    static const UChar CAD[] = {0x43, 0x41, 0x44, 0}; /*CAD*/
    static const UChar ITL[] = {0x49, 0x54, 0x4C, 0}; /*ITL*/
    UBool isChoiceFormat;
    int32_t len;
    const UBool possibleDataError = TRUE;
    // Warning: HARD-CODED LOCALE DATA in this test.  If it fails, CHECK
    // THE LOCALE DATA before diving into the code.
    assertEquals("USD.getName(SYMBOL_NAME, en)",
                 UnicodeString("$"),
                 UnicodeString(ucurr_getName(USD, "en",
                                             UCURR_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("USD.getName(NARROW_SYMBOL_NAME, en)",
                 UnicodeString("$"),
                 UnicodeString(ucurr_getName(USD, "en",
                                             UCURR_NARROW_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("USD.getName(LONG_NAME, en)",
                 UnicodeString("US Dollar"),
                 UnicodeString(ucurr_getName(USD, "en",
                                             UCURR_LONG_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("CAD.getName(SYMBOL_NAME, en)",
                 UnicodeString("CA$"),
                 UnicodeString(ucurr_getName(CAD, "en",
                                             UCURR_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("CAD.getName(NARROW_SYMBOL_NAME, en)",
                 UnicodeString("$"),
                 UnicodeString(ucurr_getName(CAD, "en",
                                             UCURR_NARROW_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("CAD.getName(SYMBOL_NAME, en_CA)",
                 UnicodeString("$"),
                 UnicodeString(ucurr_getName(CAD, "en_CA",
                                             UCURR_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("USD.getName(SYMBOL_NAME, en_CA)",
                 UnicodeString("US$"),
                 UnicodeString(ucurr_getName(USD, "en_CA",
                                             UCURR_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("USD.getName(NARROW_SYMBOL_NAME, en_CA)",
                 UnicodeString("$"),
                 UnicodeString(ucurr_getName(USD, "en_CA",
                                             UCURR_NARROW_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("USD.getName(SYMBOL_NAME) in en_NZ",
                 UnicodeString("US$"),
                 UnicodeString(ucurr_getName(USD, "en_NZ",
                                             UCURR_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("CAD.getName(SYMBOL_NAME)",
                 UnicodeString("CA$"),
                 UnicodeString(ucurr_getName(CAD, "en_NZ",
                                             UCURR_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("USX.getName(SYMBOL_NAME)",
                 UnicodeString("USX"),
                 UnicodeString(ucurr_getName(USX, "en_US",
                                             UCURR_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("USX.getName(NARROW_SYMBOL_NAME)",
                 UnicodeString("USX"),
                 UnicodeString(ucurr_getName(USX, "en_US",
                                             UCURR_NARROW_SYMBOL_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertEquals("USX.getName(LONG_NAME)",
                 UnicodeString("USX"),
                 UnicodeString(ucurr_getName(USX, "en_US",
                                             UCURR_LONG_NAME,
                                             &isChoiceFormat, &len, &ec)),
                                             possibleDataError);
    assertSuccess("ucurr_getName", ec);

    ec = U_ZERO_ERROR;

    // Test that a default or fallback warning is being returned. JB 4239.
    ucurr_getName(CAD, "es_ES", UCURR_LONG_NAME, &isChoiceFormat,
                            &len, &ec);
    assertTrue("ucurr_getName (es_ES fallback)",
                    U_USING_FALLBACK_WARNING == ec, TRUE, possibleDataError);

    ucurr_getName(CAD, "zh_TW", UCURR_LONG_NAME, &isChoiceFormat,
                            &len, &ec);
    assertTrue("ucurr_getName (zh_TW fallback)",
                    U_USING_FALLBACK_WARNING == ec, TRUE, possibleDataError);

    ucurr_getName(CAD, "en_US", UCURR_LONG_NAME, &isChoiceFormat,
                            &len, &ec);
    assertTrue("ucurr_getName (en_US default)",
                    U_USING_DEFAULT_WARNING == ec || U_USING_FALLBACK_WARNING == ec, TRUE);

    ucurr_getName(CAD, "ti", UCURR_LONG_NAME, &isChoiceFormat,
                            &len, &ec);
    assertTrue("ucurr_getName (ti default)",
                    U_USING_DEFAULT_WARNING == ec, TRUE);

    // Test that a default warning is being returned when falling back to root. JB 4536.
    ucurr_getName(ITL, "cy", UCURR_LONG_NAME, &isChoiceFormat,
                            &len, &ec);
    assertTrue("ucurr_getName (cy default to root)",
                    U_USING_DEFAULT_WARNING == ec, TRUE);

    // TODO add more tests later
}

void NumberFormatTest::Test20484_NarrowSymbolFallback(){
    IcuTestErrorCode status(*this, "Test20484_NarrowSymbolFallback");

    struct TestCase {
        const char* locale;
        const char16_t* isoCode;
        const char16_t* expectedShort;
        const char16_t* expectedNarrow;
        UErrorCode expectedNarrowError;
    } cases[] = {
        {"en-US", u"CAD", u"CA$", u"$", U_USING_DEFAULT_WARNING}, // narrow: fallback to root
        {"en-US", u"CDF", u"CDF", u"CDF", U_USING_FALLBACK_WARNING}, // narrow: fallback to short
        {"sw-CD", u"CDF", u"FC", u"FC", U_USING_FALLBACK_WARNING}, // narrow: fallback to short
        {"en-US", u"GEL", u"GEL", u"₾", U_USING_DEFAULT_WARNING}, // narrow: fallback to root
        {"ka-GE", u"GEL", u"₾", u"₾", U_USING_FALLBACK_WARNING}, // narrow: fallback to ka
        {"ka", u"GEL", u"₾", u"₾", U_ZERO_ERROR}, // no fallback on narrow
    };
    for (const auto& cas : cases) {
        status.setScope(cas.isoCode);
        UBool choiceFormatIgnored;
        int32_t lengthIgnored;
        const UChar* actualShort = ucurr_getName(
            cas.isoCode,
            cas.locale,
            UCURR_SYMBOL_NAME,
            &choiceFormatIgnored,
            &lengthIgnored,
            status);
        status.errIfFailureAndReset();
        const UChar* actualNarrow = ucurr_getName(
            cas.isoCode,
            cas.locale,
            UCURR_NARROW_SYMBOL_NAME,
            &choiceFormatIgnored,
            &lengthIgnored,
            status);
        status.expectErrorAndReset(cas.expectedNarrowError);
        assertEquals(UnicodeString("Short symbol: ") + cas.locale + u": " + cas.isoCode,
                cas.expectedShort, actualShort);
        assertEquals(UnicodeString("Narrow symbol: ") + cas.locale + ": " + cas.isoCode,
                cas.expectedNarrow, actualNarrow);
    }
}

void NumberFormatTest::TestCurrencyUnit(void){
    UErrorCode ec = U_ZERO_ERROR;
    static const UChar USD[]  = u"USD";
    static const char USD8[]  =  "USD";
    static const UChar BAD[]  = u"???";
    static const UChar BAD2[] = u"??A";
    static const UChar XXX[]  = u"XXX";
    static const char XXX8[]  =  "XXX";
    static const UChar INV[]  = u"{$%";
    static const char INV8[]  =  "{$%";
    static const UChar ZZZ[]  = u"zz";
    static const char ZZZ8[]  = "zz";

    UChar* EUR = (UChar*) malloc(6);
    EUR[0] = u'E';
    EUR[1] = u'U';
    EUR[2] = u'R';
    char* EUR8 = (char*) malloc(3);
    EUR8[0] = 'E';
    EUR8[1] = 'U';
    EUR8[2] = 'R';

    CurrencyUnit cu(USD, ec);
    assertSuccess("CurrencyUnit", ec);

    assertEquals("getISOCurrency()", USD, cu.getISOCurrency());
    assertEquals("getSubtype()", USD8, cu.getSubtype());

    CurrencyUnit inv(INV, ec);
    assertEquals("non-invariant", U_INVARIANT_CONVERSION_ERROR, ec);
    assertEquals("non-invariant", XXX, inv.getISOCurrency());
    ec = U_ZERO_ERROR;

    CurrencyUnit zzz(ZZZ, ec);
    assertEquals("too short", U_ILLEGAL_ARGUMENT_ERROR, ec);
    assertEquals("too short", XXX, zzz.getISOCurrency());
    ec = U_ZERO_ERROR;

    CurrencyUnit eur(EUR, ec);
    assertEquals("non-nul-terminated", u"EUR", eur.getISOCurrency());
    assertEquals("non-nul-terminated", "EUR", eur.getSubtype());

    // Test StringPiece constructor
    CurrencyUnit cu8(USD8, ec);
    assertEquals("StringPiece constructor", USD, cu8.getISOCurrency());

    CurrencyUnit inv8(INV8, ec);
    assertEquals("non-invariant 8", U_INVARIANT_CONVERSION_ERROR, ec);
    assertEquals("non-invariant 8", XXX, inv8.getISOCurrency());
    ec = U_ZERO_ERROR;

    CurrencyUnit zzz8(ZZZ8, ec);
    assertEquals("too short 8", U_ILLEGAL_ARGUMENT_ERROR, ec);
    assertEquals("too short 8", XXX, zzz8.getISOCurrency());
    ec = U_ZERO_ERROR;

    CurrencyUnit zzz8b({ZZZ8, 3}, ec);
    assertEquals("too short 8b", U_ILLEGAL_ARGUMENT_ERROR, ec);
    assertEquals("too short 8b", XXX, zzz8b.getISOCurrency());
    ec = U_ZERO_ERROR;

    CurrencyUnit eur8({EUR8, 3}, ec);
    assertEquals("non-nul-terminated 8", u"EUR", eur8.getISOCurrency());
    assertEquals("non-nul-terminated 8", "EUR", eur8.getSubtype());

    CurrencyUnit cu2(cu);
    if (!(cu2 == cu)){
        errln("CurrencyUnit copy constructed object should be same");
    }

    CurrencyUnit * cu3 = (CurrencyUnit *)cu.clone();
    if (!(*cu3 == cu)){
        errln("CurrencyUnit cloned object should be same");
    }
    CurrencyUnit bad(BAD, ec);
    assertSuccess("CurrencyUnit", ec);
    if (cu.getIndex() == bad.getIndex()) {
        errln("Indexes of different currencies should differ.");
    }
    CurrencyUnit bad2(BAD2, ec);
    assertSuccess("CurrencyUnit", ec);
    if (bad2.getIndex() != bad.getIndex()) {
        errln("Indexes of unrecognized currencies should be the same.");
    }
    if (bad == bad2) {
        errln("Different unrecognized currencies should not be equal.");
    }
    bad = bad2;
    if (bad != bad2) {
        errln("Currency unit assignment should be the same.");
    }
    delete cu3;

    // Test default constructor
    CurrencyUnit def;
    assertEquals("Default currency", XXX, def.getISOCurrency());
    assertEquals("Default currency as subtype", XXX8, def.getSubtype());

    // Test slicing
    MeasureUnit sliced1 = cu;
    MeasureUnit sliced2 = cu;
    assertEquals("Subtype after slicing 1", USD8, sliced1.getSubtype());
    assertEquals("Subtype after slicing 2", USD8, sliced2.getSubtype());
    CurrencyUnit restored1(sliced1, ec);
    CurrencyUnit restored2(sliced2, ec);
    assertSuccess("Restoring from MeasureUnit", ec);
    assertEquals("Subtype after restoring 1", USD8, restored1.getSubtype());
    assertEquals("Subtype after restoring 2", USD8, restored2.getSubtype());
    assertEquals("ISO Code after restoring 1", USD, restored1.getISOCurrency());
    assertEquals("ISO Code after restoring 2", USD, restored2.getISOCurrency());

    // Test copy constructor failure
    LocalPointer<MeasureUnit> meter(MeasureUnit::createMeter(ec));
    assertSuccess("Creating meter", ec);
    CurrencyUnit failure(*meter, ec);
    assertEquals("Copying from meter should fail", ec, U_ILLEGAL_ARGUMENT_ERROR);
    assertEquals("Copying should not give uninitialized ISO code", u"", failure.getISOCurrency());

    free(EUR);
    free(EUR8);
}

void NumberFormatTest::TestCurrencyAmount(void){
    UErrorCode ec = U_ZERO_ERROR;
    static const UChar USD[] = {85, 83, 68, 0}; /*USD*/
    CurrencyAmount ca(9, USD, ec);
    assertSuccess("CurrencyAmount", ec);

    CurrencyAmount ca2(ca);
    if (!(ca2 == ca)){
        errln("CurrencyAmount copy constructed object should be same");
    }

    ca2=ca;
    if (!(ca2 == ca)){
        errln("CurrencyAmount assigned object should be same");
    }

    CurrencyAmount *ca3 = (CurrencyAmount *)ca.clone();
    if (!(*ca3 == ca)){
        errln("CurrencyAmount cloned object should be same");
    }
    delete ca3;
}

void NumberFormatTest::TestSymbolsWithBadLocale(void) {
    Locale locDefault;
    static const char *badLocales[] = {
        // length < ULOC_FULLNAME_CAPACITY
        "x-crazy_ZZ_MY_SPECIAL_ADMINISTRATION_REGION_NEEDS_A_SPECIAL_VARIANT_WITH_A_REALLY_REALLY_REALLY_REALLY_REALLY_REALLY_REALLY_LONG_NAME",

        // length > ULOC_FULLNAME_CAPACITY
        "x-crazy_ZZ_MY_SPECIAL_ADMINISTRATION_REGION_NEEDS_A_SPECIAL_VARIANT_WITH_A_REALLY_REALLY_REALLY_REALLY_REALLY_REALLY_REALLY_REALLY_REALLY_REALLY_REALLY_LONG_NAME"
    }; // expect U_USING_DEFAULT_WARNING for both

    unsigned int i;
    for (i = 0; i < UPRV_LENGTHOF(badLocales); i++) {
        const char *localeName = badLocales[i];
        Locale locBad(localeName);
        assertTrue(WHERE, !locBad.isBogus());
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString intlCurrencySymbol((UChar)0xa4);

        intlCurrencySymbol.append((UChar)0xa4);

        logln("Current locale is %s", Locale::getDefault().getName());
        Locale::setDefault(locBad, status);
        logln("Current locale is %s", Locale::getDefault().getName());
        DecimalFormatSymbols mySymbols(status);
        if (status != U_USING_DEFAULT_WARNING) {
            errln("DecimalFormatSymbols should return U_USING_DEFAULT_WARNING.");
        }
        if (strcmp(mySymbols.getLocale().getName(), locBad.getName()) != 0) {
            errln("DecimalFormatSymbols does not have the right locale.", locBad.getName());
        }
        int symbolEnum = (int)DecimalFormatSymbols::kDecimalSeparatorSymbol;
        for (; symbolEnum < (int)DecimalFormatSymbols::kFormatSymbolCount; symbolEnum++) {
            UnicodeString symbolString = mySymbols.getSymbol((DecimalFormatSymbols::ENumberFormatSymbol)symbolEnum);
            logln(UnicodeString("DecimalFormatSymbols[") + symbolEnum + UnicodeString("] = ") + prettify(symbolString));
            if (symbolString.length() == 0
                && symbolEnum != (int)DecimalFormatSymbols::kGroupingSeparatorSymbol
                && symbolEnum != (int)DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol)
            {
                errln("DecimalFormatSymbols has an empty string at index %d.", symbolEnum);
            }
        }

        status = U_ZERO_ERROR;
        Locale::setDefault(locDefault, status);
        logln("Current locale is %s", Locale::getDefault().getName());
    }
}

/**
 * Check that adoptDecimalFormatSymbols and setDecimalFormatSymbols
 * behave the same, except for memory ownership semantics. (No
 * version of this test on Java, since Java has only one method.)
 */
void NumberFormatTest::TestAdoptDecimalFormatSymbols(void) {
    UErrorCode ec = U_ZERO_ERROR;
    DecimalFormatSymbols *sym = new DecimalFormatSymbols(Locale::getUS(), ec);
    if (U_FAILURE(ec)) {
        errcheckln(ec, "Fail: DecimalFormatSymbols constructor - %s", u_errorName(ec));
        delete sym;
        return;
    }
    UnicodeString pat(" #,##0.00");
    pat.insert(0, (UChar)0x00A4);
    DecimalFormat fmt(pat, sym, ec);
    if (U_FAILURE(ec)) {
        errln("Fail: DecimalFormat constructor");
        return;
    }

    UnicodeString str;
    fmt.format(2350.75, str);
    if (str == "$ 2,350.75") {
        logln(str);
    } else {
        dataerrln("Fail: " + str + ", expected $ 2,350.75");
    }

    sym = new DecimalFormatSymbols(Locale::getUS(), ec);
    if (U_FAILURE(ec)) {
        errln("Fail: DecimalFormatSymbols constructor");
        delete sym;
        return;
    }
    sym->setSymbol(DecimalFormatSymbols::kCurrencySymbol, "Q");
    fmt.adoptDecimalFormatSymbols(sym);

    str.truncate(0);
    fmt.format(2350.75, str);
    if (str == "Q 2,350.75") {
        logln(str);
    } else {
        dataerrln("Fail: adoptDecimalFormatSymbols -> " + str + ", expected Q 2,350.75");
    }

    sym = new DecimalFormatSymbols(Locale::getUS(), ec);
    if (U_FAILURE(ec)) {
        errln("Fail: DecimalFormatSymbols constructor");
        delete sym;
        return;
    }
    DecimalFormat fmt2(pat, sym, ec);
    if (U_FAILURE(ec)) {
        errln("Fail: DecimalFormat constructor");
        return;
    }

    DecimalFormatSymbols sym2(Locale::getUS(), ec);
    if (U_FAILURE(ec)) {
        errln("Fail: DecimalFormatSymbols constructor");
        return;
    }
    sym2.setSymbol(DecimalFormatSymbols::kCurrencySymbol, "Q");
    fmt2.setDecimalFormatSymbols(sym2);

    str.truncate(0);
    fmt2.format(2350.75, str);
    if (str == "Q 2,350.75") {
        logln(str);
    } else {
        dataerrln("Fail: setDecimalFormatSymbols -> " + str + ", expected Q 2,350.75");
    }
}

void NumberFormatTest::TestPerMill() {
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeString str;
    DecimalFormat fmt(ctou("###.###\\u2030"), ec);
    if (!assertSuccess("DecimalFormat ct", ec)) return;
    assertEquals("0.4857 x ###.###\\u2030",
                 ctou("485.7\\u2030"), fmt.format(0.4857, str), true);

    DecimalFormatSymbols sym(Locale::getUS(), ec);
    if (!assertSuccess("", ec, true, __FILE__, __LINE__)) {
        return;
    }
    sym.setSymbol(DecimalFormatSymbols::kPerMillSymbol, ctou("m"));
    DecimalFormat fmt2("", sym, ec);
    if (!assertSuccess("", ec, true, __FILE__, __LINE__)) {
        return;
    }
    fmt2.applyLocalizedPattern("###.###m", ec);
    if (!assertSuccess("setup", ec)) return;
    str.truncate(0);
    assertEquals("0.4857 x ###.###m",
                 "485.7m", fmt2.format(0.4857, str));
}

/**
 * Generic test for patterns that should be legal/illegal.
 */
void NumberFormatTest::TestIllegalPatterns() {
    // Test cases:
    // Prefix with "-:" for illegal patterns
    // Prefix with "+:" for legal patterns
    const char* DATA[] = {
        // Unquoted special characters in the suffix are illegal
        "-:000.000|###",
        "+:000.000'|###'",
        0
    };
    for (int32_t i=0; DATA[i]; ++i) {
        const char* pat=DATA[i];
        UBool valid = (*pat) == '+';
        pat += 2;
        UErrorCode ec = U_ZERO_ERROR;
        DecimalFormat fmt(pat, ec); // locale doesn't matter here
        if (U_SUCCESS(ec) == valid) {
            logln("Ok: pattern \"%s\": %s",
                  pat, u_errorName(ec));
        } else {
            errcheckln(ec, "FAIL: pattern \"%s\" should have %s; got %s",
                  pat, (valid?"succeeded":"failed"),
                  u_errorName(ec));
        }
    }
}

//----------------------------------------------------------------------

static const char* KEYWORDS[] = {
    /*0*/ "ref=", // <reference pattern to parse numbers>
    /*1*/ "loc=", // <locale for formats>
    /*2*/ "f:",   // <pattern or '-'> <number> <exp. string>
    /*3*/ "fp:",  // <pattern or '-'> <number> <exp. string> <exp. number>
    /*4*/ "rt:",  // <pattern or '-'> <(exp.) number> <(exp.) string>
    /*5*/ "p:",   // <pattern or '-'> <string> <exp. number>
    /*6*/ "perr:", // <pattern or '-'> <invalid string>
    /*7*/ "pat:", // <pattern or '-'> <exp. toPattern or '-' or 'err'>
    /*8*/ "fpc:", // <pattern or '-'> <curr.amt> <exp. string> <exp. curr.amt>
    0
};

/**
 * Return an integer representing the next token from this
 * iterator.  The integer will be an index into the given list, or
 * -1 if there are no more tokens, or -2 if the token is not on
 * the list.
 */
static int32_t keywordIndex(const UnicodeString& tok) {
    for (int32_t i=0; KEYWORDS[i]!=0; ++i) {
        if (tok==KEYWORDS[i]) {
            return i;
        }
    }
    return -1;
}

/**
 * Parse a CurrencyAmount using the given NumberFormat, with
 * the 'delim' character separating the number and the currency.
 */
static void parseCurrencyAmount(const UnicodeString& str,
                                const NumberFormat& fmt,
                                UChar delim,
                                Formattable& result,
                                UErrorCode& ec) {
    UnicodeString num, cur;
    int32_t i = str.indexOf(delim);
    str.extractBetween(0, i, num);
    str.extractBetween(i+1, INT32_MAX, cur);
    Formattable n;
    fmt.parse(num, n, ec);
    result.adoptObject(new CurrencyAmount(n, cur.getTerminatedBuffer(), ec));
}

void NumberFormatTest::TestCases() {
    UErrorCode ec = U_ZERO_ERROR;
    TextFile reader("NumberFormatTestCases.txt", "UTF8", ec);
    if (U_FAILURE(ec)) {
        dataerrln("Couldn't open NumberFormatTestCases.txt");
        return;
    }
    TokenIterator tokens(&reader);

    Locale loc("en", "US", "");
    DecimalFormat *ref = 0, *fmt = 0;
    MeasureFormat *mfmt = 0;
    UnicodeString pat, tok, mloc, str, out, where, currAmt;
    Formattable n;

    for (;;) {
        ec = U_ZERO_ERROR;
        if (!tokens.next(tok, ec)) {
            break;
        }
        where = UnicodeString("(") + tokens.getLineNumber() + ") ";
        int32_t cmd = keywordIndex(tok);
        switch (cmd) {
        case 0:
            // ref= <reference pattern>
            if (!tokens.next(tok, ec)) goto error;
            delete ref;
            ref = new DecimalFormat(tok,
                      new DecimalFormatSymbols(Locale::getUS(), ec), ec);
            if (U_FAILURE(ec)) {
                dataerrln("Error constructing DecimalFormat");
                goto error;
            }
            break;
        case 1:
            // loc= <locale>
            if (!tokens.next(tok, ec)) goto error;
            loc = Locale::createFromName(CharString().appendInvariantChars(tok, ec).data());
            break;
        case 2: // f:
        case 3: // fp:
        case 4: // rt:
        case 5: // p:
            if (!tokens.next(tok, ec)) goto error;
            if (tok != "-") {
                pat = tok;
                delete fmt;
                fmt = new DecimalFormat(pat, new DecimalFormatSymbols(loc, ec), ec);
                if (U_FAILURE(ec)) {
                    errln("FAIL: " + where + "Pattern \"" + pat + "\": " + u_errorName(ec));
                    ec = U_ZERO_ERROR;
                    if (!tokens.next(tok, ec)) goto error;
                    if (!tokens.next(tok, ec)) goto error;
                    if (cmd == 3) {
                        if (!tokens.next(tok, ec)) goto error;
                    }
                    continue;
                }
            }
            if (cmd == 2 || cmd == 3 || cmd == 4) {
                // f: <pattern or '-'> <number> <exp. string>
                // fp: <pattern or '-'> <number> <exp. string> <exp. number>
                // rt: <pattern or '-'> <number> <string>
                UnicodeString num;
                if (!tokens.next(num, ec)) goto error;
                if (!tokens.next(str, ec)) goto error;
                ref->parse(num, n, ec);
                assertSuccess("parse", ec);
                assertEquals(where + "\"" + pat + "\".format(" + num + ")",
                             str, fmt->format(n, out.remove(), ec));
                assertSuccess("format", ec);
                if (cmd == 3) { // fp:
                    if (!tokens.next(num, ec)) goto error;
                    ref->parse(num, n, ec);
                    assertSuccess("parse", ec);
                }
                if (cmd != 2) { // != f:
                    Formattable m;
                    fmt->parse(str, m, ec);
                    assertSuccess("parse", ec);
                    assertEquals(where + "\"" + pat + "\".parse(\"" + str + "\")",
                                 n, m);
                }
            }
            // p: <pattern or '-'> <string to parse> <exp. number>
            else {
                UnicodeString expstr;
                if (!tokens.next(str, ec)) goto error;
                if (!tokens.next(expstr, ec)) goto error;
                Formattable exp, n;
                ref->parse(expstr, exp, ec);
                assertSuccess("parse", ec);
                fmt->parse(str, n, ec);
                assertSuccess("parse", ec);
                assertEquals(where + "\"" + pat + "\".parse(\"" + str + "\")",
                             exp, n);
            }
            break;
        case 8: // fpc:
            if (!tokens.next(tok, ec)) goto error;
            if (tok != "-") {
                mloc = tok;
                delete mfmt;
                mfmt = MeasureFormat::createCurrencyFormat(
                    Locale::createFromName(
                        CharString().appendInvariantChars(mloc, ec).data()), ec);
                if (U_FAILURE(ec)) {
                    errln("FAIL: " + where + "Loc \"" + mloc + "\": " + u_errorName(ec));
                    ec = U_ZERO_ERROR;
                    if (!tokens.next(tok, ec)) goto error;
                    if (!tokens.next(tok, ec)) goto error;
                    if (!tokens.next(tok, ec)) goto error;
                    continue;
                }
            } else if (mfmt == NULL) {
                errln("FAIL: " + where + "Loc \"" + mloc + "\": skip case using previous locale, no valid MeasureFormat");
                if (!tokens.next(tok, ec)) goto error;
                if (!tokens.next(tok, ec)) goto error;
                if (!tokens.next(tok, ec)) goto error;
                continue;
            }
            // fpc: <loc or '-'> <curr.amt> <exp. string> <exp. curr.amt>
            if (!tokens.next(currAmt, ec)) goto error;
            if (!tokens.next(str, ec)) goto error;
            parseCurrencyAmount(currAmt, *ref, (UChar)0x2F/*'/'*/, n, ec);
            if (assertSuccess("parseCurrencyAmount", ec)) {
                assertEquals(where + "getCurrencyFormat(" + mloc + ").format(" + currAmt + ")",
                             str, mfmt->format(n, out.remove(), ec));
                assertSuccess("format", ec);
            }
            if (!tokens.next(currAmt, ec)) goto error;
            parseCurrencyAmount(currAmt, *ref, (UChar)0x2F/*'/'*/, n, ec);
            if (assertSuccess("parseCurrencyAmount", ec)) {
                Formattable m;

                mfmt->parseObject(str, m, ec);
                if (assertSuccess("parseCurrency", ec)) {
                    assertEquals(where + "getCurrencyFormat(" + mloc + ").parse(\"" + str + "\")",
                                 n, m);
                } else {
                    errln("FAIL: source " + str);
                }
            }
            break;
        case 6:
            // perr: <pattern or '-'> <invalid string>
            errln("FAIL: Under construction");
            goto done;
        case 7: {
            // pat: <pattern> <exp. toPattern, or '-' or 'err'>
            UnicodeString testpat;
            UnicodeString exppat;
            if (!tokens.next(testpat, ec)) goto error;
            if (!tokens.next(exppat, ec)) goto error;
            UBool err = exppat == "err";
            UBool existingPat = FALSE;
            if (testpat == "-") {
                if (err) {
                    errln("FAIL: " + where + "Invalid command \"pat: - err\"");
                    continue;
                }
                existingPat = TRUE;
                testpat = pat;
            }
            if (exppat == "-") exppat = testpat;
            DecimalFormat* f = 0;
            UErrorCode ec2 = U_ZERO_ERROR;
            if (existingPat) {
                f = fmt;
            } else {
                f = new DecimalFormat(testpat, ec2);
            }
            if (U_SUCCESS(ec2)) {
                if (err) {
                    errln("FAIL: " + where + "Invalid pattern \"" + testpat +
                          "\" was accepted");
                } else {
                    UnicodeString pat2;
                    assertEquals(where + "\"" + testpat + "\".toPattern()",
                                 exppat, f->toPattern(pat2));
                }
            } else {
                if (err) {
                    logln("Ok: " + where + "Invalid pattern \"" + testpat +
                          "\" failed: " + u_errorName(ec2));
                } else {
                    errln("FAIL: " + where + "Valid pattern \"" + testpat +
                          "\" failed: " + u_errorName(ec2));
                }
            }
            if (!existingPat) delete f;
            } break;
        case -1:
            errln("FAIL: " + where + "Unknown command \"" + tok + "\"");
            goto done;
        }
    }
    goto done;

 error:
    if (U_SUCCESS(ec)) {
        errln("FAIL: Unexpected EOF");
    } else {
        errcheckln(ec, "FAIL: " + where + "Unexpected " + u_errorName(ec));
    }

 done:
    delete mfmt;
    delete fmt;
    delete ref;
}


//----------------------------------------------------------------------
// Support methods
//----------------------------------------------------------------------

UBool NumberFormatTest::equalValue(const Formattable& a, const Formattable& b) {
    if (a.getType() == b.getType()) {
        return a == b;
    }

    if (a.getType() == Formattable::kLong) {
        if (b.getType() == Formattable::kInt64) {
            return a.getLong() == b.getLong();
        } else if (b.getType() == Formattable::kDouble) {
            return (double) a.getLong() == b.getDouble(); // TODO check use of double instead of long
        }
    } else if (a.getType() == Formattable::kDouble) {
        if (b.getType() == Formattable::kLong) {
            return a.getDouble() == (double) b.getLong();
        } else if (b.getType() == Formattable::kInt64) {
            return a.getDouble() == (double)b.getInt64();
        }
    } else if (a.getType() == Formattable::kInt64) {
        if (b.getType() == Formattable::kLong) {
                return a.getInt64() == (int64_t)b.getLong();
        } else if (b.getType() == Formattable::kDouble) {
            return a.getInt64() == (int64_t)b.getDouble();
        }
    }
    return FALSE;
}

void NumberFormatTest::expect3(NumberFormat& fmt, const Formattable& n, const UnicodeString& str) {
    // Don't round-trip format test, since we explicitly do it
    expect_rbnf(fmt, n, str, FALSE);
    expect_rbnf(fmt, str, n);
}

void NumberFormatTest::expect2(NumberFormat& fmt, const Formattable& n, const UnicodeString& str) {
    // Don't round-trip format test, since we explicitly do it
    expect(fmt, n, str, FALSE);
    expect(fmt, str, n);
}

void NumberFormatTest::expect2(NumberFormat* fmt, const Formattable& n,
                               const UnicodeString& exp,
                               UErrorCode status) {
    if (fmt == NULL || U_FAILURE(status)) {
        dataerrln("FAIL: NumberFormat constructor");
    } else {
        expect2(*fmt, n, exp);
    }
    delete fmt;
}

void NumberFormatTest::expect(NumberFormat& fmt, const UnicodeString& str, const Formattable& n) {
    UErrorCode status = U_ZERO_ERROR;
    Formattable num;
    fmt.parse(str, num, status);
    if (U_FAILURE(status)) {
        dataerrln(UnicodeString("FAIL: Parse failed for \"") + str + "\" - " + u_errorName(status));
        return;
    }
    UnicodeString pat;
    ((DecimalFormat*) &fmt)->toPattern(pat);
    if (equalValue(num, n)) {
        logln(UnicodeString("Ok   \"") + str + "\" x " +
              pat + " = " +
              toString(num));
    } else {
        dataerrln(UnicodeString("FAIL \"") + str + "\" x " +
              pat + " = " +
              toString(num) + ", expected " + toString(n));
    }
}

void NumberFormatTest::expect_rbnf(NumberFormat& fmt, const UnicodeString& str, const Formattable& n) {
    UErrorCode status = U_ZERO_ERROR;
    Formattable num;
    fmt.parse(str, num, status);
    if (U_FAILURE(status)) {
        errln(UnicodeString("FAIL: Parse failed for \"") + str + "\"");
        return;
    }
    if (equalValue(num, n)) {
        logln(UnicodeString("Ok   \"") + str + " = " +
              toString(num));
    } else {
        errln(UnicodeString("FAIL \"") + str + " = " +
              toString(num) + ", expected " + toString(n));
    }
}

void NumberFormatTest::expect_rbnf(NumberFormat& fmt, const Formattable& n,
                              const UnicodeString& exp, UBool rt) {
    UnicodeString saw;
    FieldPosition pos;
    UErrorCode status = U_ZERO_ERROR;
    fmt.format(n, saw, pos, status);
    CHECK(status, "NumberFormat::format");
    if (saw == exp) {
        logln(UnicodeString("Ok   ") + toString(n) +
              " = \"" +
              escape(saw) + "\"");
        // We should be able to round-trip the formatted string =>
        // number => string (but not the other way around: number
        // => string => number2, might have number2 != number):
        if (rt) {
            Formattable n2;
            fmt.parse(exp, n2, status);
            if (U_FAILURE(status)) {
                errln(UnicodeString("FAIL: Parse failed for \"") + exp + "\"");
                return;
            }
            UnicodeString saw2;
            fmt.format(n2, saw2, pos, status);
            CHECK(status, "NumberFormat::format");
            if (saw2 != exp) {
                errln((UnicodeString)"FAIL \"" + exp + "\" => " + toString(n2) +
                      " => \"" + saw2 + "\"");
            }
        }
    } else {
        errln(UnicodeString("FAIL ") + toString(n) +
              " = \"" +
              escape(saw) + "\", expected \"" + exp + "\"");
    }
}

void NumberFormatTest::expect(NumberFormat& fmt, const Formattable& n,
                              const UnicodeString& exp, UBool rt) {
    UnicodeString saw;
    FieldPosition pos;
    UErrorCode status = U_ZERO_ERROR;
    fmt.format(n, saw, pos, status);
    CHECK(status, "NumberFormat::format");
    UnicodeString pat;
    ((DecimalFormat*) &fmt)->toPattern(pat);
    if (saw == exp) {
        logln(UnicodeString("Ok   ") + toString(n) + " x " +
              escape(pat) + " = \"" +
              escape(saw) + "\"");
        // We should be able to round-trip the formatted string =>
        // number => string (but not the other way around: number
        // => string => number2, might have number2 != number):
        if (rt) {
            Formattable n2;
            fmt.parse(exp, n2, status);
            if (U_FAILURE(status)) {
                errln(UnicodeString("FAIL: Parse failed for \"") + exp + "\" - " + u_errorName(status));
                return;
            }
            UnicodeString saw2;
            fmt.format(n2, saw2, pos, status);
            CHECK(status, "NumberFormat::format");
            if (saw2 != exp) {
                errln((UnicodeString)"FAIL \"" + exp + "\" => " + toString(n2) +
                      " => \"" + saw2 + "\"");
            }
        }
    } else {
        dataerrln(UnicodeString("FAIL ") + toString(n) + " x " +
              escape(pat) + " = \"" +
              escape(saw) + "\", expected \"" + exp + "\"");
    }
}

void NumberFormatTest::expect(NumberFormat* fmt, const Formattable& n,
                              const UnicodeString& exp, UBool rt,
                              UErrorCode status) {
    if (fmt == NULL || U_FAILURE(status)) {
        dataerrln("FAIL: NumberFormat constructor");
    } else {
        expect(*fmt, n, exp, rt);
    }
    delete fmt;
}

void NumberFormatTest::expectCurrency(NumberFormat& nf, const Locale& locale,
                                      double value, const UnicodeString& string) {
    UErrorCode ec = U_ZERO_ERROR;
    DecimalFormat& fmt = * (DecimalFormat*) &nf;
    const UChar DEFAULT_CURR[] = {45/*-*/,0};
    UChar curr[4];
    u_strcpy(curr, DEFAULT_CURR);
    if (*locale.getLanguage() != 0) {
        ucurr_forLocale(locale.getName(), curr, 4, &ec);
        assertSuccess("ucurr_forLocale", ec);
        fmt.setCurrency(curr, ec);
        assertSuccess("DecimalFormat::setCurrency", ec);
        fmt.setCurrency(curr); //Deprecated variant, for coverage only
    }
    UnicodeString s;
    fmt.format(value, s);
    s.findAndReplace((UChar32)0x00A0, (UChar32)0x0020);

    // Default display of the number yields "1234.5599999999999"
    // instead of "1234.56".  Use a formatter to fix this.
    NumberFormat* f =
        NumberFormat::createInstance(Locale::getUS(), ec);
    UnicodeString v;
    if (U_FAILURE(ec)) {
        // Oops; bad formatter.  Use default op+= display.
        v = (UnicodeString)"" + value;
    } else {
        f->setMaximumFractionDigits(4);
        f->setGroupingUsed(FALSE);
        f->format(value, v);
    }
    delete f;

    if (s == string) {
        logln((UnicodeString)"Ok: " + v + " x " + curr + " => " + prettify(s));
    } else {
        errln((UnicodeString)"FAIL: " + v + " x " + curr + " => " + prettify(s) +
              ", expected " + prettify(string));
    }
}

void NumberFormatTest::expectPat(DecimalFormat& fmt, const UnicodeString& exp) {
    UnicodeString pat;
    fmt.toPattern(pat);
    if (pat == exp) {
        logln(UnicodeString("Ok   \"") + pat + "\"");
    } else {
        errln(UnicodeString("FAIL \"") + pat + "\", expected \"" + exp + "\"");
    }
}

void NumberFormatTest::expectPad(DecimalFormat& fmt, const UnicodeString& pat,
                                 int32_t pos) {
    expectPad(fmt, pat, pos, 0, (UnicodeString)"");
}
void NumberFormatTest::expectPad(DecimalFormat& fmt, const UnicodeString& pat,
                                 int32_t pos, int32_t width, UChar pad) {
    expectPad(fmt, pat, pos, width, UnicodeString(pad));
}
void NumberFormatTest::expectPad(DecimalFormat& fmt, const UnicodeString& pat,
                                 int32_t pos, int32_t width, const UnicodeString& pad) {
    int32_t apos = 0, awidth = 0;
    UnicodeString apadStr;
    UErrorCode status = U_ZERO_ERROR;
    fmt.applyPattern(pat, status);
    if (U_SUCCESS(status)) {
        apos = fmt.getPadPosition();
        awidth = fmt.getFormatWidth();
        apadStr=fmt.getPadCharacterString();
    } else {
        apos = -1;
        awidth = width;
        apadStr = pad;
    }
    if (apos == pos && awidth == width && apadStr == pad) {
        UnicodeString infoStr;
        if (pos == ILLEGAL) {
            infoStr = UnicodeString(" width=", "") + awidth + UnicodeString(" pad=", "") + apadStr;
        }
        logln(UnicodeString("Ok   \"") + pat + "\" pos=" + apos + infoStr);
    } else {
        errln(UnicodeString("FAIL \"") + pat + "\" pos=" + apos +
              " width=" + awidth + " pad=" + apadStr +
              ", expected " + pos + " " + width + " " + pad);
    }
}

// This test is flaky b/c the symbols for CNY and JPY are equivalent in this locale  - FIXME
void NumberFormatTest::TestCompatibleCurrencies() {
/*
    static const UChar JPY[] = {0x4A, 0x50, 0x59, 0};
    static const UChar CNY[] = {0x43, 0x4E, 0x59, 0};
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<NumberFormat> fmt(
        NumberFormat::createCurrencyInstance(Locale::getUS(), status));
    if (U_FAILURE(status)) {
        errln("Could not create number format instance.");
        return;
    }
    logln("%s:%d - testing parse of halfwidth yen sign\n", __FILE__, __LINE__);
    expectParseCurrency(*fmt, JPY, 1235,  "\\u00A51,235");
    logln("%s:%d - testing parse of fullwidth yen sign\n", __FILE__, __LINE__);
    expectParseCurrency(*fmt, JPY, 1235,  "\\uFFE51,235");
    logln("%s:%d - testing parse of halfwidth yen sign\n", __FILE__, __LINE__);
    expectParseCurrency(*fmt, CNY, 1235,  "CN\\u00A51,235");

    LocalPointer<NumberFormat> fmtTW(
        NumberFormat::createCurrencyInstance(Locale::getTaiwan(), status));

    logln("%s:%d - testing parse of halfwidth yen sign in TW\n", __FILE__, __LINE__);
    expectParseCurrency(*fmtTW, CNY, 1235,  "\\u00A51,235");
    logln("%s:%d - testing parse of fullwidth yen sign in TW\n", __FILE__, __LINE__);
    expectParseCurrency(*fmtTW, CNY, 1235,  "\\uFFE51,235");

    LocalPointer<NumberFormat> fmtJP(
        NumberFormat::createCurrencyInstance(Locale::getJapan(), status));

    logln("%s:%d - testing parse of halfwidth yen sign in JP\n", __FILE__, __LINE__);
    expectParseCurrency(*fmtJP, JPY, 1235,  "\\u00A51,235");
    logln("%s:%d - testing parse of fullwidth yen sign in JP\n", __FILE__, __LINE__);
    expectParseCurrency(*fmtJP, JPY, 1235,  "\\uFFE51,235");

    // more..
*/
}

void NumberFormatTest::expectParseCurrency(const NumberFormat &fmt, const UChar* currency, double amount, const char *text) {
    ParsePosition ppos;
    UnicodeString utext = ctou(text);
    LocalPointer<CurrencyAmount> currencyAmount(fmt.parseCurrency(utext, ppos));
    if (!ppos.getIndex()) {
        errln(UnicodeString("Parse of ") + utext + " should have succeeded.");
        return;
    }
    UErrorCode status = U_ZERO_ERROR;

    char theInfo[100];
    sprintf(theInfo, "For locale %s, string \"%s\", currency ",
            fmt.getLocale(ULOC_ACTUAL_LOCALE, status).getBaseName(),
            text);
    u_austrcpy(theInfo+uprv_strlen(theInfo), currency);

    char theOperation[100];

    uprv_strcpy(theOperation, theInfo);
    uprv_strcat(theOperation, ", check amount:");
    assertTrue(theOperation, amount ==  currencyAmount->getNumber().getDouble(status));

    uprv_strcpy(theOperation, theInfo);
    uprv_strcat(theOperation, ", check currency:");
    assertEquals(theOperation, currency, currencyAmount->getISOCurrency());
}


void NumberFormatTest::TestJB3832(){
    const char* localeID = "pt_PT@currency=PTE";
    Locale loc(localeID);
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString expected(CharsToUnicodeString("1,150$50\\u00A0\\u200B")); // per cldrbug 7670
    UnicodeString s;
    NumberFormat* currencyFmt = NumberFormat::createCurrencyInstance(loc, status);
    if(U_FAILURE(status)){
        dataerrln("Could not create currency formatter for locale %s - %s", localeID, u_errorName(status));
        return;
    }
    currencyFmt->format(1150.50, s);
    if(s!=expected){
        errln(UnicodeString("FAIL: Expected: ")+expected
                + UnicodeString(" Got: ") + s
                + UnicodeString( " for locale: ")+ UnicodeString(localeID) );
    }
    if (U_FAILURE(status)){
        errln("FAIL: Status %s", u_errorName(status));
    }
    delete currencyFmt;
}

void NumberFormatTest::TestHost()
{
#if U_PLATFORM_USES_ONLY_WIN32_API
    Win32NumberTest::testLocales(this);
#endif
    Locale loc("en_US@compat=host");
    for (UNumberFormatStyle k = UNUM_DECIMAL;
         k < UNUM_FORMAT_STYLE_COUNT; k = (UNumberFormatStyle)(k+1)) {
        UErrorCode status = U_ZERO_ERROR;
        LocalPointer<NumberFormat> full(NumberFormat::createInstance(loc, k, status));
        if (!NumberFormat::isStyleSupported(k)) {
            if (status != U_UNSUPPORTED_ERROR) {
                errln("FAIL: expected style %d to be unsupported - %s",
                      k, u_errorName(status));
            }
            continue;
        }
        if (full.isNull() || U_FAILURE(status)) {
            dataerrln("FAIL: Can't create number instance of style %d for host - %s",
                      k, u_errorName(status));
            return;
        }
        UnicodeString result1;
        Formattable number(10.00);
        full->format(number, result1, status);
        if (U_FAILURE(status)) {
            errln("FAIL: Can't format for host");
            return;
        }
        Formattable formattable;
        full->parse(result1, formattable, status);
        if (U_FAILURE(status)) {
            errln("FAIL: Can't parse for host");
            return;
        }
    }
}

void NumberFormatTest::TestHostClone()
{
    /*
    Verify that a cloned formatter gives the same results
    and is useable after the original has been deleted.
    */
    // This is mainly important on Windows.
    UErrorCode status = U_ZERO_ERROR;
    Locale loc("en_US@compat=host");
    UDate now = Calendar::getNow();
    NumberFormat *full = NumberFormat::createInstance(loc, status);
    if (full == NULL || U_FAILURE(status)) {
        dataerrln("FAIL: Can't create NumberFormat date instance - %s", u_errorName(status));
        return;
    }
    UnicodeString result1;
    full->format(now, result1, status);
    Format *fullClone = full->clone();
    delete full;
    full = NULL;

    UnicodeString result2;
    fullClone->format(now, result2, status);
    if (U_FAILURE(status)) {
        errln("FAIL: format failure.");
    }
    if (result1 != result2) {
        errln("FAIL: Clone returned different result from non-clone.");
    }
    delete fullClone;
}

void NumberFormatTest::TestCurrencyFormat()
{
    // This test is here to increase code coverage.
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat *cloneObj;
    UnicodeString str;
    Formattable toFormat, result;
    static const UChar ISO_CODE[4] = {0x0047, 0x0042, 0x0050, 0};

    Locale  saveDefaultLocale = Locale::getDefault();
    Locale::setDefault( Locale::getUK(), status );
    if (U_FAILURE(status)) {
        errln("couldn't set default Locale!");
        return;
    }

    MeasureFormat *measureObj = MeasureFormat::createCurrencyFormat(status);
    Locale::setDefault( saveDefaultLocale, status );
    if (U_FAILURE(status)){
        dataerrln("FAIL: Status %s", u_errorName(status));
        return;
    }
    cloneObj = (MeasureFormat *)measureObj->clone();
    if (cloneObj == NULL) {
        errln("Clone doesn't work");
        return;
    }
    toFormat.adoptObject(new CurrencyAmount(1234.56, ISO_CODE, status));
    measureObj->format(toFormat, str, status);
    measureObj->parseObject(str, result, status);
    if (U_FAILURE(status)){
        errln("FAIL: Status %s", u_errorName(status));
    }
    if (result != toFormat) {
        errln("measureObj does not round trip. Formatted string was \"" + str + "\" Got: " + toString(result) + " Expected: " + toString(toFormat));
    }
    status = U_ZERO_ERROR;
    str.truncate(0);
    cloneObj->format(toFormat, str, status);
    cloneObj->parseObject(str, result, status);
    if (U_FAILURE(status)){
        errln("FAIL: Status %s", u_errorName(status));
    }
    if (result != toFormat) {
        errln("Clone does not round trip. Formatted string was \"" + str + "\" Got: " + toString(result) + " Expected: " + toString(toFormat));
    }
    if (*measureObj != *cloneObj) {
        errln("Cloned object is not equal to the original object");
    }
    delete measureObj;
    delete cloneObj;

    status = U_USELESS_COLLATOR_ERROR;
    if (MeasureFormat::createCurrencyFormat(status) != NULL) {
        errln("createCurrencyFormat should have returned NULL.");
    }
}

/* Port of ICU4J rounding test. */
void NumberFormatTest::TestRounding() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat *df = (DecimalFormat*)NumberFormat::createCurrencyInstance(Locale::getEnglish(), status);

    if (U_FAILURE(status)) {
        dataerrln("Unable to create decimal formatter. - %s", u_errorName(status));
        return;
    }

    int roundingIncrements[]={1, 2, 5, 20, 50, 100};
    int testValues[]={0, 300};

    for (int j=0; j<2; j++) {
        for (int mode=DecimalFormat::kRoundUp;mode<DecimalFormat::kRoundHalfEven;mode++) {
            df->setRoundingMode((DecimalFormat::ERoundingMode)mode);
            for (int increment=0; increment<6; increment++) {
                double base=testValues[j];
                double rInc=roundingIncrements[increment];
                checkRounding(df, base, 20, rInc);
                rInc=1.000000000/rInc;
                checkRounding(df, base, 20, rInc);
            }
        }
    }
    delete df;
}

void NumberFormatTest::TestRoundingPattern() {
    UErrorCode status = U_ZERO_ERROR;
    struct {
        UnicodeString  pattern;
        double        testCase;
        UnicodeString expected;
    } tests[] = {
            { (UnicodeString)"##0.65", 1.234, (UnicodeString)"1.30" },
            { (UnicodeString)"#50",    1230,  (UnicodeString)"1250" }
    };
    int32_t numOfTests = UPRV_LENGTHOF(tests);
    UnicodeString result;

    DecimalFormat *df = (DecimalFormat*)NumberFormat::createCurrencyInstance(Locale::getEnglish(), status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create decimal formatter. - %s", u_errorName(status));
        return;
    }

    for (int32_t i = 0; i < numOfTests; i++) {
        result.remove();

        df->applyPattern(tests[i].pattern, status);
        if (U_FAILURE(status)) {
            errln("Unable to apply pattern to decimal formatter. - %s", u_errorName(status));
        }

        df->format(tests[i].testCase, result);

        if (result != tests[i].expected) {
            errln("String Pattern Rounding Test Failed: Pattern: \"" + tests[i].pattern + "\" Number: " + tests[i].testCase + " - Got: " + result + " Expected: " + tests[i].expected);
        }
    }

    delete df;
}

void NumberFormatTest::checkRounding(DecimalFormat* df, double base, int iterations, double increment) {
    df->setRoundingIncrement(increment);
    double lastParsed=INT32_MIN; //Intger.MIN_VALUE
    for (int i=-iterations; i<=iterations;i++) {
        double iValue=base+(increment*(i*0.1));
        double smallIncrement=0.00000001;
        if (iValue!=0) {
            smallIncrement*=iValue;
        }
        //we not only test the value, but some values in a small range around it
        lastParsed=checkRound(df, iValue-smallIncrement, lastParsed);
        lastParsed=checkRound(df, iValue, lastParsed);
        lastParsed=checkRound(df, iValue+smallIncrement, lastParsed);
    }
}

double NumberFormatTest::checkRound(DecimalFormat* df, double iValue, double lastParsed) {
    UErrorCode status=U_ZERO_ERROR;
    UnicodeString formattedDecimal;
    double parsed;
    Formattable result;
    df->format(iValue, formattedDecimal, status);

    if (U_FAILURE(status)) {
        errln("Error formatting number.");
    }

    df->parse(formattedDecimal, result, status);

    if (U_FAILURE(status)) {
        errln("Error parsing number.");
    }

    parsed=result.getDouble();

    if (lastParsed>parsed) {
        errln("Rounding wrong direction! %d > %d", lastParsed, parsed);
    }

    return lastParsed;
}

void NumberFormatTest::TestNonpositiveMultiplier() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols US(Locale::getUS(), status);
    CHECK(status, "DecimalFormatSymbols constructor");
    DecimalFormat df(UnicodeString("0"), US, status);
    CHECK(status, "DecimalFormat(0)");

    // test zero multiplier

    int32_t mult = df.getMultiplier();
    df.setMultiplier(0);
    if (df.getMultiplier() != mult) {
        errln("DecimalFormat.setMultiplier(0) did not ignore its zero input");
    }

    // test negative multiplier

    df.setMultiplier(-1);
    if (df.getMultiplier() != -1) {
        errln("DecimalFormat.setMultiplier(-1) ignored its negative input");
        return;
    }

    expect(df, "1122.123", -1122.123);
    expect(df, "-1122.123", 1122.123);
    expect(df, "1.2", -1.2);
    expect(df, "-1.2", 1.2);

    // Note:  the tests with the final parameter of FALSE will not round trip.
    //        The initial numeric value will format correctly, after the multiplier.
    //        Parsing the formatted text will be out-of-range for an int64, however.
    //        The expect() function could be modified to detect this and fall back
    //        to looking at the decimal parsed value, but it doesn't.
    expect(df, U_INT64_MIN,    "9223372036854775808", FALSE);
    expect(df, U_INT64_MIN+1,  "9223372036854775807");
    expect(df, (int64_t)-123,                  "123");
    expect(df, (int64_t)123,                  "-123");
    expect(df, U_INT64_MAX-1, "-9223372036854775806");
    expect(df, U_INT64_MAX,   "-9223372036854775807");

    df.setMultiplier(-2);
    expect(df, -(U_INT64_MIN/2)-1, "-9223372036854775806");
    expect(df, -(U_INT64_MIN/2),   "-9223372036854775808");
    expect(df, -(U_INT64_MIN/2)+1, "-9223372036854775810", FALSE);

    df.setMultiplier(-7);
    expect(df, -(U_INT64_MAX/7)-1, "9223372036854775814", FALSE);
    expect(df, -(U_INT64_MAX/7),   "9223372036854775807");
    expect(df, -(U_INT64_MAX/7)+1, "9223372036854775800");

    // TODO: uncomment (and fix up) all the following int64_t tests once BigInteger is ported
    // (right now the big numbers get turned into doubles and lose tons of accuracy)
    //expect2(df, U_INT64_MAX, Int64ToUnicodeString(-U_INT64_MAX));
    //expect2(df, U_INT64_MIN, UnicodeString(Int64ToUnicodeString(U_INT64_MIN), 1));
    //expect2(df, U_INT64_MAX / 2, Int64ToUnicodeString(-(U_INT64_MAX / 2)));
    //expect2(df, U_INT64_MIN / 2, Int64ToUnicodeString(-(U_INT64_MIN / 2)));

    // TODO: uncomment (and fix up) once BigDecimal is ported and DecimalFormat can handle it
    //expect2(df, BigDecimal.valueOf(Long.MAX_VALUE), BigDecimal.valueOf(Long.MAX_VALUE).negate().toString());
    //expect2(df, BigDecimal.valueOf(Long.MIN_VALUE), BigDecimal.valueOf(Long.MIN_VALUE).negate().toString());
    //expect2(df, java.math.BigDecimal.valueOf(Long.MAX_VALUE), java.math.BigDecimal.valueOf(Long.MAX_VALUE).negate().toString());
    //expect2(df, java.math.BigDecimal.valueOf(Long.MIN_VALUE), java.math.BigDecimal.valueOf(Long.MIN_VALUE).negate().toString());
}

typedef struct {
    const char * stringToParse;
    int          parsedPos;
    int          errorIndex;
    UBool        lenient;
} TestSpaceParsingItem;

void
NumberFormatTest::TestSpaceParsing() {
    // the data are:
    // the string to be parsed, parsed position, parsed error index
    const TestSpaceParsingItem DATA[] = {
        {"$124",           4, -1, FALSE},
        {"$124 $124",      4, -1, FALSE},
        {"$124 ",          4, -1, FALSE},
        {"$ 124 ",         0,  1, FALSE},
        {"$\\u00A0124 ",   5, -1, FALSE},
        {" $ 124 ",        0,  0, FALSE},
        {"124$",           0,  4, FALSE},
        {"124 $",          0,  3, FALSE},
        {"$124",           4, -1, TRUE},
        {"$124 $124",      4, -1, TRUE},
        {"$124 ",          4, -1, TRUE},
        {"$ 124 ",         5, -1, TRUE},
        {"$\\u00A0124 ",   5, -1, TRUE},
        {" $ 124 ",        6, -1, TRUE},
        {"124$",           4, -1, TRUE},
        {"124$",           4, -1, TRUE},
        {"124 $",          5, -1, TRUE},
        {"124 $",          5, -1, TRUE},
    };
    UErrorCode status = U_ZERO_ERROR;
    Locale locale("en_US");
    NumberFormat* foo = NumberFormat::createCurrencyInstance(locale, status);

    if (U_FAILURE(status)) {
        delete foo;
        return;
    }
    for (uint32_t i = 0; i < UPRV_LENGTHOF(DATA); ++i) {
        ParsePosition parsePosition(0);
        UnicodeString stringToBeParsed = ctou(DATA[i].stringToParse);
        int parsedPosition = DATA[i].parsedPos;
        int errorIndex = DATA[i].errorIndex;
        foo->setLenient(DATA[i].lenient);
        Formattable result;
        foo->parse(stringToBeParsed, result, parsePosition);
        logln("Parsing: " + stringToBeParsed);
        if (parsePosition.getIndex() != parsedPosition ||
            parsePosition.getErrorIndex() != errorIndex) {
            errln("FAILED parse " + stringToBeParsed + "; lenient: " + DATA[i].lenient + "; wrong position, expected: (" + parsedPosition + ", " + errorIndex + "); got (" + parsePosition.getIndex() + ", " + parsePosition.getErrorIndex() + ")");
        }
        if (parsePosition.getErrorIndex() == -1 &&
            result.getType() == Formattable::kLong &&
            result.getLong() != 124) {
            errln("FAILED parse " + stringToBeParsed + "; wrong number, expect: 124, got " + result.getLong());
        }
    }
    delete foo;
}

/**
 * Test using various numbering systems and numbering system keyword.
 */
typedef struct {
    const char *localeName;
    double      value;
    UBool        isRBNF;
    const char *expectedResult;
} TestNumberingSystemItem;

void NumberFormatTest::TestNumberingSystems() {

    const TestNumberingSystemItem DATA[] = {
        { "en_US@numbers=thai", 1234.567, FALSE, "\\u0E51,\\u0E52\\u0E53\\u0E54.\\u0E55\\u0E56\\u0E57" },
        { "en_US@numbers=hebr", 5678.0, TRUE, "\\u05D4\\u05F3\\u05EA\\u05E8\\u05E2\\u05F4\\u05D7" },
        { "en_US@numbers=arabext", 1234.567, FALSE, "\\u06F1\\u066c\\u06F2\\u06F3\\u06F4\\u066b\\u06F5\\u06F6\\u06F7" },
        { "ar_EG", 1234.567, FALSE, "\\u0661\\u066C\\u0662\\u0663\\u0664\\u066b\\u0665\\u0666\\u0667" },
        { "th_TH@numbers=traditional", 1234.567, FALSE, "\\u0E51,\\u0E52\\u0E53\\u0E54.\\u0E55\\u0E56\\u0E57" }, // fall back to native per TR35
        { "ar_MA", 1234.567, FALSE, "1.234,567" },
        { "en_US@numbers=hanidec", 1234.567, FALSE, "\\u4e00,\\u4e8c\\u4e09\\u56db.\\u4e94\\u516d\\u4e03" },
        { "ta_IN@numbers=native", 1234.567, FALSE, "\\u0BE7,\\u0BE8\\u0BE9\\u0BEA.\\u0BEB\\u0BEC\\u0BED" },
        { "ta_IN@numbers=traditional", 1235.0, TRUE, "\\u0BF2\\u0BE8\\u0BF1\\u0BE9\\u0BF0\\u0BEB" },
        { "ta_IN@numbers=finance", 1234.567, FALSE, "1,234.567" }, // fall back to default per TR35
        { "zh_TW@numbers=native", 1234.567, FALSE, "\\u4e00,\\u4e8c\\u4e09\\u56db.\\u4e94\\u516d\\u4e03" },
        { "zh_TW@numbers=traditional", 1234.567, TRUE, "\\u4E00\\u5343\\u4E8C\\u767E\\u4E09\\u5341\\u56DB\\u9EDE\\u4E94\\u516D\\u4E03" },
        { "zh_TW@numbers=finance", 1234.567, TRUE, "\\u58F9\\u4EDF\\u8CB3\\u4F70\\u53C3\\u62FE\\u8086\\u9EDE\\u4F0D\\u9678\\u67D2" },
        { NULL, 0, FALSE, NULL }
    };

    UErrorCode ec;

    const TestNumberingSystemItem *item;
    for (item = DATA; item->localeName != NULL; item++) {
        ec = U_ZERO_ERROR;
        Locale loc = Locale::createFromName(item->localeName);

        NumberFormat *origFmt = NumberFormat::createInstance(loc,ec);
        if (U_FAILURE(ec)) {
            dataerrln("FAIL: getInstance(%s) - %s", item->localeName, u_errorName(ec));
            continue;
        }
        // Clone to test ticket #10682
        NumberFormat *fmt = (NumberFormat *) origFmt->clone();
        delete origFmt;


        if (item->isRBNF) {
            expect3(*fmt,item->value,CharsToUnicodeString(item->expectedResult));
        } else {
            expect2(*fmt,item->value,CharsToUnicodeString(item->expectedResult));
        }
        delete fmt;
    }


    // Test bogus keyword value
    ec = U_ZERO_ERROR;
    Locale loc4 = Locale::createFromName("en_US@numbers=foobar");
    NumberFormat* fmt4= NumberFormat::createInstance(loc4, ec);
    if ( ec != U_UNSUPPORTED_ERROR ) {
        errln("FAIL: getInstance(en_US@numbers=foobar) should have returned U_UNSUPPORTED_ERROR");
        delete fmt4;
    }

    ec = U_ZERO_ERROR;
    NumberingSystem *ns = NumberingSystem::createInstance(ec);
    if (U_FAILURE(ec)) {
        dataerrln("FAIL: NumberingSystem::createInstance(ec); - %s", u_errorName(ec));
    }

    if ( ns != NULL ) {
        ns->getDynamicClassID();
        ns->getStaticClassID();
    } else {
        errln("FAIL: getInstance() returned NULL.");
    }

    NumberingSystem *ns1 = new NumberingSystem(*ns);
    if (ns1 == NULL) {
        errln("FAIL: NumberSystem copy constructor returned NULL.");
    }

    delete ns1;
    delete ns;

}


void
NumberFormatTest::TestMultiCurrencySign() {
    const char* DATA[][6] = {
        // the fields in the following test are:
        // locale,
        // currency pattern (with negative pattern),
        // currency number to be formatted,
        // currency format using currency symbol name, such as "$" for USD,
        // currency format using currency ISO name, such as "USD",
        // currency format using plural name, such as "US dollars".
        // for US locale
        {"en_US", "\\u00A4#,##0.00;-\\u00A4#,##0.00", "1234.56", "$1,234.56", "USD\\u00A01,234.56", "US dollars\\u00A01,234.56"},
        {"en_US", "\\u00A4#,##0.00;-\\u00A4#,##0.00", "-1234.56", "-$1,234.56", "-USD\\u00A01,234.56", "-US dollars\\u00A01,234.56"},
        {"en_US", "\\u00A4#,##0.00;-\\u00A4#,##0.00", "1", "$1.00", "USD\\u00A01.00", "US dollars\\u00A01.00"},
        // for CHINA locale
        {"zh_CN", "\\u00A4#,##0.00;(\\u00A4#,##0.00)", "1234.56", "\\uFFE51,234.56", "CNY\\u00A01,234.56", "\\u4EBA\\u6C11\\u5E01\\u00A01,234.56"},
        {"zh_CN", "\\u00A4#,##0.00;(\\u00A4#,##0.00)", "-1234.56", "(\\uFFE51,234.56)", "(CNY\\u00A01,234.56)", "(\\u4EBA\\u6C11\\u5E01\\u00A01,234.56)"},
        {"zh_CN", "\\u00A4#,##0.00;(\\u00A4#,##0.00)", "1", "\\uFFE51.00", "CNY\\u00A01.00", "\\u4EBA\\u6C11\\u5E01\\u00A01.00"}
    };

    const UChar doubleCurrencySign[] = {0xA4, 0xA4, 0};
    UnicodeString doubleCurrencyStr(doubleCurrencySign);
    const UChar tripleCurrencySign[] = {0xA4, 0xA4, 0xA4, 0};
    UnicodeString tripleCurrencyStr(tripleCurrencySign);

    for (uint32_t i=0; i<UPRV_LENGTHOF(DATA); ++i) {
        const char* locale = DATA[i][0];
        UnicodeString pat = ctou(DATA[i][1]);
        double numberToBeFormat = atof(DATA[i][2]);
        UErrorCode status = U_ZERO_ERROR;
        DecimalFormatSymbols* sym = new DecimalFormatSymbols(Locale(locale), status);
        if (U_FAILURE(status)) {
            delete sym;
            continue;
        }
        for (int j=1; j<=3; ++j) {
            // j represents the number of currency sign in the pattern.
            if (j == 2) {
                pat = pat.findAndReplace(ctou("\\u00A4"), doubleCurrencyStr);
            } else if (j == 3) {
                pat = pat.findAndReplace(ctou("\\u00A4\\u00A4"), tripleCurrencyStr);
            }

            DecimalFormat* fmt = new DecimalFormat(pat, new DecimalFormatSymbols(*sym), status);
            if (U_FAILURE(status)) {
                errln("FAILED init DecimalFormat ");
                delete fmt;
                continue;
            }
            UnicodeString s;
            ((NumberFormat*) fmt)->format(numberToBeFormat, s);
            // DATA[i][3] is the currency format result using a
            // single currency sign.
            // DATA[i][4] is the currency format result using
            // double currency sign.
            // DATA[i][5] is the currency format result using
            // triple currency sign.
            // DATA[i][j+2] is the currency format result using
            // 'j' number of currency sign.
            UnicodeString currencyFormatResult = ctou(DATA[i][2+j]);
            if (s.compare(currencyFormatResult)) {
                errln("FAIL format: Expected " + currencyFormatResult + "; Got " + s);
            }
            // mix style parsing
            for (int k=3; k<=5; ++k) {
              // DATA[i][3] is the currency format result using a
              // single currency sign.
              // DATA[i][4] is the currency format result using
              // double currency sign.
              // DATA[i][5] is the currency format result using
              // triple currency sign.
              UnicodeString oneCurrencyFormat = ctou(DATA[i][k]);
              UErrorCode status = U_ZERO_ERROR;
              Formattable parseRes;
              fmt->parse(oneCurrencyFormat, parseRes, status);
              if (U_FAILURE(status) ||
                  (parseRes.getType() == Formattable::kDouble &&
                   parseRes.getDouble() != numberToBeFormat) ||
                  (parseRes.getType() == Formattable::kLong &&
                   parseRes.getLong() != numberToBeFormat)) {
                  errln("FAILED parse " + oneCurrencyFormat + "; (i, j, k): " +
                        i + ", " + j + ", " + k);
              }
            }
            delete fmt;
        }
        delete sym;
    }
}


void
NumberFormatTest::TestCurrencyFormatForMixParsing() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat* curFmt = MeasureFormat::createCurrencyFormat(Locale("en_US"), status);
    if (U_FAILURE(status)) {
        delete curFmt;
        return;
    }
    const char* formats[] = {
        "$1,234.56",  // string to be parsed
        "USD1,234.56",
        "US dollars1,234.56",
        // "1,234.56 US dollars" // Fails in 62 because currency format is not compatible with pattern.
    };
    const CurrencyAmount* curramt = NULL;
    for (uint32_t i = 0; i < UPRV_LENGTHOF(formats); ++i) {
        UnicodeString stringToBeParsed = ctou(formats[i]);
        logln(UnicodeString("stringToBeParsed: ") + stringToBeParsed);
        Formattable result;
        UErrorCode status = U_ZERO_ERROR;
        curFmt->parseObject(stringToBeParsed, result, status);
        if (U_FAILURE(status)) {
          errln("FAIL: measure format parsing: '%s' ec: %s", formats[i], u_errorName(status));
        } else if (result.getType() != Formattable::kObject ||
            (curramt = dynamic_cast<const CurrencyAmount*>(result.getObject())) == NULL ||
            curramt->getNumber().getDouble() != 1234.56 ||
            UnicodeString(curramt->getISOCurrency()).compare(ISO_CURRENCY_USD)
        ) {
            errln("FAIL: getCurrencyFormat of default locale (en_US) failed roundtripping the number ");
            if (curramt->getNumber().getDouble() != 1234.56) {
                errln((UnicodeString)"wong number, expect: 1234.56" + ", got: " + curramt->getNumber().getDouble());
            }
            if (curramt->getISOCurrency() != ISO_CURRENCY_USD) {
                errln((UnicodeString)"wong currency, expect: USD" + ", got: " + curramt->getISOCurrency());
            }
        }
    }
    delete curFmt;
}


/** Starting in ICU 62, strict mode is actually strict with currency formats. */
void NumberFormatTest::TestMismatchedCurrencyFormatFail() {
    IcuTestErrorCode status(*this, "TestMismatchedCurrencyFormatFail");
    LocalPointer<DecimalFormat> df(
            dynamic_cast<DecimalFormat*>(DecimalFormat::createCurrencyInstance("en", status)), status);
    if (!assertSuccess("createCurrencyInstance() failed.", status, true, __FILE__, __LINE__)) {return;}
    UnicodeString pattern;
    assertEquals("Test assumes that currency sign is at the beginning",
            u"\u00A4#,##0.00",
            df->toPattern(pattern));
    // Should round-trip on the correct currency format:
    expect2(*df, 1.23, u"\u00A41.23");
    df->setCurrency(u"EUR", status);
    expect2(*df, 1.23, u"\u20AC1.23");
    // Should parse with currency in the wrong place in lenient mode
    df->setLenient(TRUE);
    expect(*df, u"1.23\u20AC", 1.23);
    expectParseCurrency(*df, u"EUR", 1.23, "1.23\\u20AC");
    // Should NOT parse with currency in the wrong place in STRICT mode
    df->setLenient(FALSE);
    {
        Formattable result;
        ErrorCode failStatus;
        df->parse(u"1.23\u20AC", result, failStatus);
        assertEquals("Should fail to parse", U_INVALID_FORMAT_ERROR, failStatus);
    }
    {
        ParsePosition ppos;
        df->parseCurrency(u"1.23\u20AC", ppos);
        assertEquals("Should fail to parse currency", 0, ppos.getIndex());
    }
}


void
NumberFormatTest::TestDecimalFormatCurrencyParse() {
    // Locale.US
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols* sym = new DecimalFormatSymbols(Locale("en_US"), status);
    if (U_FAILURE(status)) {
        delete sym;
        return;
    }
    UnicodeString pat;
    UChar currency = 0x00A4;
    // "\xA4#,##0.00;-\xA4#,##0.00"
    pat.append(currency).append(currency).append(currency).append("#,##0.00;-").append(currency).append(currency).append(currency).append("#,##0.00");
    DecimalFormat* fmt = new DecimalFormat(pat, sym, status);
    if (U_FAILURE(status)) {
        delete fmt;
        errln("failed to new DecimalFormat in TestDecimalFormatCurrencyParse");
        return;
    }
    const char* DATA[][2] = {
        // the data are:
        // string to be parsed, the parsed result (number)
        {"$1.00", "1"},
        {"USD1.00", "1"},
        {"1.00 US dollar", "1"},
        {"$1,234.56", "1234.56"},
        {"USD1,234.56", "1234.56"},
        {"1,234.56 US dollar", "1234.56"},
    };
    // NOTE: ICU 62 requires that the currency format match the pattern in strict mode.
    fmt->setLenient(TRUE);
    for (uint32_t i = 0; i < UPRV_LENGTHOF(DATA); ++i) {
        UnicodeString stringToBeParsed = ctou(DATA[i][0]);
        double parsedResult = atof(DATA[i][1]);
        UErrorCode status = U_ZERO_ERROR;
        Formattable result;
        fmt->parse(stringToBeParsed, result, status);
        logln((UnicodeString)"Input: " + stringToBeParsed + "; output: " + result.getDouble(status));
        if (U_FAILURE(status) ||
            (result.getType() == Formattable::kDouble &&
            result.getDouble() != parsedResult) ||
            (result.getType() == Formattable::kLong &&
            result.getLong() != parsedResult)) {
            errln((UnicodeString)"FAIL parse: Expected " + parsedResult);
        }
    }
    delete fmt;
}


void
NumberFormatTest::TestCurrencyIsoPluralFormat() {
    static const char* DATA[][6] = {
        // the data are:
        // locale,
        // currency amount to be formatted,
        // currency ISO code to be formatted,
        // format result using CURRENCYSTYLE,
        // format result using ISOCURRENCYSTYLE,
        // format result using PLURALCURRENCYSTYLE,

        {"en_US", "1", "USD", "$1.00", "USD\\u00A01.00", "1.00 US dollars"},
        {"en_US", "1234.56", "USD", "$1,234.56", "USD\\u00A01,234.56", "1,234.56 US dollars"},
        {"en_US", "-1234.56", "USD", "-$1,234.56", "-USD\\u00A01,234.56", "-1,234.56 US dollars"},
        {"zh_CN", "1", "USD", "US$1.00", "USD\\u00A01.00", "1.00\\u00A0\\u7F8E\\u5143"},
        {"zh_CN", "1234.56", "USD", "US$1,234.56", "USD\\u00A01,234.56", "1,234.56\\u00A0\\u7F8E\\u5143"},
        {"zh_CN", "1", "CNY", "\\uFFE51.00", "CNY\\u00A01.00", "1.00\\u00A0\\u4EBA\\u6C11\\u5E01"},
        {"zh_CN", "1234.56", "CNY", "\\uFFE51,234.56", "CNY\\u00A01,234.56", "1,234.56\\u00A0\\u4EBA\\u6C11\\u5E01"},
        {"ru_RU", "1", "RUB", "1,00\\u00A0\\u20BD", "1,00\\u00A0RUB", "1,00 \\u0440\\u043E\\u0441\\u0441\\u0438\\u0439\\u0441\\u043A\\u043E\\u0433\\u043E \\u0440\\u0443\\u0431\\u043B\\u044F"},
        {"ru_RU", "2", "RUB", "2,00\\u00A0\\u20BD", "2,00\\u00A0RUB", "2,00 \\u0440\\u043E\\u0441\\u0441\\u0438\\u0439\\u0441\\u043A\\u043E\\u0433\\u043E \\u0440\\u0443\\u0431\\u043B\\u044F"},
        {"ru_RU", "5", "RUB", "5,00\\u00A0\\u20BD", "5,00\\u00A0RUB", "5,00 \\u0440\\u043E\\u0441\\u0441\\u0438\\u0439\\u0441\\u043A\\u043E\\u0433\\u043E \\u0440\\u0443\\u0431\\u043B\\u044F"},
        // test locale without currency information
        {"root", "-1.23", "USD", "-US$\\u00A01.23", "-USD\\u00A01.23", "-1.23 USD"},
        // test choice format
        {"es_AR", "1", "INR", "INR\\u00A01,00", "INR\\u00A01,00", "1,00 rupia india"},
    };
    static const UNumberFormatStyle currencyStyles[] = {
        UNUM_CURRENCY,
        UNUM_CURRENCY_ISO,
        UNUM_CURRENCY_PLURAL
    };

    for (int32_t i=0; i<UPRV_LENGTHOF(DATA); ++i) {
      const char* localeString = DATA[i][0];
      double numberToBeFormat = atof(DATA[i][1]);
      const char* currencyISOCode = DATA[i][2];
      logln(UnicodeString(u"Locale: ") + localeString + "; amount: " + numberToBeFormat);
      Locale locale(localeString);
      for (int32_t kIndex = 0; kIndex < UPRV_LENGTHOF(currencyStyles); ++kIndex) {
        UNumberFormatStyle k = currencyStyles[kIndex];
        logln(UnicodeString(u"UNumberFormatStyle: ") + k);
        UErrorCode status = U_ZERO_ERROR;
        NumberFormat* numFmt = NumberFormat::createInstance(locale, k, status);
        if (U_FAILURE(status)) {
            delete numFmt;
            dataerrln((UnicodeString)"can not create instance, locale:" + localeString + ", style: " + k + " - " + u_errorName(status));
            continue;
        }
        UChar currencyCode[4];
        u_charsToUChars(currencyISOCode, currencyCode, 4);
        numFmt->setCurrency(currencyCode, status);
        if (U_FAILURE(status)) {
            delete numFmt;
            errln((UnicodeString)"can not set currency:" + currencyISOCode);
            continue;
        }

        UnicodeString strBuf;
        numFmt->format(numberToBeFormat, strBuf);
        int resultDataIndex = 3 + kIndex;
        // DATA[i][resultDataIndex] is the currency format result
        // using 'k' currency style.
        UnicodeString formatResult = ctou(DATA[i][resultDataIndex]);
        if (strBuf.compare(formatResult)) {
            errln("FAIL: Expected " + formatResult + " actual: " + strBuf);
        }
        // test parsing, and test parsing for all currency formats.
        // NOTE: ICU 62 requires that the currency format match the pattern in strict mode.
        numFmt->setLenient(TRUE);
        for (int j = 3; j < 6; ++j) {
            // DATA[i][3] is the currency format result using
            // CURRENCYSTYLE formatter.
            // DATA[i][4] is the currency format result using
            // ISOCURRENCYSTYLE formatter.
            // DATA[i][5] is the currency format result using
            // PLURALCURRENCYSTYLE formatter.
            UnicodeString oneCurrencyFormatResult = ctou(DATA[i][j]);
            UErrorCode status = U_ZERO_ERROR;
            Formattable parseResult;
            numFmt->parse(oneCurrencyFormatResult, parseResult, status);
            if (U_FAILURE(status) ||
                (parseResult.getType() == Formattable::kDouble &&
                 parseResult.getDouble() != numberToBeFormat) ||
                (parseResult.getType() == Formattable::kLong &&
                 parseResult.getLong() != numberToBeFormat)) {
                errln((UnicodeString)"FAIL: getCurrencyFormat of locale " +
                      localeString + " failed roundtripping the number");
                if (parseResult.getType() == Formattable::kDouble) {
                    errln((UnicodeString)"expected: " + numberToBeFormat + "; actual: " +parseResult.getDouble());
                } else {
                    errln((UnicodeString)"expected: " + numberToBeFormat + "; actual: " +parseResult.getLong());
                }
            }
        }
        delete numFmt;
      }
    }
}

void
NumberFormatTest::TestCurrencyParsing() {
    static const char* DATA[][6] = {
        // the data are:
        // locale,
        // currency amount to be formatted,
        // currency ISO code to be formatted,
        // format result using CURRENCYSTYLE,
        // format result using ISOCURRENCYSTYLE,
        // format result using PLURALCURRENCYSTYLE,
        {"en_US", "1", "USD", "$1.00", "USD\\u00A01.00", "1.00 US dollars"},
        {"pa_IN", "1", "USD", "US$\\u00A01.00", "USD\\u00A01.00", "1.00 \\u0a2f\\u0a42.\\u0a10\\u0a38. \\u0a21\\u0a3e\\u0a32\\u0a30"},
        {"es_AR", "1", "USD", "US$\\u00A01,00", "USD\\u00A01,00", "1,00 d\\u00f3lar estadounidense"},
        {"ar_EG", "1", "USD", "\\u0661\\u066b\\u0660\\u0660\\u00a0US$", "\\u0661\\u066b\\u0660\\u0660\\u00a0USD", "\\u0661\\u066b\\u0660\\u0660 \\u062f\\u0648\\u0644\\u0627\\u0631 \\u0623\\u0645\\u0631\\u064a\\u0643\\u064a"},
        {"fa_CA", "1", "USD", "\\u200e$\\u06f1\\u066b\\u06f0\\u06f0", "\\u200eUSD\\u06f1\\u066b\\u06f0\\u06f0", "\\u06f1\\u066b\\u06f0\\u06f0 \\u062f\\u0644\\u0627\\u0631 \\u0627\\u0645\\u0631\\u06cc\\u06a9\\u0627"},
        {"he_IL", "1", "USD", "\\u200f1.00\\u00a0$", "\\u200f1.00\\u00a0USD", "1.00 \\u05d3\\u05d5\\u05dc\\u05e8 \\u05d0\\u05de\\u05e8\\u05d9\\u05e7\\u05d0\\u05d9"},
        {"hr_HR", "1", "USD", "1,00\\u00a0USD", "1,00\\u00a0USD", "1,00 ameri\\u010Dkih dolara"},
        {"id_ID", "1", "USD", "US$\\u00A01,00", "USD\\u00A01,00", "1,00 Dolar Amerika Serikat"},
        {"it_IT", "1", "USD", "1,00\\u00a0USD", "1,00\\u00a0USD", "1,00 dollari statunitensi"},
        {"ko_KR", "1", "USD", "US$\\u00A01.00", "USD\\u00A01.00", "1.00 \\ubbf8\\uad6d \\ub2ec\\ub7ec"},
        {"ja_JP", "1", "USD", "$1.00", "USD\\u00A01.00", "1.00\\u00A0\\u7c73\\u30c9\\u30eb"},
        {"zh_CN", "1", "CNY", "\\uFFE51.00", "CNY\\u00A001.00", "1.00\\u00A0\\u4EBA\\u6C11\\u5E01"},
        {"zh_TW", "1", "CNY", "CN\\u00A51.00", "CNY\\u00A01.00", "1.00 \\u4eba\\u6c11\\u5e63"},
        {"zh_Hant", "1", "CNY", "CN\\u00A51.00", "CNY\\u00A01.00", "1.00 \\u4eba\\u6c11\\u5e63"},
        {"zh_Hant", "1", "JPY", "\\u00A51.00", "JPY\\u00A01.00", "1 \\u65E5\\u5713"},
        {"ja_JP", "1", "JPY", "\\uFFE51.00", "JPY\\u00A01.00", "1\\u00A0\\u5186"},
        // ICU 62 requires #parseCurrency() to recognize variants when parsing
        // {"ja_JP", "1", "JPY", "\\u00A51.00", "JPY\\u00A01.00", "1\\u00A0\\u5186"},
        {"ru_RU", "1", "RUB", "1,00\\u00A0\\u00A0\\u20BD", "1,00\\u00A0\\u00A0RUB", "1,00 \\u0440\\u043E\\u0441\\u0441\\u0438\\u0439\\u0441\\u043A\\u043E\\u0433\\u043E \\u0440\\u0443\\u0431\\u043B\\u044F"}
    };
    static const UNumberFormatStyle currencyStyles[] = {
        UNUM_CURRENCY,
        UNUM_CURRENCY_ISO,
        UNUM_CURRENCY_PLURAL
    };
    static const char* currencyStyleNames[] = {
      "UNUM_CURRENCY",
      "UNUM_CURRENCY_ISO",
      "UNUM_CURRENCY_PLURAL"
    };

#ifdef NUMFMTST_CACHE_DEBUG
int deadloop = 0;
for (;;) {
    printf("loop: %d\n", deadloop++);
#endif
    for (uint32_t i=0; i< UPRV_LENGTHOF(DATA); ++i) {  /* i = test case #  - should be i=0*/
      for (int32_t kIndex = 2; kIndex < UPRV_LENGTHOF(currencyStyles); ++kIndex) {
        UNumberFormatStyle k = currencyStyles[kIndex]; /* k = style */
        const char* localeString = DATA[i][0];
        double numberToBeFormat = atof(DATA[i][1]);
        const char* currencyISOCode = DATA[i][2];
        Locale locale(localeString);
        UErrorCode status = U_ZERO_ERROR;
        NumberFormat* numFmt = NumberFormat::createInstance(locale, k, status);
        logln("#%d NumberFormat(%s, %s) Currency=%s\n",
              i, localeString, currencyStyleNames[kIndex],
              currencyISOCode);

        if (U_FAILURE(status)) {
            delete numFmt;
            dataerrln((UnicodeString)"can not create instance, locale:" + localeString + ", style: " + k + " - " + u_errorName(status));
            continue;
        }
        UChar currencyCode[4];
        u_charsToUChars(currencyISOCode, currencyCode, 4);
        numFmt->setCurrency(currencyCode, status);
        if (U_FAILURE(status)) {
            delete numFmt;
            errln((UnicodeString)"can not set currency:" + currencyISOCode);
            continue;
        }

        UnicodeString strBuf;
        numFmt->format(numberToBeFormat, strBuf);
        int resultDataIndex = 3 + kIndex;
        // DATA[i][resultDataIndex] is the currency format result
        // using 'k' currency style.
        UnicodeString formatResult = ctou(DATA[i][resultDataIndex]);
        if (strBuf.compare(formatResult)) {
            errln("FAIL: Expected " + formatResult + " actual: " + strBuf);
        }
        // test parsing, and test parsing for all currency formats.
        // NOTE: ICU 62 requires that the currency format match the pattern in strict mode.
        numFmt->setLenient(TRUE);
        for (int j = 3; j < 6; ++j) {
            // DATA[i][3] is the currency format result using
            // CURRENCYSTYLE formatter.
            // DATA[i][4] is the currency format result using
            // ISOCURRENCYSTYLE formatter.
            // DATA[i][5] is the currency format result using
            // PLURALCURRENCYSTYLE formatter.
            UnicodeString oneCurrencyFormatResult = ctou(DATA[i][j]);
            UErrorCode status = U_ZERO_ERROR;
            Formattable parseResult;
            logln("parse(%s)", DATA[i][j]);
            numFmt->parse(oneCurrencyFormatResult, parseResult, status);
            if (U_FAILURE(status) ||
                (parseResult.getType() == Formattable::kDouble &&
                 parseResult.getDouble() != numberToBeFormat) ||
                (parseResult.getType() == Formattable::kLong &&
                 parseResult.getLong() != numberToBeFormat)) {
                errln((UnicodeString)"FAIL: NumberFormat(" + localeString +", " + currencyStyleNames[kIndex] +
                      "), Currency="+currencyISOCode+", parse("+DATA[i][j]+") returned error " + (UnicodeString)u_errorName(status)+".  Testcase: data[" + i + "][" + currencyStyleNames[j-3] +"="+j+"]");
                if (parseResult.getType() == Formattable::kDouble) {
                    errln((UnicodeString)"expected: " + numberToBeFormat + "; actual (double): " +parseResult.getDouble());
                } else {
                    errln((UnicodeString)"expected: " + numberToBeFormat + "; actual (long): " +parseResult.getLong());
                }
                errln((UnicodeString)" round-trip would be: " + strBuf);
            }
        }
        delete numFmt;
      }
    }
#ifdef NUMFMTST_CACHE_DEBUG
}
#endif
}


void
NumberFormatTest::TestParseCurrencyInUCurr() {
    const char* DATA[] = {
        "1.00 US DOLLAR",  // case in-sensitive
        "$1.00",
        "USD1.00",
        "usd1.00", // case in-sensitive: #13696
        "US dollar1.00",
        "US dollars1.00",
        "$1.00",
        "A$1.00",
        "ADP1.00",
        "ADP1.00",
        "AED1.00",
        "AED1.00",
        "AFA1.00",
        "AFA1.00",
        "AFN1.00",
        "ALL1.00",
        "AMD1.00",
        "ANG1.00",
        "AOA1.00",
        "AOK1.00",
        "AOK1.00",
        "AON1.00",
        "AON1.00",
        "AOR1.00",
        "AOR1.00",
        "ARS1.00",
        "ARA1.00",
        "ARA1.00",
        "ARP1.00",
        "ARP1.00",
        "ARS1.00",
        "ATS1.00",
        "ATS1.00",
        "AUD1.00",
        "AWG1.00",
        "AZM1.00",
        "AZM1.00",
        "AZN1.00",
        "Afghan Afghani (1927\\u20132002)1.00",
        "Afghan afghani (1927\\u20132002)1.00",
        "Afghan Afghani1.00",
        "Afghan Afghanis1.00",
        "Albanian Lek1.00",
        "Albanian lek1.00",
        "Albanian lek\\u00eb1.00",
        "Algerian Dinar1.00",
        "Algerian dinar1.00",
        "Algerian dinars1.00",
        "Andorran Peseta1.00",
        "Andorran peseta1.00",
        "Andorran pesetas1.00",
        "Angolan Kwanza (1977\\u20131991)1.00",
        "Angolan Readjusted Kwanza (1995\\u20131999)1.00",
        "Angolan Kwanza1.00",
        "Angolan New Kwanza (1990\\u20132000)1.00",
        "Angolan kwanza (1977\\u20131991)1.00",
        "Angolan readjusted kwanza (1995\\u20131999)1.00",
        "Angolan kwanza1.00",
        "Angolan kwanzas (1977\\u20131991)1.00",
        "Angolan readjusted kwanzas (1995\\u20131999)1.00",
        "Angolan kwanzas1.00",
        "Angolan new kwanza (1990\\u20132000)1.00",
        "Angolan new kwanzas (1990\\u20132000)1.00",
        "Argentine Austral1.00",
        "Argentine Peso (1983\\u20131985)1.00",
        "Argentine Peso1.00",
        "Argentine austral1.00",
        "Argentine australs1.00",
        "Argentine peso (1983\\u20131985)1.00",
        "Argentine peso1.00",
        "Argentine pesos (1983\\u20131985)1.00",
        "Argentine pesos1.00",
        "Armenian Dram1.00",
        "Armenian dram1.00",
        "Armenian drams1.00",
        "Aruban Florin1.00",
        "Aruban florin1.00",
        "Australian Dollar1.00",
        "Australian dollar1.00",
        "Australian dollars1.00",
        "Austrian Schilling1.00",
        "Austrian schilling1.00",
        "Austrian schillings1.00",
        "Azerbaijani Manat (1993\\u20132006)1.00",
        "Azerbaijani Manat1.00",
        "Azerbaijani manat (1993\\u20132006)1.00",
        "Azerbaijani manat1.00",
        "Azerbaijani manats (1993\\u20132006)1.00",
        "Azerbaijani manats1.00",
        "BAD1.00",
        "BAD1.00",
        "BAM1.00",
        "BBD1.00",
        "BDT1.00",
        "BEC1.00",
        "BEC1.00",
        "BEF1.00",
        "BEL1.00",
        "BEL1.00",
        "BGL1.00",
        "BGN1.00",
        "BGN1.00",
        "BHD1.00",
        "BIF1.00",
        "BMD1.00",
        "BND1.00",
        "BOB1.00",
        "BOP1.00",
        "BOP1.00",
        "BOV1.00",
        "BOV1.00",
        "BRB1.00",
        "BRB1.00",
        "BRC1.00",
        "BRC1.00",
        "BRE1.00",
        "BRE1.00",
        "BRL1.00",
        "BRN1.00",
        "BRN1.00",
        "BRR1.00",
        "BRR1.00",
        "BSD1.00",
        "BSD1.00",
        "BTN1.00",
        "BUK1.00",
        "BUK1.00",
        "BWP1.00",
        "BYB1.00",
        "BYB1.00",
        "BYR1.00",
        "BZD1.00",
        "Bahamian Dollar1.00",
        "Bahamian dollar1.00",
        "Bahamian dollars1.00",
        "Bahraini Dinar1.00",
        "Bahraini dinar1.00",
        "Bahraini dinars1.00",
        "Bangladeshi Taka1.00",
        "Bangladeshi taka1.00",
        "Bangladeshi takas1.00",
        "Barbadian Dollar1.00",
        "Barbadian dollar1.00",
        "Barbadian dollars1.00",
        "Belarusian Ruble (1994\\u20131999)1.00",
        "Belarusian Ruble1.00",
        "Belarusian ruble (1994\\u20131999)1.00",
        "Belarusian rubles (1994\\u20131999)1.00",
        "Belarusian ruble1.00",
        "Belarusian rubles1.00",
        "Belgian Franc (convertible)1.00",
        "Belgian Franc (financial)1.00",
        "Belgian Franc1.00",
        "Belgian franc (convertible)1.00",
        "Belgian franc (financial)1.00",
        "Belgian franc1.00",
        "Belgian francs (convertible)1.00",
        "Belgian francs (financial)1.00",
        "Belgian francs1.00",
        "Belize Dollar1.00",
        "Belize dollar1.00",
        "Belize dollars1.00",
        "Bermudan Dollar1.00",
        "Bermudan dollar1.00",
        "Bermudan dollars1.00",
        "Bhutanese Ngultrum1.00",
        "Bhutanese ngultrum1.00",
        "Bhutanese ngultrums1.00",
        "Bolivian Mvdol1.00",
        "Bolivian Peso1.00",
        "Bolivian mvdol1.00",
        "Bolivian mvdols1.00",
        "Bolivian peso1.00",
        "Bolivian pesos1.00",
        "Bolivian Boliviano1.00",
        "Bolivian Boliviano1.00",
        "Bolivian Bolivianos1.00",
        "Bosnia-Herzegovina Convertible Mark1.00",
        "Bosnia-Herzegovina Dinar (1992\\u20131994)1.00",
        "Bosnia-Herzegovina convertible mark1.00",
        "Bosnia-Herzegovina convertible marks1.00",
        "Bosnia-Herzegovina dinar (1992\\u20131994)1.00",
        "Bosnia-Herzegovina dinars (1992\\u20131994)1.00",
        "Botswanan Pula1.00",
        "Botswanan pula1.00",
        "Botswanan pulas1.00",
        "Brazilian New Cruzado (1989\\u20131990)1.00",
        "Brazilian Cruzado (1986\\u20131989)1.00",
        "Brazilian Cruzeiro (1990\\u20131993)1.00",
        "Brazilian New Cruzeiro (1967\\u20131986)1.00",
        "Brazilian Cruzeiro (1993\\u20131994)1.00",
        "Brazilian Real1.00",
        "Brazilian new cruzado (1989\\u20131990)1.00",
        "Brazilian new cruzados (1989\\u20131990)1.00",
        "Brazilian cruzado (1986\\u20131989)1.00",
        "Brazilian cruzados (1986\\u20131989)1.00",
        "Brazilian cruzeiro (1990\\u20131993)1.00",
        "Brazilian new cruzeiro (1967\\u20131986)1.00",
        "Brazilian cruzeiro (1993\\u20131994)1.00",
        "Brazilian cruzeiros (1990\\u20131993)1.00",
        "Brazilian new cruzeiros (1967\\u20131986)1.00",
        "Brazilian cruzeiros (1993\\u20131994)1.00",
        "Brazilian real1.00",
        "Brazilian reals1.00",
        "British Pound1.00",
        "British pound1.00",
        "British pounds1.00",
        "Brunei Dollar1.00",
        "Brunei dollar1.00",
        "Brunei dollars1.00",
        "Bulgarian Hard Lev1.00",
        "Bulgarian Lev1.00",
        "Bulgarian Leva1.00",
        "Bulgarian hard lev1.00",
        "Bulgarian hard leva1.00",
        "Bulgarian lev1.00",
        "Burmese Kyat1.00",
        "Burmese kyat1.00",
        "Burmese kyats1.00",
        "Burundian Franc1.00",
        "Burundian franc1.00",
        "Burundian francs1.00",
        "CA$1.00",
        "CAD1.00",
        "CDF1.00",
        "CDF1.00",
        "West African CFA Franc1.00",
        "Central African CFA Franc1.00",
        "West African CFA franc1.00",
        "Central African CFA franc1.00",
        "West African CFA francs1.00",
        "Central African CFA francs1.00",
        "CFP Franc1.00",
        "CFP franc1.00",
        "CFP francs1.00",
        "CFPF1.00",
        "CHE1.00",
        "CHE1.00",
        "CHF1.00",
        "CHW1.00",
        "CHW1.00",
        "CLF1.00",
        "CLF1.00",
        "CLP1.00",
        "CNY1.00",
        "COP1.00",
        "COU1.00",
        "COU1.00",
        "CRC1.00",
        "CSD1.00",
        "CSD1.00",
        "CSK1.00",
        "CSK1.00",
        "CUP1.00",
        "CUP1.00",
        "CVE1.00",
        "CYP1.00",
        "CZK1.00",
        "Cambodian Riel1.00",
        "Cambodian riel1.00",
        "Cambodian riels1.00",
        "Canadian Dollar1.00",
        "Canadian dollar1.00",
        "Canadian dollars1.00",
        "Cape Verdean Escudo1.00",
        "Cape Verdean escudo1.00",
        "Cape Verdean escudos1.00",
        "Cayman Islands Dollar1.00",
        "Cayman Islands dollar1.00",
        "Cayman Islands dollars1.00",
        "Chilean Peso1.00",
        "Chilean Unit of Account (UF)1.00",
        "Chilean peso1.00",
        "Chilean pesos1.00",
        "Chilean unit of account (UF)1.00",
        "Chilean units of account (UF)1.00",
        "Chinese Yuan1.00",
        "Chinese yuan1.00",
        "Colombian Peso1.00",
        "Colombian peso1.00",
        "Colombian pesos1.00",
        "Comorian Franc1.00",
        "Comorian franc1.00",
        "Comorian francs1.00",
        "Congolese Franc1.00",
        "Congolese franc1.00",
        "Congolese francs1.00",
        "Costa Rican Col\\u00f3n1.00",
        "Costa Rican col\\u00f3n1.00",
        "Costa Rican col\\u00f3ns1.00",
        "Croatian Dinar1.00",
        "Croatian Kuna1.00",
        "Croatian dinar1.00",
        "Croatian dinars1.00",
        "Croatian kuna1.00",
        "Croatian kunas1.00",
        "Cuban Peso1.00",
        "Cuban peso1.00",
        "Cuban pesos1.00",
        "Cypriot Pound1.00",
        "Cypriot pound1.00",
        "Cypriot pounds1.00",
        "Czech Koruna1.00",
        "Czech koruna1.00",
        "Czech korunas1.00",
        "Czechoslovak Hard Koruna1.00",
        "Czechoslovak hard koruna1.00",
        "Czechoslovak hard korunas1.00",
        "DDM1.00",
        "DDM1.00",
        "DEM1.00",
        "DEM1.00",
        "DJF1.00",
        "DKK1.00",
        "DOP1.00",
        "DZD1.00",
        "Danish Krone1.00",
        "Danish krone1.00",
        "Danish kroner1.00",
        "German Mark1.00",
        "German mark1.00",
        "German marks1.00",
        "Djiboutian Franc1.00",
        "Djiboutian franc1.00",
        "Djiboutian francs1.00",
        "Dominican Peso1.00",
        "Dominican peso1.00",
        "Dominican pesos1.00",
        "EC$1.00",
        "ECS1.00",
        "ECS1.00",
        "ECV1.00",
        "ECV1.00",
        "EEK1.00",
        "EEK1.00",
        "EGP1.00",
        "EGP1.00",
        "ERN1.00",
        "ERN1.00",
        "ESA1.00",
        "ESA1.00",
        "ESB1.00",
        "ESB1.00",
        "ESP1.00",
        "ETB1.00",
        "EUR1.00",
        "East Caribbean Dollar1.00",
        "East Caribbean dollar1.00",
        "East Caribbean dollars1.00",
        "East German Mark1.00",
        "East German mark1.00",
        "East German marks1.00",
        "Ecuadorian Sucre1.00",
        "Ecuadorian Unit of Constant Value1.00",
        "Ecuadorian sucre1.00",
        "Ecuadorian sucres1.00",
        "Ecuadorian unit of constant value1.00",
        "Ecuadorian units of constant value1.00",
        "Egyptian Pound1.00",
        "Egyptian pound1.00",
        "Egyptian pounds1.00",
        "Salvadoran Col\\u00f3n1.00",
        "Salvadoran col\\u00f3n1.00",
        "Salvadoran colones1.00",
        "Equatorial Guinean Ekwele1.00",
        "Equatorial Guinean ekwele1.00",
        "Eritrean Nakfa1.00",
        "Eritrean nakfa1.00",
        "Eritrean nakfas1.00",
        "Estonian Kroon1.00",
        "Estonian kroon1.00",
        "Estonian kroons1.00",
        "Ethiopian Birr1.00",
        "Ethiopian birr1.00",
        "Ethiopian birrs1.00",
        "Euro1.00",
        "European Composite Unit1.00",
        "European Currency Unit1.00",
        "European Monetary Unit1.00",
        "European Unit of Account (XBC)1.00",
        "European Unit of Account (XBD)1.00",
        "European composite unit1.00",
        "European composite units1.00",
        "European currency unit1.00",
        "European currency units1.00",
        "European monetary unit1.00",
        "European monetary units1.00",
        "European unit of account (XBC)1.00",
        "European unit of account (XBD)1.00",
        "European units of account (XBC)1.00",
        "European units of account (XBD)1.00",
        "FIM1.00",
        "FIM1.00",
        "FJD1.00",
        "FKP1.00",
        "FKP1.00",
        "FRF1.00",
        "FRF1.00",
        "Falkland Islands Pound1.00",
        "Falkland Islands pound1.00",
        "Falkland Islands pounds1.00",
        "Fijian Dollar1.00",
        "Fijian dollar1.00",
        "Fijian dollars1.00",
        "Finnish Markka1.00",
        "Finnish markka1.00",
        "Finnish markkas1.00",
        "CHF1.00",
        "French Franc1.00",
        "French Gold Franc1.00",
        "French UIC-Franc1.00",
        "French UIC-franc1.00",
        "French UIC-francs1.00",
        "French franc1.00",
        "French francs1.00",
        "French gold franc1.00",
        "French gold francs1.00",
        "GBP1.00",
        "GEK1.00",
        "GEK1.00",
        "GEL1.00",
        "GHC1.00",
        "GHC1.00",
        "GHS1.00",
        "GIP1.00",
        "GIP1.00",
        "GMD1.00",
        "GMD1.00",
        "GNF1.00",
        "GNS1.00",
        "GNS1.00",
        "GQE1.00",
        "GQE1.00",
        "GRD1.00",
        "GRD1.00",
        "GTQ1.00",
        "GWE1.00",
        "GWE1.00",
        "GWP1.00",
        "GWP1.00",
        "GYD1.00",
        "Gambian Dalasi1.00",
        "Gambian dalasi1.00",
        "Gambian dalasis1.00",
        "Georgian Kupon Larit1.00",
        "Georgian Lari1.00",
        "Georgian kupon larit1.00",
        "Georgian kupon larits1.00",
        "Georgian lari1.00",
        "Georgian laris1.00",
        "Ghanaian Cedi (1979\\u20132007)1.00",
        "Ghanaian Cedi1.00",
        "Ghanaian cedi (1979\\u20132007)1.00",
        "Ghanaian cedi1.00",
        "Ghanaian cedis (1979\\u20132007)1.00",
        "Ghanaian cedis1.00",
        "Gibraltar Pound1.00",
        "Gibraltar pound1.00",
        "Gibraltar pounds1.00",
        "Gold1.00",
        "Gold1.00",
        "Greek Drachma1.00",
        "Greek drachma1.00",
        "Greek drachmas1.00",
        "Guatemalan Quetzal1.00",
        "Guatemalan quetzal1.00",
        "Guatemalan quetzals1.00",
        "Guinean Franc1.00",
        "Guinean Syli1.00",
        "Guinean franc1.00",
        "Guinean francs1.00",
        "Guinean syli1.00",
        "Guinean sylis1.00",
        "Guinea-Bissau Peso1.00",
        "Guinea-Bissau peso1.00",
        "Guinea-Bissau pesos1.00",
        "Guyanaese Dollar1.00",
        "Guyanaese dollar1.00",
        "Guyanaese dollars1.00",
        "HK$1.00",
        "HKD1.00",
        "HNL1.00",
        "HRD1.00",
        "HRD1.00",
        "HRK1.00",
        "HRK1.00",
        "HTG1.00",
        "HTG1.00",
        "HUF1.00",
        "Haitian Gourde1.00",
        "Haitian gourde1.00",
        "Haitian gourdes1.00",
        "Honduran Lempira1.00",
        "Honduran lempira1.00",
        "Honduran lempiras1.00",
        "Hong Kong Dollar1.00",
        "Hong Kong dollar1.00",
        "Hong Kong dollars1.00",
        "Hungarian Forint1.00",
        "Hungarian forint1.00",
        "Hungarian forints1.00",
        "IDR1.00",
        "IEP1.00",
        "ILP1.00",
        "ILP1.00",
        "ILS1.00",
        "INR1.00",
        "IQD1.00",
        "IRR1.00",
        "ISK1.00",
        "ISK1.00",
        "ITL1.00",
        "Icelandic Kr\\u00f3na1.00",
        "Icelandic kr\\u00f3na1.00",
        "Icelandic kr\\u00f3nur1.00",
        "Indian Rupee1.00",
        "Indian rupee1.00",
        "Indian rupees1.00",
        "Indonesian Rupiah1.00",
        "Indonesian rupiah1.00",
        "Indonesian rupiahs1.00",
        "Iranian Rial1.00",
        "Iranian rial1.00",
        "Iranian rials1.00",
        "Iraqi Dinar1.00",
        "Iraqi dinar1.00",
        "Iraqi dinars1.00",
        "Irish Pound1.00",
        "Irish pound1.00",
        "Irish pounds1.00",
        "Israeli Pound1.00",
        "Israeli new shekel1.00",
        "Israeli pound1.00",
        "Israeli pounds1.00",
        "Italian Lira1.00",
        "Italian lira1.00",
        "Italian liras1.00",
        "JMD1.00",
        "JOD1.00",
        "JPY1.00",
        "Jamaican Dollar1.00",
        "Jamaican dollar1.00",
        "Jamaican dollars1.00",
        "Japanese Yen1.00",
        "Japanese yen1.00",
        "Jordanian Dinar1.00",
        "Jordanian dinar1.00",
        "Jordanian dinars1.00",
        "KES1.00",
        "KGS1.00",
        "KHR1.00",
        "KMF1.00",
        "KPW1.00",
        "KPW1.00",
        "KRW1.00",
        "KWD1.00",
        "KYD1.00",
        "KYD1.00",
        "KZT1.00",
        "Kazakhstani Tenge1.00",
        "Kazakhstani tenge1.00",
        "Kazakhstani tenges1.00",
        "Kenyan Shilling1.00",
        "Kenyan shilling1.00",
        "Kenyan shillings1.00",
        "Kuwaiti Dinar1.00",
        "Kuwaiti dinar1.00",
        "Kuwaiti dinars1.00",
        "Kyrgystani Som1.00",
        "Kyrgystani som1.00",
        "Kyrgystani soms1.00",
        "HNL1.00",
        "LAK1.00",
        "LAK1.00",
        "LBP1.00",
        "LKR1.00",
        "LRD1.00",
        "LRD1.00",
        "LSL1.00",
        "LTL1.00",
        "LTL1.00",
        "LTT1.00",
        "LTT1.00",
        "LUC1.00",
        "LUC1.00",
        "LUF1.00",
        "LUF1.00",
        "LUL1.00",
        "LUL1.00",
        "LVL1.00",
        "LVL1.00",
        "LVR1.00",
        "LVR1.00",
        "LYD1.00",
        "Laotian Kip1.00",
        "Laotian kip1.00",
        "Laotian kips1.00",
        "Latvian Lats1.00",
        "Latvian Ruble1.00",
        "Latvian lats1.00",
        "Latvian lati1.00",
        "Latvian ruble1.00",
        "Latvian rubles1.00",
        "Lebanese Pound1.00",
        "Lebanese pound1.00",
        "Lebanese pounds1.00",
        "Lesotho Loti1.00",
        "Lesotho loti1.00",
        "Lesotho lotis1.00",
        "Liberian Dollar1.00",
        "Liberian dollar1.00",
        "Liberian dollars1.00",
        "Libyan Dinar1.00",
        "Libyan dinar1.00",
        "Libyan dinars1.00",
        "Lithuanian Litas1.00",
        "Lithuanian Talonas1.00",
        "Lithuanian litas1.00",
        "Lithuanian litai1.00",
        "Lithuanian talonas1.00",
        "Lithuanian talonases1.00",
        "Luxembourgian Convertible Franc1.00",
        "Luxembourg Financial Franc1.00",
        "Luxembourgian Franc1.00",
        "Luxembourgian convertible franc1.00",
        "Luxembourgian convertible francs1.00",
        "Luxembourg financial franc1.00",
        "Luxembourg financial francs1.00",
        "Luxembourgian franc1.00",
        "Luxembourgian francs1.00",
        "MAD1.00",
        "MAD1.00",
        "MAF1.00",
        "MAF1.00",
        "MDL1.00",
        "MDL1.00",
        "MX$1.00",
        "MGA1.00",
        "MGA1.00",
        "MGF1.00",
        "MGF1.00",
        "MKD1.00",
        "MLF1.00",
        "MLF1.00",
        "MMK1.00",
        "MMK1.00",
        "MNT1.00",
        "MOP1.00",
        "MOP1.00",
        "MRO1.00",
        "MTL1.00",
        "MTP1.00",
        "MTP1.00",
        "MUR1.00",
        "MUR1.00",
        "MVR1.00",
        "MVR1.00",
        "MWK1.00",
        "MXN1.00",
        "MXP1.00",
        "MXP1.00",
        "MXV1.00",
        "MXV1.00",
        "MYR1.00",
        "MZE1.00",
        "MZE1.00",
        "MZM1.00",
        "MZN1.00",
        "Macanese Pataca1.00",
        "Macanese pataca1.00",
        "Macanese patacas1.00",
        "Macedonian Denar1.00",
        "Macedonian denar1.00",
        "Macedonian denari1.00",
        "Malagasy Ariaries1.00",
        "Malagasy Ariary1.00",
        "Malagasy Ariary1.00",
        "Malagasy Franc1.00",
        "Malagasy franc1.00",
        "Malagasy francs1.00",
        "Malawian Kwacha1.00",
        "Malawian Kwacha1.00",
        "Malawian Kwachas1.00",
        "Malaysian Ringgit1.00",
        "Malaysian ringgit1.00",
        "Malaysian ringgits1.00",
        "Maldivian Rufiyaa1.00",
        "Maldivian rufiyaa1.00",
        "Maldivian rufiyaas1.00",
        "Malian Franc1.00",
        "Malian franc1.00",
        "Malian francs1.00",
        "Maltese Lira1.00",
        "Maltese Pound1.00",
        "Maltese lira1.00",
        "Maltese lira1.00",
        "Maltese pound1.00",
        "Maltese pounds1.00",
        "Mauritanian Ouguiya1.00",
        "Mauritanian ouguiya1.00",
        "Mauritanian ouguiyas1.00",
        "Mauritian Rupee1.00",
        "Mauritian rupee1.00",
        "Mauritian rupees1.00",
        "Mexican Peso1.00",
        "Mexican Silver Peso (1861\\u20131992)1.00",
        "Mexican Investment Unit1.00",
        "Mexican peso1.00",
        "Mexican pesos1.00",
        "Mexican silver peso (1861\\u20131992)1.00",
        "Mexican silver pesos (1861\\u20131992)1.00",
        "Mexican investment unit1.00",
        "Mexican investment units1.00",
        "Moldovan Leu1.00",
        "Moldovan leu1.00",
        "Moldovan lei1.00",
        "Mongolian Tugrik1.00",
        "Mongolian tugrik1.00",
        "Mongolian tugriks1.00",
        "Moroccan Dirham1.00",
        "Moroccan Franc1.00",
        "Moroccan dirham1.00",
        "Moroccan dirhams1.00",
        "Moroccan franc1.00",
        "Moroccan francs1.00",
        "Mozambican Escudo1.00",
        "Mozambican Metical1.00",
        "Mozambican escudo1.00",
        "Mozambican escudos1.00",
        "Mozambican metical1.00",
        "Mozambican meticals1.00",
        "Myanmar Kyat1.00",
        "Myanmar kyat1.00",
        "Myanmar kyats1.00",
        "NAD1.00",
        "NGN1.00",
        "NIC1.00",
        "NIO1.00",
        "NIO1.00",
        "NLG1.00",
        "NLG1.00",
        "NOK1.00",
        "NPR1.00",
        "NT$1.00",
        "NZ$1.00",
        "NZD1.00",
        "Namibian Dollar1.00",
        "Namibian dollar1.00",
        "Namibian dollars1.00",
        "Nepalese Rupee1.00",
        "Nepalese rupee1.00",
        "Nepalese rupees1.00",
        "Netherlands Antillean Guilder1.00",
        "Netherlands Antillean guilder1.00",
        "Netherlands Antillean guilders1.00",
        "Dutch Guilder1.00",
        "Dutch guilder1.00",
        "Dutch guilders1.00",
        "Israeli New Shekel1.00",
        "Israeli New Shekels1.00",
        "New Zealand Dollar1.00",
        "New Zealand dollar1.00",
        "New Zealand dollars1.00",
        "Nicaraguan C\\u00f3rdoba1.00",
        "Nicaraguan C\\u00f3rdoba (1988\\u20131991)1.00",
        "Nicaraguan c\\u00f3rdoba1.00",
        "Nicaraguan c\\u00f3rdobas1.00",
        "Nicaraguan c\\u00f3rdoba (1988\\u20131991)1.00",
        "Nicaraguan c\\u00f3rdobas (1988\\u20131991)1.00",
        "Nigerian Naira1.00",
        "Nigerian naira1.00",
        "Nigerian nairas1.00",
        "North Korean Won1.00",
        "North Korean won1.00",
        "North Korean won1.00",
        "Norwegian Krone1.00",
        "Norwegian krone1.00",
        "Norwegian kroner1.00",
        "OMR1.00",
        "Mozambican Metical (1980\\u20132006)1.00",
        "Mozambican metical (1980\\u20132006)1.00",
        "Mozambican meticals (1980\\u20132006)1.00",
        "Romanian Lei (1952\\u20132006)1.00",
        "Romanian Leu (1952\\u20132006)1.00",
        "Romanian leu (1952\\u20132006)1.00",
        "Serbian Dinar (2002\\u20132006)1.00",
        "Serbian dinar (2002\\u20132006)1.00",
        "Serbian dinars (2002\\u20132006)1.00",
        "Sudanese Dinar (1992\\u20132007)1.00",
        "Sudanese Pound (1957\\u20131998)1.00",
        "Sudanese dinar (1992\\u20132007)1.00",
        "Sudanese dinars (1992\\u20132007)1.00",
        "Sudanese pound (1957\\u20131998)1.00",
        "Sudanese pounds (1957\\u20131998)1.00",
        "Turkish Lira (1922\\u20132005)1.00",
        "Turkish Lira (1922\\u20132005)1.00",
        "Omani Rial1.00",
        "Omani rial1.00",
        "Omani rials1.00",
        "PAB1.00",
        "PAB1.00",
        "PEI1.00",
        "PEI1.00",
        "PEN1.00",
        "PEN1.00",
        "PES1.00",
        "PES1.00",
        "PGK1.00",
        "PGK1.00",
        "PHP1.00",
        "PKR1.00",
        "PLN1.00",
        "PLZ1.00",
        "PLZ1.00",
        "PTE1.00",
        "PTE1.00",
        "PYG1.00",
        "Pakistani Rupee1.00",
        "Pakistani rupee1.00",
        "Pakistani rupees1.00",
        "Palladium1.00",
        "Palladium1.00",
        "Panamanian Balboa1.00",
        "Panamanian balboa1.00",
        "Panamanian balboas1.00",
        "Papua New Guinean Kina1.00",
        "Papua New Guinean kina1.00",
        "Papua New Guinean kina1.00",
        "Paraguayan Guarani1.00",
        "Paraguayan guarani1.00",
        "Paraguayan guaranis1.00",
        "Peruvian Inti1.00",
        "Peruvian Sol1.00",
        "Peruvian Sol (1863\\u20131965)1.00",
        "Peruvian inti1.00",
        "Peruvian intis1.00",
        "Peruvian sol1.00",
        "Peruvian soles1.00",
        "Peruvian sol (1863\\u20131965)1.00",
        "Peruvian soles (1863\\u20131965)1.00",
        "Philippine Piso1.00",
        "Philippine piso1.00",
        "Philippine pisos1.00",
        "Platinum1.00",
        "Platinum1.00",
        "Polish Zloty (1950\\u20131995)1.00",
        "Polish Zloty1.00",
        "Polish zlotys1.00",
        "Polish zloty (PLZ)1.00",
        "Polish zloty1.00",
        "Polish zlotys (PLZ)1.00",
        "Portuguese Escudo1.00",
        "Portuguese Guinea Escudo1.00",
        "Portuguese Guinea escudo1.00",
        "Portuguese Guinea escudos1.00",
        "Portuguese escudo1.00",
        "Portuguese escudos1.00",
        "GTQ1.00",
        "QAR1.00",
        "Qatari Rial1.00",
        "Qatari rial1.00",
        "Qatari rials1.00",
        "RHD1.00",
        "RHD1.00",
        "RINET Funds1.00",
        "RINET Funds1.00",
        "CN\\u00a51.00",
        "ROL1.00",
        "ROL1.00",
        "RON1.00",
        "RON1.00",
        "RSD1.00",
        "RSD1.00",
        "RUB1.00",
        "RUR1.00",
        "RUR1.00",
        "RWF1.00",
        "RWF1.00",
        "Rhodesian Dollar1.00",
        "Rhodesian dollar1.00",
        "Rhodesian dollars1.00",
        "Romanian Leu1.00",
        "Romanian lei1.00",
        "Romanian leu1.00",
        "Russian Ruble (1991\\u20131998)1.00",
        "Russian Ruble1.00",
        "Russian ruble (1991\\u20131998)1.00",
        "Russian ruble1.00",
        "Russian rubles (1991\\u20131998)1.00",
        "Russian rubles1.00",
        "Rwandan Franc1.00",
        "Rwandan franc1.00",
        "Rwandan francs1.00",
        "SAR1.00",
        "SBD1.00",
        "SCR1.00",
        "SDD1.00",
        "SDD1.00",
        "SDG1.00",
        "SDG1.00",
        "SDP1.00",
        "SDP1.00",
        "SEK1.00",
        "SGD1.00",
        "SHP1.00",
        "SHP1.00",
        "SIT1.00",
        "SIT1.00",
        "SKK1.00",
        "SLL1.00",
        "SLL1.00",
        "SOS1.00",
        "SRD1.00",
        "SRD1.00",
        "SRG1.00",
        "STD1.00",
        "SUR1.00",
        "SUR1.00",
        "SVC1.00",
        "SVC1.00",
        "SYP1.00",
        "SZL1.00",
        "St. Helena Pound1.00",
        "St. Helena pound1.00",
        "St. Helena pounds1.00",
        "S\\u00e3o Tom\\u00e9 & Pr\\u00edncipe Dobra1.00",
        "S\\u00e3o Tom\\u00e9 & Pr\\u00edncipe dobra1.00",
        "S\\u00e3o Tom\\u00e9 & Pr\\u00edncipe dobras1.00",
        "Saudi Riyal1.00",
        "Saudi riyal1.00",
        "Saudi riyals1.00",
        "Serbian Dinar1.00",
        "Serbian dinar1.00",
        "Serbian dinars1.00",
        "Seychellois Rupee1.00",
        "Seychellois rupee1.00",
        "Seychellois rupees1.00",
        "Sierra Leonean Leone1.00",
        "Sierra Leonean leone1.00",
        "Sierra Leonean leones1.00",
        "Silver1.00",
        "Silver1.00",
        "Singapore Dollar1.00",
        "Singapore dollar1.00",
        "Singapore dollars1.00",
        "Slovak Koruna1.00",
        "Slovak koruna1.00",
        "Slovak korunas1.00",
        "Slovenian Tolar1.00",
        "Slovenian tolar1.00",
        "Slovenian tolars1.00",
        "Solomon Islands Dollar1.00",
        "Solomon Islands dollar1.00",
        "Solomon Islands dollars1.00",
        "Somali Shilling1.00",
        "Somali shilling1.00",
        "Somali shillings1.00",
        "South African Rand (financial)1.00",
        "South African Rand1.00",
        "South African rand (financial)1.00",
        "South African rand1.00",
        "South African rands (financial)1.00",
        "South African rand1.00",
        "South Korean Won1.00",
        "South Korean won1.00",
        "South Korean won1.00",
        "Soviet Rouble1.00",
        "Soviet rouble1.00",
        "Soviet roubles1.00",
        "Spanish Peseta (A account)1.00",
        "Spanish Peseta (convertible account)1.00",
        "Spanish Peseta1.00",
        "Spanish peseta (A account)1.00",
        "Spanish peseta (convertible account)1.00",
        "Spanish peseta1.00",
        "Spanish pesetas (A account)1.00",
        "Spanish pesetas (convertible account)1.00",
        "Spanish pesetas1.00",
        "Special Drawing Rights1.00",
        "Sri Lankan Rupee1.00",
        "Sri Lankan rupee1.00",
        "Sri Lankan rupees1.00",
        "Sudanese Pound1.00",
        "Sudanese pound1.00",
        "Sudanese pounds1.00",
        "Surinamese Dollar1.00",
        "Surinamese dollar1.00",
        "Surinamese dollars1.00",
        "Surinamese Guilder1.00",
        "Surinamese guilder1.00",
        "Surinamese guilders1.00",
        "Swazi Lilangeni1.00",
        "Swazi lilangeni1.00",
        "Swazi emalangeni1.00",
        "Swedish Krona1.00",
        "Swedish krona1.00",
        "Swedish kronor1.00",
        "Swiss Franc1.00",
        "Swiss franc1.00",
        "Swiss francs1.00",
        "Syrian Pound1.00",
        "Syrian pound1.00",
        "Syrian pounds1.00",
        "THB1.00",
        "TJR1.00",
        "TJR1.00",
        "TJS1.00",
        "TJS1.00",
        "TMM1.00",
        "TMM1.00",
        "TND1.00",
        "TND1.00",
        "TOP1.00",
        "TPE1.00",
        "TPE1.00",
        "TRL1.00",
        "TRY1.00",
        "TRY1.00",
        "TTD1.00",
        "TWD1.00",
        "TZS1.00",
        "New Taiwan Dollar1.00",
        "New Taiwan dollar1.00",
        "New Taiwan dollars1.00",
        "Tajikistani Ruble1.00",
        "Tajikistani Somoni1.00",
        "Tajikistani ruble1.00",
        "Tajikistani rubles1.00",
        "Tajikistani somoni1.00",
        "Tajikistani somonis1.00",
        "Tanzanian Shilling1.00",
        "Tanzanian shilling1.00",
        "Tanzanian shillings1.00",
        "Testing Currency Code1.00",
        "Testing Currency Code1.00",
        "Thai Baht1.00",
        "Thai baht1.00",
        "Thai baht1.00",
        "Timorese Escudo1.00",
        "Timorese escudo1.00",
        "Timorese escudos1.00",
        "Tongan Pa\\u02bbanga1.00",
        "Tongan pa\\u02bbanga1.00",
        "Tongan pa\\u02bbanga1.00",
        "Trinidad & Tobago Dollar1.00",
        "Trinidad & Tobago dollar1.00",
        "Trinidad & Tobago dollars1.00",
        "Tunisian Dinar1.00",
        "Tunisian dinar1.00",
        "Tunisian dinars1.00",
        "Turkish Lira1.00",
        "Turkish Lira1.00",
        "Turkish lira1.00",
        "Turkmenistani Manat1.00",
        "Turkmenistani manat1.00",
        "Turkmenistani manat1.00",
        "UAE dirham1.00",
        "UAE dirhams1.00",
        "UAH1.00",
        "UAK1.00",
        "UAK1.00",
        "UGS1.00",
        "UGS1.00",
        "UGX1.00",
        "US Dollar (Next day)1.00",
        "US Dollar (Same day)1.00",
        "US Dollar1.00",
        "US dollar (next day)1.00",
        "US dollar (same day)1.00",
        "US dollar1.00",
        "US dollars (next day)1.00",
        "US dollars (same day)1.00",
        "US dollars1.00",
        "USD1.00",
        "USN1.00",
        "USN1.00",
        "USS1.00",
        "USS1.00",
        "UYI1.00",
        "UYI1.00",
        "UYP1.00",
        "UYP1.00",
        "UYU1.00",
        "UZS1.00",
        "UZS1.00",
        "Ugandan Shilling (1966\\u20131987)1.00",
        "Ugandan Shilling1.00",
        "Ugandan shilling (1966\\u20131987)1.00",
        "Ugandan shilling1.00",
        "Ugandan shillings (1966\\u20131987)1.00",
        "Ugandan shillings1.00",
        "Ukrainian Hryvnia1.00",
        "Ukrainian Karbovanets1.00",
        "Ukrainian hryvnia1.00",
        "Ukrainian hryvnias1.00",
        "Ukrainian karbovanets1.00",
        "Ukrainian karbovantsiv1.00",
        "Colombian Real Value Unit1.00",
        "United Arab Emirates Dirham1.00",
        "Unknown Currency1.00",
        "Uruguayan Peso (1975\\u20131993)1.00",
        "Uruguayan Peso1.00",
        "Uruguayan Peso (Indexed Units)1.00",
        "Uruguayan peso (1975\\u20131993)1.00",
        "Uruguayan peso (indexed units)1.00",
        "Uruguayan peso1.00",
        "Uruguayan pesos (1975\\u20131993)1.00",
        "Uruguayan pesos (indexed units)1.00",
        "Uruguayan pesos1.00",
        "Uzbekistani Som1.00",
        "Uzbekistani som1.00",
        "Uzbekistani som1.00",
        "VEB1.00",
        "VEF1.00",
        "VND1.00",
        "VUV1.00",
        "Vanuatu Vatu1.00",
        "Vanuatu vatu1.00",
        "Vanuatu vatus1.00",
        "Venezuelan Bol\\u00edvar1.00",
        "Venezuelan Bol\\u00edvar (1871\\u20132008)1.00",
        "Venezuelan bol\\u00edvar1.00",
        "Venezuelan bol\\u00edvars1.00",
        "Venezuelan bol\\u00edvar (1871\\u20132008)1.00",
        "Venezuelan bol\\u00edvars (1871\\u20132008)1.00",
        "Vietnamese Dong1.00",
        "Vietnamese dong1.00",
        "Vietnamese dong1.00",
        "WIR Euro1.00",
        "WIR Franc1.00",
        "WIR euro1.00",
        "WIR euros1.00",
        "WIR franc1.00",
        "WIR francs1.00",
        "WST1.00",
        "WST1.00",
        "Samoan Tala1.00",
        "Samoan tala1.00",
        "Samoan tala1.00",
        "XAF1.00",
        "XAF1.00",
        "XAG1.00",
        "XAG1.00",
        "XAU1.00",
        "XAU1.00",
        "XBA1.00",
        "XBA1.00",
        "XBB1.00",
        "XBB1.00",
        "XBC1.00",
        "XBC1.00",
        "XBD1.00",
        "XBD1.00",
        "XCD1.00",
        "XDR1.00",
        "XDR1.00",
        "XEU1.00",
        "XEU1.00",
        "XFO1.00",
        "XFO1.00",
        "XFU1.00",
        "XFU1.00",
        "XOF1.00",
        "XOF1.00",
        "XPD1.00",
        "XPD1.00",
        "XPF1.00",
        "XPT1.00",
        "XPT1.00",
        "XRE1.00",
        "XRE1.00",
        "XTS1.00",
        "XTS1.00",
        "XXX1.00",
        "XXX1.00",
        "YDD1.00",
        "YDD1.00",
        "YER1.00",
        "YUD1.00",
        "YUD1.00",
        "YUM1.00",
        "YUM1.00",
        "YUN1.00",
        "YUN1.00",
        "Yemeni Dinar1.00",
        "Yemeni Rial1.00",
        "Yemeni dinar1.00",
        "Yemeni dinars1.00",
        "Yemeni rial1.00",
        "Yemeni rials1.00",
        "Yugoslavian Convertible Dinar (1990\\u20131992)1.00",
        "Yugoslavian Hard Dinar (1966\\u20131990)1.00",
        "Yugoslavian New Dinar (1994\\u20132002)1.00",
        "Yugoslavian convertible dinar (1990\\u20131992)1.00",
        "Yugoslavian convertible dinars (1990\\u20131992)1.00",
        "Yugoslavian hard dinar (1966\\u20131990)1.00",
        "Yugoslavian hard dinars (1966\\u20131990)1.00",
        "Yugoslavian new dinar (1994\\u20132002)1.00",
        "Yugoslavian new dinars (1994\\u20132002)1.00",
        "ZAL1.00",
        "ZAL1.00",
        "ZAR1.00",
        "ZMK1.00",
        "ZMK1.00",
        "ZRN1.00",
        "ZRN1.00",
        "ZRZ1.00",
        "ZRZ1.00",
        "ZWD1.00",
        "Zairean New Zaire (1993\\u20131998)1.00",
        "Zairean Zaire (1971\\u20131993)1.00",
        "Zairean new zaire (1993\\u20131998)1.00",
        "Zairean new zaires (1993\\u20131998)1.00",
        "Zairean zaire (1971\\u20131993)1.00",
        "Zairean zaires (1971\\u20131993)1.00",
        "Zambian Kwacha1.00",
        "Zambian kwacha1.00",
        "Zambian kwachas1.00",
        "Zimbabwean Dollar (1980\\u20132008)1.00",
        "Zimbabwean dollar (1980\\u20132008)1.00",
        "Zimbabwean dollars (1980\\u20132008)1.00",
        "euro1.00",
        "euros1.00",
        "Turkish lira (1922\\u20132005)1.00",
        "special drawing rights1.00",
        "Colombian real value unit1.00",
        "Colombian real value units1.00",
        "unknown currency1.00",
        "\\u00a31.00",
        "\\u00a51.00",
        "\\u20ab1.00",
        "\\u20aa1.00",
        "\\u20ac1.00",
        "\\u20b91.00",
        //
        // Following has extra text, should be parsed correctly too
        "$1.00 random",
        "USD1.00 random",
        "1.00 US dollar random",
        "1.00 US dollars random",
        "1.00 Afghan Afghani random",
        "1.00 Afghan Afghani random",
        "1.00 Afghan Afghanis (1927\\u20131992) random",
        "1.00 Afghan Afghanis random",
        "1.00 Albanian Lek random",
        "1.00 Albanian lek random",
        "1.00 Albanian lek\\u00eb random",
        "1.00 Algerian Dinar random",
        "1.00 Algerian dinar random",
        "1.00 Algerian dinars random",
        "1.00 Andorran Peseta random",
        "1.00 Andorran peseta random",
        "1.00 Andorran pesetas random",
        "1.00 Angolan Kwanza (1977\\u20131990) random",
        "1.00 Angolan Readjusted Kwanza (1995\\u20131999) random",
        "1.00 Angolan Kwanza random",
        "1.00 Angolan New Kwanza (1990\\u20132000) random",
        "1.00 Angolan kwanza (1977\\u20131991) random",
        "1.00 Angolan readjusted kwanza (1995\\u20131999) random",
        "1.00 Angolan kwanza random",
        "1.00 Angolan kwanzas (1977\\u20131991) random",
        "1.00 Angolan readjusted kwanzas (1995\\u20131999) random",
        "1.00 Angolan kwanzas random",
        "1.00 Angolan new kwanza (1990\\u20132000) random",
        "1.00 Angolan new kwanzas (1990\\u20132000) random",
        "1.00 Argentine Austral random",
        "1.00 Argentine Peso (1983\\u20131985) random",
        "1.00 Argentine Peso random",
        "1.00 Argentine austral random",
        "1.00 Argentine australs random",
        "1.00 Argentine peso (1983\\u20131985) random",
        "1.00 Argentine peso random",
        "1.00 Argentine pesos (1983\\u20131985) random",
        "1.00 Argentine pesos random",
        "1.00 Armenian Dram random",
        "1.00 Armenian dram random",
        "1.00 Armenian drams random",
        "1.00 Aruban Florin random",
        "1.00 Aruban florin random",
        "1.00 Australian Dollar random",
        "1.00 Australian dollar random",
        "1.00 Australian dollars random",
        "1.00 Austrian Schilling random",
        "1.00 Austrian schilling random",
        "1.00 Austrian schillings random",
        "1.00 Azerbaijani Manat (1993\\u20132006) random",
        "1.00 Azerbaijani Manat random",
        "1.00 Azerbaijani manat (1993\\u20132006) random",
        "1.00 Azerbaijani manat random",
        "1.00 Azerbaijani manats (1993\\u20132006) random",
        "1.00 Azerbaijani manats random",
        "1.00 Bahamian Dollar random",
        "1.00 Bahamian dollar random",
        "1.00 Bahamian dollars random",
        "1.00 Bahraini Dinar random",
        "1.00 Bahraini dinar random",
        "1.00 Bahraini dinars random",
        "1.00 Bangladeshi Taka random",
        "1.00 Bangladeshi taka random",
        "1.00 Bangladeshi takas random",
        "1.00 Barbadian Dollar random",
        "1.00 Barbadian dollar random",
        "1.00 Barbadian dollars random",
        "1.00 Belarusian Ruble (1994\\u20131999) random",
        "1.00 Belarusian Ruble random",
        "1.00 Belarusian ruble (1994\\u20131999) random",
        "1.00 Belarusian rubles (1994\\u20131999) random",
        "1.00 Belarusian ruble random",
        "1.00 Belarusian rubles random",
        "1.00 Belgian Franc (convertible) random",
        "1.00 Belgian Franc (financial) random",
        "1.00 Belgian Franc random",
        "1.00 Belgian franc (convertible) random",
        "1.00 Belgian franc (financial) random",
        "1.00 Belgian franc random",
        "1.00 Belgian francs (convertible) random",
        "1.00 Belgian francs (financial) random",
        "1.00 Belgian francs random",
        "1.00 Belize Dollar random",
        "1.00 Belize dollar random",
        "1.00 Belize dollars random",
        "1.00 Bermudan Dollar random",
        "1.00 Bermudan dollar random",
        "1.00 Bermudan dollars random",
        "1.00 Bhutanese Ngultrum random",
        "1.00 Bhutanese ngultrum random",
        "1.00 Bhutanese ngultrums random",
        "1.00 Bolivian Mvdol random",
        "1.00 Bolivian Peso random",
        "1.00 Bolivian mvdol random",
        "1.00 Bolivian mvdols random",
        "1.00 Bolivian peso random",
        "1.00 Bolivian pesos random",
        "1.00 Bolivian Boliviano random",
        "1.00 Bolivian Boliviano random",
        "1.00 Bolivian Bolivianos random",
        "1.00 Bosnia-Herzegovina Convertible Mark random",
        "1.00 Bosnia-Herzegovina Dinar (1992\\u20131994) random",
        "1.00 Bosnia-Herzegovina convertible mark random",
        "1.00 Bosnia-Herzegovina convertible marks random",
        "1.00 Bosnia-Herzegovina dinar (1992\\u20131994) random",
        "1.00 Bosnia-Herzegovina dinars (1992\\u20131994) random",
        "1.00 Botswanan Pula random",
        "1.00 Botswanan pula random",
        "1.00 Botswanan pulas random",
        "1.00 Brazilian New Cruzado (1989\\u20131990) random",
        "1.00 Brazilian Cruzado (1986\\u20131989) random",
        "1.00 Brazilian Cruzeiro (1990\\u20131993) random",
        "1.00 Brazilian New Cruzeiro (1967\\u20131986) random",
        "1.00 Brazilian Cruzeiro (1993\\u20131994) random",
        "1.00 Brazilian Real random",
        "1.00 Brazilian new cruzado (1989\\u20131990) random",
        "1.00 Brazilian new cruzados (1989\\u20131990) random",
        "1.00 Brazilian cruzado (1986\\u20131989) random",
        "1.00 Brazilian cruzados (1986\\u20131989) random",
        "1.00 Brazilian cruzeiro (1990\\u20131993) random",
        "1.00 Brazilian new cruzeiro (1967\\u20131986) random",
        "1.00 Brazilian cruzeiro (1993\\u20131994) random",
        "1.00 Brazilian cruzeiros (1990\\u20131993) random",
        "1.00 Brazilian new cruzeiros (1967\\u20131986) random",
        "1.00 Brazilian cruzeiros (1993\\u20131994) random",
        "1.00 Brazilian real random",
        "1.00 Brazilian reals random",
        "1.00 British Pound random",
        "1.00 British pound random",
        "1.00 British pounds random",
        "1.00 Brunei Dollar random",
        "1.00 Brunei dollar random",
        "1.00 Brunei dollars random",
        "1.00 Bulgarian Hard Lev random",
        "1.00 Bulgarian Lev random",
        "1.00 Bulgarian Leva random",
        "1.00 Bulgarian hard lev random",
        "1.00 Bulgarian hard leva random",
        "1.00 Bulgarian lev random",
        "1.00 Burmese Kyat random",
        "1.00 Burmese kyat random",
        "1.00 Burmese kyats random",
        "1.00 Burundian Franc random",
        "1.00 Burundian franc random",
        "1.00 Burundian francs random",
        "1.00 Cambodian Riel random",
        "1.00 Cambodian riel random",
        "1.00 Cambodian riels random",
        "1.00 Canadian Dollar random",
        "1.00 Canadian dollar random",
        "1.00 Canadian dollars random",
        "1.00 Cape Verdean Escudo random",
        "1.00 Cape Verdean escudo random",
        "1.00 Cape Verdean escudos random",
        "1.00 Cayman Islands Dollar random",
        "1.00 Cayman Islands dollar random",
        "1.00 Cayman Islands dollars random",
        "1.00 Chilean Peso random",
        "1.00 Chilean Unit of Account (UF) random",
        "1.00 Chilean peso random",
        "1.00 Chilean pesos random",
        "1.00 Chilean unit of account (UF) random",
        "1.00 Chilean units of account (UF) random",
        "1.00 Chinese Yuan random",
        "1.00 Chinese yuan random",
        "1.00 Colombian Peso random",
        "1.00 Colombian peso random",
        "1.00 Colombian pesos random",
        "1.00 Comorian Franc random",
        "1.00 Comorian franc random",
        "1.00 Comorian francs random",
        "1.00 Congolese Franc Congolais random",
        "1.00 Congolese franc Congolais random",
        "1.00 Congolese francs Congolais random",
        "1.00 Costa Rican Col\\u00f3n random",
        "1.00 Costa Rican col\\u00f3n random",
        "1.00 Costa Rican col\\u00f3ns random",
        "1.00 Croatian Dinar random",
        "1.00 Croatian Kuna random",
        "1.00 Croatian dinar random",
        "1.00 Croatian dinars random",
        "1.00 Croatian kuna random",
        "1.00 Croatian kunas random",
        "1.00 Cuban Peso random",
        "1.00 Cuban peso random",
        "1.00 Cuban pesos random",
        "1.00 Cypriot Pound random",
        "1.00 Cypriot pound random",
        "1.00 Cypriot pounds random",
        "1.00 Czech Koruna random",
        "1.00 Czech koruna random",
        "1.00 Czech korunas random",
        "1.00 Czechoslovak Hard Koruna random",
        "1.00 Czechoslovak hard koruna random",
        "1.00 Czechoslovak hard korunas random",
        "1.00 Danish Krone random",
        "1.00 Danish krone random",
        "1.00 Danish kroner random",
        "1.00 German Mark random",
        "1.00 German mark random",
        "1.00 German marks random",
        "1.00 Djiboutian Franc random",
        "1.00 Djiboutian franc random",
        "1.00 Djiboutian francs random",
        "1.00 Dominican Peso random",
        "1.00 Dominican peso random",
        "1.00 Dominican pesos random",
        "1.00 East Caribbean Dollar random",
        "1.00 East Caribbean dollar random",
        "1.00 East Caribbean dollars random",
        "1.00 East German Mark random",
        "1.00 East German mark random",
        "1.00 East German marks random",
        "1.00 Ecuadorian Sucre random",
        "1.00 Ecuadorian Unit of Constant Value random",
        "1.00 Ecuadorian sucre random",
        "1.00 Ecuadorian sucres random",
        "1.00 Ecuadorian unit of constant value random",
        "1.00 Ecuadorian units of constant value random",
        "1.00 Egyptian Pound random",
        "1.00 Egyptian pound random",
        "1.00 Egyptian pounds random",
        "1.00 Salvadoran Col\\u00f3n random",
        "1.00 Salvadoran col\\u00f3n random",
        "1.00 Salvadoran colones random",
        "1.00 Equatorial Guinean Ekwele random",
        "1.00 Equatorial Guinean ekwele random",
        "1.00 Eritrean Nakfa random",
        "1.00 Eritrean nakfa random",
        "1.00 Eritrean nakfas random",
        "1.00 Estonian Kroon random",
        "1.00 Estonian kroon random",
        "1.00 Estonian kroons random",
        "1.00 Ethiopian Birr random",
        "1.00 Ethiopian birr random",
        "1.00 Ethiopian birrs random",
        "1.00 European Composite Unit random",
        "1.00 European Currency Unit random",
        "1.00 European Monetary Unit random",
        "1.00 European Unit of Account (XBC) random",
        "1.00 European Unit of Account (XBD) random",
        "1.00 European composite unit random",
        "1.00 European composite units random",
        "1.00 European currency unit random",
        "1.00 European currency units random",
        "1.00 European monetary unit random",
        "1.00 European monetary units random",
        "1.00 European unit of account (XBC) random",
        "1.00 European unit of account (XBD) random",
        "1.00 European units of account (XBC) random",
        "1.00 European units of account (XBD) random",
        "1.00 Falkland Islands Pound random",
        "1.00 Falkland Islands pound random",
        "1.00 Falkland Islands pounds random",
        "1.00 Fijian Dollar random",
        "1.00 Fijian dollar random",
        "1.00 Fijian dollars random",
        "1.00 Finnish Markka random",
        "1.00 Finnish markka random",
        "1.00 Finnish markkas random",
        "1.00 French Franc random",
        "1.00 French Gold Franc random",
        "1.00 French UIC-Franc random",
        "1.00 French UIC-franc random",
        "1.00 French UIC-francs random",
        "1.00 French franc random",
        "1.00 French francs random",
        "1.00 French gold franc random",
        "1.00 French gold francs random",
        "1.00 Gambian Dalasi random",
        "1.00 Gambian dalasi random",
        "1.00 Gambian dalasis random",
        "1.00 Georgian Kupon Larit random",
        "1.00 Georgian Lari random",
        "1.00 Georgian kupon larit random",
        "1.00 Georgian kupon larits random",
        "1.00 Georgian lari random",
        "1.00 Georgian laris random",
        "1.00 Ghanaian Cedi (1979\\u20132007) random",
        "1.00 Ghanaian Cedi random",
        "1.00 Ghanaian cedi (1979\\u20132007) random",
        "1.00 Ghanaian cedi random",
        "1.00 Ghanaian cedis (1979\\u20132007) random",
        "1.00 Ghanaian cedis random",
        "1.00 Gibraltar Pound random",
        "1.00 Gibraltar pound random",
        "1.00 Gibraltar pounds random",
        "1.00 Gold random",
        "1.00 Gold random",
        "1.00 Greek Drachma random",
        "1.00 Greek drachma random",
        "1.00 Greek drachmas random",
        "1.00 Guatemalan Quetzal random",
        "1.00 Guatemalan quetzal random",
        "1.00 Guatemalan quetzals random",
        "1.00 Guinean Franc random",
        "1.00 Guinean Syli random",
        "1.00 Guinean franc random",
        "1.00 Guinean francs random",
        "1.00 Guinean syli random",
        "1.00 Guinean sylis random",
        "1.00 Guinea-Bissau Peso random",
        "1.00 Guinea-Bissau peso random",
        "1.00 Guinea-Bissau pesos random",
        "1.00 Guyanaese Dollar random",
        "1.00 Guyanaese dollar random",
        "1.00 Guyanaese dollars random",
        "1.00 Haitian Gourde random",
        "1.00 Haitian gourde random",
        "1.00 Haitian gourdes random",
        "1.00 Honduran Lempira random",
        "1.00 Honduran lempira random",
        "1.00 Honduran lempiras random",
        "1.00 Hong Kong Dollar random",
        "1.00 Hong Kong dollar random",
        "1.00 Hong Kong dollars random",
        "1.00 Hungarian Forint random",
        "1.00 Hungarian forint random",
        "1.00 Hungarian forints random",
        "1.00 Icelandic Kr\\u00f3na random",
        "1.00 Icelandic kr\\u00f3na random",
        "1.00 Icelandic kr\\u00f3nur random",
        "1.00 Indian Rupee random",
        "1.00 Indian rupee random",
        "1.00 Indian rupees random",
        "1.00 Indonesian Rupiah random",
        "1.00 Indonesian rupiah random",
        "1.00 Indonesian rupiahs random",
        "1.00 Iranian Rial random",
        "1.00 Iranian rial random",
        "1.00 Iranian rials random",
        "1.00 Iraqi Dinar random",
        "1.00 Iraqi dinar random",
        "1.00 Iraqi dinars random",
        "1.00 Irish Pound random",
        "1.00 Irish pound random",
        "1.00 Irish pounds random",
        "1.00 Israeli Pound random",
        "1.00 Israeli new shekel random",
        "1.00 Israeli pound random",
        "1.00 Israeli pounds random",
        "1.00 Italian Lira random",
        "1.00 Italian lira random",
        "1.00 Italian liras random",
        "1.00 Jamaican Dollar random",
        "1.00 Jamaican dollar random",
        "1.00 Jamaican dollars random",
        "1.00 Japanese Yen random",
        "1.00 Japanese yen random",
        "1.00 Jordanian Dinar random",
        "1.00 Jordanian dinar random",
        "1.00 Jordanian dinars random",
        "1.00 Kazakhstani Tenge random",
        "1.00 Kazakhstani tenge random",
        "1.00 Kazakhstani tenges random",
        "1.00 Kenyan Shilling random",
        "1.00 Kenyan shilling random",
        "1.00 Kenyan shillings random",
        "1.00 Kuwaiti Dinar random",
        "1.00 Kuwaiti dinar random",
        "1.00 Kuwaiti dinars random",
        "1.00 Kyrgystani Som random",
        "1.00 Kyrgystani som random",
        "1.00 Kyrgystani soms random",
        "1.00 Laotian Kip random",
        "1.00 Laotian kip random",
        "1.00 Laotian kips random",
        "1.00 Latvian Lats random",
        "1.00 Latvian Ruble random",
        "1.00 Latvian lats random",
        "1.00 Latvian lati random",
        "1.00 Latvian ruble random",
        "1.00 Latvian rubles random",
        "1.00 Lebanese Pound random",
        "1.00 Lebanese pound random",
        "1.00 Lebanese pounds random",
        "1.00 Lesotho Loti random",
        "1.00 Lesotho loti random",
        "1.00 Lesotho lotis random",
        "1.00 Liberian Dollar random",
        "1.00 Liberian dollar random",
        "1.00 Liberian dollars random",
        "1.00 Libyan Dinar random",
        "1.00 Libyan dinar random",
        "1.00 Libyan dinars random",
        "1.00 Lithuanian Litas random",
        "1.00 Lithuanian Talonas random",
        "1.00 Lithuanian litas random",
        "1.00 Lithuanian litai random",
        "1.00 Lithuanian talonas random",
        "1.00 Lithuanian talonases random",
        "1.00 Luxembourgian Convertible Franc random",
        "1.00 Luxembourg Financial Franc random",
        "1.00 Luxembourgian Franc random",
        "1.00 Luxembourgian convertible franc random",
        "1.00 Luxembourgian convertible francs random",
        "1.00 Luxembourg financial franc random",
        "1.00 Luxembourg financial francs random",
        "1.00 Luxembourgian franc random",
        "1.00 Luxembourgian francs random",
        "1.00 Macanese Pataca random",
        "1.00 Macanese pataca random",
        "1.00 Macanese patacas random",
        "1.00 Macedonian Denar random",
        "1.00 Macedonian denar random",
        "1.00 Macedonian denari random",
        "1.00 Malagasy Ariaries random",
        "1.00 Malagasy Ariary random",
        "1.00 Malagasy Ariary random",
        "1.00 Malagasy Franc random",
        "1.00 Malagasy franc random",
        "1.00 Malagasy francs random",
        "1.00 Malawian Kwacha random",
        "1.00 Malawian Kwacha random",
        "1.00 Malawian Kwachas random",
        "1.00 Malaysian Ringgit random",
        "1.00 Malaysian ringgit random",
        "1.00 Malaysian ringgits random",
        "1.00 Maldivian Rufiyaa random",
        "1.00 Maldivian rufiyaa random",
        "1.00 Maldivian rufiyaas random",
        "1.00 Malian Franc random",
        "1.00 Malian franc random",
        "1.00 Malian francs random",
        "1.00 Maltese Lira random",
        "1.00 Maltese Pound random",
        "1.00 Maltese lira random",
        "1.00 Maltese liras random",
        "1.00 Maltese pound random",
        "1.00 Maltese pounds random",
        "1.00 Mauritanian Ouguiya random",
        "1.00 Mauritanian ouguiya random",
        "1.00 Mauritanian ouguiyas random",
        "1.00 Mauritian Rupee random",
        "1.00 Mauritian rupee random",
        "1.00 Mauritian rupees random",
        "1.00 Mexican Peso random",
        "1.00 Mexican Silver Peso (1861\\u20131992) random",
        "1.00 Mexican Investment Unit random",
        "1.00 Mexican peso random",
        "1.00 Mexican pesos random",
        "1.00 Mexican silver peso (1861\\u20131992) random",
        "1.00 Mexican silver pesos (1861\\u20131992) random",
        "1.00 Mexican investment unit random",
        "1.00 Mexican investment units random",
        "1.00 Moldovan Leu random",
        "1.00 Moldovan leu random",
        "1.00 Moldovan lei random",
        "1.00 Mongolian Tugrik random",
        "1.00 Mongolian tugrik random",
        "1.00 Mongolian tugriks random",
        "1.00 Moroccan Dirham random",
        "1.00 Moroccan Franc random",
        "1.00 Moroccan dirham random",
        "1.00 Moroccan dirhams random",
        "1.00 Moroccan franc random",
        "1.00 Moroccan francs random",
        "1.00 Mozambican Escudo random",
        "1.00 Mozambican Metical random",
        "1.00 Mozambican escudo random",
        "1.00 Mozambican escudos random",
        "1.00 Mozambican metical random",
        "1.00 Mozambican meticals random",
        "1.00 Myanmar Kyat random",
        "1.00 Myanmar kyat random",
        "1.00 Myanmar kyats random",
        "1.00 Namibian Dollar random",
        "1.00 Namibian dollar random",
        "1.00 Namibian dollars random",
        "1.00 Nepalese Rupee random",
        "1.00 Nepalese rupee random",
        "1.00 Nepalese rupees random",
        "1.00 Netherlands Antillean Guilder random",
        "1.00 Netherlands Antillean guilder random",
        "1.00 Netherlands Antillean guilders random",
        "1.00 Dutch Guilder random",
        "1.00 Dutch guilder random",
        "1.00 Dutch guilders random",
        "1.00 Israeli New Shekel random",
        "1.00 Israeli new shekels random",
        "1.00 New Zealand Dollar random",
        "1.00 New Zealand dollar random",
        "1.00 New Zealand dollars random",
        "1.00 Nicaraguan C\\u00f3rdoba random",
        "1.00 Nicaraguan C\\u00f3rdoba (1988\\u20131991) random",
        "1.00 Nicaraguan c\\u00f3rdoba random",
        "1.00 Nicaraguan c\\u00f3rdoba random",
        "1.00 Nicaraguan c\\u00f3rdoba (1988\\u20131991) random",
        "1.00 Nicaraguan c\\u00f3rdobas (1988\\u20131991) random",
        "1.00 Nigerian Naira random",
        "1.00 Nigerian naira random",
        "1.00 Nigerian nairas random",
        "1.00 North Korean Won random",
        "1.00 North Korean won random",
        "1.00 North Korean won random",
        "1.00 Norwegian Krone random",
        "1.00 Norwegian krone random",
        "1.00 Norwegian kroner random",
        "1.00 Mozambican Metical (1980\\u20132006) random",
        "1.00 Mozambican metical (1980\\u20132006) random",
        "1.00 Mozambican meticals (1980\\u20132006) random",
        "1.00 Romanian Lei (1952\\u20132006) random",
        "1.00 Romanian Leu (1952\\u20132006) random",
        "1.00 Romanian leu (1952\\u20132006) random",
        "1.00 Serbian Dinar (2002\\u20132006) random",
        "1.00 Serbian dinar (2002\\u20132006) random",
        "1.00 Serbian dinars (2002\\u20132006) random",
        "1.00 Sudanese Dinar (1992\\u20132007) random",
        "1.00 Sudanese Pound (1957\\u20131998) random",
        "1.00 Sudanese dinar (1992\\u20132007) random",
        "1.00 Sudanese dinars (1992\\u20132007) random",
        "1.00 Sudanese pound (1957\\u20131998) random",
        "1.00 Sudanese pounds (1957\\u20131998) random",
        "1.00 Turkish Lira (1922\\u20132005) random",
        "1.00 Turkish Lira (1922\\u20132005) random",
        "1.00 Omani Rial random",
        "1.00 Omani rial random",
        "1.00 Omani rials random",
        "1.00 Pakistani Rupee random",
        "1.00 Pakistani rupee random",
        "1.00 Pakistani rupees random",
        "1.00 Palladium random",
        "1.00 Palladium random",
        "1.00 Panamanian Balboa random",
        "1.00 Panamanian balboa random",
        "1.00 Panamanian balboas random",
        "1.00 Papua New Guinean Kina random",
        "1.00 Papua New Guinean kina random",
        "1.00 Papua New Guinean kina random",
        "1.00 Paraguayan Guarani random",
        "1.00 Paraguayan guarani random",
        "1.00 Paraguayan guaranis random",
        "1.00 Peruvian Inti random",
        "1.00 Peruvian Sol random",
        "1.00 Peruvian Sol (1863\\u20131965) random",
        "1.00 Peruvian inti random",
        "1.00 Peruvian intis random",
        "1.00 Peruvian sol random",
        "1.00 Peruvian soles random",
        "1.00 Peruvian sol (1863\\u20131965) random",
        "1.00 Peruvian soles (1863\\u20131965) random",
        "1.00 Philippine Piso random",
        "1.00 Philippine piso random",
        "1.00 Philippine pisos random",
        "1.00 Platinum random",
        "1.00 Platinum random",
        "1.00 Polish Zloty (1950\\u20131995) random",
        "1.00 Polish Zloty random",
        "1.00 Polish zlotys random",
        "1.00 Polish zloty (PLZ) random",
        "1.00 Polish zloty random",
        "1.00 Polish zlotys (PLZ) random",
        "1.00 Portuguese Escudo random",
        "1.00 Portuguese Guinea Escudo random",
        "1.00 Portuguese Guinea escudo random",
        "1.00 Portuguese Guinea escudos random",
        "1.00 Portuguese escudo random",
        "1.00 Portuguese escudos random",
        "1.00 Qatari Rial random",
        "1.00 Qatari rial random",
        "1.00 Qatari rials random",
        "1.00 RINET Funds random",
        "1.00 RINET Funds random",
        "1.00 Rhodesian Dollar random",
        "1.00 Rhodesian dollar random",
        "1.00 Rhodesian dollars random",
        "1.00 Romanian Leu random",
        "1.00 Romanian lei random",
        "1.00 Romanian leu random",
        "1.00 Russian Ruble (1991\\u20131998) random",
        "1.00 Russian Ruble random",
        "1.00 Russian ruble (1991\\u20131998) random",
        "1.00 Russian ruble random",
        "1.00 Russian rubles (1991\\u20131998) random",
        "1.00 Russian rubles random",
        "1.00 Rwandan Franc random",
        "1.00 Rwandan franc random",
        "1.00 Rwandan francs random",
        "1.00 St. Helena Pound random",
        "1.00 St. Helena pound random",
        "1.00 St. Helena pounds random",
        "1.00 S\\u00e3o Tom\\u00e9 & Pr\\u00edncipe Dobra random",
        "1.00 S\\u00e3o Tom\\u00e9 & Pr\\u00edncipe dobra random",
        "1.00 S\\u00e3o Tom\\u00e9 & Pr\\u00edncipe dobras random",
        "1.00 Saudi Riyal random",
        "1.00 Saudi riyal random",
        "1.00 Saudi riyals random",
        "1.00 Serbian Dinar random",
        "1.00 Serbian dinar random",
        "1.00 Serbian dinars random",
        "1.00 Seychellois Rupee random",
        "1.00 Seychellois rupee random",
        "1.00 Seychellois rupees random",
        "1.00 Sierra Leonean Leone random",
        "1.00 Sierra Leonean leone random",
        "1.00 Sierra Leonean leones random",
        "1.00 Singapore Dollar random",
        "1.00 Singapore dollar random",
        "1.00 Singapore dollars random",
        "1.00 Slovak Koruna random",
        "1.00 Slovak koruna random",
        "1.00 Slovak korunas random",
        "1.00 Slovenian Tolar random",
        "1.00 Slovenian tolar random",
        "1.00 Slovenian tolars random",
        "1.00 Solomon Islands Dollar random",
        "1.00 Solomon Islands dollar random",
        "1.00 Solomon Islands dollars random",
        "1.00 Somali Shilling random",
        "1.00 Somali shilling random",
        "1.00 Somali shillings random",
        "1.00 South African Rand (financial) random",
        "1.00 South African Rand random",
        "1.00 South African rand (financial) random",
        "1.00 South African rand random",
        "1.00 South African rands (financial) random",
        "1.00 South African rand random",
        "1.00 South Korean Won random",
        "1.00 South Korean won random",
        "1.00 South Korean won random",
        "1.00 Soviet Rouble random",
        "1.00 Soviet rouble random",
        "1.00 Soviet roubles random",
        "1.00 Spanish Peseta (A account) random",
        "1.00 Spanish Peseta (convertible account) random",
        "1.00 Spanish Peseta random",
        "1.00 Spanish peseta (A account) random",
        "1.00 Spanish peseta (convertible account) random",
        "1.00 Spanish peseta random",
        "1.00 Spanish pesetas (A account) random",
        "1.00 Spanish pesetas (convertible account) random",
        "1.00 Spanish pesetas random",
        "1.00 Special Drawing Rights random",
        "1.00 Sri Lankan Rupee random",
        "1.00 Sri Lankan rupee random",
        "1.00 Sri Lankan rupees random",
        "1.00 Sudanese Pound random",
        "1.00 Sudanese pound random",
        "1.00 Sudanese pounds random",
        "1.00 Surinamese Dollar random",
        "1.00 Surinamese dollar random",
        "1.00 Surinamese dollars random",
        "1.00 Surinamese Guilder random",
        "1.00 Surinamese guilder random",
        "1.00 Surinamese guilders random",
        "1.00 Swazi Lilangeni random",
        "1.00 Swazi lilangeni random",
        "1.00 Swazi emalangeni random",
        "1.00 Swedish Krona random",
        "1.00 Swedish krona random",
        "1.00 Swedish kronor random",
        "1.00 Swiss Franc random",
        "1.00 Swiss franc random",
        "1.00 Swiss francs random",
        "1.00 Syrian Pound random",
        "1.00 Syrian pound random",
        "1.00 Syrian pounds random",
        "1.00 New Taiwan Dollar random",
        "1.00 New Taiwan dollar random",
        "1.00 New Taiwan dollars random",
        "1.00 Tajikistani Ruble random",
        "1.00 Tajikistani Somoni random",
        "1.00 Tajikistani ruble random",
        "1.00 Tajikistani rubles random",
        "1.00 Tajikistani somoni random",
        "1.00 Tajikistani somonis random",
        "1.00 Tanzanian Shilling random",
        "1.00 Tanzanian shilling random",
        "1.00 Tanzanian shillings random",
        "1.00 Testing Currency Code random",
        "1.00 Testing Currency Code random",
        "1.00 Thai Baht random",
        "1.00 Thai baht random",
        "1.00 Thai baht random",
        "1.00 Timorese Escudo random",
        "1.00 Timorese escudo random",
        "1.00 Timorese escudos random",
        "1.00 Trinidad & Tobago Dollar random",
        "1.00 Trinidad & Tobago dollar random",
        "1.00 Trinidad & Tobago dollars random",
        "1.00 Tunisian Dinar random",
        "1.00 Tunisian dinar random",
        "1.00 Tunisian dinars random",
        "1.00 Turkish Lira random",
        "1.00 Turkish Lira random",
        "1.00 Turkish lira random",
        "1.00 Turkmenistani Manat random",
        "1.00 Turkmenistani manat random",
        "1.00 Turkmenistani manat random",
        "1.00 US Dollar (Next day) random",
        "1.00 US Dollar (Same day) random",
        "1.00 US Dollar random",
        "1.00 US dollar (next day) random",
        "1.00 US dollar (same day) random",
        "1.00 US dollar random",
        "1.00 US dollars (next day) random",
        "1.00 US dollars (same day) random",
        "1.00 US dollars random",
        "1.00 Ugandan Shilling (1966\\u20131987) random",
        "1.00 Ugandan Shilling random",
        "1.00 Ugandan shilling (1966\\u20131987) random",
        "1.00 Ugandan shilling random",
        "1.00 Ugandan shillings (1966\\u20131987) random",
        "1.00 Ugandan shillings random",
        "1.00 Ukrainian Hryvnia random",
        "1.00 Ukrainian Karbovanets random",
        "1.00 Ukrainian hryvnia random",
        "1.00 Ukrainian hryvnias random",
        "1.00 Ukrainian karbovanets random",
        "1.00 Ukrainian karbovantsiv random",
        "1.00 Colombian Real Value Unit random",
        "1.00 United Arab Emirates Dirham random",
        "1.00 Unknown Currency random",
        "1.00 Uruguayan Peso (1975\\u20131993) random",
        "1.00 Uruguayan Peso random",
        "1.00 Uruguayan Peso (Indexed Units) random",
        "1.00 Uruguayan peso (1975\\u20131993) random",
        "1.00 Uruguayan peso (indexed units) random",
        "1.00 Uruguayan peso random",
        "1.00 Uruguayan pesos (1975\\u20131993) random",
        "1.00 Uruguayan pesos (indexed units) random",
        "1.00 Uzbekistani Som random",
        "1.00 Uzbekistani som random",
        "1.00 Uzbekistani som random",
        "1.00 Vanuatu Vatu random",
        "1.00 Vanuatu vatu random",
        "1.00 Vanuatu vatus random",
        "1.00 Venezuelan Bol\\u00edvar random",
        "1.00 Venezuelan Bol\\u00edvar (1871\\u20132008) random",
        "1.00 Venezuelan bol\\u00edvar random",
        "1.00 Venezuelan bol\\u00edvars random",
        "1.00 Venezuelan bol\\u00edvar (1871\\u20132008) random",
        "1.00 Venezuelan bol\\u00edvars (1871\\u20132008) random",
        "1.00 Vietnamese Dong random",
        "1.00 Vietnamese dong random",
        "1.00 Vietnamese dong random",
        "1.00 WIR Euro random",
        "1.00 WIR Franc random",
        "1.00 WIR euro random",
        "1.00 WIR euros random",
        "1.00 WIR franc random",
        "1.00 WIR francs random",
        "1.00 Samoan Tala random",
        "1.00 Samoan tala random",
        "1.00 Samoan tala random",
        "1.00 Yemeni Dinar random",
        "1.00 Yemeni Rial random",
        "1.00 Yemeni dinar random",
        "1.00 Yemeni dinars random",
        "1.00 Yemeni rial random",
        "1.00 Yemeni rials random",
        "1.00 Yugoslavian Convertible Dinar (1990\\u20131992) random",
        "1.00 Yugoslavian Hard Dinar (1966\\u20131990) random",
        "1.00 Yugoslavian New Dinar (1994\\u20132002) random",
        "1.00 Yugoslavian convertible dinar (1990\\u20131992) random",
        "1.00 Yugoslavian convertible dinars (1990\\u20131992) random",
        "1.00 Yugoslavian hard dinar (1966\\u20131990) random",
        "1.00 Yugoslavian hard dinars (1966\\u20131990) random",
        "1.00 Yugoslavian new dinar (1994\\u20132002) random",
        "1.00 Yugoslavian new dinars (1994\\u20132002) random",
        "1.00 Zairean New Zaire (1993\\u20131998) random",
        "1.00 Zairean Zaire (1971\\u20131993) random",
        "1.00 Zairean new zaire (1993\\u20131998) random",
        "1.00 Zairean new zaires (1993\\u20131998) random",
        "1.00 Zairean zaire (1971\\u20131993) random",
        "1.00 Zairean zaires (1971\\u20131993) random",
        "1.00 Zambian Kwacha random",
        "1.00 Zambian kwacha random",
        "1.00 Zambian kwachas random",
        "1.00 Zimbabwean Dollar (1980\\u20132008) random",
        "1.00 Zimbabwean dollar (1980\\u20132008) random",
        "1.00 Zimbabwean dollars (1980\\u20132008) random",
        "1.00 euro random",
        "1.00 euros random",
        "1.00 Turkish lira (1922\\u20132005) random",
        "1.00 special drawing rights random",
        "1.00 Colombian real value unit random",
        "1.00 Colombian real value units random",
        "1.00 unknown currency random",
    };

    const char* WRONG_DATA[] = {
        // Following are missing one last char in the currency name
        "1.00 Nicaraguan Cordob",
        "1.00 Namibian Dolla",
        "1.00 Namibian dolla",
        "1.00 Nepalese Rupe",
        "1.00 Nepalese rupe",
        "1.00 Netherlands Antillean Guilde",
        "1.00 Netherlands Antillean guilde",
        "1.00 Dutch Guilde",
        "1.00 Dutch guilde",
        "1.00 Israeli New Sheqe",
        "1.00 New Zealand Dolla",
        "1.00 New Zealand dolla",
        "1.00 Nicaraguan cordob",
        "1.00 Nigerian Nair",
        "1.00 Nigerian nair",
        "1.00 North Korean Wo",
        "1.00 North Korean wo",
        "1.00 Norwegian Kron",
        "1.00 Norwegian kron",
        "1.00 US dolla",
        "1.00",
        "A1.00",
        "AD1.00",
        "AE1.00",
        "AF1.00",
        "AL1.00",
        "AM1.00",
        "AN1.00",
        "AO1.00",
        "AR1.00",
        "AT1.00",
        "AU1.00",
        "AW1.00",
        "AZ1.00",
        "Afghan Afghan1.00",
        "Afghan Afghani (1927\\u201320021.00",
        "Afl1.00",
        "Albanian Le1.00",
        "Algerian Dina1.00",
        "Andorran Peset1.00",
        "Angolan Kwanz1.00",
        "Angolan Kwanza (1977\\u201319901.00",
        "Angolan Readjusted Kwanza (1995\\u201319991.00",
        "Angolan New Kwanza (1990\\u201320001.00",
        "Argentine Austra1.00",
        "Argentine Pes1.00",
        "Argentine Peso (1983\\u201319851.00",
        "Armenian Dra1.00",
        "Aruban Flori1.00",
        "Australian Dolla1.00",
        "Austrian Schillin1.00",
        "Azerbaijani Mana1.00",
        "Azerbaijani Manat (1993\\u201320061.00",
        "B1.00",
        "BA1.00",
        "BB1.00",
        "BE1.00",
        "BG1.00",
        "BH1.00",
        "BI1.00",
        "BM1.00",
        "BN1.00",
        "BO1.00",
        "BR1.00",
        "BS1.00",
        "BT1.00",
        "BU1.00",
        "BW1.00",
        "BY1.00",
        "BZ1.00",
        "Bahamian Dolla1.00",
        "Bahraini Dina1.00",
        "Bangladeshi Tak1.00",
        "Barbadian Dolla1.00",
        "Bds1.00",
        "Belarusian Ruble (1994\\u201319991.00",
        "Belarusian Rubl1.00",
        "Belgian Fran1.00",
        "Belgian Franc (convertible1.00",
        "Belgian Franc (financial1.00",
        "Belize Dolla1.00",
        "Bermudan Dolla1.00",
        "Bhutanese Ngultru1.00",
        "Bolivian Mvdo1.00",
        "Bolivian Pes1.00",
        "Bolivian Bolivian1.00",
        "Bosnia-Herzegovina Convertible Mar1.00",
        "Bosnia-Herzegovina Dina1.00",
        "Botswanan Pul1.00",
        "Brazilian Cruzad1.00",
        "Brazilian Cruzado Nov1.00",
        "Brazilian Cruzeir1.00",
        "Brazilian Cruzeiro (1990\\u201319931.00",
        "Brazilian New Cruzeiro (1967\\u201319861.00",
        "Brazilian Rea1.00",
        "British Pound Sterlin1.00",
        "Brunei Dolla1.00",
        "Bulgarian Hard Le1.00",
        "Bulgarian Le1.00",
        "Burmese Kya1.00",
        "Burundian Fran1.00",
        "C1.00",
        "CA1.00",
        "CD1.00",
        "CFP Fran1.00",
        "CFP1.00",
        "CH1.00",
        "CL1.00",
        "CN1.00",
        "CO1.00",
        "CS1.00",
        "CU1.00",
        "CV1.00",
        "CY1.00",
        "CZ1.00",
        "Cambodian Rie1.00",
        "Canadian Dolla1.00",
        "Cape Verdean Escud1.00",
        "Cayman Islands Dolla1.00",
        "Chilean Pes1.00",
        "Chilean Unit of Accoun1.00",
        "Chinese Yua1.00",
        "Colombian Pes1.00",
        "Comoro Fran1.00",
        "Congolese Fran1.00",
        "Costa Rican Col\\u00f31.00",
        "Croatian Dina1.00",
        "Croatian Kun1.00",
        "Cuban Pes1.00",
        "Cypriot Poun1.00",
        "Czech Republic Korun1.00",
        "Czechoslovak Hard Korun1.00",
        "D1.00",
        "DD1.00",
        "DE1.00",
        "DJ1.00",
        "DK1.00",
        "DO1.00",
        "DZ1.00",
        "Danish Kron1.00",
        "German Mar1.00",
        "Djiboutian Fran1.00",
        "Dk1.00",
        "Dominican Pes1.00",
        "EC1.00",
        "EE1.00",
        "EG1.00",
        "EQ1.00",
        "ER1.00",
        "ES1.00",
        "ET1.00",
        "EU1.00",
        "East Caribbean Dolla1.00",
        "East German Ostmar1.00",
        "Ecuadorian Sucr1.00",
        "Ecuadorian Unit of Constant Valu1.00",
        "Egyptian Poun1.00",
        "Ekwel1.00",
        "Salvadoran Col\\u00f31.00",
        "Equatorial Guinean Ekwel1.00",
        "Eritrean Nakf1.00",
        "Es1.00",
        "Estonian Kroo1.00",
        "Ethiopian Bir1.00",
        "Eur1.00",
        "European Composite Uni1.00",
        "European Currency Uni1.00",
        "European Monetary Uni1.00",
        "European Unit of Account (XBC1.00",
        "European Unit of Account (XBD1.00",
        "F1.00",
        "FB1.00",
        "FI1.00",
        "FJ1.00",
        "FK1.00",
        "FR1.00",
        "Falkland Islands Poun1.00",
        "Fd1.00",
        "Fijian Dolla1.00",
        "Finnish Markk1.00",
        "Fr1.00",
        "French Fran1.00",
        "French Gold Fran1.00",
        "French UIC-Fran1.00",
        "G1.00",
        "GB1.00",
        "GE1.00",
        "GH1.00",
        "GI1.00",
        "GM1.00",
        "GN1.00",
        "GQ1.00",
        "GR1.00",
        "GT1.00",
        "GW1.00",
        "GY1.00",
        "Gambian Dalas1.00",
        "Georgian Kupon Lari1.00",
        "Georgian Lar1.00",
        "Ghanaian Ced1.00",
        "Ghanaian Cedi (1979\\u201320071.00",
        "Gibraltar Poun1.00",
        "Gol1.00",
        "Greek Drachm1.00",
        "Guatemalan Quetza1.00",
        "Guinean Fran1.00",
        "Guinean Syl1.00",
        "Guinea-Bissau Pes1.00",
        "Guyanaese Dolla1.00",
        "HK1.00",
        "HN1.00",
        "HR1.00",
        "HT1.00",
        "HU1.00",
        "Haitian Gourd1.00",
        "Honduran Lempir1.00",
        "Hong Kong Dolla1.00",
        "Hungarian Forin1.00",
        "I1.00",
        "IE1.00",
        "IL1.00",
        "IN1.00",
        "IQ1.00",
        "IR1.00",
        "IS1.00",
        "IT1.00",
        "Icelandic Kron1.00",
        "Indian Rupe1.00",
        "Indonesian Rupia1.00",
        "Iranian Ria1.00",
        "Iraqi Dina1.00",
        "Irish Poun1.00",
        "Israeli Poun1.00",
        "Italian Lir1.00",
        "J1.00",
        "JM1.00",
        "JO1.00",
        "JP1.00",
        "Jamaican Dolla1.00",
        "Japanese Ye1.00",
        "Jordanian Dina1.00",
        "K S1.00",
        "K1.00",
        "KE1.00",
        "KG1.00",
        "KH1.00",
        "KP1.00",
        "KR1.00",
        "KW1.00",
        "KY1.00",
        "KZ1.00",
        "Kazakhstani Teng1.00",
        "Kenyan Shillin1.00",
        "Kuwaiti Dina1.00",
        "Kyrgystani So1.00",
        "LA1.00",
        "LB1.00",
        "LK1.00",
        "LR1.00",
        "LT1.00",
        "LU1.00",
        "LV1.00",
        "LY1.00",
        "Laotian Ki1.00",
        "Latvian Lat1.00",
        "Latvian Rubl1.00",
        "Lebanese Poun1.00",
        "Lesotho Lot1.00",
        "Liberian Dolla1.00",
        "Libyan Dina1.00",
        "Lithuanian Lit1.00",
        "Lithuanian Talona1.00",
        "Luxembourgian Convertible Fran1.00",
        "Luxembourg Financial Fran1.00",
        "Luxembourgian Fran1.00",
        "MA1.00",
        "MD1.00",
        "MDe1.00",
        "MEX1.00",
        "MG1.00",
        "ML1.00",
        "MM1.00",
        "MN1.00",
        "MO1.00",
        "MR1.00",
        "MT1.00",
        "MU1.00",
        "MV1.00",
        "MW1.00",
        "MX1.00",
        "MY1.00",
        "MZ1.00",
        "Macanese Patac1.00",
        "Macedonian Dena1.00",
        "Malagasy Ariar1.00",
        "Malagasy Fran1.00",
        "Malawian Kwach1.00",
        "Malaysian Ringgi1.00",
        "Maldivian Rufiya1.00",
        "Malian Fran1.00",
        "Malot1.00",
        "Maltese Lir1.00",
        "Maltese Poun1.00",
        "Mauritanian Ouguiy1.00",
        "Mauritian Rupe1.00",
        "Mexican Pes1.00",
        "Mexican Silver Peso (1861\\u201319921.00",
        "Mexican Investment Uni1.00",
        "Moldovan Le1.00",
        "Mongolian Tugri1.00",
        "Moroccan Dirha1.00",
        "Moroccan Fran1.00",
        "Mozambican Escud1.00",
        "Mozambican Metica1.00",
        "Myanmar Kya1.00",
        "N1.00",
        "NA1.00",
        "NAf1.00",
        "NG1.00",
        "NI1.00",
        "NK1.00",
        "NL1.00",
        "NO1.00",
        "NP1.00",
        "NT1.00",
        "Namibian Dolla1.00",
        "Nepalese Rupe1.00",
        "Netherlands Antillean Guilde1.00",
        "Dutch Guilde1.00",
        "Israeli New Sheqe1.00",
        "New Zealand Dolla1.00",
        "Nicaraguan C\\u00f3rdoba (1988\\u201319911.00",
        "Nicaraguan C\\u00f3rdob1.00",
        "Nigerian Nair1.00",
        "North Korean Wo1.00",
        "Norwegian Kron1.00",
        "Nr1.00",
        "OM1.00",
        "Old Mozambican Metica1.00",
        "Romanian Leu (1952\\u201320061.00",
        "Serbian Dinar (2002\\u201320061.00",
        "Sudanese Dinar (1992\\u201320071.00",
        "Sudanese Pound (1957\\u201319981.00",
        "Turkish Lira (1922\\u201320051.00",
        "Omani Ria1.00",
        "PA1.00",
        "PE1.00",
        "PG1.00",
        "PH1.00",
        "PK1.00",
        "PL1.00",
        "PT1.00",
        "PY1.00",
        "Pakistani Rupe1.00",
        "Palladiu1.00",
        "Panamanian Balbo1.00",
        "Papua New Guinean Kin1.00",
        "Paraguayan Guaran1.00",
        "Peruvian Int1.00",
        "Peruvian Sol (1863\\u201319651.00",
        "Peruvian Sol Nuev1.00",
        "Philippine Pes1.00",
        "Platinu1.00",
        "Polish Zlot1.00",
        "Polish Zloty (1950\\u201319951.00",
        "Portuguese Escud1.00",
        "Portuguese Guinea Escud1.00",
        "Pr1.00",
        "QA1.00",
        "Qatari Ria1.00",
        "RD1.00",
        "RH1.00",
        "RINET Fund1.00",
        "RS1.00",
        "RU1.00",
        "RW1.00",
        "Rb1.00",
        "Rhodesian Dolla1.00",
        "Romanian Le1.00",
        "Russian Rubl1.00",
        "Russian Ruble (1991\\u201319981.00",
        "Rwandan Fran1.00",
        "S1.00",
        "SA1.00",
        "SB1.00",
        "SC1.00",
        "SD1.00",
        "SE1.00",
        "SG1.00",
        "SH1.00",
        "SI1.00",
        "SK1.00",
        "SL R1.00",
        "SL1.00",
        "SO1.00",
        "ST1.00",
        "SU1.00",
        "SV1.00",
        "SY1.00",
        "SZ1.00",
        "St. Helena Poun1.00",
        "S\\u00e3o Tom\\u00e9 & Pr\\u00edncipe Dobr1.00",
        "Saudi Riya1.00",
        "Serbian Dina1.00",
        "Seychellois Rupe1.00",
        "Sh1.00",
        "Sierra Leonean Leon1.00",
        "Silve1.00",
        "Singapore Dolla1.00",
        "Slovak Korun1.00",
        "Slovenian Tola1.00",
        "Solomon Islands Dolla1.00",
        "Somali Shillin1.00",
        "South African Ran1.00",
        "South African Rand (financial1.00",
        "South Korean Wo1.00",
        "Soviet Roubl1.00",
        "Spanish Peset1.00",
        "Spanish Peseta (A account1.00",
        "Spanish Peseta (convertible account1.00",
        "Special Drawing Right1.00",
        "Sri Lankan Rupe1.00",
        "Sudanese Poun1.00",
        "Surinamese Dolla1.00",
        "Surinamese Guilde1.00",
        "Swazi Lilangen1.00",
        "Swedish Kron1.00",
        "Swiss Fran1.00",
        "Syrian Poun1.00",
        "T S1.00",
        "TH1.00",
        "TJ1.00",
        "TM1.00",
        "TN1.00",
        "TO1.00",
        "TP1.00",
        "TR1.00",
        "TT1.00",
        "TW1.00",
        "TZ1.00",
        "New Taiwan Dolla1.00",
        "Tajikistani Rubl1.00",
        "Tajikistani Somon1.00",
        "Tanzanian Shillin1.00",
        "Testing Currency Cod1.00",
        "Thai Bah1.00",
        "Timorese Escud1.00",
        "Tongan Pa\\u20bbang1.00",
        "Trinidad & Tobago Dolla1.00",
        "Tunisian Dina1.00",
        "Turkish Lir1.00",
        "Turkmenistani Mana1.00",
        "U S1.00",
        "U1.00",
        "UA1.00",
        "UG1.00",
        "US Dolla1.00",
        "US Dollar (Next day1.00",
        "US Dollar (Same day1.00",
        "US1.00",
        "UY1.00",
        "UZ1.00",
        "Ugandan Shillin1.00",
        "Ugandan Shilling (1966\\u201319871.00",
        "Ukrainian Hryvni1.00",
        "Ukrainian Karbovanet1.00",
        "Colombian Real Value Uni1.00",
        "United Arab Emirates Dirha1.00",
        "Unknown Currenc1.00",
        "Ur1.00",
        "Uruguay Peso (1975\\u201319931.00",
        "Uruguay Peso Uruguay1.00",
        "Uruguay Peso (Indexed Units1.00",
        "Uzbekistani So1.00",
        "V1.00",
        "VE1.00",
        "VN1.00",
        "VU1.00",
        "Vanuatu Vat1.00",
        "Venezuelan Bol\\u00edva1.00",
        "Venezuelan Bol\\u00edvar Fuert1.00",
        "Vietnamese Don1.00",
        "West African CFA Fran1.00",
        "Central African CFA Fran1.00",
        "WIR Eur1.00",
        "WIR Fran1.00",
        "WS1.00",
        "Samoa Tal1.00",
        "XA1.00",
        "XB1.00",
        "XC1.00",
        "XD1.00",
        "XE1.00",
        "XF1.00",
        "XO1.00",
        "XP1.00",
        "XR1.00",
        "XT1.00",
        "XX1.00",
        "YD1.00",
        "YE1.00",
        "YU1.00",
        "Yemeni Dina1.00",
        "Yemeni Ria1.00",
        "Yugoslavian Convertible Dina1.00",
        "Yugoslavian Hard Dinar (1966\\u201319901.00",
        "Yugoslavian New Dina1.00",
        "Z1.00",
        "ZA1.00",
        "ZM1.00",
        "ZR1.00",
        "ZW1.00",
        "Zairean New Zaire (1993\\u201319981.00",
        "Zairean Zair1.00",
        "Zambian Kwach1.00",
        "Zimbabwean Dollar (1980\\u201320081.00",
        "dra1.00",
        "lar1.00",
        "le1.00",
        "man1.00",
        "so1.00",
    };

    Locale locale("en_US");
    for (uint32_t i=0; i<UPRV_LENGTHOF(DATA); ++i) {
        UnicodeString formatted = ctou(DATA[i]);
        UErrorCode status = U_ZERO_ERROR;
        LocalPointer<NumberFormat> numFmt(NumberFormat::createInstance(locale, UNUM_CURRENCY, status), status);
        if (!assertSuccess("", status, true, __FILE__, __LINE__)) {
            return;
        }
        // NOTE: ICU 62 requires that the currency format match the pattern in strict mode.
        numFmt->setLenient(TRUE);
        ParsePosition parsePos;
        LocalPointer<CurrencyAmount> currAmt(numFmt->parseCurrency(formatted, parsePos));
        if (parsePos.getIndex() > 0) {
            double doubleVal = currAmt->getNumber().getDouble(status);
            if ( doubleVal != 1.0 ) {
                errln("Parsed as currency value other than 1.0: " + formatted + " -> " + doubleVal);
            }
        } else {
            errln("Failed to parse as currency: " + formatted);
        }
    }

    for (uint32_t i=0; i<UPRV_LENGTHOF(WRONG_DATA); ++i) {
      UnicodeString formatted = ctou(WRONG_DATA[i]);
      UErrorCode status = U_ZERO_ERROR;
      NumberFormat* numFmt = NumberFormat::createInstance(locale, UNUM_CURRENCY, status);
      if (numFmt != NULL && U_SUCCESS(status)) {
          ParsePosition parsePos;
          LocalPointer<CurrencyAmount> currAmt(numFmt->parseCurrency(formatted, parsePos));
          if (parsePos.getIndex() > 0) {
              double doubleVal = currAmt->getNumber().getDouble(status);
              errln("Parsed as currency, should not have: " + formatted + " -> " + doubleVal);
          }
      } else {
          dataerrln("Unable to create NumberFormat. - %s", u_errorName(status));
          delete numFmt;
          break;
      }
      delete numFmt;
    }
}

const char* attrString(int32_t);

// UnicodeString s;
//  std::string ss;
//  std::cout << s.toUTF8String(ss)
void NumberFormatTest::expectPositions(FieldPositionIterator& iter, int32_t *values, int32_t tupleCount,
                                       const UnicodeString& str)  {
  UBool found[10];
  FieldPosition fp;

  if (tupleCount > 10) {
    assertTrue("internal error, tupleCount too large", FALSE);
  } else {
    for (int i = 0; i < tupleCount; ++i) {
      found[i] = FALSE;
    }
  }

  logln(str);
  while (iter.next(fp)) {
    UBool ok = FALSE;
    int32_t id = fp.getField();
    int32_t start = fp.getBeginIndex();
    int32_t limit = fp.getEndIndex();

    // is there a logln using printf?
    char buf[128];
    sprintf(buf, "%24s %3d %3d %3d", attrString(id), id, start, limit);
    logln(buf);

    for (int i = 0; i < tupleCount; ++i) {
      if (found[i]) {
        continue;
      }
      if (values[i*3] == id &&
          values[i*3+1] == start &&
          values[i*3+2] == limit) {
        found[i] = ok = TRUE;
        break;
      }
    }

    assertTrue((UnicodeString)"found [" + id + "," + start + "," + limit + "]", ok);
  }

  // check that all were found
  UBool ok = TRUE;
  for (int i = 0; i < tupleCount; ++i) {
    if (!found[i]) {
      ok = FALSE;
      assertTrue((UnicodeString) "missing [" + values[i*3] + "," + values[i*3+1] + "," + values[i*3+2] + "]", found[i]);
    }
  }
  assertTrue("no expected values were missing", ok);
}

void NumberFormatTest::expectPosition(FieldPosition& pos, int32_t id, int32_t start, int32_t limit,
                                       const UnicodeString& str)  {
  logln(str);
  assertTrue((UnicodeString)"id " + id + " == " + pos.getField(), id == pos.getField());
  assertTrue((UnicodeString)"begin " + start + " == " + pos.getBeginIndex(), start == pos.getBeginIndex());
  assertTrue((UnicodeString)"end " + limit + " == " + pos.getEndIndex(), limit == pos.getEndIndex());
}

void NumberFormatTest::TestFieldPositionIterator() {
  // bug 7372
  UErrorCode status = U_ZERO_ERROR;
  FieldPositionIterator iter1;
  FieldPositionIterator iter2;
  FieldPosition pos;

  DecimalFormat *decFmt = (DecimalFormat *) NumberFormat::createInstance(status);
  if (failure(status, "NumberFormat::createInstance", TRUE)) return;

  double num = 1234.56;
  UnicodeString str1;
  UnicodeString str2;

  assertTrue((UnicodeString)"self==", iter1 == iter1);
  assertTrue((UnicodeString)"iter1==iter2", iter1 == iter2);

  decFmt->format(num, str1, &iter1, status);
  assertTrue((UnicodeString)"iter1 != iter2", iter1 != iter2);
  decFmt->format(num, str2, &iter2, status);
  assertTrue((UnicodeString)"iter1 == iter2 (2)", iter1 == iter2);
  iter1.next(pos);
  assertTrue((UnicodeString)"iter1 != iter2 (2)", iter1 != iter2);
  iter2.next(pos);
  assertTrue((UnicodeString)"iter1 == iter2 (3)", iter1 == iter2);

  // should format ok with no iterator
  str2.remove();
  decFmt->format(num, str2, NULL, status);
  assertEquals("null fpiter", str1, str2);

  delete decFmt;
}

void NumberFormatTest::TestFormatAttributes() {
  Locale locale("en_US");
  UErrorCode status = U_ZERO_ERROR;
  DecimalFormat *decFmt = (DecimalFormat *) NumberFormat::createInstance(locale, UNUM_CURRENCY, status);
    if (failure(status, "NumberFormat::createInstance", TRUE)) return;
  double val = 12345.67;

  {
    int32_t expected[] = {
      UNUM_CURRENCY_FIELD, 0, 1,
      UNUM_GROUPING_SEPARATOR_FIELD, 3, 4,
      UNUM_INTEGER_FIELD, 1, 7,
      UNUM_DECIMAL_SEPARATOR_FIELD, 7, 8,
      UNUM_FRACTION_FIELD, 8, 10,
    };
    int32_t tupleCount = UPRV_LENGTHOF(expected)/3;

    FieldPositionIterator posIter;
    UnicodeString result;
    decFmt->format(val, result, &posIter, status);
    expectPositions(posIter, expected, tupleCount, result);
  }
  {
    FieldPosition fp(UNUM_INTEGER_FIELD);
    UnicodeString result;
    decFmt->format(val, result, fp);
    expectPosition(fp, UNUM_INTEGER_FIELD, 1, 7, result);
  }
  {
    FieldPosition fp(UNUM_FRACTION_FIELD);
    UnicodeString result;
    decFmt->format(val, result, fp);
    expectPosition(fp, UNUM_FRACTION_FIELD, 8, 10, result);
  }
  delete decFmt;

  decFmt = (DecimalFormat *) NumberFormat::createInstance(locale, UNUM_SCIENTIFIC, status);
  val = -0.0000123;
  {
    int32_t expected[] = {
      UNUM_SIGN_FIELD, 0, 1,
      UNUM_INTEGER_FIELD, 1, 2,
      UNUM_DECIMAL_SEPARATOR_FIELD, 2, 3,
      UNUM_FRACTION_FIELD, 3, 5,
      UNUM_EXPONENT_SYMBOL_FIELD, 5, 6,
      UNUM_EXPONENT_SIGN_FIELD, 6, 7,
      UNUM_EXPONENT_FIELD, 7, 8
    };
    int32_t tupleCount = UPRV_LENGTHOF(expected)/3;

    FieldPositionIterator posIter;
    UnicodeString result;
    decFmt->format(val, result, &posIter, status);
    expectPositions(posIter, expected, tupleCount, result);
  }
  {
    FieldPosition fp(UNUM_INTEGER_FIELD);
    UnicodeString result;
    decFmt->format(val, result, fp);
    expectPosition(fp, UNUM_INTEGER_FIELD, 1, 2, result);
  }
  {
    FieldPosition fp(UNUM_FRACTION_FIELD);
    UnicodeString result;
    decFmt->format(val, result, fp);
    expectPosition(fp, UNUM_FRACTION_FIELD, 3, 5, result);
  }
  delete decFmt;

  fflush(stderr);
}

const char* attrString(int32_t attrId) {
  switch (attrId) {
    case UNUM_INTEGER_FIELD: return "integer";
    case UNUM_FRACTION_FIELD: return "fraction";
    case UNUM_DECIMAL_SEPARATOR_FIELD: return "decimal separator";
    case UNUM_EXPONENT_SYMBOL_FIELD: return "exponent symbol";
    case UNUM_EXPONENT_SIGN_FIELD: return "exponent sign";
    case UNUM_EXPONENT_FIELD: return "exponent";
    case UNUM_GROUPING_SEPARATOR_FIELD: return "grouping separator";
    case UNUM_CURRENCY_FIELD: return "currency";
    case UNUM_PERCENT_FIELD: return "percent";
    case UNUM_PERMILL_FIELD: return "permille";
    case UNUM_SIGN_FIELD: return "sign";
    default: return "";
  }
}

//
//   Test formatting & parsing of big decimals.
//      API test, not a comprehensive test.
//      See DecimalFormatTest/DataDrivenTests
//
#define ASSERT_SUCCESS(status) { \
    assertSuccess(UnicodeString("file ") + __FILE__ + ", line " + __LINE__, (status)); \
}
#define ASSERT_EQUALS(expected, actual) { \
    assertEquals(UnicodeString("file ") + __FILE__ + ", line " + __LINE__, (expected), (actual)); \
}

void NumberFormatTest::TestDecimal() {
    {
        UErrorCode  status = U_ZERO_ERROR;
        Formattable f("12.345678999987654321E666", status);
        ASSERT_SUCCESS(status);
        StringPiece s = f.getDecimalNumber(status);
        ASSERT_SUCCESS(status);
        ASSERT_EQUALS("1.2345678999987654321E+667", s.data());
        //printf("%s\n", s.data());
    }

    {
        UErrorCode status = U_ZERO_ERROR;
        Formattable f1("this is not a number", status);
        ASSERT_EQUALS(U_DECIMAL_NUMBER_SYNTAX_ERROR, status);
    }

    {
        UErrorCode status = U_ZERO_ERROR;
        Formattable f;
        f.setDecimalNumber("123.45", status);
        ASSERT_SUCCESS(status);
        ASSERT_EQUALS( Formattable::kDouble, f.getType());
        ASSERT_EQUALS(123.45, f.getDouble());
        ASSERT_EQUALS(123.45, f.getDouble(status));
        ASSERT_SUCCESS(status);
        ASSERT_EQUALS("123.45", f.getDecimalNumber(status).data());
        ASSERT_SUCCESS(status);

        f.setDecimalNumber("4.5678E7", status);
        int32_t n;
        n = f.getLong();
        ASSERT_EQUALS(45678000, n);

        status = U_ZERO_ERROR;
        f.setDecimalNumber("-123", status);
        ASSERT_SUCCESS(status);
        ASSERT_EQUALS( Formattable::kLong, f.getType());
        ASSERT_EQUALS(-123, f.getLong());
        ASSERT_EQUALS(-123, f.getLong(status));
        ASSERT_SUCCESS(status);
        ASSERT_EQUALS("-123", f.getDecimalNumber(status).data());
        ASSERT_SUCCESS(status);

        status = U_ZERO_ERROR;
        f.setDecimalNumber("1234567890123", status);  // Number too big for 32 bits
        ASSERT_SUCCESS(status);
        ASSERT_EQUALS( Formattable::kInt64, f.getType());
        ASSERT_EQUALS(1234567890123LL, f.getInt64());
        ASSERT_EQUALS(1234567890123LL, f.getInt64(status));
        ASSERT_SUCCESS(status);
        ASSERT_EQUALS("1234567890123", f.getDecimalNumber(status).data());
        ASSERT_SUCCESS(status);
    }

    {
        UErrorCode status = U_ZERO_ERROR;
        NumberFormat *fmtr = NumberFormat::createInstance(Locale::getUS(), UNUM_DECIMAL, status);
        if (U_FAILURE(status) || fmtr == NULL) {
            dataerrln("Unable to create NumberFormat");
        } else {
            UnicodeString formattedResult;
            StringPiece num("244444444444444444444444444444444444446.4");
            fmtr->format(num, formattedResult, NULL, status);
            ASSERT_SUCCESS(status);
            ASSERT_EQUALS("244,444,444,444,444,444,444,444,444,444,444,444,446.4", formattedResult);
            //std::string ss; std::cout << formattedResult.toUTF8String(ss);
            delete fmtr;
        }
    }

    {
        // Check formatting a DigitList.  DigitList is internal, but this is
        // a critical interface that must work.
        UErrorCode status = U_ZERO_ERROR;
        NumberFormat *fmtr = NumberFormat::createInstance(Locale::getUS(), UNUM_DECIMAL, status);
        if (U_FAILURE(status) || fmtr == NULL) {
            dataerrln("Unable to create NumberFormat");
        } else {
            UnicodeString formattedResult;
            DecimalQuantity dl;
            StringPiece num("123.4566666666666666666666666666666666621E+40");
            dl.setToDecNumber(num, status);
            ASSERT_SUCCESS(status);
            fmtr->format(dl, formattedResult, NULL, status);
            ASSERT_SUCCESS(status);
            ASSERT_EQUALS("1,234,566,666,666,666,666,666,666,666,666,666,666,621,000", formattedResult);

            status = U_ZERO_ERROR;
            num.set("666.666");
            dl.setToDecNumber(num, status);
            FieldPosition pos(NumberFormat::FRACTION_FIELD);
            ASSERT_SUCCESS(status);
            formattedResult.remove();
            fmtr->format(dl, formattedResult, pos, status);
            ASSERT_SUCCESS(status);
            ASSERT_EQUALS("666.666", formattedResult);
            ASSERT_EQUALS(4, pos.getBeginIndex());
            ASSERT_EQUALS(7, pos.getEndIndex());
            delete fmtr;
        }
    }

    {
        // Check a parse with a formatter with a multiplier.
        UErrorCode status = U_ZERO_ERROR;
        NumberFormat *fmtr = NumberFormat::createInstance(Locale::getUS(), UNUM_PERCENT, status);
        if (U_FAILURE(status) || fmtr == NULL) {
            dataerrln("Unable to create NumberFormat");
        } else {
            UnicodeString input = "1.84%";
            Formattable result;
            fmtr->parse(input, result, status);
            ASSERT_SUCCESS(status);
            ASSERT_EQUALS("0.0184", result.getDecimalNumber(status).data());
            //std::cout << result.getDecimalNumber(status).data();
            delete fmtr;
        }
    }

#if U_PLATFORM != U_PF_CYGWIN || defined(CYGWINMSVC)
    /*
     * This test fails on Cygwin (1.7.16) using GCC because of a rounding issue with strtod().
     * See #9463
     */
    {
        // Check that a parse returns a decimal number with full accuracy
        UErrorCode status = U_ZERO_ERROR;
        NumberFormat *fmtr = NumberFormat::createInstance(Locale::getUS(), UNUM_DECIMAL, status);
        if (U_FAILURE(status) || fmtr == NULL) {
            dataerrln("Unable to create NumberFormat");
        } else {
            UnicodeString input = "1.002200044400088880000070000";
            Formattable result;
            fmtr->parse(input, result, status);
            ASSERT_SUCCESS(status);
            ASSERT_EQUALS(0, strcmp("1.00220004440008888000007", result.getDecimalNumber(status).data()));
            ASSERT_EQUALS(1.00220004440008888,   result.getDouble());
            //std::cout << result.getDecimalNumber(status).data();
            delete fmtr;
        }
    }
#endif

}

void NumberFormatTest::TestCurrencyFractionDigits() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString text1, text2;
    double value = 99.12345;

    // Create currenct instance
    NumberFormat* fmt = NumberFormat::createCurrencyInstance("ja_JP", status);
    if (U_FAILURE(status) || fmt == NULL) {
        dataerrln("Unable to create NumberFormat");
    } else {
        fmt->format(value, text1);

        // Reset the same currency and format the test value again
        fmt->setCurrency(fmt->getCurrency(), status);
        ASSERT_SUCCESS(status);
        fmt->format(value, text2);

        if (text1 != text2) {
            errln((UnicodeString)"NumberFormat::format() should return the same result - text1="
                + text1 + " text2=" + text2);
        }
    }
    delete fmt;
}

void NumberFormatTest::TestExponentParse() {

    UErrorCode status = U_ZERO_ERROR;
    Formattable result;
    ParsePosition parsePos(0);

    // set the exponent symbol
    status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols(Locale::getDefault(), status);
    if(U_FAILURE(status)) {
        dataerrln((UnicodeString)"ERROR: Could not create DecimalFormatSymbols (Default)");
        return;
    }

    // create format instance
    status = U_ZERO_ERROR;
    DecimalFormat fmt(u"#####", symbols, status);
    if(U_FAILURE(status)) {
        errln((UnicodeString)"ERROR: Could not create DecimalFormat (pattern, symbols*)");
    }

    // parse the text
    fmt.parse("5.06e-27", result, parsePos);
    if(result.getType() != Formattable::kDouble &&
       result.getDouble() != 5.06E-27 &&
       parsePos.getIndex() != 8
       )
    {
        errln("ERROR: parse failed - expected 5.06E-27, 8  - returned %d, %i",
              result.getDouble(), parsePos.getIndex());
    }
}

void NumberFormatTest::TestExplicitParents() {

    /* Test that number formats are properly inherited from es_419 */
    /* These could be subject to change if the CLDR data changes */
    static const char* parentLocaleTests[][2]= {
    /* locale ID */  /* expected */
    {"es_CO", "1.250,75" },
    {"es_ES", "1.250,75" },
    {"es_GQ", "1.250,75" },
    {"es_MX", "1,250.75" },
    {"es_US", "1,250.75" },
    {"es_VE", "1.250,75" },
    };

    UnicodeString s;

    for(int i=0; i < UPRV_LENGTHOF(parentLocaleTests); i++){
        UErrorCode status = U_ZERO_ERROR;
        const char *localeID = parentLocaleTests[i][0];
        UnicodeString expected(parentLocaleTests[i][1], -1, US_INV);
        expected = expected.unescape();
        char loc[256]={0};
        uloc_canonicalize(localeID, loc, 256, &status);
        NumberFormat *fmt= NumberFormat::createInstance(Locale(loc), status);
        if(U_FAILURE(status)){
            dataerrln("Could not create number formatter for locale %s - %s",localeID, u_errorName(status));
            continue;
        }
        s.remove();
        fmt->format(1250.75, s);
        if(s!=expected){
            errln(UnicodeString("FAIL: Expected: ")+expected
                    + UnicodeString(" Got: ") + s
                    + UnicodeString( " for locale: ")+ UnicodeString(localeID) );
        }
        if (U_FAILURE(status)){
            errln((UnicodeString)"FAIL: Status " + (int32_t)status);
        }
        delete fmt;
    }

}

/**
 * Test available numbering systems API.
 */
void NumberFormatTest::TestAvailableNumberingSystems() {
    IcuTestErrorCode status(*this, "TestAvailableNumberingSystems");
    StringEnumeration *availableNumberingSystems = NumberingSystem::getAvailableNames(status);
    CHECK_DATA(status, "NumberingSystem::getAvailableNames()")

    int32_t nsCount = availableNumberingSystems->count(status);
    if ( nsCount < 74 ) {
        errln("FAIL: Didn't get as many numbering systems as we had hoped for. Need at least 74, got %d",nsCount);
    }

    /* A relatively simple test of the API.  We call getAvailableNames() and cycle through */
    /* each name returned, attempting to create a numbering system based on that name and  */
    /* verifying that the name returned from the resulting numbering system is the same    */
    /* one that we initially thought.                                                      */

    int32_t len;
    const char* prevName = nullptr;
    for ( int32_t i = 0 ; i < nsCount ; i++ ) {
        const char *nsname = availableNumberingSystems->next(&len,status);
        NumberingSystem* ns = NumberingSystem::createInstanceByName(nsname,status);
        logln("OK for ns = %s",nsname);
        if ( uprv_strcmp(nsname,ns->getName()) ) {
            errln("FAIL: Numbering system name didn't match for name = %s\n",nsname);
        }
        if (prevName != nullptr) {
            int comp = uprv_strcmp(prevName, nsname);
            assertTrue(
                UnicodeString(u"NS names should be in alphabetical order: ")
                    + prevName + u" vs " + nsname,
                // TODO: Why are there duplicates? This doesn't work if comp < 0
                comp <= 0);
        }
        prevName = nsname;

        delete ns;
    }

    LocalPointer<NumberingSystem> dummy(NumberingSystem::createInstanceByName("dummy", status), status);
    status.expectErrorAndReset(U_UNSUPPORTED_ERROR);
    assertTrue("Non-existent numbering system should return null", dummy.isNull());

    delete availableNumberingSystems;
}

void
NumberFormatTest::Test9087(void)
{
    U_STRING_DECL(pattern,"#",1);
    U_STRING_INIT(pattern,"#",1);

    U_STRING_DECL(infstr,"INF",3);
    U_STRING_INIT(infstr,"INF",3);

    U_STRING_DECL(nanstr,"NAN",3);
    U_STRING_INIT(nanstr,"NAN",3);

    UChar outputbuf[50] = {0};
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormat* fmt = unum_open(UNUM_PATTERN_DECIMAL,pattern,1,NULL,NULL,&status);
    if ( U_FAILURE(status) ) {
        dataerrln("FAIL: error in unum_open() - %s", u_errorName(status));
        return;
    }

    unum_setSymbol(fmt,UNUM_INFINITY_SYMBOL,infstr,3,&status);
    unum_setSymbol(fmt,UNUM_NAN_SYMBOL,nanstr,3,&status);
    if ( U_FAILURE(status) ) {
        errln("FAIL: error setting symbols");
    }

    double inf = uprv_getInfinity();

    unum_setAttribute(fmt,UNUM_ROUNDING_MODE,UNUM_ROUND_HALFEVEN);
    unum_setDoubleAttribute(fmt,UNUM_ROUNDING_INCREMENT,0);

    UFieldPosition position = { 0, 0, 0};
    unum_formatDouble(fmt,inf,outputbuf,50,&position,&status);

    if ( u_strcmp(infstr, outputbuf)) {
        errln((UnicodeString)"FAIL: unexpected result for infinity - expected " + infstr + " got " + outputbuf);
    }

    unum_close(fmt);
}

void NumberFormatTest::TestFormatFastpaths() {
    // get some additional case
    {
        UErrorCode status=U_ZERO_ERROR;
        DecimalFormat df(UnicodeString(u"0000"),status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        } else {
            int64_t long_number = 1;
            UnicodeString expect = "0001";
            UnicodeString result;
            FieldPosition pos;
            df.format(long_number, result, pos);
            if(U_FAILURE(status)||expect!=result) {
                dataerrln("%s:%d FAIL: expected '%s' got '%s' status %s",
                          __FILE__, __LINE__, CStr(expect)(), CStr(result)(), u_errorName(status));
             } else {
                logln("OK:  got expected '"+result+"' status "+UnicodeString(u_errorName(status),""));
            }
        }
    }
    {
        UErrorCode status=U_ZERO_ERROR;
        DecimalFormat df(UnicodeString(u"0000000000000000000"),status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        } else {
            int64_t long_number = U_INT64_MIN; // -9223372036854775808L;
            // uint8_t bits[8];
            // memcpy(bits,&long_number,8);
            // for(int i=0;i<8;i++) {
            //   logln("bits: %02X", (unsigned int)bits[i]);
            // }
            UnicodeString expect = "-9223372036854775808";
            UnicodeString result;
            FieldPosition pos;
            df.format(long_number, result, pos);
            if(U_FAILURE(status)||expect!=result) {
                dataerrln("%s:%d FAIL: expected '%s' got '%s' status %s on -9223372036854775808",
                          __FILE__, __LINE__, CStr(expect)(), CStr(result)(), u_errorName(status));
            } else {
                logln("OK:  got expected '"+result+"' status "+UnicodeString(u_errorName(status),"")+" on -9223372036854775808");
            }
        }
    }
    {
        UErrorCode status=U_ZERO_ERROR;
        DecimalFormat df(UnicodeString(u"0000000000000000000"),status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        } else {
            int64_t long_number = U_INT64_MAX; // -9223372036854775808L;
            // uint8_t bits[8];
            // memcpy(bits,&long_number,8);
            // for(int i=0;i<8;i++) {
            //   logln("bits: %02X", (unsigned int)bits[i]);
            // }
            UnicodeString expect = "9223372036854775807";
            UnicodeString result;
            FieldPosition pos;
            df.format(long_number, result, pos);
            if(U_FAILURE(status)||expect!=result) {
                dataerrln("%s:%d FAIL: expected '%s' got '%s' status %s on U_INT64_MAX",
                          __FILE__, __LINE__, CStr(expect)(), CStr(result)(), u_errorName(status));
            } else {
                logln("OK:  got expected '"+result+"' status "+UnicodeString(u_errorName(status),"")+" on U_INT64_MAX");
            }
        }
    }
    {
        UErrorCode status=U_ZERO_ERROR;
        DecimalFormat df(UnicodeString("0000000000000000000",""),status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        } else {
            int64_t long_number = 0;
            // uint8_t bits[8];
            // memcpy(bits,&long_number,8);
            // for(int i=0;i<8;i++) {
            //   logln("bits: %02X", (unsigned int)bits[i]);
            // }
            UnicodeString expect = "0000000000000000000";
            UnicodeString result;
            FieldPosition pos;
            df.format(long_number, result, pos);
            if(U_FAILURE(status)||expect!=result) {
                dataerrln("%s:%d FAIL: expected '%s' got '%s' status %s on 0",
                          __FILE__, __LINE__, CStr(expect)(), CStr(result)(), u_errorName(status));
            } else {
                logln("OK:  got expected '"+result+"' status "+UnicodeString(u_errorName(status),"")+" on 0");
            }
        }
    }
    {
        UErrorCode status=U_ZERO_ERROR;
        DecimalFormat df(UnicodeString("0000000000000000000",""),status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        } else {
            int64_t long_number = U_INT64_MIN + 1;
            UnicodeString expect = "-9223372036854775807";
            UnicodeString result;
            FieldPosition pos;
            df.format(long_number, result, pos);
            if(U_FAILURE(status)||expect!=result) {
                dataerrln("%s:%d FAIL: expected '%s' got '%s' status %s on -9223372036854775807",
                          __FILE__, __LINE__, CStr(expect)(), CStr(result)(), u_errorName(status));
            } else {
                logln("OK:  got expected '"+result+"' status "+UnicodeString(u_errorName(status),"")+" on -9223372036854775807");
            }
        }
    }
}


void NumberFormatTest::TestFormattableSize(void) {
  if(sizeof(Formattable) > 112) {
    errln("Error: sizeof(Formattable)=%d, 112=%d\n",
          sizeof(Formattable), 112);
  } else if(sizeof(Formattable) < 112) {
    logln("Warning: sizeof(Formattable)=%d, 112=%d\n",
        sizeof(Formattable), 112);
  } else {
    logln("sizeof(Formattable)=%d, 112=%d\n",
        sizeof(Formattable), 112);
  }
}

UBool NumberFormatTest::testFormattableAsUFormattable(const char *file, int line, Formattable &f) {
  UnicodeString fileLine = UnicodeString(file)+UnicodeString(":")+line+UnicodeString(": ");

  UFormattable *u = f.toUFormattable();
  logln();
  if (u == NULL) {
    errln("%s:%d: Error: f.toUFormattable() retuned NULL.");
    return FALSE;
  }
  logln("%s:%d: comparing Formattable with UFormattable", file, line);
  logln(fileLine + toString(f));

  UErrorCode status = U_ZERO_ERROR;
  UErrorCode valueStatus = U_ZERO_ERROR;
  UFormattableType expectUType = UFMT_COUNT; // invalid

  UBool triedExact = FALSE; // did we attempt an exact comparison?
  UBool exactMatch = FALSE; // was the exact comparison true?

  switch( f.getType() ) {
  case Formattable::kDate:
    expectUType = UFMT_DATE;
    exactMatch = (f.getDate()==ufmt_getDate(u, &valueStatus));
    triedExact = TRUE;
    break;
  case Formattable::kDouble:
    expectUType = UFMT_DOUBLE;
    exactMatch = (f.getDouble()==ufmt_getDouble(u, &valueStatus));
    triedExact = TRUE;
    break;
  case Formattable::kLong:
    expectUType = UFMT_LONG;
    exactMatch = (f.getLong()==ufmt_getLong(u, &valueStatus));
    triedExact = TRUE;
    break;
  case Formattable::kString:
    expectUType = UFMT_STRING;
    {
      UnicodeString str;
      f.getString(str);
      int32_t len;
      const UChar* uch = ufmt_getUChars(u, &len, &valueStatus);
      if(U_SUCCESS(valueStatus)) {
        UnicodeString str2(uch, len);
        assertTrue("UChar* NULL-terminated", uch[len]==0);
        exactMatch = (str == str2);
      }
      triedExact = TRUE;
    }
    break;
  case Formattable::kArray:
    expectUType = UFMT_ARRAY;
    triedExact = TRUE;
    {
      int32_t count = ufmt_getArrayLength(u, &valueStatus);
      int32_t count2;
      const Formattable *array2 = f.getArray(count2);
      exactMatch = assertEquals(fileLine + " array count", count, count2);

      if(exactMatch) {
        for(int i=0;U_SUCCESS(valueStatus) && i<count;i++) {
          UFormattable *uu = ufmt_getArrayItemByIndex(u, i, &valueStatus);
          if(*Formattable::fromUFormattable(uu) != (array2[i])) {
            errln("%s:%d: operator== did not match at index[%d] - %p vs %p", file, line, i,
                  (const void*)Formattable::fromUFormattable(uu), (const void*)&(array2[i]));
            exactMatch = FALSE;
          } else {
            if(!testFormattableAsUFormattable("(sub item)",i,*Formattable::fromUFormattable(uu))) {
              exactMatch = FALSE;
            }
          }
        }
      }
    }
    break;
  case Formattable::kInt64:
    expectUType = UFMT_INT64;
    exactMatch = (f.getInt64()==ufmt_getInt64(u, &valueStatus));
    triedExact = TRUE;
    break;
  case Formattable::kObject:
    expectUType = UFMT_OBJECT;
    exactMatch = (f.getObject()==ufmt_getObject(u, &valueStatus));
    triedExact = TRUE;
    break;
  }
  UFormattableType uType = ufmt_getType(u, &status);

  if(U_FAILURE(status)) {
    errln("%s:%d: Error calling ufmt_getType - %s", file, line, u_errorName(status));
    return FALSE;
  }

  if(uType != expectUType) {
    errln("%s:%d: got type (%d) expected (%d) from ufmt_getType", file, line, (int) uType, (int) expectUType);
  }

  if(triedExact) {
    if(U_FAILURE(valueStatus)) {
      errln("%s:%d: got err %s trying to ufmt_get...() for exact match check", file, line, u_errorName(valueStatus));
    } else if(!exactMatch) {
     errln("%s:%d: failed exact match for the Formattable type", file, line);
    } else {
      logln("%s:%d: exact match OK", file, line);
    }
  } else {
    logln("%s:%d: note, did not attempt exact match for this formattable type", file, line);
  }

  if( assertEquals(fileLine + " isNumeric()", f.isNumeric(), ufmt_isNumeric(u))
      && f.isNumeric()) {
    UErrorCode convStatus = U_ZERO_ERROR;

    if(uType != UFMT_INT64) { // may fail to compare
      assertTrue(fileLine + " as doubles ==", f.getDouble(convStatus)==ufmt_getDouble(u, &convStatus));
    }

    if( assertSuccess(fileLine + " (numeric conversion status)", convStatus) ) {
      StringPiece fDecNum = f.getDecimalNumber(convStatus);
#if 1
      int32_t len;
      const char *decNumChars = ufmt_getDecNumChars(u, &len, &convStatus);
#else
      // copy version
      char decNumChars[200];
      int32_t len = ufmt_getDecNumChars(u, decNumChars, 200, &convStatus);
#endif

      if( assertSuccess(fileLine + " (decNumbers conversion)", convStatus) ) {
        logln(fileLine + decNumChars);
        assertEquals(fileLine + " decNumChars length==", len, fDecNum.length());
        assertEquals(fileLine + " decNumChars digits", decNumChars, fDecNum.data());
      }

      UErrorCode int64ConversionF = U_ZERO_ERROR;
      int64_t l = f.getInt64(int64ConversionF);
      UErrorCode int64ConversionU = U_ZERO_ERROR;
      int64_t r = ufmt_getInt64(u, &int64ConversionU);

      if( (l==r)
          && ( uType != UFMT_INT64 ) // int64 better not overflow
          && (U_INVALID_FORMAT_ERROR==int64ConversionU)
          && (U_INVALID_FORMAT_ERROR==int64ConversionF) ) {
        logln("%s:%d: OK: 64 bit overflow", file, line);
      } else {
        assertEquals(fileLine + " as int64 ==", l, r);
        assertSuccess(fileLine + " Formattable.getnt64()", int64ConversionF);
        assertSuccess(fileLine + " ufmt_getInt64()", int64ConversionU);
      }
    }
  }
  return exactMatch || !triedExact;
}

void NumberFormatTest::TestUFormattable(void) {
  {
    // test that a default formattable is equal to Formattable()
    UErrorCode status = U_ZERO_ERROR;
    LocalUFormattablePointer defaultUFormattable(ufmt_open(&status));
    assertSuccess("calling umt_open", status);
    Formattable defaultFormattable;
    assertTrue((UnicodeString)"comparing ufmt_open() with Formattable()",
               (defaultFormattable
                == *(Formattable::fromUFormattable(defaultUFormattable.getAlias()))));
    assertTrue((UnicodeString)"comparing ufmt_open() with Formattable()",
               (defaultFormattable
                == *(Formattable::fromUFormattable(defaultUFormattable.getAlias()))));
    assertTrue((UnicodeString)"comparing Formattable() round tripped through UFormattable",
               (defaultFormattable
                == *(Formattable::fromUFormattable(defaultFormattable.toUFormattable()))));
    assertTrue((UnicodeString)"comparing &Formattable() round tripped through UFormattable",
               ((&defaultFormattable)
                == Formattable::fromUFormattable(defaultFormattable.toUFormattable())));
    assertFalse((UnicodeString)"comparing &Formattable() with ufmt_open()",
               ((&defaultFormattable)
                == Formattable::fromUFormattable(defaultUFormattable.getAlias())));
    testFormattableAsUFormattable(__FILE__, __LINE__, defaultFormattable);
  }
  // test some random Formattables
  {
    Formattable f(ucal_getNow(), Formattable::kIsDate);
    testFormattableAsUFormattable(__FILE__, __LINE__,  f);
  }
  {
    Formattable f((double)1.61803398874989484820); // golden ratio
    testFormattableAsUFormattable(__FILE__, __LINE__,  f);
  }
  {
    Formattable f((int64_t)80994231587905127LL); // weight of the moon, in kilotons http://solarsystem.nasa.gov/planets/profile.cfm?Display=Facts&Object=Moon
    testFormattableAsUFormattable(__FILE__, __LINE__,  f);
  }
  {
    Formattable f((int32_t)4); // random number, source: http://www.xkcd.com/221/
    testFormattableAsUFormattable(__FILE__, __LINE__,  f);
  }
  {
    Formattable f("Hello world."); // should be invariant?
    testFormattableAsUFormattable(__FILE__, __LINE__,  f);
  }
  {
    UErrorCode status2 = U_ZERO_ERROR;
    Formattable f(StringPiece("73476730924573500000000.0"), status2); // weight of the moon, kg
    assertSuccess("Constructing a StringPiece", status2);
    testFormattableAsUFormattable(__FILE__, __LINE__,  f);
  }
  {
    UErrorCode status2 = U_ZERO_ERROR;
    UObject *obj = new Locale();
    Formattable f(obj);
    assertSuccess("Constructing a Formattable from a default constructed Locale()", status2);
    testFormattableAsUFormattable(__FILE__, __LINE__,  f);
  }
  {
    const Formattable array[] = {
      Formattable(ucal_getNow(), Formattable::kIsDate),
      Formattable((int32_t)4),
      Formattable((double)1.234),
    };

    Formattable fa(array, 3);
    testFormattableAsUFormattable(__FILE__, __LINE__, fa);
  }
}

void NumberFormatTest::TestSignificantDigits(void) {
  double input[] = {
        0, 0,
        0.1, -0.1,
        123, -123,
        12345, -12345,
        123.45, -123.45,
        123.44501, -123.44501,
        0.001234, -0.001234,
        0.00000000123, -0.00000000123,
        0.0000000000000000000123, -0.0000000000000000000123,
        1.2, -1.2,
        0.0000000012344501, -0.0000000012344501,
        123445.01, -123445.01,
        12344501000000000000000000000000000.0, -12344501000000000000000000000000000.0,
    };
    const char* expected[] = {
        "0.00", "0.00",
        "0.100", "-0.100",
        "123", "-123",
        "12345", "-12345",
        "123.45", "-123.45",
        "123.45", "-123.45",
        "0.001234", "-0.001234",
        "0.00000000123", "-0.00000000123",
        "0.0000000000000000000123", "-0.0000000000000000000123",
        "1.20", "-1.20",
        "0.0000000012345", "-0.0000000012345",
        "123450", "-123450",
        "12345000000000000000000000000000000", "-12345000000000000000000000000000000",
    };

    UErrorCode status = U_ZERO_ERROR;
    Locale locale("en_US");
    LocalPointer<DecimalFormat> numberFormat(static_cast<DecimalFormat*>(
            NumberFormat::createInstance(locale, status)));
    CHECK_DATA(status,"NumberFormat::createInstance")

    numberFormat->setSignificantDigitsUsed(TRUE);
    numberFormat->setMinimumSignificantDigits(3);
    numberFormat->setMaximumSignificantDigits(5);
    numberFormat->setGroupingUsed(false);

    UnicodeString result;
    UnicodeString expectedResult;
    for (unsigned int i = 0; i < UPRV_LENGTHOF(input); ++i) {
        numberFormat->format(input[i], result);
        UnicodeString expectedResult(expected[i]);
        if (result != expectedResult) {
          errln((UnicodeString)"Expected: '" + expectedResult + "' got '" + result);
        }
        result.remove();
    }

    // Test for ICU-20063
    {
        DecimalFormat df({"en-us", status}, status);
        df.setSignificantDigitsUsed(TRUE);
        expect(df, 9.87654321, u"9.87654");
        df.setMaximumSignificantDigits(3);
        expect(df, 9.87654321, u"9.88");
        // setSignificantDigitsUsed with maxSig only
        df.setSignificantDigitsUsed(TRUE);
        expect(df, 9.87654321, u"9.88");
        df.setMinimumSignificantDigits(2);
        expect(df, 9, u"9.0");
        // setSignificantDigitsUsed with both minSig and maxSig
        df.setSignificantDigitsUsed(TRUE);
        expect(df, 9, u"9.0");
        // setSignificantDigitsUsed to false: should revert to fraction rounding
        df.setSignificantDigitsUsed(FALSE);
        expect(df, 9.87654321, u"9.876543");
        expect(df, 9, u"9");
        df.setSignificantDigitsUsed(TRUE);
        df.setMinimumSignificantDigits(2);
        expect(df, 9.87654321, u"9.87654");
        expect(df, 9, u"9.0");
        // setSignificantDigitsUsed with minSig only
        df.setSignificantDigitsUsed(TRUE);
        expect(df, 9.87654321, u"9.87654");
        expect(df, 9, u"9.0");
    }
}

void NumberFormatTest::TestShowZero() {
    UErrorCode status = U_ZERO_ERROR;
    Locale locale("en_US");
    LocalPointer<DecimalFormat> numberFormat(static_cast<DecimalFormat*>(
            NumberFormat::createInstance(locale, status)));
    CHECK_DATA(status, "NumberFormat::createInstance")

    numberFormat->setSignificantDigitsUsed(TRUE);
    numberFormat->setMaximumSignificantDigits(3);

    UnicodeString result;
    numberFormat->format(0.0, result);
    if (result != "0") {
        errln((UnicodeString)"Expected: 0, got " + result);
    }
}

void NumberFormatTest::TestBug9936() {
    UErrorCode status = U_ZERO_ERROR;
    Locale locale("en_US");
    LocalPointer<DecimalFormat> numberFormat(static_cast<DecimalFormat*>(
            NumberFormat::createInstance(locale, status)));
    if (U_FAILURE(status)) {
        dataerrln("File %s, Line %d: status = %s.\n", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    if (numberFormat->areSignificantDigitsUsed() == TRUE) {
        errln("File %s, Line %d: areSignificantDigitsUsed() was TRUE, expected FALSE.\n", __FILE__, __LINE__);
    }
    numberFormat->setSignificantDigitsUsed(TRUE);
    if (numberFormat->areSignificantDigitsUsed() == FALSE) {
        errln("File %s, Line %d: areSignificantDigitsUsed() was FALSE, expected TRUE.\n", __FILE__, __LINE__);
    }

    numberFormat->setSignificantDigitsUsed(FALSE);
    if (numberFormat->areSignificantDigitsUsed() == TRUE) {
        errln("File %s, Line %d: areSignificantDigitsUsed() was TRUE, expected FALSE.\n", __FILE__, __LINE__);
    }

    numberFormat->setMinimumSignificantDigits(3);
    if (numberFormat->areSignificantDigitsUsed() == FALSE) {
        errln("File %s, Line %d: areSignificantDigitsUsed() was FALSE, expected TRUE.\n", __FILE__, __LINE__);
    }

    numberFormat->setSignificantDigitsUsed(FALSE);
    numberFormat->setMaximumSignificantDigits(6);
    if (numberFormat->areSignificantDigitsUsed() == FALSE) {
        errln("File %s, Line %d: areSignificantDigitsUsed() was FALSE, expected TRUE.\n", __FILE__, __LINE__);
    }

}

void NumberFormatTest::TestParseNegativeWithFaLocale() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat *test = (DecimalFormat *) NumberFormat::createInstance("fa", status);
    CHECK_DATA(status, "NumberFormat::createInstance")
    test->setLenient(TRUE);
    Formattable af;
    ParsePosition ppos;
    UnicodeString value("\\u200e-0,5");
    value = value.unescape();
    test->parse(value, af, ppos);
    if (ppos.getIndex() == 0) {
        errln("Expected -0,5 to parse for Farsi.");
    }
    delete test;
}

void NumberFormatTest::TestParseNegativeWithAlternateMinusSign() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat *test = (DecimalFormat *) NumberFormat::createInstance("en", status);
    CHECK_DATA(status, "NumberFormat::createInstance")
    test->setLenient(TRUE);
    Formattable af;
    ParsePosition ppos;
    UnicodeString value("\\u208B0.5");
    value = value.unescape();
    test->parse(value, af, ppos);
    if (ppos.getIndex() == 0) {
        errln(UnicodeString("Expected ") + value + UnicodeString(" to parse."));
    }
    delete test;
}

void NumberFormatTest::TestCustomCurrencySignAndSeparator() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols custom(Locale::getUS(), status);
    CHECK(status, "DecimalFormatSymbols constructor");

    custom.setSymbol(DecimalFormatSymbols::kCurrencySymbol, "*");
    custom.setSymbol(DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol, "^");
    custom.setSymbol(DecimalFormatSymbols::kMonetarySeparatorSymbol, ":");

    UnicodeString pat(" #,##0.00");
    pat.insert(0, (UChar)0x00A4);

    DecimalFormat fmt(pat, custom, status);
    CHECK(status, "DecimalFormat constructor");

    UnicodeString numstr("* 1^234:56");
    expect2(fmt, (Formattable)((double)1234.56), numstr);
}

typedef struct {
    const char *   locale;
    UBool          lenient;
    UnicodeString  numString;
    double         value;
} SignsAndMarksItem;


void NumberFormatTest::TestParseSignsAndMarks() {
    const SignsAndMarksItem items[] = {
        // locale               lenient numString                                                       value
        { "en",                 FALSE,  CharsToUnicodeString("12"),                                      12 },
        { "en",                 TRUE,   CharsToUnicodeString("12"),                                      12 },
        { "en",                 FALSE,  CharsToUnicodeString("-23"),                                    -23 },
        { "en",                 TRUE,   CharsToUnicodeString("-23"),                                    -23 },
        { "en",                 TRUE,   CharsToUnicodeString("- 23"),                                   -23 },
        { "en",                 FALSE,  CharsToUnicodeString("\\u200E-23"),                             -23 },
        { "en",                 TRUE,   CharsToUnicodeString("\\u200E-23"),                             -23 },
        { "en",                 TRUE,   CharsToUnicodeString("\\u200E- 23"),                            -23 },

        { "en@numbers=arab",    FALSE,  CharsToUnicodeString("\\u0663\\u0664"),                          34 },
        { "en@numbers=arab",    TRUE,   CharsToUnicodeString("\\u0663\\u0664"),                          34 },
        { "en@numbers=arab",    FALSE,  CharsToUnicodeString("-\\u0664\\u0665"),                        -45 },
        { "en@numbers=arab",    TRUE,   CharsToUnicodeString("-\\u0664\\u0665"),                        -45 },
        { "en@numbers=arab",    TRUE,   CharsToUnicodeString("- \\u0664\\u0665"),                       -45 },
        { "en@numbers=arab",    FALSE,  CharsToUnicodeString("\\u200F-\\u0664\\u0665"),                 -45 },
        { "en@numbers=arab",    TRUE,   CharsToUnicodeString("\\u200F-\\u0664\\u0665"),                 -45 },
        { "en@numbers=arab",    TRUE,   CharsToUnicodeString("\\u200F- \\u0664\\u0665"),                -45 },

        { "en@numbers=arabext", FALSE,  CharsToUnicodeString("\\u06F5\\u06F6"),                          56 },
        { "en@numbers=arabext", TRUE,   CharsToUnicodeString("\\u06F5\\u06F6"),                          56 },
        { "en@numbers=arabext", FALSE,  CharsToUnicodeString("-\\u06F6\\u06F7"),                        -67 },
        { "en@numbers=arabext", TRUE,   CharsToUnicodeString("-\\u06F6\\u06F7"),                        -67 },
        { "en@numbers=arabext", TRUE,   CharsToUnicodeString("- \\u06F6\\u06F7"),                       -67 },
        { "en@numbers=arabext", FALSE,  CharsToUnicodeString("\\u200E-\\u200E\\u06F6\\u06F7"),          -67 },
        { "en@numbers=arabext", TRUE,   CharsToUnicodeString("\\u200E-\\u200E\\u06F6\\u06F7"),          -67 },
        { "en@numbers=arabext", TRUE,   CharsToUnicodeString("\\u200E-\\u200E \\u06F6\\u06F7"),         -67 },

        { "he",                 FALSE,  CharsToUnicodeString("12"),                                      12 },
        { "he",                 TRUE,   CharsToUnicodeString("12"),                                      12 },
        { "he",                 FALSE,  CharsToUnicodeString("-23"),                                    -23 },
        { "he",                 TRUE,   CharsToUnicodeString("-23"),                                    -23 },
        { "he",                 TRUE,   CharsToUnicodeString("- 23"),                                   -23 },
        { "he",                 FALSE,  CharsToUnicodeString("\\u200E-23"),                             -23 },
        { "he",                 TRUE,   CharsToUnicodeString("\\u200E-23"),                             -23 },
        { "he",                 TRUE,   CharsToUnicodeString("\\u200E- 23"),                            -23 },

        { "ar",                 FALSE,  CharsToUnicodeString("\\u0663\\u0664"),                          34 },
        { "ar",                 TRUE,   CharsToUnicodeString("\\u0663\\u0664"),                          34 },
        { "ar",                 FALSE,  CharsToUnicodeString("-\\u0664\\u0665"),                        -45 },
        { "ar",                 TRUE,   CharsToUnicodeString("-\\u0664\\u0665"),                        -45 },
        { "ar",                 TRUE,   CharsToUnicodeString("- \\u0664\\u0665"),                       -45 },
        { "ar",                 FALSE,  CharsToUnicodeString("\\u200F-\\u0664\\u0665"),                 -45 },
        { "ar",                 TRUE,   CharsToUnicodeString("\\u200F-\\u0664\\u0665"),                 -45 },
        { "ar",                 TRUE,   CharsToUnicodeString("\\u200F- \\u0664\\u0665"),                -45 },

        { "ar_MA",              FALSE,  CharsToUnicodeString("12"),                                      12 },
        { "ar_MA",              TRUE,   CharsToUnicodeString("12"),                                      12 },
        { "ar_MA",              FALSE,  CharsToUnicodeString("-23"),                                    -23 },
        { "ar_MA",              TRUE,   CharsToUnicodeString("-23"),                                    -23 },
        { "ar_MA",              TRUE,   CharsToUnicodeString("- 23"),                                   -23 },
        { "ar_MA",              FALSE,  CharsToUnicodeString("\\u200E-23"),                             -23 },
        { "ar_MA",              TRUE,   CharsToUnicodeString("\\u200E-23"),                             -23 },
        { "ar_MA",              TRUE,   CharsToUnicodeString("\\u200E- 23"),                            -23 },

        { "fa",                 FALSE,  CharsToUnicodeString("\\u06F5\\u06F6"),                          56 },
        { "fa",                 TRUE,   CharsToUnicodeString("\\u06F5\\u06F6"),                          56 },
        { "fa",                 FALSE,  CharsToUnicodeString("\\u2212\\u06F6\\u06F7"),                  -67 },
        { "fa",                 TRUE,   CharsToUnicodeString("\\u2212\\u06F6\\u06F7"),                  -67 },
        { "fa",                 TRUE,   CharsToUnicodeString("\\u2212 \\u06F6\\u06F7"),                 -67 },
        { "fa",                 FALSE,  CharsToUnicodeString("\\u200E\\u2212\\u200E\\u06F6\\u06F7"),    -67 },
        { "fa",                 TRUE,   CharsToUnicodeString("\\u200E\\u2212\\u200E\\u06F6\\u06F7"),    -67 },
        { "fa",                 TRUE,   CharsToUnicodeString("\\u200E\\u2212\\u200E \\u06F6\\u06F7"),   -67 },

        { "ps",                 FALSE,  CharsToUnicodeString("\\u06F5\\u06F6"),                          56 },
        { "ps",                 TRUE,   CharsToUnicodeString("\\u06F5\\u06F6"),                          56 },
        { "ps",                 FALSE,  CharsToUnicodeString("-\\u06F6\\u06F7"),                        -67 },
        { "ps",                 TRUE,   CharsToUnicodeString("-\\u06F6\\u06F7"),                        -67 },
        { "ps",                 TRUE,   CharsToUnicodeString("- \\u06F6\\u06F7"),                       -67 },
        { "ps",                 FALSE,  CharsToUnicodeString("\\u200E-\\u200E\\u06F6\\u06F7"),          -67 },
        { "ps",                 TRUE,   CharsToUnicodeString("\\u200E-\\u200E\\u06F6\\u06F7"),          -67 },
        { "ps",                 TRUE,   CharsToUnicodeString("\\u200E-\\u200E \\u06F6\\u06F7"),         -67 },
        { "ps",                 FALSE,  CharsToUnicodeString("-\\u200E\\u06F6\\u06F7"),                 -67 },
        { "ps",                 TRUE,   CharsToUnicodeString("-\\u200E\\u06F6\\u06F7"),                 -67 },
        { "ps",                 TRUE,   CharsToUnicodeString("-\\u200E \\u06F6\\u06F7"),                -67 },
        // terminator
        { NULL,                 0,      UnicodeString(""),                                                0 },
    };

    const SignsAndMarksItem * itemPtr;
    for (itemPtr = items; itemPtr->locale != NULL; itemPtr++ ) {
        UErrorCode status = U_ZERO_ERROR;
        NumberFormat *numfmt = NumberFormat::createInstance(Locale(itemPtr->locale), status);
        if (U_SUCCESS(status)) {
            numfmt->setLenient(itemPtr->lenient);
            Formattable fmtobj;
            ParsePosition ppos;
            numfmt->parse(itemPtr->numString, fmtobj, ppos);
            if (ppos.getIndex() == itemPtr->numString.length()) {
                double parsedValue = fmtobj.getDouble(status);
                if (U_FAILURE(status) || parsedValue != itemPtr->value) {
                    errln((UnicodeString)"FAIL: locale " + itemPtr->locale + ", lenient " + itemPtr->lenient + ", parse of \"" + itemPtr->numString + "\" gives value " + parsedValue);
                }
            } else {
                errln((UnicodeString)"FAIL: locale " + itemPtr->locale + ", lenient " + itemPtr->lenient + ", parse of \"" + itemPtr->numString + "\" gives position " + ppos.getIndex());
            }
        } else {
            dataerrln("FAIL: NumberFormat::createInstance for locale % gives error %s", itemPtr->locale, u_errorName(status));
        }
        delete numfmt;
    }
}

typedef struct {
  DecimalFormat::ERoundingMode mode;
  double value;
  UnicodeString expected;
} Test10419Data;


// Tests that rounding works right when fractional digits is set to 0.
void NumberFormatTest::Test10419RoundingWith0FractionDigits() {
    const Test10419Data items[] = {
        { DecimalFormat::kRoundCeiling, 1.488,  "2"},
        { DecimalFormat::kRoundDown, 1.588,  "1"},
        { DecimalFormat::kRoundFloor, 1.888,  "1"},
        { DecimalFormat::kRoundHalfDown, 1.5,  "1"},
        { DecimalFormat::kRoundHalfEven, 2.5,  "2"},
        { DecimalFormat::kRoundHalfUp, 2.5,  "3"},
        { DecimalFormat::kRoundUp, 1.5,  "2"},
    };
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> decfmt((DecimalFormat *) NumberFormat::createInstance(Locale("en_US"), status));
    if (U_FAILURE(status)) {
        dataerrln("Failure creating DecimalFormat %s", u_errorName(status));
        return;
    }
    for (int32_t i = 0; i < UPRV_LENGTHOF(items); ++i) {
        decfmt->setRoundingMode(items[i].mode);
        decfmt->setMaximumFractionDigits(0);
        UnicodeString actual;
        if (items[i].expected != decfmt->format(items[i].value, actual)) {
            errln("Expected " + items[i].expected + ", got " + actual);
        }
    }
}

void NumberFormatTest::Test10468ApplyPattern() {
    // Padding char of fmt is now 'a'
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat fmt("'I''ll'*a###.##", status);

    if (U_FAILURE(status)) {
        errcheckln(status, "DecimalFormat constructor failed - %s", u_errorName(status));
        return;
    }

    assertEquals("Padding character should be 'a'.", u"a", fmt.getPadCharacterString());

    // Padding char of fmt ought to be '*' since that is the default and no
    // explicit padding char is specified in the new pattern.
    fmt.applyPattern("AA#,##0.00ZZ", status);

    // Oops this still prints 'a' even though we changed the pattern.
    assertEquals("applyPattern did not clear padding character.", u" ", fmt.getPadCharacterString());
}

void NumberFormatTest::TestRoundingScientific10542() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat format("0.00E0", status);
    if (U_FAILURE(status)) {
        errcheckln(status, "DecimalFormat constructor failed - %s", u_errorName(status));
        return;
    }

    DecimalFormat::ERoundingMode roundingModes[] = {
            DecimalFormat::kRoundCeiling,
            DecimalFormat::kRoundDown,
            DecimalFormat::kRoundFloor,
            DecimalFormat::kRoundHalfDown,
            DecimalFormat::kRoundHalfEven,
            DecimalFormat::kRoundHalfUp,
            DecimalFormat::kRoundUp};
    const char *descriptions[] = {
            "Round Ceiling",
            "Round Down",
            "Round Floor",
            "Round half down",
            "Round half even",
            "Round half up",
            "Round up"};

    {
        double values[] = {-0.003006, -0.003005, -0.003004, 0.003014, 0.003015, 0.003016};
        // The order of these expected values correspond to the order of roundingModes and the order of values.
        const char *expected[] = {
                "-3.00E-3", "-3.00E-3", "-3.00E-3", "3.02E-3", "3.02E-3", "3.02E-3",
                "-3.00E-3", "-3.00E-3", "-3.00E-3", "3.01E-3", "3.01E-3", "3.01E-3",
                "-3.01E-3", "-3.01E-3", "-3.01E-3", "3.01E-3", "3.01E-3", "3.01E-3",
                "-3.01E-3", "-3.00E-3", "-3.00E-3", "3.01E-3", "3.01E-3", "3.02E-3",
                "-3.01E-3", "-3.00E-3", "-3.00E-3", "3.01E-3", "3.02E-3", "3.02E-3",
                "-3.01E-3", "-3.01E-3", "-3.00E-3", "3.01E-3", "3.02E-3", "3.02E-3",
                "-3.01E-3", "-3.01E-3", "-3.01E-3", "3.02E-3", "3.02E-3", "3.02E-3"};
        verifyRounding(
                format,
                values,
                expected,
                roundingModes,
                descriptions,
                UPRV_LENGTHOF(values),
                UPRV_LENGTHOF(roundingModes));
    }
    {
        double values[] = {-3006.0, -3005, -3004, 3014, 3015, 3016};
        // The order of these expected values correspond to the order of roundingModes and the order of values.
        const char *expected[] = {
                "-3.00E3", "-3.00E3", "-3.00E3", "3.02E3", "3.02E3", "3.02E3",
                "-3.00E3", "-3.00E3", "-3.00E3", "3.01E3", "3.01E3", "3.01E3",
                "-3.01E3", "-3.01E3", "-3.01E3", "3.01E3", "3.01E3", "3.01E3",
                "-3.01E3", "-3.00E3", "-3.00E3", "3.01E3", "3.01E3", "3.02E3",
                "-3.01E3", "-3.00E3", "-3.00E3", "3.01E3", "3.02E3", "3.02E3",
                "-3.01E3", "-3.01E3", "-3.00E3", "3.01E3", "3.02E3", "3.02E3",
                "-3.01E3", "-3.01E3", "-3.01E3", "3.02E3", "3.02E3", "3.02E3"};
        verifyRounding(
                format,
                values,
                expected,
                roundingModes,
                descriptions,
                UPRV_LENGTHOF(values),
                UPRV_LENGTHOF(roundingModes));
    }
/* Commented out for now until we decide how rounding to zero should work, +0 vs. -0
    {
        double values[] = {0.0, -0.0};
        // The order of these expected values correspond to the order of roundingModes and the order of values.
        const char *expected[] = {
                "0.00E0", "-0.00E0",
                "0.00E0", "-0.00E0",
                "0.00E0", "-0.00E0",
                "0.00E0", "-0.00E0",
                "0.00E0", "-0.00E0",
                "0.00E0", "-0.00E0",
                "0.00E0", "-0.00E0"};
        verifyRounding(
                format,
                values,
                expected,
                roundingModes,
                descriptions,
                UPRV_LENGTHOF(values),
                UPRV_LENGTHOF(roundingModes));
    }
*/
    {

        double values[] = {1e25, 1e25 + 1e15, 1e25 - 1e15};
        // The order of these expected values correspond to the order of roundingModes and the order of values.
        const char *expected[] = {
                "1.00E25", "1.01E25", "1.00E25",
                "1.00E25", "1.00E25", "9.99E24",
                "1.00E25", "1.00E25", "9.99E24",
                "1.00E25", "1.00E25", "1.00E25",
                "1.00E25", "1.00E25", "1.00E25",
                "1.00E25", "1.00E25", "1.00E25",
                "1.00E25", "1.01E25", "1.00E25"};
        verifyRounding(
                format,
                values,
                expected,
                roundingModes,
                descriptions,
                UPRV_LENGTHOF(values),
                UPRV_LENGTHOF(roundingModes));
        }
    {
        double values[] = {-1e25, -1e25 + 1e15, -1e25 - 1e15};
        // The order of these expected values correspond to the order of roundingModes and the order of values.
        const char *expected[] = {
                "-1.00E25", "-9.99E24", "-1.00E25",
                "-1.00E25", "-9.99E24", "-1.00E25",
                "-1.00E25", "-1.00E25", "-1.01E25",
                "-1.00E25", "-1.00E25", "-1.00E25",
                "-1.00E25", "-1.00E25", "-1.00E25",
                "-1.00E25", "-1.00E25", "-1.00E25",
                "-1.00E25", "-1.00E25", "-1.01E25"};
        verifyRounding(
                format,
                values,
                expected,
                roundingModes,
                descriptions,
                UPRV_LENGTHOF(values),
                UPRV_LENGTHOF(roundingModes));
        }
    {
        double values[] = {1e-25, 1e-25 + 1e-35, 1e-25 - 1e-35};
        // The order of these expected values correspond to the order of roundingModes and the order of values.
        const char *expected[] = {
                "1.00E-25", "1.01E-25", "1.00E-25",
                "1.00E-25", "1.00E-25", "9.99E-26",
                "1.00E-25", "1.00E-25", "9.99E-26",
                "1.00E-25", "1.00E-25", "1.00E-25",
                "1.00E-25", "1.00E-25", "1.00E-25",
                "1.00E-25", "1.00E-25", "1.00E-25",
                "1.00E-25", "1.01E-25", "1.00E-25"};
        verifyRounding(
                format,
                values,
                expected,
                roundingModes,
                descriptions,
                UPRV_LENGTHOF(values),
                UPRV_LENGTHOF(roundingModes));
        }
    {
        double values[] = {-1e-25, -1e-25 + 1e-35, -1e-25 - 1e-35};
        // The order of these expected values correspond to the order of roundingModes and the order of values.
        const char *expected[] = {
                "-1.00E-25", "-9.99E-26", "-1.00E-25",
                "-1.00E-25", "-9.99E-26", "-1.00E-25",
                "-1.00E-25", "-1.00E-25", "-1.01E-25",
                "-1.00E-25", "-1.00E-25", "-1.00E-25",
                "-1.00E-25", "-1.00E-25", "-1.00E-25",
                "-1.00E-25", "-1.00E-25", "-1.00E-25",
                "-1.00E-25", "-1.00E-25", "-1.01E-25"};
        verifyRounding(
                format,
                values,
                expected,
                roundingModes,
                descriptions,
                UPRV_LENGTHOF(values),
                UPRV_LENGTHOF(roundingModes));
    }
}

void NumberFormatTest::TestZeroScientific10547() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat fmt("0.00E0", status);
    if (!assertSuccess("Format creation", status)) {
        return;
    }
    UnicodeString out;
    fmt.format(-0.0, out);
    assertEquals("format", "-0.00E0", out, true);
}

void NumberFormatTest::verifyRounding(
        DecimalFormat& format,
        const double *values,
        const char * const *expected,
        const DecimalFormat::ERoundingMode *roundingModes,
        const char * const *descriptions,
        int32_t valueSize,
        int32_t roundingModeSize) {
    for (int32_t i = 0; i < roundingModeSize; ++i) {
        format.setRoundingMode(roundingModes[i]);
        for (int32_t j = 0; j < valueSize; j++) {
            UnicodeString currentExpected(expected[i * valueSize + j]);
            currentExpected = currentExpected.unescape();
            UnicodeString actual;
            format.format(values[j], actual);
            if (currentExpected != actual) {
                dataerrln("For %s value %f, expected '%s', got '%s'",
                          descriptions[i], values[j], CStr(currentExpected)(), CStr(actual)());
            }
        }
    }
}

void NumberFormatTest::TestAccountingCurrency() {
    UErrorCode status = U_ZERO_ERROR;
    UNumberFormatStyle style = UNUM_CURRENCY_ACCOUNTING;

    expect(NumberFormat::createInstance("en_US", style, status),
        (Formattable)(double)1234.5, "$1,234.50", TRUE, status);
    expect(NumberFormat::createInstance("en_US", style, status),
        (Formattable)(double)-1234.5, "($1,234.50)", TRUE, status);
    expect(NumberFormat::createInstance("en_US", style, status),
        (Formattable)(double)0, "$0.00", TRUE, status);
    expect(NumberFormat::createInstance("en_US", style, status),
        (Formattable)(double)-0.2, "($0.20)", TRUE, status);
    expect(NumberFormat::createInstance("ja_JP", style, status),
        (Formattable)(double)10000, UnicodeString("\\uFFE510,000").unescape(), TRUE, status);
    expect(NumberFormat::createInstance("ja_JP", style, status),
        (Formattable)(double)-1000.5, UnicodeString("(\\uFFE51,000)").unescape(), FALSE, status);
    expect(NumberFormat::createInstance("de_DE", style, status),
        (Formattable)(double)-23456.7, UnicodeString("-23.456,70\\u00A0\\u20AC").unescape(), TRUE, status);
}

// for #5186
void NumberFormatTest::TestEquality() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols(Locale("root"), status);
    if (U_FAILURE(status)) {
    	dataerrln("Fail: can't create DecimalFormatSymbols for root");
    	return;
    }
    UnicodeString pattern("#,##0.###");
    DecimalFormat fmtBase(pattern, symbols, status);
    if (U_FAILURE(status)) {
    	dataerrln("Fail: can't create DecimalFormat using root symbols");
    	return;
    }

    DecimalFormat* fmtClone = (DecimalFormat*)fmtBase.clone();
    fmtClone->setFormatWidth(fmtBase.getFormatWidth() + 32);
    if (*fmtClone == fmtBase) {
        errln("Error: DecimalFormat == does not distinguish objects that differ only in FormatWidth");
    }
    delete fmtClone;
}

void NumberFormatTest::TestCurrencyUsage() {
    double agent = 123.567;

    UErrorCode status;
    DecimalFormat *fmt;

    // compare the Currency and Currency Cash Digits
    // Note that as of CLDR 26:
    // * TWD and PKR switched from 0 decimals to 2; ISK still has 0, so change test to that
    // * CAD rounds to .05 in cash mode only
    // 1st time for getter/setter, 2nd time for factory method
    Locale enUS_ISK("en_US@currency=ISK");

    for(int i=0; i<2; i++){
        status = U_ZERO_ERROR;
        if(i == 0){
            fmt = (DecimalFormat *) NumberFormat::createInstance(enUS_ISK, UNUM_CURRENCY, status);
            if (assertSuccess("en_US@currency=ISK/CURRENCY", status, TRUE) == FALSE) {
                continue;
            }

            UnicodeString original;
            fmt->format(agent,original);
            assertEquals("Test Currency Usage 1", u"ISK\u00A0124", original);

            // test the getter here
            UCurrencyUsage curUsage = fmt->getCurrencyUsage();
            assertEquals("Test usage getter - standard", (int32_t)curUsage, (int32_t)UCURR_USAGE_STANDARD);

            fmt->setCurrencyUsage(UCURR_USAGE_CASH, &status);
        }else{
            fmt = (DecimalFormat *) NumberFormat::createInstance(enUS_ISK, UNUM_CASH_CURRENCY, status);
            if (assertSuccess("en_US@currency=ISK/CASH", status, TRUE) == FALSE) {
                continue;
            }
        }

        // must be usage = cash
        UCurrencyUsage curUsage = fmt->getCurrencyUsage();
        assertEquals("Test usage getter - cash", (int32_t)curUsage, (int32_t)UCURR_USAGE_CASH);

        UnicodeString cash_currency;
        fmt->format(agent,cash_currency);
        assertEquals("Test Currency Usage 2", u"ISK\u00A0124", cash_currency);
        delete fmt;
    }

    // compare the Currency and Currency Cash Rounding
    // 1st time for getter/setter, 2nd time for factory method
    Locale enUS_CAD("en_US@currency=CAD");
    for(int i=0; i<2; i++){
        status = U_ZERO_ERROR;
        if(i == 0){
            fmt = (DecimalFormat *) NumberFormat::createInstance(enUS_CAD, UNUM_CURRENCY, status);
            if (assertSuccess("en_US@currency=CAD/CURRENCY", status, TRUE) == FALSE) {
                continue;
            }

            UnicodeString original_rounding;
            fmt->format(agent, original_rounding);
            assertEquals("Test Currency Usage 3", u"CA$123.57", original_rounding);
            fmt->setCurrencyUsage(UCURR_USAGE_CASH, &status);
        }else{
            fmt = (DecimalFormat *) NumberFormat::createInstance(enUS_CAD, UNUM_CASH_CURRENCY, status);
            if (assertSuccess("en_US@currency=CAD/CASH", status, TRUE) == FALSE) {
                continue;
            }
        }

        UnicodeString cash_rounding_currency;
        fmt->format(agent, cash_rounding_currency);
        assertEquals("Test Currency Usage 4", u"CA$123.55", cash_rounding_currency);
        delete fmt;
    }

    // Test the currency change
    // 1st time for getter/setter, 2nd time for factory method
    const UChar CUR_PKR[] = {0x50, 0x4B, 0x52, 0};
    for(int i=0; i<2; i++){
        status = U_ZERO_ERROR;
        if(i == 0){
            fmt = (DecimalFormat *) NumberFormat::createInstance(enUS_CAD, UNUM_CURRENCY, status);
            if (assertSuccess("en_US@currency=CAD/CURRENCY", status, TRUE) == FALSE) {
                continue;
            }
            fmt->setCurrencyUsage(UCURR_USAGE_CASH, &status);
        }else{
            fmt = (DecimalFormat *) NumberFormat::createInstance(enUS_CAD, UNUM_CASH_CURRENCY, status);
            if (assertSuccess("en_US@currency=CAD/CASH", status, TRUE) == FALSE) {
                continue;
            }
        }

        UnicodeString cur_original;
        fmt->setCurrencyUsage(UCURR_USAGE_STANDARD, &status);
        fmt->format(agent, cur_original);
        assertEquals("Test Currency Usage 5", u"CA$123.57", cur_original);

        fmt->setCurrency(CUR_PKR, status);
        assertSuccess("Set currency to PKR", status);

        UnicodeString PKR_changed;
        fmt->format(agent, PKR_changed);
        assertEquals("Test Currency Usage 6", u"PKR\u00A0123.57", PKR_changed);
        delete fmt;
    }
}


// Check the constant MAX_INT64_IN_DOUBLE.
// The value should convert to a double with no loss of precision.
// A failure may indicate a platform with a different double format, requiring
// a revision to the constant.
//
// Note that this is actually hard to test, because the language standard gives
//  compilers considerable flexibility to do unexpected things with rounding and
//  with overflow in simple int to/from float conversions. Some compilers will completely optimize
//  away a simple round-trip conversion from int64_t -> double -> int64_t.

void NumberFormatTest::TestDoubleLimit11439() {
    char  buf[50];
    for (int64_t num = MAX_INT64_IN_DOUBLE-10; num<=MAX_INT64_IN_DOUBLE; num++) {
        sprintf(buf, "%lld", (long long)num);
        double fNum = 0.0;
        sscanf(buf, "%lf", &fNum);
        int64_t rtNum = static_cast<int64_t>(fNum);
        if (num != rtNum) {
            errln("%s:%d MAX_INT64_IN_DOUBLE test, %lld did not round trip. Got %lld", __FILE__, __LINE__, (long long)num, (long long)rtNum);
            return;
        }
    }
    for (int64_t num = -MAX_INT64_IN_DOUBLE+10; num>=-MAX_INT64_IN_DOUBLE; num--) {
        sprintf(buf, "%lld", (long long)num);
        double fNum = 0.0;
        sscanf(buf, "%lf", &fNum);
        int64_t rtNum = static_cast<int64_t>(fNum);
        if (num != rtNum) {
            errln("%s:%d MAX_INT64_IN_DOUBLE test, %lld did not round trip. Got %lld", __FILE__, __LINE__, (long long)num, (long long)rtNum);
            return;
        }
    }
}

void NumberFormatTest::TestGetAffixes() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols sym("en_US", status);
    UnicodeString pattern("\\u00a4\\u00a4\\u00a4 0.00 %\\u00a4\\u00a4");
    pattern = pattern.unescape();
    DecimalFormat fmt(pattern, sym, status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        return;
    }
    UnicodeString affixStr;
    assertEquals("", "US dollars ", fmt.getPositivePrefix(affixStr));
    assertEquals("", " %USD", fmt.getPositiveSuffix(affixStr));
    assertEquals("", "-US dollars ", fmt.getNegativePrefix(affixStr));
    assertEquals("", " %USD", fmt.getNegativeSuffix(affixStr));

    // Test equality with affixes. set affix methods can't capture special
    // characters which is why equality should fail.
    {
        DecimalFormat fmtCopy(fmt);
        assertTrue("", fmt == fmtCopy);
        UnicodeString someAffix;
        fmtCopy.setPositivePrefix(fmtCopy.getPositivePrefix(someAffix));
        assertTrue("", fmt != fmtCopy);
    }
    {
        DecimalFormat fmtCopy(fmt);
        assertTrue("", fmt == fmtCopy);
        UnicodeString someAffix;
        fmtCopy.setPositiveSuffix(fmtCopy.getPositiveSuffix(someAffix));
        assertTrue("", fmt != fmtCopy);
    }
    {
        DecimalFormat fmtCopy(fmt);
        assertTrue("", fmt == fmtCopy);
        UnicodeString someAffix;
        fmtCopy.setNegativePrefix(fmtCopy.getNegativePrefix(someAffix));
        assertTrue("", fmt != fmtCopy);
    }
    {
        DecimalFormat fmtCopy(fmt);
        assertTrue("", fmt == fmtCopy);
        UnicodeString someAffix;
        fmtCopy.setNegativeSuffix(fmtCopy.getNegativeSuffix(someAffix));
        assertTrue("", fmt != fmtCopy);
    }
    fmt.setPositivePrefix("Don't");
    fmt.setPositiveSuffix("do");
    UnicodeString someAffix("be''eet\\u00a4\\u00a4\\u00a4 it.");
    someAffix = someAffix.unescape();
    fmt.setNegativePrefix(someAffix);
    fmt.setNegativeSuffix("%");
    assertEquals("", "Don't", fmt.getPositivePrefix(affixStr));
    assertEquals("", "do", fmt.getPositiveSuffix(affixStr));
    assertEquals("", someAffix, fmt.getNegativePrefix(affixStr));
    assertEquals("", "%", fmt.getNegativeSuffix(affixStr));
}

void NumberFormatTest::TestToPatternScientific11648() {
    UErrorCode status = U_ZERO_ERROR;
    Locale en("en");
    DecimalFormatSymbols sym(en, status);
    DecimalFormat fmt("0.00", sym, status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        return;
    }
    fmt.setScientificNotation(TRUE);
    UnicodeString pattern;
    assertEquals("", "0.00E0", fmt.toPattern(pattern));
    DecimalFormat fmt2(pattern, sym, status);
    assertSuccess("", status);
}

void NumberFormatTest::TestBenchmark() {
/*
    UErrorCode status = U_ZERO_ERROR;
    Locale en("en");
    DecimalFormatSymbols sym(en, status);
    DecimalFormat fmt("0.0000000", new DecimalFormatSymbols(sym), status);
//    DecimalFormat fmt("0.00000E0", new DecimalFormatSymbols(sym), status);
//    DecimalFormat fmt("0", new DecimalFormatSymbols(sym), status);
    FieldPosition fpos(FieldPosition::DONT_CARE);
    clock_t start = clock();
    for (int32_t i = 0; i < 1000000; ++i) {
        UnicodeString append;
        fmt.format(3.0, append, fpos, status);
//        fmt.format(4.6692016, append, fpos, status);
//        fmt.format(1234567.8901, append, fpos, status);
//        fmt.format(2.99792458E8, append, fpos, status);
//        fmt.format(31, append);
    }
    errln("Took %f", (double) (clock() - start) / CLOCKS_PER_SEC);
    assertSuccess("", status);

    UErrorCode status = U_ZERO_ERROR;
    MessageFormat fmt("{0, plural, one {I have # friend.} other {I have # friends.}}", status);
    FieldPosition fpos(FieldPosition::DONT_CARE);
    Formattable one(1.0);
    Formattable three(3.0);
    clock_t start = clock();
    for (int32_t i = 0; i < 500000; ++i) {
        UnicodeString append;
        fmt.format(&one, 1, append, fpos, status);
        UnicodeString append2;
        fmt.format(&three, 1, append2, fpos, status);
    }
    errln("Took %f", (double) (clock() - start) / CLOCKS_PER_SEC);
    assertSuccess("", status);

    UErrorCode status = U_ZERO_ERROR;
    Locale en("en");
    Measure measureC(23, MeasureUnit::createCelsius(status), status);
    MeasureFormat fmt(en, UMEASFMT_WIDTH_WIDE, status);
    FieldPosition fpos(FieldPosition::DONT_CARE);
    clock_t start = clock();
    for (int32_t i = 0; i < 1000000; ++i) {
        UnicodeString appendTo;
        fmt.formatMeasures(
                &measureC, 1, appendTo, fpos, status);
    }
    errln("Took %f", (double) (clock() - start) / CLOCKS_PER_SEC);
    assertSuccess("", status);
*/
}

void NumberFormatTest::TestFractionalDigitsForCurrency() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<NumberFormat> fmt(NumberFormat::createCurrencyInstance("en", status));
    if (U_FAILURE(status)) {
        dataerrln("Error creating NumberFormat - %s", u_errorName(status));
        return;
    }
    UChar JPY[] = {0x4A, 0x50, 0x59, 0x0};
    fmt->setCurrency(JPY, status);
    if (!assertSuccess("", status)) {
        return;
    }
    assertEquals("", 0, fmt->getMaximumFractionDigits());
}


void NumberFormatTest::TestFormatCurrencyPlural() {
    UErrorCode status = U_ZERO_ERROR;
    Locale locale = Locale::createCanonical("en_US");
    NumberFormat *fmt = NumberFormat::createInstance(locale, UNUM_CURRENCY_PLURAL, status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating NumberFormat - %s", u_errorName(status));
        return;
    }
   UnicodeString formattedNum;
   fmt->format(11234.567, formattedNum, NULL, status);
   assertEquals("", "11,234.57 US dollars", formattedNum);
   delete fmt;
}

void NumberFormatTest::TestCtorApplyPatternDifference() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols sym("en_US", status);
    UnicodeString pattern("\\u00a40");
    DecimalFormat fmt(pattern.unescape(), sym, status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating DecimalFormat - %s", u_errorName(status));
        return;
    }
    UnicodeString result;
    assertEquals(
            "ctor favors precision of currency",
            "$5.00",
            fmt.format((double)5, result));
    result.remove();
    fmt.applyPattern(pattern.unescape(), status);
    assertEquals(
            "applyPattern favors precision of pattern",
            "$5",
            fmt.format((double)5, result));
}

void NumberFormatTest::Test11868() {
    double posAmt = 34.567;
    double negAmt = -9876.543;

    Locale selectedLocale("en_US");
    UErrorCode status = U_ZERO_ERROR;

    UnicodeString result;
    FieldPosition fpCurr(UNUM_CURRENCY_FIELD);
    LocalPointer<NumberFormat> fmt(
            NumberFormat::createInstance(
                    selectedLocale, UNUM_CURRENCY_PLURAL, status));
    if (!assertSuccess("Format creation", status)) {
        return;
    }
    fmt->format(posAmt, result, fpCurr, status);
    assertEquals("", "34.57 US dollars", result);
    assertEquals("begin index", 6, fpCurr.getBeginIndex());
    assertEquals("end index", 16, fpCurr.getEndIndex());

    // Test field position iterator
    {
        NumberFormatTest_Attributes attributes[] = {
                {UNUM_INTEGER_FIELD, 0, 2},
                {UNUM_DECIMAL_SEPARATOR_FIELD, 2, 3},
                {UNUM_FRACTION_FIELD, 3, 5},
                {UNUM_CURRENCY_FIELD, 6, 16},
                {0, -1, 0}};
        UnicodeString result;
        FieldPositionIterator iter;
        fmt->format(posAmt, result, &iter, status);
        assertEquals("", "34.57 US dollars", result);
        verifyFieldPositionIterator(attributes, iter);
    }

    result.remove();
    fmt->format(negAmt, result, fpCurr, status);
    assertEquals("", "-9,876.54 US dollars", result);
    assertEquals("begin index", 10, fpCurr.getBeginIndex());
    assertEquals("end index", 20, fpCurr.getEndIndex());

    // Test field position iterator
    {
        NumberFormatTest_Attributes attributes[] = {
                {UNUM_SIGN_FIELD, 0, 1},
                {UNUM_GROUPING_SEPARATOR_FIELD, 2, 3},
                {UNUM_INTEGER_FIELD, 1, 6},
                {UNUM_DECIMAL_SEPARATOR_FIELD, 6, 7},
                {UNUM_FRACTION_FIELD, 7, 9},
                {UNUM_CURRENCY_FIELD, 10, 20},
                {0, -1, 0}};
        UnicodeString result;
        FieldPositionIterator iter;
        fmt->format(negAmt, result, &iter, status);
        assertEquals("", "-9,876.54 US dollars", result);
        verifyFieldPositionIterator(attributes, iter);
    }
}

void NumberFormatTest::Test10727_RoundingZero() {
    IcuTestErrorCode status(*this, "Test10727_RoundingZero");
    DecimalQuantity dq;
    dq.setToDouble(-0.0);
    assertTrue("", dq.isNegative());
    dq.roundToMagnitude(0, UNUM_ROUND_HALFEVEN, status);
    assertTrue("", dq.isNegative());
}

void NumberFormatTest::Test11739_ParseLongCurrency() {
    IcuTestErrorCode status(*this, "Test11739_ParseLongCurrency");
    LocalPointer<NumberFormat> nf(NumberFormat::createCurrencyInstance("sr_BA", status));
    if (status.errDataIfFailureAndReset()) { return; }
    ((DecimalFormat*) nf.getAlias())->applyPattern(u"#,##0.0 ¤¤¤", status);
    ParsePosition ppos(0);
    LocalPointer<CurrencyAmount> result(nf->parseCurrency(u"1.500 амерички долар", ppos));
    assertEquals("Should parse to 1500 USD", -1, ppos.getErrorIndex());
    if (ppos.getErrorIndex() != -1) {
        return;
    }
    assertEquals("Should parse to 1500 USD", 1500LL, result->getNumber().getInt64(status));
    assertEquals("Should parse to 1500 USD", u"USD", result->getISOCurrency());
}

void NumberFormatTest::Test13035_MultiCodePointPaddingInPattern() {
    IcuTestErrorCode status(*this, "Test13035_MultiCodePointPaddingInPattern");
    DecimalFormat df(u"a*'நி'###0b", status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    UnicodeString result;
    df.format(12, result.remove());
    // TODO(13034): Re-enable this test when support is added in ICU4C.
    //assertEquals("Multi-codepoint padding should not be split", u"aநிநி12b", result);
    df = DecimalFormat(u"a*\U0001F601###0b", status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    result = df.format(12, result.remove());
    assertEquals("Single-codepoint padding should not be split", u"a\U0001F601\U0001F60112b", result, true);
    df = DecimalFormat(u"a*''###0b", status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    result = df.format(12, result.remove());
    assertEquals("Quote should be escapable in padding syntax", "a''12b", result, true);
}

void NumberFormatTest::Test13737_ParseScientificStrict() {
    IcuTestErrorCode status(*this, "Test13737_ParseScientificStrict");
    LocalPointer<NumberFormat> df(NumberFormat::createScientificInstance("en", status), status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) {return;}
    df->setLenient(FALSE);
    // Parse Test
    expect(*df, u"1.2", 1.2);
}

void NumberFormatTest::Test11376_getAndSetPositivePrefix() {
    {
        const UChar USD[] = {0x55, 0x53, 0x44, 0x0};
        UErrorCode status = U_ZERO_ERROR;
        LocalPointer<NumberFormat> fmt(
                NumberFormat::createCurrencyInstance("en", status));
        if (!assertSuccess("", status)) {
            return;
        }
        DecimalFormat *dfmt = (DecimalFormat *) fmt.getAlias();
        dfmt->setCurrency(USD);
        UnicodeString result;

        // This line should be a no-op. I am setting the positive prefix
        // to be the same thing it was before.
        dfmt->setPositivePrefix(dfmt->getPositivePrefix(result));

        UnicodeString appendTo;
        assertEquals("", "$3.78", dfmt->format(3.78, appendTo, status));
        assertSuccess("", status);
    }
    {
        const UChar USD[] = {0x55, 0x53, 0x44, 0x0};
        UErrorCode status = U_ZERO_ERROR;
        LocalPointer<NumberFormat> fmt(
                NumberFormat::createInstance("en", UNUM_CURRENCY_PLURAL, status));
        if (!assertSuccess("", status)) {
            return;
        }
        DecimalFormat *dfmt = (DecimalFormat *) fmt.getAlias();
        UnicodeString result;
        assertEquals("", u" (unknown currency)", dfmt->getPositiveSuffix(result));
        dfmt->setCurrency(USD);

        // getPositiveSuffix() always returns the suffix for the
        // "other" plural category
        assertEquals("", " US dollars", dfmt->getPositiveSuffix(result));
        UnicodeString appendTo;
        assertEquals("", "3.78 US dollars", dfmt->format(3.78, appendTo, status));
        assertEquals("", " US dollars", dfmt->getPositiveSuffix(result));
        dfmt->setPositiveSuffix("booya");
        appendTo.remove();
        assertEquals("", "3.78booya", dfmt->format(3.78, appendTo, status));
        assertEquals("", "booya", dfmt->getPositiveSuffix(result));
    }
}

void NumberFormatTest::Test11475_signRecognition() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols sym("en", status);
    UnicodeString result;
    {
        DecimalFormat fmt("+0.00", sym, status);
        if (!assertSuccess("", status)) {
            return;
        }
        NumberFormatTest_Attributes attributes[] = {
                {UNUM_SIGN_FIELD, 0, 1},
                {UNUM_INTEGER_FIELD, 1, 2},
                {UNUM_DECIMAL_SEPARATOR_FIELD, 2, 3},
                {UNUM_FRACTION_FIELD, 3, 5},
                {0, -1, 0}};
        UnicodeString result;
        FieldPositionIterator iter;
        fmt.format(2.3, result, &iter, status);
        assertEquals("", "+2.30", result);
        verifyFieldPositionIterator(attributes, iter);
    }
    {
        DecimalFormat fmt("++0.00+;-(#)--", sym, status);
        if (!assertSuccess("", status)) {
            return;
        }
        {
            NumberFormatTest_Attributes attributes[] = {
                    {UNUM_SIGN_FIELD, 0, 2},
                    {UNUM_INTEGER_FIELD, 2, 3},
                    {UNUM_DECIMAL_SEPARATOR_FIELD, 3, 4},
                    {UNUM_FRACTION_FIELD, 4, 6},
                    {UNUM_SIGN_FIELD, 6, 7},
                    {0, -1, 0}};
            UnicodeString result;
            FieldPositionIterator iter;
            fmt.format(2.3, result, &iter, status);
            assertEquals("", "++2.30+", result);
            verifyFieldPositionIterator(attributes, iter);
        }
        {
            NumberFormatTest_Attributes attributes[] = {
                    {UNUM_SIGN_FIELD, 0, 1},
                    {UNUM_INTEGER_FIELD, 2, 3},
                    {UNUM_DECIMAL_SEPARATOR_FIELD, 3, 4},
                    {UNUM_FRACTION_FIELD, 4, 6},
                    {UNUM_SIGN_FIELD, 7, 9},
                    {0, -1, 0}};
            UnicodeString result;
            FieldPositionIterator iter;
            fmt.format(-2.3, result, &iter, status);
            assertEquals("", "-(2.30)--", result);
            verifyFieldPositionIterator(attributes, iter);
        }
    }
}

void NumberFormatTest::Test11640_getAffixes() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols("en_US", status);
    if (!assertSuccess("", status)) {
        return;
    }
    UnicodeString pattern("\\u00a4\\u00a4\\u00a4 0.00 %\\u00a4\\u00a4");
    pattern = pattern.unescape();
    DecimalFormat fmt(pattern, symbols, status);
    if (!assertSuccess("", status)) {
        return;
    }
    UnicodeString affixStr;
    assertEquals("", "US dollars ", fmt.getPositivePrefix(affixStr));
    assertEquals("", " %USD", fmt.getPositiveSuffix(affixStr));
    assertEquals("", "-US dollars ", fmt.getNegativePrefix(affixStr));
    assertEquals("", " %USD", fmt.getNegativeSuffix(affixStr));
}

void NumberFormatTest::Test11649_toPatternWithMultiCurrency() {
    UnicodeString pattern("\\u00a4\\u00a4\\u00a4 0.00");
    pattern = pattern.unescape();
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat fmt(pattern, status);
    if (!assertSuccess("", status)) {
        return;
    }
    static UChar USD[] = {0x55, 0x53, 0x44, 0x0};
    fmt.setCurrency(USD);
    UnicodeString appendTo;

    assertEquals("", "US dollars 12.34", fmt.format(12.34, appendTo));

    UnicodeString topattern;
    fmt.toPattern(topattern);
    DecimalFormat fmt2(topattern, status);
    if (!assertSuccess("", status)) {
        return;
    }
    fmt2.setCurrency(USD);

    appendTo.remove();
    assertEquals("", "US dollars 12.34", fmt2.format(12.34, appendTo));
}

void NumberFormatTest::Test13327_numberingSystemBufferOverflow() {
    UErrorCode status = U_ZERO_ERROR;
    for (int runId = 0; runId < 2; runId++) {
        // Construct a locale string with a very long "numbers" value.
        // The first time, make the value length exactly equal to ULOC_KEYWORDS_CAPACITY.
        // The second time, make it exceed ULOC_KEYWORDS_CAPACITY.
        int extraLength = (runId == 0) ? 0 : 5;

        CharString localeId("en@numbers=", status);
        for (int i = 0; i < ULOC_KEYWORDS_CAPACITY + extraLength; i++) {
            localeId.append('x', status);
        }
        assertSuccess("Constructing locale string", status);
        Locale locale(localeId.data());

        LocalPointer<NumberingSystem> ns(NumberingSystem::createInstance(locale, status));
        assertFalse("Should not be null", ns.getAlias() == nullptr);
        assertSuccess("Should create with no error", status);
    }
}

void NumberFormatTest::Test13391_chakmaParsing() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DecimalFormat> df(dynamic_cast<DecimalFormat*>(
        NumberFormat::createInstance(Locale("ccp"), status)));
    if (df == nullptr) {
        dataerrln("%s %d Chakma df is null",  __FILE__, __LINE__);
        return;
    }
    const UChar* expected = u"\U00011137\U00011138,\U00011139\U0001113A\U0001113B";
    UnicodeString actual;
    df->format(12345, actual, status);
    assertSuccess("Should not fail when formatting in ccp", status);
    assertEquals("Should produce expected output in ccp", expected, actual);

    Formattable result;
    df->parse(expected, result, status);
    assertSuccess("Should not fail when parsing in ccp", status);
    assertEquals("Should parse to 12345 in ccp", 12345, result);

    const UChar* expectedScientific = u"\U00011137.\U00011139E\U00011138";
    UnicodeString actualScientific;
    df.adoptInstead(static_cast<DecimalFormat*>(
        NumberFormat::createScientificInstance(Locale("ccp"), status)));
    df->format(130, actualScientific, status);
    assertSuccess("Should not fail when formatting scientific in ccp", status);
    assertEquals("Should produce expected scientific output in ccp",
        expectedScientific, actualScientific);

    Formattable resultScientific;
    df->parse(expectedScientific, resultScientific, status);
    assertSuccess("Should not fail when parsing scientific in ccp", status);
    assertEquals("Should parse scientific to 130 in ccp", 130, resultScientific);
}


void NumberFormatTest::verifyFieldPositionIterator(
        NumberFormatTest_Attributes *expected, FieldPositionIterator &iter) {
    int32_t idx = 0;
    FieldPosition fp;
    while (iter.next(fp)) {
        if (expected[idx].spos == -1) {
            errln("Iterator should have ended. got %d", fp.getField());
            return;
        }
        assertEquals("id", expected[idx].id, fp.getField());
        assertEquals("start", expected[idx].spos, fp.getBeginIndex());
        assertEquals("end", expected[idx].epos, fp.getEndIndex());
        ++idx;
    }
    if (expected[idx].spos != -1) {
        errln("Premature end of iterator. expected %d", expected[idx].id);
    }
}

void NumberFormatTest::Test11735_ExceptionIssue() {
    IcuTestErrorCode status(*this, "Test11735_ExceptionIssue");
    Locale enLocale("en");
    DecimalFormatSymbols symbols(enLocale, status);
    if (status.isSuccess()) {
        DecimalFormat fmt("0", symbols, status);
        assertSuccess("Fail: Construct DecimalFormat formatter", status, true, __FILE__, __LINE__);
        ParsePosition ppos(0);
        fmt.parseCurrency("53.45", ppos);  // NPE thrown here in ICU4J.
        assertEquals("Issue11735 ppos", 0, ppos.getIndex());
    }
}

void NumberFormatTest::Test11035_FormatCurrencyAmount() {
    UErrorCode status = U_ZERO_ERROR;
    double amount = 12345.67;
    const char16_t* expected = u"12,345$67 ​";

    // Test two ways to set a currency via API

    Locale loc1 = Locale("pt_PT");
    LocalPointer<NumberFormat> fmt1(NumberFormat::createCurrencyInstance("loc1", status),
                                    status);
    if (U_FAILURE(status)) {
      dataerrln("%s %d NumberFormat instance fmt1 is null",  __FILE__, __LINE__);
      return;
    }
    fmt1->setCurrency(u"PTE", status);
    assertSuccess("Setting currency on fmt1", status);
    UnicodeString actualSetCurrency;
    fmt1->format(amount, actualSetCurrency);

    Locale loc2 = Locale("pt_PT@currency=PTE");
    LocalPointer<NumberFormat> fmt2(NumberFormat::createCurrencyInstance(loc2, status));
    assertSuccess("Creating fmt2", status);
    UnicodeString actualLocaleString;
    fmt2->format(amount, actualLocaleString);

    // TODO: The following test will fail until DecimalFormat wraps NumberFormatter.
    if (!logKnownIssue("13574")) {
        assertEquals("Custom Currency Pattern, Set Currency", expected, actualSetCurrency);
    }
}

void NumberFormatTest::Test11318_DoubleConversion() {
    IcuTestErrorCode status(*this, "Test11318_DoubleConversion");
    LocalPointer<NumberFormat> nf(NumberFormat::createInstance("en", status), status);
    if (U_FAILURE(status)) {
      dataerrln("%s %d Error in NumberFormat instance creation",  __FILE__, __LINE__);
      return;
    }
    nf->setMaximumFractionDigits(40);
    nf->setMaximumIntegerDigits(40);
    UnicodeString appendTo;
    nf->format(999999999999999.9, appendTo);
    assertEquals("Should render all digits", u"999,999,999,999,999.9", appendTo);
}

void NumberFormatTest::TestParsePercentRegression() {
    IcuTestErrorCode status(*this, "TestParsePercentRegression");
    LocalPointer<DecimalFormat> df1((DecimalFormat*) NumberFormat::createInstance("en", status), status);
    LocalPointer<DecimalFormat> df2((DecimalFormat*) NumberFormat::createPercentInstance("en", status), status);
    if (status.isFailure()) {return; }
    df1->setLenient(TRUE);
    df2->setLenient(TRUE);

    {
        ParsePosition ppos;
        Formattable result;
        df1->parse("50%", result, ppos);
        assertEquals("df1 should accept a number but not the percent sign", 2, ppos.getIndex());
        assertEquals("df1 should return the number as 50", 50.0, result.getDouble(status));
    }
    {
        ParsePosition ppos;
        Formattable result;
        df2->parse("50%", result, ppos);
        assertEquals("df2 should accept the percent sign", 3, ppos.getIndex());
        assertEquals("df2 should return the number as 0.5", 0.5, result.getDouble(status));
    }
    {
        ParsePosition ppos;
        Formattable result;
        df2->parse("50", result, ppos);
        assertEquals("df2 should return the number as 0.5 even though the percent sign is missing",
                0.5,
                result.getDouble(status));
    }
}

void NumberFormatTest::TestMultiplierWithScale() {
    IcuTestErrorCode status(*this, "TestMultiplierWithScale");

    // Test magnitude combined with multiplier, as shown in API docs
    DecimalFormat df("0", {"en", status}, status);
    if (status.isSuccess()) {
        df.setMultiplier(5);
        df.setMultiplierScale(-1);
        expect2(df, 100, u"50"); // round-trip test
    }
}

void NumberFormatTest::TestFastFormatInt32() {
    IcuTestErrorCode status(*this, "TestFastFormatInt32");

    // The two simplest formatters, old API and new API.
    // Old API should use the fastpath for ints.
    LocalizedNumberFormatter lnf = NumberFormatter::withLocale("en");
    LocalPointer<NumberFormat> df(NumberFormat::createInstance("en", status), status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) {return;}

    double nums[] = {
            0.0,
            -0.0,
            NAN,
            INFINITY,
            0.1,
            1.0,
            1.1,
            2.0,
            3.0,
            9.0,
            10.0,
            99.0,
            100.0,
            999.0,
            1000.0,
            9999.0,
            10000.0,
            99999.0,
            100000.0,
            999999.0,
            1000000.0,
            static_cast<double>(INT32_MAX) - 1,
            static_cast<double>(INT32_MAX),
            static_cast<double>(INT32_MAX) + 1,
            static_cast<double>(INT32_MIN) - 1,
            static_cast<double>(INT32_MIN),
            static_cast<double>(INT32_MIN) + 1};

    for (auto num : nums) {
        UnicodeString expected = lnf.formatDouble(num, status).toString(status);
        UnicodeString actual;
        df->format(num, actual);
        assertEquals(UnicodeString("d = ") + num, expected, actual);
    }
}

void NumberFormatTest::Test11646_Equality() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols(Locale::getEnglish(), status);
    UnicodeString pattern(u"\u00a4\u00a4\u00a4 0.00 %\u00a4\u00a4");
    DecimalFormat fmt(pattern, symbols, status);
    if (!assertSuccess("", status)) return;

    // Test equality with affixes. set affix methods can't capture special
    // characters which is why equality should fail.
    {
        DecimalFormat fmtCopy(fmt);
        assertTrue("", fmt == fmtCopy);
        UnicodeString positivePrefix;
        fmtCopy.setPositivePrefix(fmtCopy.getPositivePrefix(positivePrefix));
        assertFalse("", fmt == fmtCopy);
    }
    {
        DecimalFormat fmtCopy = DecimalFormat(fmt);
        assertTrue("", fmt == fmtCopy);
        UnicodeString positivePrefix;
        fmtCopy.setPositiveSuffix(fmtCopy.getPositiveSuffix(positivePrefix));
        assertFalse("", fmt == fmtCopy);
    }
    {
        DecimalFormat fmtCopy(fmt);
        assertTrue("", fmt == fmtCopy);
        UnicodeString negativePrefix;
        fmtCopy.setNegativePrefix(fmtCopy.getNegativePrefix(negativePrefix));
        assertFalse("", fmt == fmtCopy);
    }
    {
        DecimalFormat fmtCopy(fmt);
        assertTrue("", fmt == fmtCopy);
        UnicodeString negativePrefix;
        fmtCopy.setNegativeSuffix(fmtCopy.getNegativeSuffix(negativePrefix));
        assertFalse("", fmt == fmtCopy);
    }
}

void NumberFormatTest::TestParseNaN() {
    IcuTestErrorCode status(*this, "TestParseNaN");

    DecimalFormat df("0", { "en", status }, status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    Formattable parseResult;
    df.parse(u"NaN", parseResult, status);
    assertEquals("NaN should parse successfully", NAN, parseResult.getDouble());
    assertFalse("Result NaN should be positive", std::signbit(parseResult.getDouble()));
    UnicodeString formatResult;
    df.format(parseResult.getDouble(), formatResult);
    assertEquals("NaN should round-trip", u"NaN", formatResult);
}

void NumberFormatTest::TestFormatFailIfMoreThanMaxDigits() {
    IcuTestErrorCode status(*this, "TestFormatFailIfMoreThanMaxDigits");

    DecimalFormat df("0", {"en-US", status}, status);
    if (status.errDataIfFailureAndReset()) {
        return;
    }
    assertEquals("Coverage for getter 1", (UBool) FALSE, df.isFormatFailIfMoreThanMaxDigits());
    df.setFormatFailIfMoreThanMaxDigits(TRUE);
    assertEquals("Coverage for getter 2", (UBool) TRUE, df.isFormatFailIfMoreThanMaxDigits());
    df.setMaximumIntegerDigits(2);
    UnicodeString result;
    df.format(1234, result, status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
}

void NumberFormatTest::TestParseCaseSensitive() {
    IcuTestErrorCode status(*this, "TestParseCaseSensitive");

    DecimalFormat df(u"0", {"en-US", status}, status);
    if (status.errDataIfFailureAndReset()) {
        return;
    }
    assertEquals("Coverage for getter 1", (UBool) FALSE, df.isParseCaseSensitive());
    df.setParseCaseSensitive(TRUE);
    assertEquals("Coverage for getter 1", (UBool) TRUE, df.isParseCaseSensitive());
    Formattable result;
    ParsePosition ppos;
    df.parse(u"1e2", result, ppos);
    assertEquals("Should parse only 1 digit", 1, ppos.getIndex());
    assertEquals("Result should be 1", 1.0, result.getDouble(status));
}

void NumberFormatTest::TestParseNoExponent() {
    IcuTestErrorCode status(*this, "TestParseNoExponent");

    DecimalFormat df(u"0", {"en-US", status}, status);
    if (status.errDataIfFailureAndReset()) {
        return;
    }
    assertEquals("Coverage for getter 1", (UBool) FALSE, df.isParseNoExponent());
    df.setParseNoExponent(TRUE);
    assertEquals("Coverage for getter 1", (UBool) TRUE, df.isParseNoExponent());
    Formattable result;
    ParsePosition ppos;
    df.parse(u"1E2", result, ppos);
    assertEquals("Should parse only 1 digit", 1, ppos.getIndex());
    assertEquals("Result should be 1", 1.0, result.getDouble(status));
}

void NumberFormatTest::TestSignAlwaysShown() {
    IcuTestErrorCode status(*this, "TestSignAlwaysShown");

    DecimalFormat df(u"0", {"en-US", status}, status);
    if (status.errDataIfFailureAndReset()) {
        return;
    }
    assertEquals("Coverage for getter 1", (UBool) FALSE, df.isSignAlwaysShown());
    df.setSignAlwaysShown(TRUE);
    assertEquals("Coverage for getter 1", (UBool) TRUE, df.isSignAlwaysShown());
    UnicodeString result;
    df.format(1234, result, status);
    status.errIfFailureAndReset();
    assertEquals("Should show sign on positive number", u"+1234", result);
}

void NumberFormatTest::TestMinimumGroupingDigits() {
    IcuTestErrorCode status(*this, "TestMinimumGroupingDigits");

    DecimalFormat df(u"#,##0", {"en-US", status}, status);
    if (status.errDataIfFailureAndReset()) {
        return;
    }
    assertEquals("Coverage for getter 1", -1, df.getMinimumGroupingDigits());
    df.setMinimumGroupingDigits(2);
    assertEquals("Coverage for getter 1", 2, df.getMinimumGroupingDigits());
    UnicodeString result;
    df.format(1234, result, status);
    status.errIfFailureAndReset();
    assertEquals("Should not have grouping", u"1234", result);
    df.format(12345, result.remove(), status);
    status.errIfFailureAndReset();
    assertEquals("Should have grouping", u"12,345", result);
}

void NumberFormatTest::Test11897_LocalizedPatternSeparator() {
    IcuTestErrorCode status(*this, "Test11897_LocalizedPatternSeparator");

    // In a locale with a different <list> symbol, like arabic,
    // kPatternSeparatorSymbol should still be ';'
    {
        DecimalFormatSymbols dfs("ar", status);
        assertEquals("pattern separator symbol should be ;",
                u";",
                dfs.getSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol));
    }

    // However, the custom symbol should be used in localized notation
    // when set manually via API
    {
        DecimalFormatSymbols dfs("en", status);
        dfs.setSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol, u"!", FALSE);
        DecimalFormat df(u"0", dfs, status);
        if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
        df.applyPattern("a0;b0", status); // should not throw
        UnicodeString result;
        assertEquals("should apply the normal pattern",
                df.getNegativePrefix(result.remove()),
                "b");
        df.applyLocalizedPattern(u"c0!d0", status); // should not throw
        assertEquals("should apply the localized pattern",
                df.getNegativePrefix(result.remove()),
                "d");
    }
}

void NumberFormatTest::Test13055_PercentageRounding() {
  IcuTestErrorCode status(*this, "PercentageRounding");
  UnicodeString actual;
  LocalPointer<NumberFormat>pFormat(NumberFormat::createPercentInstance("en_US", status));
  if (U_FAILURE(status)) {
      dataerrln("Failure creating DecimalFormat %s", u_errorName(status));
      return;
  }
  pFormat->setMaximumFractionDigits(0);
  pFormat->setRoundingMode(DecimalFormat::kRoundHalfEven);
  pFormat->format(2.155, actual);
  assertEquals("Should round percent toward even number", "216%", actual);
}
  
void NumberFormatTest::Test11839() {
    IcuTestErrorCode errorCode(*this, "Test11839");
    // Ticket #11839: DecimalFormat does not respect custom plus sign
    LocalPointer<DecimalFormatSymbols> dfs(new DecimalFormatSymbols(Locale::getEnglish(), errorCode), errorCode);
    if (!assertSuccess("", errorCode, true, __FILE__, __LINE__)) { return; }
    dfs->setSymbol(DecimalFormatSymbols::kMinusSignSymbol, u"a∸");
    dfs->setSymbol(DecimalFormatSymbols::kPlusSignSymbol, u"b∔"); //  ∔  U+2214 DOT PLUS
    DecimalFormat df(u"0.00+;0.00-", dfs.orphan(), errorCode);
    UnicodeString result;
    df.format(-1.234, result, errorCode);
    assertEquals("Locale-specific minus sign should be used", u"1.23a∸", result);
    df.format(1.234, result.remove(), errorCode);
    assertEquals("Locale-specific plus sign should be used", u"1.23b∔", result);
    // Test round-trip with parse
    expect2(df, -456, u"456.00a∸");
    expect2(df, 456, u"456.00b∔");
}

void NumberFormatTest::Test10354() {
    IcuTestErrorCode errorCode(*this, "Test10354");
    // Ticket #10354: invalid FieldPositionIterator when formatting with empty NaN
    DecimalFormatSymbols dfs(errorCode);
    UnicodeString empty;
    dfs.setSymbol(DecimalFormatSymbols::kNaNSymbol, empty);
    DecimalFormat df(errorCode);
    df.setDecimalFormatSymbols(dfs);
    UnicodeString result;
    FieldPositionIterator positions;
    df.format(NAN, result, &positions, errorCode);
    errorCode.errIfFailureAndReset("DecimalFormat.format(NAN, FieldPositionIterator) failed");
    FieldPosition fp;
    while (positions.next(fp)) {
        // Should not loop forever
    }
}

void NumberFormatTest::Test11645_ApplyPatternEquality() {
    IcuTestErrorCode status(*this, "Test11645_ApplyPatternEquality");
    const char16_t* pattern = u"#,##0.0#";
    LocalPointer<DecimalFormat> fmt((DecimalFormat*) NumberFormat::createInstance(status), status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    fmt->applyPattern(pattern, status);
    LocalPointer<DecimalFormat> fmtCopy;

    static const int32_t newMultiplier = 37;
    fmtCopy.adoptInstead(new DecimalFormat(*fmt));
    assertFalse("Value before setter", fmtCopy->getMultiplier() == newMultiplier);
    fmtCopy->setMultiplier(newMultiplier);
    assertEquals("Value after setter", fmtCopy->getMultiplier(), newMultiplier);
    fmtCopy->applyPattern(pattern, status);
    assertEquals("Value after applyPattern", fmtCopy->getMultiplier(), newMultiplier);
    assertFalse("multiplier", *fmt == *fmtCopy);

    static const NumberFormat::ERoundingMode newRoundingMode = NumberFormat::ERoundingMode::kRoundCeiling;
    fmtCopy.adoptInstead(new DecimalFormat(*fmt));
    assertFalse("Value before setter", fmtCopy->getRoundingMode() == newRoundingMode);
    fmtCopy->setRoundingMode(newRoundingMode);
    assertEquals("Value after setter", fmtCopy->getRoundingMode(), newRoundingMode);
    fmtCopy->applyPattern(pattern, status);
    assertEquals("Value after applyPattern", fmtCopy->getRoundingMode(), newRoundingMode);
    assertFalse("roundingMode", *fmt == *fmtCopy);

    static const char16_t *const newCurrency = u"EAT";
    fmtCopy.adoptInstead(new DecimalFormat(*fmt));
    assertFalse("Value before setter", fmtCopy->getCurrency() == newCurrency);
    fmtCopy->setCurrency(newCurrency);
    assertEquals("Value after setter", fmtCopy->getCurrency(), newCurrency);
    fmtCopy->applyPattern(pattern, status);
    assertEquals("Value after applyPattern", fmtCopy->getCurrency(), newCurrency);
    assertFalse("currency", *fmt == *fmtCopy);

    static const UCurrencyUsage newCurrencyUsage = UCurrencyUsage::UCURR_USAGE_CASH;
    fmtCopy.adoptInstead(new DecimalFormat(*fmt));
    assertFalse("Value before setter", fmtCopy->getCurrencyUsage() == newCurrencyUsage);
    fmtCopy->setCurrencyUsage(newCurrencyUsage, status);
    assertEquals("Value after setter", fmtCopy->getCurrencyUsage(), newCurrencyUsage);
    fmtCopy->applyPattern(pattern, status);
    assertEquals("Value after applyPattern", fmtCopy->getCurrencyUsage(), newCurrencyUsage);
    assertFalse("currencyUsage", *fmt == *fmtCopy);
}

void NumberFormatTest::Test12567() {
    IcuTestErrorCode errorCode(*this, "Test12567");
    // Ticket #12567: DecimalFormat.equals() may not be symmetric
    LocalPointer<DecimalFormat> df1((DecimalFormat *)
        NumberFormat::createInstance(Locale::getUS(), UNUM_CURRENCY, errorCode));
    LocalPointer<DecimalFormat> df2((DecimalFormat *)
        NumberFormat::createInstance(Locale::getUS(), UNUM_DECIMAL, errorCode));
    if (!assertSuccess("", errorCode, true, __FILE__, __LINE__)) { return; }
    // NOTE: CurrencyPluralInfo equality not tested in C++ because its operator== is not defined.
    df1->applyPattern(u"0.00", errorCode);
    df2->applyPattern(u"0.00", errorCode);
    assertTrue("df1 == df2", *df1 == *df2);
    assertTrue("df2 == df1", *df2 == *df1);
    df2->setPositivePrefix(u"abc");
    assertTrue("df1 != df2", *df1 != *df2);
    assertTrue("df2 != df1", *df2 != *df1);
}

void NumberFormatTest::Test11626_CustomizeCurrencyPluralInfo() {
    IcuTestErrorCode errorCode(*this, "Test11626_CustomizeCurrencyPluralInfo");
    // Ticket #11626: No unit test demonstrating how to use CurrencyPluralInfo to
    // change formatting spelled out currencies
    // Use locale sr because it has interesting plural rules.
    Locale locale("sr");
    LocalPointer<DecimalFormatSymbols> symbols(new DecimalFormatSymbols(locale, errorCode), errorCode);
    CurrencyPluralInfo info(locale, errorCode);
    if (!assertSuccess("", errorCode, true, __FILE__, __LINE__)) { return; }
    info.setCurrencyPluralPattern(u"one", u"0 qwerty", errorCode);
    info.setCurrencyPluralPattern(u"few", u"0 dvorak", errorCode);
    DecimalFormat df(u"#", symbols.orphan(), UNUM_CURRENCY_PLURAL, errorCode);
    df.setCurrencyPluralInfo(info);
    df.setCurrency(u"USD");
    df.setMaximumFractionDigits(0);

    UnicodeString result;
    assertEquals("Plural one", u"1 qwerty", df.format(1, result, errorCode));
    assertEquals("Plural few", u"3 dvorak", df.format(3, result.remove(), errorCode));
    assertEquals("Plural other", u"99 америчких долара", df.format(99, result.remove(), errorCode));

    info.setPluralRules(u"few: n is 1; one: n in 2..4", errorCode);
    df.setCurrencyPluralInfo(info);
    assertEquals("Plural one", u"1 dvorak", df.format(1, result.remove(), errorCode));
    assertEquals("Plural few", u"3 qwerty", df.format(3, result.remove(), errorCode));
    assertEquals("Plural other", u"99 америчких долара", df.format(99, result.remove(), errorCode));
}

void NumberFormatTest::Test20073_StrictPercentParseErrorIndex() {
    IcuTestErrorCode status(*this, "Test20073_StrictPercentParseErrorIndex");
    ParsePosition parsePosition(0);
    DecimalFormat df(u"0%", {"en-us", status}, status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create DecimalFormat instance.");
        return;
    }
    df.setLenient(FALSE);
    Formattable result;
    df.parse(u"%2%", result, parsePosition);
    assertEquals("", 0, parsePosition.getIndex());
    assertEquals("", 0, parsePosition.getErrorIndex());
}

void NumberFormatTest::Test13056_GroupingSize() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat df(u"#,##0", status);
    if (!assertSuccess("", status)) return;
    assertEquals("Primary grouping should return 3", 3, df.getGroupingSize());
    assertEquals("Secondary grouping should return 0", 0, df.getSecondaryGroupingSize());
    df.setSecondaryGroupingSize(3);
    assertEquals("Primary grouping should still return 3", 3, df.getGroupingSize());
    assertEquals("Secondary grouping should round-trip", 3, df.getSecondaryGroupingSize());
    df.setGroupingSize(4);
    assertEquals("Primary grouping should return 4", 4, df.getGroupingSize());
    assertEquals("Secondary should remember explicit setting and return 3", 3, df.getSecondaryGroupingSize());
}


void NumberFormatTest::Test11025_CurrencyPadding() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString pattern(u"¤¤ **####0.00");
    DecimalFormatSymbols sym(Locale::getFrance(), status);
    if (!assertSuccess("", status)) return;
    DecimalFormat fmt(pattern, sym, status);
    if (!assertSuccess("", status)) return;
    UnicodeString result;
    fmt.format(433.0, result);
    assertEquals("Number should be padded to 11 characters", "EUR *433,00", result);
}

void NumberFormatTest::Test11648_ExpDecFormatMalPattern() {
    UErrorCode status = U_ZERO_ERROR;

    DecimalFormat fmt("0.00", {"en", status}, status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    fmt.setScientificNotation(TRUE);
    UnicodeString pattern;

    assertEquals("A valid scientific notation pattern should be produced",
            "0.00E0",
            fmt.toPattern(pattern));

    DecimalFormat fmt2(pattern, status);
    assertSuccess("", status);
}

void NumberFormatTest::Test11649_DecFmtCurrencies() {
    IcuTestErrorCode status(*this, "Test11649_DecFmtCurrencies");
    UnicodeString pattern("\\u00a4\\u00a4\\u00a4 0.00");
    pattern = pattern.unescape();
    DecimalFormat fmt(pattern, status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    static const UChar USD[] = u"USD";
    fmt.setCurrency(USD);
    UnicodeString appendTo;

    assertEquals("", "US dollars 12.34", fmt.format(12.34, appendTo));
    UnicodeString topattern;

    assertEquals("", pattern, fmt.toPattern(topattern));
    DecimalFormat fmt2(topattern, status);
    fmt2.setCurrency(USD);

    appendTo.remove();
    assertEquals("", "US dollars 12.34", fmt2.format(12.34, appendTo));
}

void NumberFormatTest::Test13148_ParseGroupingSeparators() {
  IcuTestErrorCode status(*this, "Test13148");
  LocalPointer<DecimalFormat> fmt(
      (DecimalFormat*)NumberFormat::createInstance("en-ZA", status), status);
  if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }

  DecimalFormatSymbols symbols = *fmt->getDecimalFormatSymbols();

  symbols.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, u'.');
  symbols.setSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol, u',');
  fmt->setDecimalFormatSymbols(symbols);
  Formattable number;
  fmt->parse(u"300,000", number, status);
  assertEquals("Should parse as 300000", 300000LL, number.getInt64(status));
}

void NumberFormatTest::Test12753_PatternDecimalPoint() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols(Locale::getUS(), status);
    symbols.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, u"*", false);
    DecimalFormat df(u"0.00", symbols, status);
    if (!assertSuccess("", status)) return;
    df.setDecimalPatternMatchRequired(true);
    Formattable result;
    df.parse(u"123",result, status);
    assertEquals("Parsing integer succeeded even though setDecimalPatternMatchRequired was set",
                 U_INVALID_FORMAT_ERROR, status);
    }

 void NumberFormatTest::Test11647_PatternCurrencySymbols() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat df(status);
    df.applyPattern(u"¤¤¤¤#", status);
    if (!assertSuccess("", status)) return;
    UnicodeString actual;
    df.format(123, actual);
    assertEquals("Should replace 4 currency signs with U+FFFD", u"\uFFFD123", actual);
}

void NumberFormatTest::Test11913_BigDecimal() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<NumberFormat> df(NumberFormat::createInstance(Locale::getEnglish(), status), status);
    if (!assertSuccess("", status)) return;
    UnicodeString result;
    df->format(StringPiece("1.23456789E400"), result, nullptr, status);
    assertSuccess("", status);
    assertEquals("Should format more than 309 digits", u"12,345,678", UnicodeString(result, 0, 10));
    assertEquals("Should format more than 309 digits", 534, result.length());
}

void NumberFormatTest::Test11020_RoundingInScientificNotation() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols sym(Locale::getFrance(), status);
    DecimalFormat fmt(u"0.05E0", sym, status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    assertSuccess("", status);
    UnicodeString result;
    fmt.format(12301.2, result);
    assertEquals("Rounding increment should be applied after magnitude scaling", u"1,25E4", result);
}

void NumberFormatTest::Test11640_TripleCurrencySymbol() {
    IcuTestErrorCode status(*this, "Test11640_TripleCurrencySymbol");
    UnicodeString actual;
    DecimalFormat dFormat(u"¤¤¤ 0", status);
    if (U_FAILURE(status)) {
        dataerrln("Failure creating DecimalFormat %s", u_errorName(status));
        return;
    }
    dFormat.setCurrency(u"USD");
    UnicodeString result;
    dFormat.getPositivePrefix(result);
    assertEquals("Triple-currency should give long name on getPositivePrefix",
                "US dollars ", result);
}


void NumberFormatTest::Test13763_FieldPositionIteratorOffset() {
    IcuTestErrorCode status(*this, "Test13763_FieldPositionIteratorOffset");
    FieldPositionIterator fpi;
    UnicodeString result(u"foo\U0001F4FBbar"); // 8 code units
    LocalPointer<NumberFormat> nf(NumberFormat::createInstance("en", status), status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    nf->format(5142.3, result, &fpi, status);

    int32_t expected[] = {
      UNUM_GROUPING_SEPARATOR_FIELD, 9, 10,
      UNUM_INTEGER_FIELD, 8, 13,
      UNUM_DECIMAL_SEPARATOR_FIELD, 13, 14,
      UNUM_FRACTION_FIELD, 14, 15,
    };
    int32_t tupleCount = UPRV_LENGTHOF(expected)/3;
    expectPositions(fpi, expected, tupleCount, result);
}

void NumberFormatTest::Test13777_ParseLongNameNonCurrencyMode() {
    IcuTestErrorCode status(*this, "Test13777_ParseLongNameNonCurrencyMode");

    LocalPointer<NumberFormat> df(
        NumberFormat::createInstance("en-us", UNumberFormatStyle::UNUM_CURRENCY_PLURAL, status), status);
    if (!assertSuccess("", status, true, __FILE__, __LINE__)) { return; }
    expect2(*df, 1.5, u"1.50 US dollars");
}

void NumberFormatTest::Test13804_EmptyStringsWhenParsing() {
    IcuTestErrorCode status(*this, "Test13804_EmptyStringsWhenParsing");

    DecimalFormatSymbols dfs("en", status);
    if (status.errIfFailureAndReset()) {
        return;
    }
    dfs.setSymbol(DecimalFormatSymbols::kCurrencySymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kZeroDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kOneDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kTwoDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kThreeDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kFourDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kFiveDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kSixDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kSevenDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kEightDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kNineDigitSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kExponentMultiplicationSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kExponentialSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kInfinitySymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kIntlCurrencySymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kMinusSignSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kMonetarySeparatorSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kNaNSymbol, u"", FALSE);
    dfs.setPatternForCurrencySpacing(UNUM_CURRENCY_INSERT, FALSE, u"");
    dfs.setPatternForCurrencySpacing(UNUM_CURRENCY_INSERT, TRUE, u"");
    dfs.setSymbol(DecimalFormatSymbols::kPercentSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kPerMillSymbol, u"", FALSE);
    dfs.setSymbol(DecimalFormatSymbols::kPlusSignSymbol, u"", FALSE);

    DecimalFormat df("0", dfs, status);
    if (status.errIfFailureAndReset()) {
        return;
    }
    df.setGroupingUsed(TRUE);
    df.setScientificNotation(TRUE);
    df.setLenient(TRUE); // enable all matchers
    {
        UnicodeString result;
        df.format(0, result); // should not crash or hit infinite loop
    }
    const char16_t* samples[] = {
            u"",
            u"123",
            u"$123",
            u"-",
            u"+",
            u"44%",
            u"1E+2.3"
    };
    for (auto& sample : samples) {
        logln(UnicodeString(u"Attempting parse on: ") + sample);
        status.setScope(sample);
        // We don't care about the results, only that we don't crash and don't loop.
        Formattable result;
        ParsePosition ppos(0);
        df.parse(sample, result, ppos);
        ppos = ParsePosition(0);
        LocalPointer<CurrencyAmount> curramt(df.parseCurrency(sample, ppos));
        status.errIfFailureAndReset();
    }

    // Test with a nonempty exponent separator symbol to cover more code
    dfs.setSymbol(DecimalFormatSymbols::kExponentialSymbol, u"E", FALSE);
    df.setDecimalFormatSymbols(dfs);
    {
        Formattable result;
        ParsePosition ppos(0);
        df.parse(u"1E+2.3", result, ppos);
    }
}

void NumberFormatTest::Test20037_ScientificIntegerOverflow() {
    IcuTestErrorCode status(*this, "Test20037_ScientificIntegerOverflow");

    LocalPointer<NumberFormat> nf(NumberFormat::createInstance(status));
    if (U_FAILURE(status)) {
        dataerrln("Unable to create NumberFormat instance.");
        return;
    }
    Formattable result;

    // Test overflow of exponent
    nf->parse(u"1E-2147483648", result, status);
    StringPiece sp = result.getDecimalNumber(status);
    assertEquals(u"Should snap to zero",
                 u"0",
                 {sp.data(), sp.length(), US_INV});

    // Test edge case overflow of exponent
    result = Formattable();
    nf->parse(u"1E-2147483647E-1", result, status);
    sp = result.getDecimalNumber(status);
    assertEquals(u"Should not overflow and should parse only the first exponent",
                 u"1E-2147483647",
                 {sp.data(), sp.length(), US_INV});

    // Test edge case overflow of exponent
    result = Formattable();
    nf->parse(u".0003e-2147483644", result, status);
    sp = result.getDecimalNumber(status);
    assertEquals(u"Should not overflow",
                 u"3E-2147483648",
                 {sp.data(), sp.length(), US_INV});

    // Test largest parseable exponent
    result = Formattable();
    nf->parse(u"9876e2147483643", result, status);
    sp = result.getDecimalNumber(status);
    assertEquals(u"Should not overflow",
                 u"9.876E+2147483646",
                 {sp.data(), sp.length(), US_INV});

    // Test max value as well
    const char16_t* infinityInputs[] = {
            u"9876e2147483644",
            u"9876e2147483645",
            u"9876e2147483646",
            u"9876e2147483647",
            u"9876e2147483648",
            u"9876e2147483649",
    };
    for (const auto& input : infinityInputs) {
        result = Formattable();
        nf->parse(input, result, status);
        sp = result.getDecimalNumber(status);
        assertEquals(UnicodeString("Should become Infinity: ") + input,
                    u"Infinity",
                    {sp.data(), sp.length(), US_INV});
    }
}

void NumberFormatTest::Test13840_ParseLongStringCrash() {
    IcuTestErrorCode status(*this, "Test13840_ParseLongStringCrash");

    LocalPointer<NumberFormat> nf(NumberFormat::createInstance("en", status), status);
    if (status.errIfFailureAndReset()) { return; }

    Formattable result;
    static const char16_t* bigString =
        u"111111111111111111111111111111111111111111111111111111111111111111111"
        u"111111111111111111111111111111111111111111111111111111111111111111111"
        u"111111111111111111111111111111111111111111111111111111111111111111111"
        u"111111111111111111111111111111111111111111111111111111111111111111111"
        u"111111111111111111111111111111111111111111111111111111111111111111111"
        u"111111111111111111111111111111111111111111111111111111111111111111111";
    nf->parse(bigString, result, status);

    // Normalize the input string:
    CharString expectedChars;
    expectedChars.appendInvariantChars(bigString, status);
    DecimalQuantity expectedDQ;
    expectedDQ.setToDecNumber(expectedChars.toStringPiece(), status);
    UnicodeString expectedUString = expectedDQ.toScientificString();

    // Get the output string:
    StringPiece actualChars = result.getDecimalNumber(status);
    UnicodeString actualUString = UnicodeString(actualChars.data(), actualChars.length(), US_INV);

    assertEquals("Should round-trip without crashing", expectedUString, actualUString);
}

void NumberFormatTest::Test13850_EmptyStringCurrency() {
    IcuTestErrorCode status(*this, "Test13840_EmptyStringCurrency");

    struct TestCase {
        const char16_t* currencyArg;
        UErrorCode expectedError;
    } cases[] = {
        {u"", U_ZERO_ERROR},
        {u"U", U_ILLEGAL_ARGUMENT_ERROR},
        {u"Us", U_ILLEGAL_ARGUMENT_ERROR},
        {nullptr, U_ZERO_ERROR},
        {u"U$D", U_INVARIANT_CONVERSION_ERROR},
        {u"Xxx", U_ZERO_ERROR}
    };
    for (const auto& cas : cases) {
        UnicodeString message(u"with currency arg: ");
        if (cas.currencyArg == nullptr) {
            message += u"nullptr";
        } else {
            message += UnicodeString(cas.currencyArg);
        }
        status.setScope(message);
        LocalPointer<NumberFormat> nf(NumberFormat::createCurrencyInstance("en-US", status), status);
        if (status.errIfFailureAndReset()) { return; }
        UnicodeString actual;
        nf->format(1, actual, status);
        status.errIfFailureAndReset();
        assertEquals(u"Should format with US currency " + message, u"$1.00", actual);
        nf->setCurrency(cas.currencyArg, status);
        if (status.expectErrorAndReset(cas.expectedError)) {
            // If an error occurred, do not check formatting.
            continue;
        }
        nf->format(1, actual.remove(), status);
        assertEquals(u"Should unset the currency " + message, u"\u00A41.00", actual);
        status.errIfFailureAndReset();
    }
}

void NumberFormatTest::Test20348_CurrencyPrefixOverride() {
    IcuTestErrorCode status(*this, "Test20348_CurrencyPrefixOverride");
    LocalPointer<DecimalFormat> fmt(static_cast<DecimalFormat*>(
        NumberFormat::createCurrencyInstance("en", status)));
    if (status.errIfFailureAndReset()) { return; }
    UnicodeString result;
    assertEquals("Initial pattern",
        u"¤#,##0.00", fmt->toPattern(result.remove()));
    assertEquals("Initial prefix",
        u"¤", fmt->getPositivePrefix(result.remove()));
    assertEquals("Initial suffix",
        u"-¤", fmt->getNegativePrefix(result.remove()));
    assertEquals("Initial format",
        u"\u00A4100.00", fmt->format(100, result.remove(), NULL, status));

    fmt->setPositivePrefix(u"$");
    assertEquals("Set positive prefix pattern",
        u"$#,##0.00;-\u00A4#,##0.00", fmt->toPattern(result.remove()));
    assertEquals("Set positive prefix prefix",
        u"$", fmt->getPositivePrefix(result.remove()));
    assertEquals("Set positive prefix suffix",
        u"-¤", fmt->getNegativePrefix(result.remove()));
    assertEquals("Set positive prefix format",
        u"$100.00", fmt->format(100, result.remove(), NULL, status));

    fmt->setNegativePrefix(u"-$");
    assertEquals("Set negative prefix pattern",
        u"$#,##0.00;'-'$#,##0.00", fmt->toPattern(result.remove()));
    assertEquals("Set negative prefix prefix",
        u"$", fmt->getPositivePrefix(result.remove()));
    assertEquals("Set negative prefix suffix",
        u"-$", fmt->getNegativePrefix(result.remove()));
    assertEquals("Set negative prefix format",
        u"$100.00", fmt->format(100, result.remove(), NULL, status));
}

void NumberFormatTest::Test20358_GroupingInPattern() {
    IcuTestErrorCode status(*this, "Test20358_GroupingInPattern");
    LocalPointer<DecimalFormat> fmt(static_cast<DecimalFormat*>(
        NumberFormat::createInstance("en", status)));
    if (status.errIfFailureAndReset()) { return; }
    UnicodeString result;
    assertEquals("Initial pattern",
        u"#,##0.###", fmt->toPattern(result.remove()));
    assertTrue("Initial grouping",
        fmt->isGroupingUsed());
    assertEquals("Initial format",
        u"54,321", fmt->format(54321, result.remove(), NULL, status));

    fmt->setGroupingUsed(false);
    assertEquals("Set grouping false",
        u"0.###", fmt->toPattern(result.remove()));
    assertFalse("Set grouping false grouping",
        fmt->isGroupingUsed());
    assertEquals("Set grouping false format",
        u"54321", fmt->format(54321, result.remove(), NULL, status));

    fmt->setGroupingUsed(true);
    assertEquals("Set grouping true",
        u"#,##0.###", fmt->toPattern(result.remove()));
    assertTrue("Set grouping true grouping",
        fmt->isGroupingUsed());
    assertEquals("Set grouping true format",
        u"54,321", fmt->format(54321, result.remove(), NULL, status));
}

void NumberFormatTest::Test13731_DefaultCurrency() {
    IcuTestErrorCode status(*this, "Test13731_DefaultCurrency");
    UnicodeString result;
    {
        LocalPointer<NumberFormat> nf(NumberFormat::createInstance(
            "en", UNumberFormatStyle::UNUM_CURRENCY, status), status);
        if (status.errIfFailureAndReset()) { return; }
        assertEquals("symbol", u"¤1.10",
            nf->format(1.1, result.remove(), status));
        assertEquals("currency", u"XXX", nf->getCurrency());
    }
    {
        LocalPointer<NumberFormat> nf(NumberFormat::createInstance(
            "en", UNumberFormatStyle::UNUM_CURRENCY_ISO, status), status);
        if (status.errIfFailureAndReset()) { return; }
        assertEquals("iso_code", u"XXX 1.10",
            nf->format(1.1, result.remove(), status));
        assertEquals("currency", u"XXX", nf->getCurrency());
    }
    {
        LocalPointer<NumberFormat> nf(NumberFormat::createInstance(
            "en", UNumberFormatStyle::UNUM_CURRENCY_PLURAL, status), status);
        if (status.errIfFailureAndReset()) { return; }
        assertEquals("plural", u"1.10 (unknown currency)",
            nf->format(1.1, result.remove(), status));
        assertEquals("currency", u"XXX", nf->getCurrency());
    }
}

void NumberFormatTest::Test20499_CurrencyVisibleDigitsPlural() {
    IcuTestErrorCode status(*this, "Test20499_CurrencyVisibleDigitsPlural");
    LocalPointer<NumberFormat> nf(NumberFormat::createInstance(
        "ro-RO", UNumberFormatStyle::UNUM_CURRENCY_PLURAL, status), status);
    const char16_t* expected = u"24,00 lei românești";
    for (int32_t i=0; i<5; i++) {
        UnicodeString actual;
        nf->format(24, actual, status);
        assertEquals(UnicodeString(u"iteration ") + Int64ToUnicodeString(i),
            expected, actual);
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
