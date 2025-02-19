// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2015, International Business Machines
 * Corporation and others. All Rights Reserved.
 ********************************************************************/

/**
 * IntlTestFormat is the medium level test class for everything in the directory "format".
 */

#include "unicode/utypes.h"
#include "unicode/localpointer.h"

#if !UCONFIG_NO_FORMATTING

#include "itformat.h"
#include "tsdate.h"
#include "tsnmfmt.h"
#include "caltest.h"
#include "callimts.h"
#include "tztest.h"
#include "tzbdtest.h"
#include "tsdcfmsy.h"       // DecimalFormatSymbols
#include "tchcfmt.h"
#include "tsdtfmsy.h"       // DateFormatSymbols
#include "dcfmapts.h"       // DecimalFormatAPI
#include "tfsmalls.h"       // Format Small Classes
#include "nmfmapts.h"       // NumberFormatAPI
#include "numfmtst.h"       // NumberFormatTest
#include "sdtfmtts.h"       // SimpleDateFormatAPI
#include "dtfmapts.h"       // DateFormatAPI
#include "dtfmttst.h"       // DateFormatTest
#include "tmsgfmt.h"        // TestMessageFormat
#include "dtfmrgts.h"       // DateFormatRegressionTest
#include "msfmrgts.h"       // MessageFormatRegressionTest
#include "miscdtfm.h"       // DateFormatMiscTests
#include "nmfmtrt.h"        // NumberFormatRoundTripTest
#include "numrgts.h"        // NumberFormatRegressionTest
#include "dtfmtrtts.h"      // DateFormatRoundTripTest
#include "pptest.h"         // ParsePositionTest
#include "calregts.h"       // CalendarRegressionTest
#include "tzregts.h"        // TimeZoneRegressionTest
#include "astrotst.h"       // AstroTest
#include "incaltst.h"       // IntlCalendarTest
#include "calcasts.h"       // CalendarCaseTest
#include "tzrulets.h"       // TimeZoneRuleTest
#include "dadrcal.h"        // DataDrivenCalendarTest
#include "dadrfmt.h"        // DataDrivenFormatTest
#include "dtptngts.h"       // IntlTestDateTimePatternGeneratorAPI
#include "tzoffloc.h"       // TimeZoneOffsetLocalTest
#include "tzfmttst.h"       // TimeZoneFormatTest
#include "plurults.h"       // PluralRulesTest
#include "plurfmts.h"       // PluralFormatTest
#include "selfmts.h"       // PluralFormatTest
#include "dtifmtts.h"       // DateIntervalFormatTest
#include "locnmtst.h"       // LocaleDisplayNamesTest
#include "dcfmtest.h"       // DecimalFormatTest
#include "listformattertest.h"  // ListFormatterTest
#include "regiontst.h"      // RegionTest
#include "numbertest.h"     // NumberTest
#include "erarulestest.h"   // EraRulesTest

extern IntlTest *createCompactDecimalFormatTest();
extern IntlTest *createGenderInfoTest();
#if !UCONFIG_NO_BREAK_ITERATION
extern IntlTest *createRelativeDateTimeFormatterTest();
#endif
extern IntlTest *createTimeUnitTest();
extern IntlTest *createMeasureFormatTest();
extern IntlTest *createNumberFormatSpecificationTest();
extern IntlTest *createScientificNumberFormatterTest();
extern IntlTest *createFormattedValueTest();


#define TESTCLASS(id, TestClass)          \
    case id:                              \
        name = #TestClass;                \
        if (exec) {                       \
            logln(#TestClass " test---"); \
            logln((UnicodeString)"");     \
            TestClass test;               \
            callTest(test, par);          \
        }                                 \
        break

void IntlTestFormat::runIndexedTest( int32_t index, UBool exec, const char* &name, char* par )
{
    // for all format tests, always set default Locale and TimeZone to ENGLISH and PST.
    TimeZone* saveDefaultTimeZone = NULL;
    Locale  saveDefaultLocale = Locale::getDefault();
    if (exec) {
        saveDefaultTimeZone = TimeZone::createDefault();
        TimeZone *tz = TimeZone::createTimeZone("America/Los_Angeles");
        TimeZone::setDefault(*tz);
        delete tz;
        UErrorCode status = U_ZERO_ERROR;
        Locale::setDefault( Locale::getEnglish(), status );
        if (U_FAILURE(status)) {
            errln("itformat: couldn't set default Locale to ENGLISH!");
        }
    }
    if (exec) logln("TestSuite Format: ");
    switch (index) {
        TESTCLASS(0,IntlTestDateFormat);
        TESTCLASS(1,IntlTestNumberFormat);
        TESTCLASS(2,CalendarTest);
        TESTCLASS(3,CalendarLimitTest);
        TESTCLASS(4,TimeZoneTest);
        TESTCLASS(5,TimeZoneBoundaryTest);
        TESTCLASS(6,TestChoiceFormat);
        TESTCLASS(7,IntlTestDecimalFormatSymbols);
        TESTCLASS(8,IntlTestDateFormatSymbols);
        TESTCLASS(9,IntlTestDecimalFormatAPI);
        TESTCLASS(10,TestFormatSmallClasses);
        TESTCLASS(11,IntlTestNumberFormatAPI);
        TESTCLASS(12,IntlTestSimpleDateFormatAPI);
        TESTCLASS(13,IntlTestDateFormatAPI);
        TESTCLASS(14,DateFormatTest);
        TESTCLASS(15,TestMessageFormat);
        TESTCLASS(16,NumberFormatTest);
        TESTCLASS(17,DateFormatRegressionTest);
        TESTCLASS(18,MessageFormatRegressionTest);
        TESTCLASS(19,DateFormatMiscTests);
        TESTCLASS(20,NumberFormatRoundTripTest);
        TESTCLASS(21,NumberFormatRegressionTest);
        TESTCLASS(22,DateFormatRoundTripTest);
        TESTCLASS(23,ParsePositionTest);
        TESTCLASS(24,CalendarRegressionTest);
        TESTCLASS(25,TimeZoneRegressionTest);
        TESTCLASS(26,IntlCalendarTest);
        TESTCLASS(27,AstroTest);
        TESTCLASS(28,CalendarCaseTest);
        TESTCLASS(29,TimeZoneRuleTest);
#if !UCONFIG_NO_FILE_IO && !UCONFIG_NO_LEGACY_CONVERSION
        TESTCLASS(30,DataDrivenCalendarTest);
        TESTCLASS(31,DataDrivenFormatTest);
#endif
        TESTCLASS(32,IntlTestDateTimePatternGeneratorAPI);
        TESTCLASS(33,TimeZoneOffsetLocalTest);
        TESTCLASS(34,TimeZoneFormatTest);
        TESTCLASS(35,PluralRulesTest);
        TESTCLASS(36,PluralFormatTest);
        TESTCLASS(37,DateIntervalFormatTest);
        case 38:
          name = "TimeUnitTest";
          if (exec) {
            logln("TimeUnitTest test---");
            logln((UnicodeString)"");
            LocalPointer<IntlTest> test(createTimeUnitTest());
            callTest(*test, par);
          }
          break;
        TESTCLASS(39,SelectFormatTest);
        TESTCLASS(40,LocaleDisplayNamesTest);
#if !UCONFIG_NO_REGULAR_EXPRESSIONS
        TESTCLASS(41,DecimalFormatTest);
#endif
        TESTCLASS(42,ListFormatterTest);
        case 43:
          name = "GenderInfoTest";
          if (exec) {
            logln("GenderInfoTest test---");
            logln((UnicodeString)"");
            LocalPointer<IntlTest> test(createGenderInfoTest());
            callTest(*test, par);
          }
          break;
        case 44:
          name = "CompactDecimalFormatTest";
          if (exec) {
            logln("CompactDecimalFormatTest test---");
            logln((UnicodeString)"");
            LocalPointer<IntlTest> test(createCompactDecimalFormatTest());
            callTest(*test, par);
          }
          break;
        TESTCLASS(45,RegionTest);
        case 46:
#if !UCONFIG_NO_BREAK_ITERATION
          name = "RelativeDateTimeFormatterTest";
          if (exec) {
            logln("RelativeDateTimeFormatterTest test---");
            logln((UnicodeString)"");
            LocalPointer<IntlTest> test(createRelativeDateTimeFormatterTest());
            callTest(*test, par);
          }
#endif
          break;
        case 47:
          name = "MeasureFormatTest";
          if (exec) {
            logln("MeasureFormatTest test---");
            logln((UnicodeString)"");
            LocalPointer<IntlTest> test(createMeasureFormatTest());
            callTest(*test, par);
          }
          break;
        case 48:
          name = "NumberFormatSpecificationTest";
          if (exec) {
            logln("NumberFormatSpecificationTest test---");
            logln((UnicodeString)"");
            LocalPointer<IntlTest> test(createNumberFormatSpecificationTest());
            callTest(*test, par);
          }
          break;
        case 49:
          name = "ScientificNumberFormatterTest";
          if (exec) {
            logln("ScientificNumberFormatterTest test---");
            logln((UnicodeString)"");
            LocalPointer<IntlTest> test(createScientificNumberFormatterTest());
            callTest(*test, par);
          }
          break;
        TESTCLASS(50,NumberFormatDataDrivenTest);
        TESTCLASS(51,NumberTest);
        TESTCLASS(52,EraRulesTest);
        case 53:
          name = "FormattedValueTest";
          if (exec) {
            logln("FormattedValueTest test---");
            logln((UnicodeString)"");
            LocalPointer<IntlTest> test(createFormattedValueTest());
            callTest(*test, par);
          }
          break;
        default: name = ""; break; //needed to end loop
    }
    if (exec) {
        // restore saved Locale and TimeZone
        TimeZone::adoptDefault(saveDefaultTimeZone);
        UErrorCode status = U_ZERO_ERROR;
        Locale::setDefault( saveDefaultLocale, status );
        if (U_FAILURE(status)) {
            errln("itformat: couldn't re-set default Locale!");
        }
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
