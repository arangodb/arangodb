// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/************************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation
 * and others. All Rights Reserved.
 ************************************************************************/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "caltest.h"
#include "unicode/dtfmtsym.h"
#include "unicode/gregocal.h"
#include "unicode/localpointer.h"
#include "hebrwcal.h"
#include "unicode/smpdtfmt.h"
#include "unicode/simpletz.h"
#include "dbgutil.h"
#include "unicode/udat.h"
#include "unicode/ustring.h"
#include "cstring.h"
#include "unicode/localpointer.h"
#include "chnsecal.h"
#include "intltest.h"
#include "coptccal.h"
#include "ethpccal.h"
#include "islamcal.h"

#define mkcstr(U) u_austrcpy(calloc(8, u_strlen(U) + 1), U)

#define TEST_CHECK_STATUS UPRV_BLOCK_MACRO_BEGIN { \
    if (U_FAILURE(status)) { \
        if (status == U_MISSING_RESOURCE_ERROR) { \
            dataerrln("%s:%d: Test failure.  status=%s", __FILE__, __LINE__, u_errorName(status)); \
        } else { \
            errln("%s:%d: Test failure.  status=%s", __FILE__, __LINE__, u_errorName(status)); \
        } \
        return; \
    } \
} UPRV_BLOCK_MACRO_END

#define TEST_CHECK_STATUS_LOCALE(testlocale) UPRV_BLOCK_MACRO_BEGIN { \
    if (U_FAILURE(status)) { \
        if (status == U_MISSING_RESOURCE_ERROR) { \
            dataerrln("%s:%d: Test failure, locale %s.  status=%s", __FILE__, __LINE__, testlocale, u_errorName(status)); \
        } else { \
            errln("%s:%d: Test failure, locale %s.  status=%s", __FILE__, __LINE__, testlocale, u_errorName(status)); \
        } \
        return; \
    } \
} UPRV_BLOCK_MACRO_END

#define TEST_ASSERT(expr) UPRV_BLOCK_MACRO_BEGIN { \
    if ((expr)==false) { \
        errln("%s:%d: Test failure \n", __FILE__, __LINE__); \
    } \
} UPRV_BLOCK_MACRO_END

// *****************************************************************************
// class CalendarTest
// *****************************************************************************

UnicodeString CalendarTest::calToStr(const Calendar & cal)
{
  UnicodeString out;
  UErrorCode status = U_ZERO_ERROR;
  int i;
  UDate d;
  for(i = 0;i<UCAL_FIELD_COUNT;i++) {
    out += (UnicodeString("") + fieldName((UCalendarDateFields)i) + "=" +  cal.get((UCalendarDateFields)i, status) + UnicodeString(" "));
  }
  out += "[" + UnicodeString(cal.getType()) + "]";

  if(cal.inDaylightTime(status)) {
    out += UnicodeString(" (in DST), zone=");
  }
  else {
    out += UnicodeString(", zone=");
  }

  UnicodeString str2;
  out += cal.getTimeZone().getDisplayName(str2);
  d = cal.getTime(status);
  out += UnicodeString(" :","") + d;

  return out;
}

void CalendarTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite TestCalendar");

    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestDOW943);
    TESTCASE_AUTO(TestClonesUnique908);
    TESTCASE_AUTO(TestGregorianChange768);
    TESTCASE_AUTO(TestDisambiguation765);
    TESTCASE_AUTO(TestGMTvsLocal4064654);
    TESTCASE_AUTO(TestAddSetOrder621);
    TESTCASE_AUTO(TestAdd520);
    TESTCASE_AUTO(TestFieldSet4781);
    //  TESTCASE_AUTO(TestSerialize337);
    TESTCASE_AUTO(TestSecondsZero121);
    TESTCASE_AUTO(TestAddSetGet0610);
    TESTCASE_AUTO(TestFields060);
    TESTCASE_AUTO(TestEpochStartFields);
    TESTCASE_AUTO(TestDOWProgression);
    TESTCASE_AUTO(TestGenericAPI);
    TESTCASE_AUTO(TestAddRollExtensive);
    TESTCASE_AUTO(TestDOW_LOCALandYEAR_WOY);
    TESTCASE_AUTO(TestWOY);
    TESTCASE_AUTO(TestRog);
    TESTCASE_AUTO(TestYWOY);
    TESTCASE_AUTO(TestJD);
    TESTCASE_AUTO(TestDebug);
    TESTCASE_AUTO(Test6703);
    TESTCASE_AUTO(Test3785);
    TESTCASE_AUTO(Test1624);
    TESTCASE_AUTO(TestTimeStamp);
    TESTCASE_AUTO(TestISO8601);
    TESTCASE_AUTO(TestAmbiguousWallTimeAPIs);
    TESTCASE_AUTO(TestRepeatedWallTime);
    TESTCASE_AUTO(TestSkippedWallTime);
    TESTCASE_AUTO(TestCloneLocale);
    TESTCASE_AUTO(TestIslamicUmAlQura);
    TESTCASE_AUTO(TestIslamicTabularDates);
    TESTCASE_AUTO(TestHebrewMonthValidation);
    TESTCASE_AUTO(TestWeekData);
    TESTCASE_AUTO(TestAddAcrossZoneTransition);
    TESTCASE_AUTO(TestChineseCalendarMapping);
    TESTCASE_AUTO(TestTimeZoneInLocale);
    TESTCASE_AUTO(TestBasicConversionISO8601);
    TESTCASE_AUTO(TestBasicConversionJapanese);
    TESTCASE_AUTO(TestBasicConversionBuddhist);
    TESTCASE_AUTO(TestBasicConversionTaiwan);
    TESTCASE_AUTO(TestBasicConversionPersian);
    TESTCASE_AUTO(TestBasicConversionIslamic);
    TESTCASE_AUTO(TestBasicConversionIslamicTBLA);
    TESTCASE_AUTO(TestBasicConversionIslamicCivil);
    TESTCASE_AUTO(TestBasicConversionIslamicRGSA);
    TESTCASE_AUTO(TestBasicConversionIslamicUmalqura);
    TESTCASE_AUTO(TestBasicConversionHebrew);
    TESTCASE_AUTO(TestBasicConversionChinese);
    TESTCASE_AUTO(TestBasicConversionDangi);
    TESTCASE_AUTO(TestBasicConversionIndian);
    TESTCASE_AUTO(TestBasicConversionCoptic);
    TESTCASE_AUTO(TestBasicConversionEthiopic);
    TESTCASE_AUTO(TestBasicConversionEthiopicAmeteAlem);
    TESTCASE_AUTO(TestGregorianCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestChineseCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestDangiCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestHebrewCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestIslamicCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestIslamicCivilCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestIslamicUmalquraCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestIslamicRGSACalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestIslamicTBLACalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestPersianCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestIndianCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestTaiwanCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestJapaneseCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestBuddhistCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestCopticCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestEthiopicCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestEthiopicAmeteAlemCalendarInTemporalLeapYear);
    TESTCASE_AUTO(TestChineseCalendarGetTemporalMonthCode);
    TESTCASE_AUTO(TestDangiCalendarGetTemporalMonthCode);
    TESTCASE_AUTO(TestHebrewCalendarGetTemporalMonthCode);
    TESTCASE_AUTO(TestCopticCalendarGetTemporalMonthCode);
    TESTCASE_AUTO(TestEthiopicCalendarGetTemporalMonthCode);
    TESTCASE_AUTO(TestEthiopicAmeteAlemCalendarGetTemporalMonthCode);
    TESTCASE_AUTO(TestGregorianCalendarSetTemporalMonthCode);
    TESTCASE_AUTO(TestChineseCalendarSetTemporalMonthCode);
    TESTCASE_AUTO(TestHebrewCalendarSetTemporalMonthCode);
    TESTCASE_AUTO(TestCopticCalendarSetTemporalMonthCode);
    TESTCASE_AUTO(TestEthiopicCalendarSetTemporalMonthCode);
    TESTCASE_AUTO(TestMostCalendarsOrdinalMonthSet);
    TESTCASE_AUTO(TestChineseCalendarOrdinalMonthSet);
    TESTCASE_AUTO(TestDangiCalendarOrdinalMonthSet);
    TESTCASE_AUTO(TestHebrewCalendarOrdinalMonthSet);
    TESTCASE_AUTO(TestCalendarAddOrdinalMonth);
    TESTCASE_AUTO(TestCalendarRollOrdinalMonth);
    TESTCASE_AUTO(TestLimitsOrdinalMonth);
    TESTCASE_AUTO(TestActualLimitsOrdinalMonth);
    TESTCASE_AUTO(TestChineseCalendarMonthInSpecialYear);
    TESTCASE_AUTO(TestClearMonth);

    TESTCASE_AUTO(TestFWWithISO8601);
    TESTCASE_AUTO(TestDangiOverflowIsLeapMonthBetween22507);
    TESTCASE_AUTO(TestRollWeekOfYear);
    TESTCASE_AUTO(TestFirstDayOfWeek);

    TESTCASE_AUTO(Test22633ChineseOverflow);
    TESTCASE_AUTO(Test22633IndianOverflow);
    TESTCASE_AUTO(Test22633IslamicUmalquraOverflow);
    TESTCASE_AUTO(Test22633PersianOverflow);
    TESTCASE_AUTO(Test22633HebrewOverflow);
    TESTCASE_AUTO(Test22633AMPMOverflow);
    TESTCASE_AUTO(Test22633SetGetTimeOverflow);
    TESTCASE_AUTO(Test22633Set2FieldsGetTimeOverflow);
    TESTCASE_AUTO(Test22633SetAddGetTimeOverflow);
    TESTCASE_AUTO(Test22633SetRollGetTimeOverflow);
    TESTCASE_AUTO(Test22633AddTwiceGetTimeOverflow);
    TESTCASE_AUTO(Test22633RollTwiceGetTimeOverflow);

    TESTCASE_AUTO(Test22633HebrewLargeNegativeDay);
    TESTCASE_AUTO(Test22730JapaneseOverflow);

    TESTCASE_AUTO(TestAddOverflow);


    TESTCASE_AUTO(TestChineseCalendarComputeMonthStart);

    TESTCASE_AUTO_END;
}

// ---------------------------------------------------------------------------------

UnicodeString CalendarTest::fieldName(UCalendarDateFields f) {
    switch (f) {
#define FIELD_NAME_STR(x) case x: return (#x)+5
      FIELD_NAME_STR( UCAL_ERA );
      FIELD_NAME_STR( UCAL_YEAR );
      FIELD_NAME_STR( UCAL_MONTH );
      FIELD_NAME_STR( UCAL_WEEK_OF_YEAR );
      FIELD_NAME_STR( UCAL_WEEK_OF_MONTH );
      FIELD_NAME_STR( UCAL_DATE );
      FIELD_NAME_STR( UCAL_DAY_OF_YEAR );
      FIELD_NAME_STR( UCAL_DAY_OF_WEEK );
      FIELD_NAME_STR( UCAL_DAY_OF_WEEK_IN_MONTH );
      FIELD_NAME_STR( UCAL_AM_PM );
      FIELD_NAME_STR( UCAL_HOUR );
      FIELD_NAME_STR( UCAL_HOUR_OF_DAY );
      FIELD_NAME_STR( UCAL_MINUTE );
      FIELD_NAME_STR( UCAL_SECOND );
      FIELD_NAME_STR( UCAL_MILLISECOND );
      FIELD_NAME_STR( UCAL_ZONE_OFFSET );
      FIELD_NAME_STR( UCAL_DST_OFFSET );
      FIELD_NAME_STR( UCAL_YEAR_WOY );
      FIELD_NAME_STR( UCAL_DOW_LOCAL );
      FIELD_NAME_STR( UCAL_EXTENDED_YEAR );
      FIELD_NAME_STR( UCAL_JULIAN_DAY );
      FIELD_NAME_STR( UCAL_MILLISECONDS_IN_DAY );
#undef FIELD_NAME_STR
    default:
        return UnicodeString("") + ((int32_t)f);
    }
}

/**
 * Test various API methods for API completeness.
 */
void
CalendarTest::TestGenericAPI()
{
    UErrorCode status = U_ZERO_ERROR;
    UDate d;
    UnicodeString str;
    UBool eq = false,b4 = false,af = false;

    UDate when = date(90, UCAL_APRIL, 15);

    UnicodeString tzid("TestZone");
    int32_t tzoffset = 123400;

    SimpleTimeZone *zone = new SimpleTimeZone(tzoffset, tzid);
    Calendar *cal = Calendar::createInstance(zone->clone(), status);
    if (failure(status, "Calendar::createInstance #1", true)) return;

    if (*zone != cal->getTimeZone()) errln("FAIL: Calendar::getTimeZone failed");

    Calendar *cal2 = Calendar::createInstance(cal->getTimeZone(), status);
    if (failure(status, "Calendar::createInstance #2")) return;
    cal->setTime(when, status);
    cal2->setTime(when, status);
    if (failure(status, "Calendar::setTime")) return;

    if (!(*cal == *cal2)) errln("FAIL: Calendar::operator== failed");
    if ((*cal != *cal2))  errln("FAIL: Calendar::operator!= failed");
    if (!cal->equals(*cal2, status) ||
        cal->before(*cal2, status) ||
        cal->after(*cal2, status) ||
        U_FAILURE(status)) errln("FAIL: equals/before/after failed");

    logln(UnicodeString("cal=")  +cal->getTime(status)  + UnicodeString(calToStr(*cal)));
    logln(UnicodeString("cal2=")  +cal2->getTime(status)  + UnicodeString(calToStr(*cal2)));
    logln("cal2->setTime(when+1000)");
    cal2->setTime(when + 1000, status);
    logln(UnicodeString("cal2=")  +cal2->getTime(status)  + UnicodeString(calToStr(*cal2)));

    if (failure(status, "Calendar::setTime")) return;
    if (cal->equals(*cal2, status) ||
        cal2->before(*cal, status) ||
        cal->after(*cal2, status) ||
        U_FAILURE(status)) errln("FAIL: equals/before/after failed after setTime(+1000)");

    logln("cal->roll(UCAL_SECOND)");
    cal->roll(UCAL_SECOND, (UBool) true, status);
    logln(UnicodeString("cal=")  +cal->getTime(status)  + UnicodeString(calToStr(*cal)));
    cal->roll(UCAL_SECOND, (int32_t)0, status);
    logln(UnicodeString("cal=")  +cal->getTime(status)  + UnicodeString(calToStr(*cal)));
    if (failure(status, "Calendar::roll")) return;

    if (!(eq=cal->equals(*cal2, status)) ||
        (b4=cal->before(*cal2, status)) ||
        (af=cal->after(*cal2, status)) ||
        U_FAILURE(status)) {
      errln("FAIL: equals[%c]/before[%c]/after[%c] failed after roll 1 second [should be T/F/F]",
            eq?'T':'F',
            b4?'T':'F',
            af?'T':'F');
      logln(UnicodeString("cal=")  +cal->getTime(status)  + UnicodeString(calToStr(*cal)));
      logln(UnicodeString("cal2=")  +cal2->getTime(status)  + UnicodeString(calToStr(*cal2)));
    }

    // Roll back to January
    cal->roll(UCAL_MONTH, (int32_t)(1 + UCAL_DECEMBER - cal->get(UCAL_MONTH, status)), status);
    if (failure(status, "Calendar::roll")) return;
    if (cal->equals(*cal2, status) ||
        cal2->before(*cal, status) ||
        cal->after(*cal2, status) ||
        U_FAILURE(status)) errln("FAIL: equals/before/after failed after rollback to January");

    TimeZone *z = cal->orphanTimeZone();
    if (z->getID(str) != tzid ||
        z->getRawOffset() != tzoffset)
        errln("FAIL: orphanTimeZone failed");

    int32_t i;
    for (i=0; i<2; ++i)
    {
        UBool lenient = ( i > 0 );
        cal->setLenient(lenient);
        if (lenient != cal->isLenient()) errln("FAIL: setLenient/isLenient failed");
        // Later: Check for lenient behavior
    }

    for (i=UCAL_SUNDAY; i<=UCAL_SATURDAY; ++i)
    {
        cal->setFirstDayOfWeek((UCalendarDaysOfWeek)i);
        if (cal->getFirstDayOfWeek() != i) errln("FAIL: set/getFirstDayOfWeek failed");
        UErrorCode aStatus = U_ZERO_ERROR;
        if (cal->getFirstDayOfWeek(aStatus) != i || U_FAILURE(aStatus)) errln("FAIL: getFirstDayOfWeek(status) failed");
    }

    for (i=1; i<=7; ++i)
    {
        cal->setMinimalDaysInFirstWeek((uint8_t)i);
        if (cal->getMinimalDaysInFirstWeek() != i) errln("FAIL: set/getFirstDayOfWeek failed");
    }

    for (i=0; i<UCAL_FIELD_COUNT; ++i)
    {
        if (cal->getMinimum((UCalendarDateFields)i) > cal->getGreatestMinimum((UCalendarDateFields)i))
            errln(UnicodeString("FAIL: getMinimum larger than getGreatestMinimum for field ") + i);
        if (cal->getLeastMaximum((UCalendarDateFields)i) > cal->getMaximum((UCalendarDateFields)i))
            errln(UnicodeString("FAIL: getLeastMaximum larger than getMaximum for field ") + i);
        if (cal->getMinimum((UCalendarDateFields)i) >= cal->getMaximum((UCalendarDateFields)i))
            errln(UnicodeString("FAIL: getMinimum not less than getMaximum for field ") + i);
    }

    cal->adoptTimeZone(TimeZone::createDefault());
    cal->clear();
    cal->set(1984, 5, 24);
    if (cal->getTime(status) != date(84, 5, 24) || U_FAILURE(status))
        errln("FAIL: Calendar::set(3 args) failed");

    cal->clear();
    cal->set(1985, 3, 2, 11, 49);
    if (cal->getTime(status) != date(85, 3, 2, 11, 49) || U_FAILURE(status))
        errln("FAIL: Calendar::set(5 args) failed");

    cal->clear();
    cal->set(1995, 9, 12, 1, 39, 55);
    if (cal->getTime(status) != date(95, 9, 12, 1, 39, 55) || U_FAILURE(status))
        errln("FAIL: Calendar::set(6 args) failed");

    cal->getTime(status);
    if (failure(status, "Calendar::getTime")) return;
    for (i=0; i<UCAL_FIELD_COUNT; ++i)
    {
        switch(i) {
            case UCAL_YEAR: case UCAL_MONTH: case UCAL_DATE:
            case UCAL_HOUR_OF_DAY: case UCAL_MINUTE: case UCAL_SECOND:
            case UCAL_EXTENDED_YEAR:
              if (!cal->isSet((UCalendarDateFields)i)) errln("FAIL: Calendar::isSet F, should be T " + fieldName((UCalendarDateFields)i));
                break;
            default:
              if (cal->isSet((UCalendarDateFields)i)) errln("FAIL: Calendar::isSet = T, should be F  " + fieldName((UCalendarDateFields)i));
        }
        cal->clear((UCalendarDateFields)i);
        if (cal->isSet((UCalendarDateFields)i)) errln("FAIL: Calendar::clear/isSet failed " + fieldName((UCalendarDateFields)i));
    }

    if(cal->getActualMinimum(Calendar::SECOND, status) != 0){
        errln("Calendar is suppose to return 0 for getActualMinimum");
    }

    Calendar *cal3 = Calendar::createInstance(status);
    cal3->roll(Calendar::SECOND, (int32_t)0, status);
    if (failure(status, "Calendar::roll(EDateFields, int32_t, UErrorCode)")) return;

    delete cal;
    delete cal2;
    delete cal3;

    int32_t count;
    const Locale* loc = Calendar::getAvailableLocales(count);
    if (count < 1 || loc == nullptr)
    {
        dataerrln("FAIL: getAvailableLocales failed");
    }
    else
    {
        for (i=0; i<count; ++i)
        {
            cal = Calendar::createInstance(loc[i], status);
            if (U_FAILURE(status)) {
                errcheckln(status, UnicodeString("FAIL: Calendar::createInstance #3, locale ") +  loc[i].getName() + " , error " + u_errorName(status));
                return;
            }
            delete cal;
        }
    }

    cal = Calendar::createInstance(TimeZone::createDefault(), Locale::getEnglish(), status);
    if (failure(status, "Calendar::createInstance #4")) return;
    delete cal;

    cal = Calendar::createInstance(*zone, Locale::getEnglish(), status);
    if (failure(status, "Calendar::createInstance #5")) return;
    delete cal;

    GregorianCalendar *gc = new GregorianCalendar(*zone, status);
    if (failure(status, "new GregorianCalendar")) return;
    delete gc;

    gc = new GregorianCalendar(Locale::getEnglish(), status);
    if (failure(status, "new GregorianCalendar")) return;
    delete gc;

    gc = new GregorianCalendar(Locale::getEnglish(), status);
    delete gc;

    gc = new GregorianCalendar(*zone, Locale::getEnglish(), status);
    if (failure(status, "new GregorianCalendar")) return;
    delete gc;

    gc = new GregorianCalendar(zone, status);
    if (failure(status, "new GregorianCalendar")) return;
    delete gc;

    gc = new GregorianCalendar(1998, 10, 14, 21, 43, status);
    if (gc->getTime(status) != (d =date(98, 10, 14, 21, 43) )|| U_FAILURE(status))
      errln("FAIL: new GregorianCalendar(ymdhm) failed with " + UnicodeString(u_errorName(status)) + ",  cal="  + gc->getTime(status)  + UnicodeString(calToStr(*gc)) + ", d=" + d);
    else
      logln(UnicodeString("GOOD: cal=")  +gc->getTime(status)  + UnicodeString(calToStr(*gc)) + ", d=" + d);
    delete gc;

    gc = new GregorianCalendar(1998, 10, 14, 21, 43, 55, status);
    if (gc->getTime(status) != (d=date(98, 10, 14, 21, 43, 55)) || U_FAILURE(status))
      errln("FAIL: new GregorianCalendar(ymdhms) failed with " + UnicodeString(u_errorName(status)));

    GregorianCalendar gc2(Locale::getEnglish(), status);
    if (failure(status, "new GregorianCalendar")) return;
    gc2 = *gc;
    if (gc2 != *gc || !(gc2 == *gc)) errln("FAIL: GregorianCalendar assignment/operator==/operator!= failed");
    delete gc;
    delete z;

    /* Code coverage for Calendar class. */
    cal = Calendar::createInstance(status);
    if (failure(status, "Calendar::createInstance #6")) {
        return;
    }else {
        cal->roll(UCAL_HOUR, (int32_t)100, status);
        cal->clear(UCAL_HOUR);
#if !UCONFIG_NO_SERVICE
        URegistryKey key = cal->registerFactory(nullptr, status);
        cal->unregister(key, status);
#endif
    }
    delete cal;

    status = U_ZERO_ERROR;
    cal = Calendar::createInstance(Locale("he_IL@calendar=hebrew"), status);
    if (failure(status, "Calendar::createInstance #7")) {
        return;
    } else {
        cal->roll(Calendar::MONTH, (int32_t)100, status);
    }

    LocalPointer<StringEnumeration> values(
        Calendar::getKeywordValuesForLocale("calendar", Locale("he"), false, status));
    if (values.isNull() || U_FAILURE(status)) {
        dataerrln("FAIL: Calendar::getKeywordValuesForLocale(he): %s", u_errorName(status));
    } else {
        UBool containsHebrew = false;
        const char *charValue;
        int32_t valueLength;
        while ((charValue = values->next(&valueLength, status)) != nullptr) {
            if (valueLength == 6 && uprv_strcmp(charValue, "hebrew") == 0) {
                containsHebrew = true;
            }
        }
        if (!containsHebrew) {
            errln("Calendar::getKeywordValuesForLocale(he)->next() does not contain \"hebrew\"");
        }

        values->reset(status);
        containsHebrew = false;
        UnicodeString hebrew = UNICODE_STRING_SIMPLE("hebrew");
        const char16_t *ucharValue;
        while ((ucharValue = values->unext(&valueLength, status)) != nullptr) {
            UnicodeString value(false, ucharValue, valueLength);
            if (value == hebrew) {
                containsHebrew = true;
            }
        }
        if (!containsHebrew) {
            errln("Calendar::getKeywordValuesForLocale(he)->unext() does not contain \"hebrew\"");
        }

        values->reset(status);
        containsHebrew = false;
        const UnicodeString *stringValue;
        while ((stringValue = values->snext(status)) != nullptr) {
            if (*stringValue == hebrew) {
                containsHebrew = true;
            }
        }
        if (!containsHebrew) {
            errln("Calendar::getKeywordValuesForLocale(he)->snext() does not contain \"hebrew\"");
        }
    }
    delete cal;
}

// -------------------------------------

/**
 * This test confirms the correct behavior of add when incrementing
 * through subsequent days.
 */
void
CalendarTest::TestRog()
{
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar* gc = new GregorianCalendar(status);
    if (failure(status, "new GregorianCalendar", true)) return;
    int32_t year = 1997, month = UCAL_APRIL, date = 1;
    gc->set(year, month, date);
    gc->set(UCAL_HOUR_OF_DAY, 23);
    gc->set(UCAL_MINUTE, 0);
    gc->set(UCAL_SECOND, 0);
    gc->set(UCAL_MILLISECOND, 0);
    for (int32_t i = 0; i < 9; i++, gc->add(UCAL_DATE, 1, status)) {
        if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
        if (gc->get(UCAL_YEAR, status) != year ||
            gc->get(UCAL_MONTH, status) != month ||
            gc->get(UCAL_DATE, status) != (date + i)) errln("FAIL: Date wrong");
        if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
    }
    delete gc;
}

// -------------------------------------

/**
 * Test the handling of the day of the week, checking for correctness and
 * for correct minimum and maximum values.
 */
void
CalendarTest::TestDOW943()
{
    dowTest(false);
    dowTest(true);
}

void CalendarTest::dowTest(UBool lenient)
{
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar* cal = new GregorianCalendar(status);
    if (failure(status, "new GregorianCalendar", true)) return;
    logln("cal - Aug 12, 1997\n");
    cal->set(1997, UCAL_AUGUST, 12);
    cal->getTime(status);
    if (U_FAILURE(status)) { errln("Calendar::getTime failed"); return; }
    logln((lenient?UnicodeString("LENIENT0: "):UnicodeString("nonlenient0: ")) + UnicodeString(calToStr(*cal)));
    cal->setLenient(lenient);
    logln("cal - Dec 1, 1996\n");
    cal->set(1996, UCAL_DECEMBER, 1);
    logln((lenient?UnicodeString("LENIENT: "):UnicodeString("nonlenient: ")) + UnicodeString(calToStr(*cal)));
    int32_t dow = cal->get(UCAL_DAY_OF_WEEK, status);
    if (U_FAILURE(status)) { errln("Calendar::get failed [%s]", u_errorName(status)); return; }
    int32_t min = cal->getMinimum(UCAL_DAY_OF_WEEK);
    int32_t max = cal->getMaximum(UCAL_DAY_OF_WEEK);
    if (dow < min ||
        dow > max) errln(UnicodeString("FAIL: Day of week ") + (int32_t)dow + " out of range");
    if (dow != UCAL_SUNDAY) errln("FAIL: Day of week should be SUNDAY[%d] not %d", UCAL_SUNDAY, dow);
    if (min != UCAL_SUNDAY ||
        max != UCAL_SATURDAY) errln("FAIL: Min/max bad");
    delete cal;
}

// -------------------------------------

/**
 * Confirm that cloned Calendar objects do not inadvertently share substructures.
 */
void
CalendarTest::TestClonesUnique908()
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *c = Calendar::createInstance(status);
    if (failure(status, "Calendar::createInstance", true)) return;
    Calendar *d = c->clone();
    c->set(UCAL_MILLISECOND, 123);
    d->set(UCAL_MILLISECOND, 456);
    if (c->get(UCAL_MILLISECOND, status) != 123 ||
        d->get(UCAL_MILLISECOND, status) != 456) {
        errln("FAIL: Clones share fields");
    }
    if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
    delete c;
    delete d;
}

// -------------------------------------

/**
 * Confirm that the Gregorian cutoff value works as advertised.
 */
void
CalendarTest::TestGregorianChange768()
{
    UBool b;
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString str;
    GregorianCalendar* c = new GregorianCalendar(status);
    if (failure(status, "new GregorianCalendar", true)) return;
    logln(UnicodeString("With cutoff ") + dateToString(c->getGregorianChange(), str));
    b = c->isLeapYear(1800);
    logln(UnicodeString(" isLeapYear(1800) = ") + (b ? "true" : "false"));
    logln(UnicodeString(" (should be false)"));
    if (b) errln("FAIL");
    c->setGregorianChange(date(0, 0, 1), status);
    if (U_FAILURE(status)) { errln("GregorianCalendar::setGregorianChange failed"); return; }
    logln(UnicodeString("With cutoff ") + dateToString(c->getGregorianChange(), str));
    b = c->isLeapYear(1800);
    logln(UnicodeString(" isLeapYear(1800) = ") + (b ? "true" : "false"));
    logln(UnicodeString(" (should be true)"));
    if (!b) errln("FAIL");
    delete c;
}

// -------------------------------------

/**
 * Confirm the functioning of the field disambiguation algorithm.
 */
void
CalendarTest::TestDisambiguation765()
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *c = Calendar::createInstance("en_US", status);
    if (failure(status, "Calendar::createInstance", true)) return;
    c->setLenient(false);
    c->clear();
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_MONTH, UCAL_JUNE);
    c->set(UCAL_DATE, 3);
    verify765("1997 third day of June = ", c, 1997, UCAL_JUNE, 3);
    c->clear();
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_MONTH, UCAL_JUNE);
    c->set(UCAL_DAY_OF_WEEK_IN_MONTH, 1);
    verify765("1997 first Tuesday in June = ", c, 1997, UCAL_JUNE, 3);
    c->clear();
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_MONTH, UCAL_JUNE);
    c->set(UCAL_DAY_OF_WEEK_IN_MONTH, - 1);
    verify765("1997 last Tuesday in June = ", c, 1997, UCAL_JUNE, 24);

    status = U_ZERO_ERROR;
    c->clear();
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_MONTH, UCAL_JUNE);
    c->set(UCAL_DAY_OF_WEEK_IN_MONTH, 0);
    c->getTime(status);
    verify765("1997 zero-th Tuesday in June = ", status);

    c->clear();
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_MONTH, UCAL_JUNE);
    c->set(UCAL_WEEK_OF_MONTH, 1);
    verify765("1997 Tuesday in week 1 of June = ", c, 1997, UCAL_JUNE, 3);
    c->clear();
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_MONTH, UCAL_JUNE);
    c->set(UCAL_WEEK_OF_MONTH, 5);
    verify765("1997 Tuesday in week 5 of June = ", c, 1997, UCAL_JULY, 1);

    status = U_ZERO_ERROR;
    c->clear();
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_MONTH, UCAL_JUNE);
    c->set(UCAL_WEEK_OF_MONTH, 0);
    c->setMinimalDaysInFirstWeek(1);
    c->getTime(status);
    verify765("1997 Tuesday in week 0 of June = ", status);

    /* Note: The following test used to expect YEAR 1997, WOY 1 to
     * resolve to a date in Dec 1996; that is, to behave as if
     * YEAR_WOY were 1997.  With the addition of a new explicit
     * YEAR_WOY field, YEAR_WOY must itself be set if that is what is
     * desired.  Using YEAR in combination with WOY is ambiguous, and
     * results in the first WOY/DOW day of the year satisfying the
     * given fields (there may be up to two such days). In this case,
     * it properly resolves to Tue Dec 30 1997, which has a WOY value
     * of 1 (for YEAR_WOY 1998) and a DOW of Tuesday, and falls in the
     * _calendar_ year 1997, as specified. - aliu */
    c->clear();
    c->set(UCAL_YEAR_WOY, 1997); // aliu
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_WEEK_OF_YEAR, 1);
    verify765("1997 Tuesday in week 1 of yearWOY = ", c, 1996, UCAL_DECEMBER, 31);
    c->clear(); // - add test for YEAR
    c->setMinimalDaysInFirstWeek(1);
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_WEEK_OF_YEAR, 1);
    verify765("1997 Tuesday in week 1 of year = ", c, 1997, UCAL_DECEMBER, 30);
    c->clear();
    c->set(UCAL_YEAR, 1997);
    c->set(UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    c->set(UCAL_WEEK_OF_YEAR, 10);
    verify765("1997 Tuesday in week 10 of year = ", c, 1997, UCAL_MARCH, 4);
    //try {

    // {sfb} week 0 is no longer a valid week of year
    /*c->clear();
    c->set(Calendar::YEAR, 1997);
    c->set(Calendar::DAY_OF_WEEK, Calendar::TUESDAY);
    //c->set(Calendar::WEEK_OF_YEAR, 0);
    c->set(Calendar::WEEK_OF_YEAR, 1);
    verify765("1997 Tuesday in week 0 of year = ", c, 1996, Calendar::DECEMBER, 24);*/

    //}
    //catch(IllegalArgumentException ex) {
    //    errln("FAIL: Exception seen:");
    //    ex.printStackTrace(log);
    //}
    delete c;
}

// -------------------------------------

void
CalendarTest::verify765(const UnicodeString& msg, Calendar* c, int32_t year, int32_t month, int32_t day)
{
    UnicodeString str;
    UErrorCode status = U_ZERO_ERROR;
    int32_t y = c->get(UCAL_YEAR, status);
    int32_t m = c->get(UCAL_MONTH, status);
    int32_t d = c->get(UCAL_DATE, status);
    if ( y == year &&
         m == month &&
         d == day) {
        if (U_FAILURE(status)) { errln("FAIL: Calendar::get failed"); return; }
        logln("PASS: " + msg + dateToString(c->getTime(status), str));
        if (U_FAILURE(status)) { errln("Calendar::getTime failed"); return; }
    }
    else {
        errln("FAIL: " + msg + dateToString(c->getTime(status), str) + "; expected " + (int32_t)year + "/" + (int32_t)(month + 1) + "/" + (int32_t)day +
            "; got " + (int32_t)y + "/" + (int32_t)(m + 1) + "/" + (int32_t)d + " for Locale: " + c->getLocaleID(ULOC_ACTUAL_LOCALE,status));
        if (U_FAILURE(status)) { errln("Calendar::getTime failed"); return; }
    }
}

// -------------------------------------

void
CalendarTest::verify765(const UnicodeString& msg/*, IllegalArgumentException e*/, UErrorCode status)
{
    if (status != U_ILLEGAL_ARGUMENT_ERROR) errln("FAIL: No IllegalArgumentException for " + msg);
    else logln("PASS: " + msg + "IllegalArgument as expected");
}

// -------------------------------------

/**
 * Confirm that the offset between local time and GMT behaves as expected.
 */
void
CalendarTest::TestGMTvsLocal4064654()
{
    test4064654(1997, 1, 1, 12, 0, 0);
    test4064654(1997, 4, 16, 18, 30, 0);
}

// -------------------------------------

void
CalendarTest::test4064654(int32_t yr, int32_t mo, int32_t dt, int32_t hr, int32_t mn, int32_t sc)
{
    UDate date;
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString str;
    Calendar *gmtcal = Calendar::createInstance(status);
    if (failure(status, "Calendar::createInstance", true)) return;
    gmtcal->adoptTimeZone(TimeZone::createTimeZone("Africa/Casablanca"));
    gmtcal->set(yr, mo - 1, dt, hr, mn, sc);
    gmtcal->set(UCAL_MILLISECOND, 0);
    date = gmtcal->getTime(status);
    if (U_FAILURE(status)) { errln("Calendar::getTime failed"); return; }
    logln("date = " + dateToString(date, str));
    Calendar *cal = Calendar::createInstance(status);
    if (U_FAILURE(status)) { errln("Calendar::createInstance failed"); return; }
    cal->setTime(date, status);
    if (U_FAILURE(status)) { errln("Calendar::setTime failed"); return; }
    int32_t offset = cal->getTimeZone().getOffset((uint8_t)cal->get(UCAL_ERA, status),
                                                  cal->get(UCAL_YEAR, status),
                                                  cal->get(UCAL_MONTH, status),
                                                  cal->get(UCAL_DATE, status),
                                                  (uint8_t)cal->get(UCAL_DAY_OF_WEEK, status),
                                                  cal->get(UCAL_MILLISECOND, status), status);
    if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
    logln("offset for " + dateToString(date, str) + "= " + (offset / 1000 / 60 / 60.0) + "hr");
    int32_t utc = ((cal->get(UCAL_HOUR_OF_DAY, status) * 60 +
                    cal->get(UCAL_MINUTE, status)) * 60 +
                   cal->get(UCAL_SECOND, status)) * 1000 +
        cal->get(UCAL_MILLISECOND, status) - offset;
    if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
    int32_t expected = ((hr * 60 + mn) * 60 + sc) * 1000;
    if (utc != expected) errln(UnicodeString("FAIL: Discrepancy of ") + (utc - expected) +
                               " millis = " + ((utc - expected) / 1000 / 60 / 60.0) + " hr");
    delete gmtcal;
    delete cal;
}

// -------------------------------------

/**
 * The operations of adding and setting should not exhibit pathological
 * dependence on the order of operations.  This test checks for this.
 */
void
CalendarTest::TestAddSetOrder621()
{
    UDate d = date(97, 4, 14, 13, 23, 45);
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = Calendar::createInstance(status);
    if (failure(status, "Calendar::createInstance", true)) return;

    cal->setTime(d, status);
    if (U_FAILURE(status)) {
        errln("Calendar::setTime failed");
        delete cal;
        return;
    }
    cal->add(UCAL_DATE, - 5, status);
    if (U_FAILURE(status)) {
        errln("Calendar::add failed");
        delete cal;
        return;
    }
    cal->set(UCAL_HOUR_OF_DAY, 0);
    cal->set(UCAL_MINUTE, 0);
    cal->set(UCAL_SECOND, 0);
    UnicodeString s;
    dateToString(cal->getTime(status), s);
    if (U_FAILURE(status)) {
        errln("Calendar::getTime failed");
        delete cal;
        return;
    }
    delete cal;

    cal = Calendar::createInstance(status);
    if (U_FAILURE(status)) {
        errln("Calendar::createInstance failed");
        delete cal;
        return;
    }
    cal->setTime(d, status);
    if (U_FAILURE(status)) {
        errln("Calendar::setTime failed");
        delete cal;
        return;
    }
    cal->set(UCAL_HOUR_OF_DAY, 0);
    cal->set(UCAL_MINUTE, 0);
    cal->set(UCAL_SECOND, 0);
    cal->add(UCAL_DATE, - 5, status);
    if (U_FAILURE(status)) {
        errln("Calendar::add failed");
        delete cal;
        return;
    }
    UnicodeString s2;
    dateToString(cal->getTime(status), s2);
    if (U_FAILURE(status)) {
        errln("Calendar::getTime failed");
        delete cal;
        return;
    }
    if (s == s2)
        logln("Pass: " + s + " == " + s2);
    else
        errln("FAIL: " + s + " != " + s2);
    delete cal;
}

// -------------------------------------

/**
 * Confirm that adding to various fields works.
 */
void
CalendarTest::TestAdd520()
{
    int32_t y = 1997, m = UCAL_FEBRUARY, d = 1;
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar *temp = new GregorianCalendar(y, m, d, status);
    if (failure(status, "new GregorianCalendar", true)) return;
    check520(temp, y, m, d);
    temp->add(UCAL_YEAR, 1, status);
    if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
    y++;
    check520(temp, y, m, d);
    temp->add(UCAL_MONTH, 1, status);
    if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
    m++;
    check520(temp, y, m, d);
    temp->add(UCAL_DATE, 1, status);
    if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
    d++;
    check520(temp, y, m, d);
    temp->add(UCAL_DATE, 2, status);
    if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
    d += 2;
    check520(temp, y, m, d);
    temp->add(UCAL_DATE, 28, status);
    if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
    d = 1;++m;
    check520(temp, y, m, d);
    delete temp;
}

// -------------------------------------

/**
 * Execute adding and rolling in GregorianCalendar extensively,
 */
void
CalendarTest::TestAddRollExtensive()
{
    int32_t maxlimit = 40;
    int32_t y = 1997, m = UCAL_FEBRUARY, d = 1, hr = 1, min = 1, sec = 0, ms = 0;
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar *temp = new GregorianCalendar(y, m, d, status);
    if (failure(status, "new GregorianCalendar", true)) return;

    temp->set(UCAL_HOUR, hr);
    temp->set(UCAL_MINUTE, min);
    temp->set(UCAL_SECOND, sec);
    temp->set(UCAL_MILLISECOND, ms);
    temp->setMinimalDaysInFirstWeek(1);

    UCalendarDateFields e;

    logln("Testing GregorianCalendar add...");
    e = UCAL_YEAR;
    while (e < UCAL_FIELD_COUNT) {
        int32_t i;
        int32_t limit = maxlimit;
        status = U_ZERO_ERROR;
        for (i = 0; i < limit; i++) {
            temp->add(e, 1, status);
            if (U_FAILURE(status)) {
                limit = i;
                status = U_ZERO_ERROR;
                break;      // Suppress compile warning. Shouldn't be necessary, but it is.
            }
        }
        for (i = 0; i < limit; i++) {
            temp->add(e, -1, status);
            if (U_FAILURE(status)) { errln("GregorianCalendar::add -1 failed"); return; }
        }
        check520(temp, y, m, d, hr, min, sec, ms, e);

        e = (UCalendarDateFields) ((int32_t) e + 1);
    }

    logln("Testing GregorianCalendar roll...");
    e = UCAL_YEAR;
    while (e < UCAL_FIELD_COUNT) {
        int32_t i;
        int32_t limit = maxlimit;
        status = U_ZERO_ERROR;
        for (i = 0; i < limit; i++) {
            logln(calToStr(*temp) + UnicodeString("  " ) + fieldName(e) + UnicodeString("++") );
            temp->roll(e, 1, status);
            if (U_FAILURE(status)) {
              logln("caltest.cpp:%d e=%d, i=%d - roll(+) err %s\n",  __LINE__, (int) e, (int) i, u_errorName(status));
              logln(calToStr(*temp));
              limit = i; status = U_ZERO_ERROR;
            }
        }
        for (i = 0; i < limit; i++) {
            logln("caltest.cpp:%d e=%d, i=%d\n",  __LINE__, (int) e, (int) i);
            logln(calToStr(*temp) + UnicodeString("  " ) + fieldName(e) + UnicodeString("--") );
            temp->roll(e, -1, status);
            if (U_FAILURE(status)) { errln(UnicodeString("GregorianCalendar::roll ") + CalendarTest::fieldName(e) + " count=" + UnicodeString('@'+i) + " by -1 failed with " + u_errorName(status) ); return; }
        }
        check520(temp, y, m, d, hr, min, sec, ms, e);

        e = (UCalendarDateFields) ((int32_t) e + 1);
    }

    delete temp;
}

// -------------------------------------
void
CalendarTest::check520(Calendar* c,
                        int32_t y, int32_t m, int32_t d,
                        int32_t hr, int32_t min, int32_t sec,
                        int32_t ms, UCalendarDateFields field)

{
    UErrorCode status = U_ZERO_ERROR;
    if (c->get(UCAL_YEAR, status) != y ||
        c->get(UCAL_MONTH, status) != m ||
        c->get(UCAL_DATE, status) != d ||
        c->get(UCAL_HOUR, status) != hr ||
        c->get(UCAL_MINUTE, status) != min ||
        c->get(UCAL_SECOND, status) != sec ||
        c->get(UCAL_MILLISECOND, status) != ms) {
        errln(UnicodeString("U_FAILURE for field ") + (int32_t)field +
                ": Expected y/m/d h:m:s:ms of " +
                y + "/" + (m + 1) + "/" + d + " " +
              hr + ":" + min + ":" + sec + ":" + ms +
              "; got " + c->get(UCAL_YEAR, status) +
              "/" + (c->get(UCAL_MONTH, status) + 1) +
              "/" + c->get(UCAL_DATE, status) +
              " " + c->get(UCAL_HOUR, status) + ":" +
              c->get(UCAL_MINUTE, status) + ":" +
              c->get(UCAL_SECOND, status) + ":" +
              c->get(UCAL_MILLISECOND, status)
              );

        if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
    }
    else
        logln(UnicodeString("Confirmed: ") + y + "/" +
                (m + 1) + "/" + d + " " +
                hr + ":" + min + ":" + sec + ":" + ms);
}

// -------------------------------------
void
CalendarTest::check520(Calendar* c,
                        int32_t y, int32_t m, int32_t d)

{
    UErrorCode status = U_ZERO_ERROR;
    if (c->get(UCAL_YEAR, status) != y ||
        c->get(UCAL_MONTH, status) != m ||
        c->get(UCAL_DATE, status) != d) {
        errln(UnicodeString("FAILURE: Expected y/m/d of ") +
              y + "/" + (m + 1) + "/" + d + " " +
              "; got " + c->get(UCAL_YEAR, status) +
              "/" + (c->get(UCAL_MONTH, status) + 1) +
              "/" + c->get(UCAL_DATE, status)
              );

        if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
    }
    else
        logln(UnicodeString("Confirmed: ") + y + "/" +
                (m + 1) + "/" + d);
}

// -------------------------------------

/**
 * Test that setting of fields works.  In particular, make sure that all instances
 * of GregorianCalendar don't share a static instance of the fields array.
 */
void
CalendarTest::TestFieldSet4781()
{
    // try {
        UErrorCode status = U_ZERO_ERROR;
        GregorianCalendar *g = new GregorianCalendar(status);
        if (failure(status, "new GregorianCalendar", true)) return;
        GregorianCalendar *g2 = new GregorianCalendar(status);
        if (U_FAILURE(status)) { errln("Couldn't create GregorianCalendar"); return; }
        g2->set(UCAL_HOUR, 12, status);
        g2->set(UCAL_MINUTE, 0, status);
        g2->set(UCAL_SECOND, 0, status);
        if (U_FAILURE(status)) { errln("Calendar::set failed"); return; }
        if (*g == *g2) logln("Same");
        else logln("Different");
    //}
        //catch(IllegalArgumentException e) {
        //errln("Unexpected exception seen: " + e);
    //}
        delete g;
        delete g2;
}

// -------------------------------------

/* We don't support serialization on C++
void
CalendarTest::TestSerialize337()
{
    Calendar cal = Calendar::getInstance();
    UBool ok = false;
    try {
        FileOutputStream f = new FileOutputStream(FILENAME);
        ObjectOutput s = new ObjectOutputStream(f);
        s.writeObject(PREFIX);
        s.writeObject(cal);
        s.writeObject(POSTFIX);
        f.close();
        FileInputStream in = new FileInputStream(FILENAME);
        ObjectInputStream t = new ObjectInputStream(in);
        UnicodeString& pre = (UnicodeString&) t.readObject();
        Calendar c = (Calendar) t.readObject();
        UnicodeString& post = (UnicodeString&) t.readObject();
        in.close();
        ok = pre.equals(PREFIX) &&
            post.equals(POSTFIX) &&
            cal->equals(c);
        File fl = new File(FILENAME);
        fl.delete();
    }
    catch(IOException e) {
        errln("FAIL: Exception received:");
        e.printStackTrace(log);
    }
    catch(ClassNotFoundException e) {
        errln("FAIL: Exception received:");
        e.printStackTrace(log);
    }
    if (!ok) errln("Serialization of Calendar object failed.");
}

UnicodeString& CalendarTest::PREFIX = "abc";

UnicodeString& CalendarTest::POSTFIX = "def";

UnicodeString& CalendarTest::FILENAME = "tmp337.bin";
 */

// -------------------------------------

/**
 * Verify that the seconds of a Calendar can be zeroed out through the
 * expected sequence of operations.
 */
void
CalendarTest::TestSecondsZero121()
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = new GregorianCalendar(status);
    if (failure(status, "new GregorianCalendar", true)) return;
    cal->setTime(Calendar::getNow(), status);
    if (U_FAILURE(status)) { errln("Calendar::setTime failed"); return; }
    cal->set(UCAL_SECOND, 0);
    if (U_FAILURE(status)) { errln("Calendar::set failed"); return; }
    UDate d = cal->getTime(status);
    if (U_FAILURE(status)) { errln("Calendar::getTime failed"); return; }
    UnicodeString s;
    dateToString(d, s);
    if (s.indexOf("DATE_FORMAT_FAILURE") >= 0) {
        dataerrln("Got: \"DATE_FORMAT_FAILURE\".");
    } else if (s.indexOf(":00 ") < 0) {
        errln("Expected to see :00 in " + s);
    }
    delete cal;
}

// -------------------------------------

/**
 * Verify that a specific sequence of adding and setting works as expected;
 * it should not vary depending on when and whether the get method is
 * called.
 */
void
CalendarTest::TestAddSetGet0610()
{
    UnicodeString EXPECTED_0610("1993/0/5", "");
    UErrorCode status = U_ZERO_ERROR;
    {
        Calendar *calendar = new GregorianCalendar(status);
        if (failure(status, "new GregorianCalendar", true)) return;
        calendar->set(1993, UCAL_JANUARY, 4);
        logln("1A) " + value(calendar));
        calendar->add(UCAL_DATE, 1, status);
        if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
        UnicodeString v = value(calendar);
        logln("1B) " + v);
        logln("--) 1993/0/5");
        if (!(v == EXPECTED_0610)) errln("Expected " + EXPECTED_0610 + "; saw " + v);
        delete calendar;
    }
    {
        Calendar *calendar = new GregorianCalendar(1993, UCAL_JANUARY, 4, status);
        if (U_FAILURE(status)) { errln("Couldn't create GregorianCalendar"); return; }
        logln("2A) " + value(calendar));
        calendar->add(UCAL_DATE, 1, status);
        if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
        UnicodeString v = value(calendar);
        logln("2B) " + v);
        logln("--) 1993/0/5");
        if (!(v == EXPECTED_0610)) errln("Expected " + EXPECTED_0610 + "; saw " + v);
        delete calendar;
    }
    {
        Calendar *calendar = new GregorianCalendar(1993, UCAL_JANUARY, 4, status);
        if (U_FAILURE(status)) { errln("Couldn't create GregorianCalendar"); return; }
        logln("3A) " + value(calendar));
        calendar->getTime(status);
        if (U_FAILURE(status)) { errln("Calendar::getTime failed"); return; }
        calendar->add(UCAL_DATE, 1, status);
        if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
        UnicodeString v = value(calendar);
        logln("3B) " + v);
        logln("--) 1993/0/5");
        if (!(v == EXPECTED_0610)) errln("Expected " + EXPECTED_0610 + "; saw " + v);
        delete calendar;
    }
}

// -------------------------------------

UnicodeString
CalendarTest::value(Calendar* calendar)
{
    UErrorCode status = U_ZERO_ERROR;
    return UnicodeString("") + (int32_t)calendar->get(UCAL_YEAR, status) +
        "/" + (int32_t)calendar->get(UCAL_MONTH, status) +
        "/" + (int32_t)calendar->get(UCAL_DATE, status) +
        (U_FAILURE(status) ? " FAIL: Calendar::get failed" : "");
}


// -------------------------------------

/**
 * Verify that various fields on a known date are set correctly.
 */
void
CalendarTest::TestFields060()
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t year = 1997;
    int32_t month = UCAL_OCTOBER;
    int32_t dDate = 22;
    GregorianCalendar* calendar = nullptr;
    calendar = new GregorianCalendar(year, month, dDate, status);
    if (failure(status, "new GregorianCalendar", true)) return;
    for (int32_t i = 0; i < EXPECTED_FIELDS_length;) {
        UCalendarDateFields field = (UCalendarDateFields)EXPECTED_FIELDS[i++];
        int32_t expected = EXPECTED_FIELDS[i++];
        if (calendar->get(field, status) != expected) {
            errln(UnicodeString("Expected field ") + (int32_t)field + " to have value " + (int32_t)expected +
                  "; received " + (int32_t)calendar->get(field, status) + " instead");
            if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
        }
    }
    delete calendar;
}

int32_t CalendarTest::EXPECTED_FIELDS[] = {
    UCAL_YEAR, 1997,
    UCAL_MONTH, UCAL_OCTOBER,
    UCAL_DATE, 22,
    UCAL_DAY_OF_WEEK, UCAL_WEDNESDAY,
    UCAL_DAY_OF_WEEK_IN_MONTH, 4,
    UCAL_DAY_OF_YEAR, 295
};

const int32_t CalendarTest::EXPECTED_FIELDS_length = (int32_t)(sizeof(CalendarTest::EXPECTED_FIELDS) /
    sizeof(CalendarTest::EXPECTED_FIELDS[0]));

// -------------------------------------

/**
 * Verify that various fields on a known date are set correctly.  In this
 * case, the start of the epoch (January 1 1970).
 */
void
CalendarTest::TestEpochStartFields()
{
    UErrorCode status = U_ZERO_ERROR;
    TimeZone *z = TimeZone::createDefault();
    Calendar *c = Calendar::createInstance(status);
    if (failure(status, "Calendar::createInstance", true)) return;
    UDate d = - z->getRawOffset();
    GregorianCalendar *gc = new GregorianCalendar(status);
    if (U_FAILURE(status)) { errln("Couldn't create GregorianCalendar"); return; }
    gc->setTimeZone(*z);
    gc->setTime(d, status);
    if (U_FAILURE(status)) { errln("Calendar::setTime failed"); return; }
    UBool idt = gc->inDaylightTime(status);
    if (U_FAILURE(status)) { errln("GregorianCalendar::inDaylightTime failed"); return; }
    if (idt) {
        UnicodeString str;
        logln("Warning: Skipping test because " + dateToString(d, str) + " is in DST.");
    }
    else {
        c->setTime(d, status);
        if (U_FAILURE(status)) { errln("Calendar::setTime failed"); return; }
        for (int32_t i = 0; i < UCAL_ZONE_OFFSET;++i) {
            if (c->get((UCalendarDateFields)i, status) != EPOCH_FIELDS[i])
                dataerrln(UnicodeString("Expected field ") + i + " to have value " + EPOCH_FIELDS[i] +
                      "; saw " + c->get((UCalendarDateFields)i, status) + " instead");
            if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
        }
        if (c->get(UCAL_ZONE_OFFSET, status) != z->getRawOffset())
        {
            errln(UnicodeString("Expected field ZONE_OFFSET to have value ") + z->getRawOffset() +
                  "; saw " + c->get(UCAL_ZONE_OFFSET, status) + " instead");
            if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
        }
        if (c->get(UCAL_DST_OFFSET, status) != 0)
        {
            errln(UnicodeString("Expected field DST_OFFSET to have value 0") +
                  "; saw " + c->get(UCAL_DST_OFFSET, status) + " instead");
            if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
        }
    }
    delete c;
    delete z;
    delete gc;
}

int32_t CalendarTest::EPOCH_FIELDS[] = {
    1, 1970, 0, 1, 1, 1, 1, 5, 1, 0, 0, 0, 0, 0, 0, - 28800000, 0
};

// -------------------------------------

/**
 * Test that the days of the week progress properly when add is called repeatedly
 * for increments of 24 days.
 */
void
CalendarTest::TestDOWProgression()
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = new GregorianCalendar(1972, UCAL_OCTOBER, 26, status);
    if (failure(status, "new GregorianCalendar", true)) return;
    marchByDelta(cal, 24);
    delete cal;
}

// -------------------------------------

void
CalendarTest::TestDOW_LOCALandYEAR_WOY()
{
    /* Note: I've commented out the loop_addroll tests for YEAR and
     * YEAR_WOY below because these two fields should NOT behave
     * identically when adding.  YEAR should keep the month/dom
     * invariant.  YEAR_WOY should keep the woy/dow invariant.  I've
     * added a new test that checks for this in place of the old call
     * to loop_addroll. - aliu */
    UErrorCode status = U_ZERO_ERROR;
    int32_t times = 20;
    Calendar *cal=Calendar::createInstance(Locale::getGermany(), status);
    if (failure(status, "Calendar::createInstance", true)) return;
    SimpleDateFormat *sdf=new SimpleDateFormat(UnicodeString("YYYY'-W'ww-ee"), Locale::getGermany(), status);
    if (U_FAILURE(status)) { dataerrln("Couldn't create SimpleDateFormat - %s", u_errorName(status)); return; }

    // ICU no longer use localized date-time pattern characters by default.
    // So we set pattern chars using 'J' instead of 'Y'.
    DateFormatSymbols *dfs = new DateFormatSymbols(Locale::getGermany(), status);
    dfs->setLocalPatternChars(UnicodeString("GyMdkHmsSEDFwWahKzJeugAZvcLQq"));
    sdf->adoptDateFormatSymbols(dfs);
    sdf->applyLocalizedPattern(UnicodeString("JJJJ'-W'ww-ee"), status);
    if (U_FAILURE(status)) { errln("Couldn't apply localized pattern"); return; }

    cal->clear();
    cal->set(1997, UCAL_DECEMBER, 25);
    doYEAR_WOYLoop(cal, sdf, times, status);
    //loop_addroll(cal, /*sdf,*/ times, UCAL_YEAR_WOY, UCAL_YEAR,  status);
    yearAddTest(*cal, status); // aliu
    loop_addroll(cal, /*sdf,*/ times, UCAL_DOW_LOCAL, UCAL_DAY_OF_WEEK, status);
    if (U_FAILURE(status)) { errln("Error in parse/calculate test for 1997"); return; }

    cal->clear();
    cal->set(1998, UCAL_DECEMBER, 25);
    doYEAR_WOYLoop(cal, sdf, times, status);
    //loop_addroll(cal, /*sdf,*/ times, UCAL_YEAR_WOY, UCAL_YEAR,  status);
    yearAddTest(*cal, status); // aliu
    loop_addroll(cal, /*sdf,*/ times, UCAL_DOW_LOCAL, UCAL_DAY_OF_WEEK, status);
    if (U_FAILURE(status)) { errln("Error in parse/calculate test for 1998"); return; }

    cal->clear();
    cal->set(1582, UCAL_OCTOBER, 1);
    doYEAR_WOYLoop(cal, sdf, times, status);
    //loop_addroll(cal, /*sdf,*/ times, Calendar::YEAR_WOY, Calendar::YEAR,  status);
    yearAddTest(*cal, status); // aliu
    loop_addroll(cal, /*sdf,*/ times, UCAL_DOW_LOCAL, UCAL_DAY_OF_WEEK, status);
    if (U_FAILURE(status)) { errln("Error in parse/calculate test for 1582"); return; }
    delete sdf;
    delete cal;
}

/**
 * Confirm that adding a YEAR and adding a YEAR_WOY work properly for
 * the given Calendar at its current setting.
 */
void CalendarTest::yearAddTest(Calendar& cal, UErrorCode& status) {
    /**
     * When adding the YEAR, the month and day should remain constant.
     * When adding the YEAR_WOY, the WOY and DOW should remain constant. - aliu
     * Examples:
     *  Wed Jan 14 1998 / 1998-W03-03 Add(YEAR_WOY, 1) -> Wed Jan 20 1999 / 1999-W03-03
     *                                Add(YEAR, 1)     -> Thu Jan 14 1999 / 1999-W02-04
     *  Thu Jan 14 1999 / 1999-W02-04 Add(YEAR_WOY, 1) -> Thu Jan 13 2000 / 2000-W02-04
     *                                Add(YEAR, 1)     -> Fri Jan 14 2000 / 2000-W02-05
     *  Sun Oct 31 1582 / 1582-W42-07 Add(YEAR_WOY, 1) -> Sun Oct 23 1583 / 1583-W42-07
     *                                Add(YEAR, 1)     -> Mon Oct 31 1583 / 1583-W44-01
     */
    int32_t y   = cal.get(UCAL_YEAR, status);
    int32_t mon = cal.get(UCAL_MONTH, status);
    int32_t day = cal.get(UCAL_DATE, status);
    int32_t ywy = cal.get(UCAL_YEAR_WOY, status);
    int32_t woy = cal.get(UCAL_WEEK_OF_YEAR, status);
    int32_t dow = cal.get(UCAL_DOW_LOCAL, status);
    UDate t = cal.getTime(status);

    if(U_FAILURE(status)){
        errln(UnicodeString("Failed to create Calendar for locale. Error: ") + UnicodeString(u_errorName(status)));
        return;
    }
    UnicodeString str, str2;
    SimpleDateFormat fmt(UnicodeString("EEE MMM dd yyyy / YYYY'-W'ww-ee"), status);
    fmt.setCalendar(cal);

    fmt.format(t, str.remove());
    str += ".add(YEAR, 1)    =>";
    cal.add(UCAL_YEAR, 1, status);
    int32_t y2   = cal.get(UCAL_YEAR, status);
    int32_t mon2 = cal.get(UCAL_MONTH, status);
    int32_t day2 = cal.get(UCAL_DATE, status);
    fmt.format(cal.getTime(status), str);
    if (y2 != (y+1) || mon2 != mon || day2 != day) {
        str += (UnicodeString)", expected year " +
            (y+1) + ", month " + (mon+1) + ", day " + day;
        errln((UnicodeString)"FAIL: " + str);
        logln( UnicodeString(" -> ") + CalendarTest::calToStr(cal) );
    } else {
        logln(str);
    }

    fmt.format(t, str.remove());
    str += ".add(YEAR_WOY, 1)=>";
    cal.setTime(t, status);
    logln( UnicodeString(" <- ") + CalendarTest::calToStr(cal) );
    cal.add(UCAL_YEAR_WOY, 1, status);
    int32_t ywy2 = cal.get(UCAL_YEAR_WOY, status);
    int32_t woy2 = cal.get(UCAL_WEEK_OF_YEAR, status);
    int32_t dow2 = cal.get(UCAL_DOW_LOCAL, status);
    fmt.format(cal.getTime(status), str);
    if (ywy2 != (ywy+1) || woy2 != woy || dow2 != dow) {
        str += (UnicodeString)", expected yearWOY " +
            (ywy+1) + ", woy " + woy + ", dowLocal " + dow;
        errln((UnicodeString)"FAIL: " + str);
        logln( UnicodeString(" -> ") + CalendarTest::calToStr(cal) );
    } else {
        logln(str);
    }
}

// -------------------------------------

void CalendarTest::loop_addroll(Calendar *cal, /*SimpleDateFormat *sdf,*/ int times, UCalendarDateFields field, UCalendarDateFields field2, UErrorCode& errorCode) {
    Calendar *calclone;
    SimpleDateFormat fmt(UnicodeString("EEE MMM dd yyyy / YYYY'-W'ww-ee"), errorCode);
    fmt.setCalendar(*cal);
    int i;

    for(i = 0; i<times; i++) {
        calclone = cal->clone();
        UDate start = cal->getTime(errorCode);
        cal->add(field,1,errorCode);
        if (U_FAILURE(errorCode)) { errln("Error in add"); delete calclone; return; }
        calclone->add(field2,1,errorCode);
        if (U_FAILURE(errorCode)) { errln("Error in add"); delete calclone; return; }
        if(cal->getTime(errorCode) != calclone->getTime(errorCode)) {
            UnicodeString str("FAIL: Results of add differ. "), str2;
            str += fmt.format(start, str2) + " ";
            str += UnicodeString("Add(") + fieldName(field) + ", 1) -> " +
                fmt.format(cal->getTime(errorCode), str2.remove()) + "; ";
            str += UnicodeString("Add(") + fieldName(field2) + ", 1) -> " +
                fmt.format(calclone->getTime(errorCode), str2.remove());
            errln(str);
            delete calclone;
            return;
        }
        delete calclone;
    }

    for(i = 0; i<times; i++) {
        calclone = cal->clone();
        cal->roll(field,(int32_t)1,errorCode);
        if (U_FAILURE(errorCode)) { errln("Error in roll"); delete calclone; return; }
        calclone->roll(field2,(int32_t)1,errorCode);
        if (U_FAILURE(errorCode)) { errln("Error in roll"); delete calclone; return; }
        if(cal->getTime(errorCode) != calclone->getTime(errorCode)) {
            delete calclone;
            errln("Results of roll differ!");
            return;
        }
        delete calclone;
    }
}

// -------------------------------------

void
CalendarTest::doYEAR_WOYLoop(Calendar *cal, SimpleDateFormat *sdf,
                                    int32_t times, UErrorCode& errorCode) {

    UnicodeString us;
    UDate tst, original;
    Calendar *tstres = new GregorianCalendar(Locale::getGermany(), errorCode);
    for(int i=0; i<times; ++i) {
        sdf->format(Formattable(cal->getTime(errorCode),Formattable::kIsDate), us, errorCode);
        //logln("expected: "+us);
        if (U_FAILURE(errorCode)) { errln("Format error"); return; }
        tst=sdf->parse(us,errorCode);
        if (U_FAILURE(errorCode)) { errln("Parse error"); return; }
        tstres->clear();
        tstres->setTime(tst, errorCode);
        //logln((UnicodeString)"Parsed week of year is "+tstres->get(UCAL_WEEK_OF_YEAR, errorCode));
        if (U_FAILURE(errorCode)) { errln("Set time error"); return; }
        original = cal->getTime(errorCode);
        us.remove();
        sdf->format(Formattable(tst,Formattable::kIsDate), us, errorCode);
        //logln("got: "+us);
        if (U_FAILURE(errorCode)) { errln("Get time error"); return; }
        if(original!=tst) {
            us.remove();
            sdf->format(Formattable(original, Formattable::kIsDate), us, errorCode);
            errln("FAIL: Parsed time doesn't match with regular");
            logln("expected "+us + " " + calToStr(*cal));
            us.remove();
            sdf->format(Formattable(tst, Formattable::kIsDate), us, errorCode);
            logln("got "+us + " " + calToStr(*tstres));
        }
        tstres->clear();
        tstres->set(UCAL_YEAR_WOY, cal->get(UCAL_YEAR_WOY, errorCode));
        tstres->set(UCAL_WEEK_OF_YEAR, cal->get(UCAL_WEEK_OF_YEAR, errorCode));
        tstres->set(UCAL_DOW_LOCAL, cal->get(UCAL_DOW_LOCAL, errorCode));
        if(cal->get(UCAL_YEAR, errorCode) != tstres->get(UCAL_YEAR, errorCode)) {
            errln("FAIL: Different Year!");
            logln((UnicodeString)"Expected "+cal->get(UCAL_YEAR, errorCode));
            logln((UnicodeString)"Got "+tstres->get(UCAL_YEAR, errorCode));
            return;
        }
        if(cal->get(UCAL_DAY_OF_YEAR, errorCode) != tstres->get(UCAL_DAY_OF_YEAR, errorCode)) {
            errln("FAIL: Different Day Of Year!");
            logln((UnicodeString)"Expected "+cal->get(UCAL_DAY_OF_YEAR, errorCode));
            logln((UnicodeString)"Got "+tstres->get(UCAL_DAY_OF_YEAR, errorCode));
            return;
        }
        //logln(calToStr(*cal));
        cal->add(UCAL_DATE, 1, errorCode);
        if (U_FAILURE(errorCode)) { errln("Add error"); return; }
        us.remove();
    }
    delete (tstres);
}
// -------------------------------------

void
CalendarTest::marchByDelta(Calendar* cal, int32_t delta)
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cur = cal->clone();
    int32_t initialDOW = cur->get(UCAL_DAY_OF_WEEK, status);
    if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
    int32_t DOW, newDOW = initialDOW;
    do {
        UnicodeString str;
        DOW = newDOW;
        logln(UnicodeString("DOW = ") + DOW + "  " + dateToString(cur->getTime(status), str));
        if (U_FAILURE(status)) { errln("Calendar::getTime failed"); return; }
        cur->add(UCAL_DAY_OF_WEEK, delta, status);
        if (U_FAILURE(status)) { errln("Calendar::add failed"); return; }
        newDOW = cur->get(UCAL_DAY_OF_WEEK, status);
        if (U_FAILURE(status)) { errln("Calendar::get failed"); return; }
        int32_t expectedDOW = 1 + (DOW + delta - 1) % 7;
        if (newDOW != expectedDOW) {
            errln(UnicodeString("Day of week should be ") + expectedDOW + " instead of " + newDOW +
                  " on " + dateToString(cur->getTime(status), str));
            if (U_FAILURE(status)) { errln("Calendar::getTime failed"); return; }
            return;
        }
    }
    while (newDOW != initialDOW);
    delete cur;
}

#define CHECK(status, msg) UPRV_BLOCK_MACRO_BEGIN { \
    if (U_FAILURE(status)) { \
        errcheckln(status, msg); \
        return; \
    } \
} UPRV_BLOCK_MACRO_END

void CalendarTest::TestWOY() {
    /*
      FDW = Mon, MDFW = 4:
         Sun Dec 26 1999, WOY 51
         Mon Dec 27 1999, WOY 52
         Tue Dec 28 1999, WOY 52
         Wed Dec 29 1999, WOY 52
         Thu Dec 30 1999, WOY 52
         Fri Dec 31 1999, WOY 52
         Sat Jan 01 2000, WOY 52 ***
         Sun Jan 02 2000, WOY 52 ***
         Mon Jan 03 2000, WOY 1
         Tue Jan 04 2000, WOY 1
         Wed Jan 05 2000, WOY 1
         Thu Jan 06 2000, WOY 1
         Fri Jan 07 2000, WOY 1
         Sat Jan 08 2000, WOY 1
         Sun Jan 09 2000, WOY 1
         Mon Jan 10 2000, WOY 2

      FDW = Mon, MDFW = 2:
         Sun Dec 26 1999, WOY 52
         Mon Dec 27 1999, WOY 1  ***
         Tue Dec 28 1999, WOY 1  ***
         Wed Dec 29 1999, WOY 1  ***
         Thu Dec 30 1999, WOY 1  ***
         Fri Dec 31 1999, WOY 1  ***
         Sat Jan 01 2000, WOY 1
         Sun Jan 02 2000, WOY 1
         Mon Jan 03 2000, WOY 2
         Tue Jan 04 2000, WOY 2
         Wed Jan 05 2000, WOY 2
         Thu Jan 06 2000, WOY 2
         Fri Jan 07 2000, WOY 2
         Sat Jan 08 2000, WOY 2
         Sun Jan 09 2000, WOY 2
         Mon Jan 10 2000, WOY 3
    */

    UnicodeString str;
    UErrorCode status = U_ZERO_ERROR;
    int32_t i;

    GregorianCalendar cal(status);
    SimpleDateFormat fmt(UnicodeString("EEE MMM dd yyyy', WOY' w"), status);
    if (failure(status, "Cannot construct calendar/format", true)) return;

    UCalendarDaysOfWeek fdw = (UCalendarDaysOfWeek) 0;

    //for (int8_t pass=2; pass<=2; ++pass) {
    for (int8_t pass=1; pass<=2; ++pass) {
        switch (pass) {
        case 1:
            fdw = UCAL_MONDAY;
            cal.setFirstDayOfWeek(fdw);
            cal.setMinimalDaysInFirstWeek(4);
            fmt.adoptCalendar(cal.clone());
            break;
        case 2:
            fdw = UCAL_MONDAY;
            cal.setFirstDayOfWeek(fdw);
            cal.setMinimalDaysInFirstWeek(2);
            fmt.adoptCalendar(cal.clone());
            break;
        }

        //for (i=2; i<=6; ++i) {
        for (i=0; i<16; ++i) {
        UDate t, t2;
        int32_t t_y, t_woy, t_dow;
        cal.clear();
        cal.set(1999, UCAL_DECEMBER, 26 + i);
        fmt.format(t = cal.getTime(status), str.remove());
        CHECK(status, "Fail: getTime failed");
        logln(UnicodeString("* ") + str);
        int32_t dow = cal.get(UCAL_DAY_OF_WEEK, status);
        int32_t woy = cal.get(UCAL_WEEK_OF_YEAR, status);
        int32_t year = cal.get(UCAL_YEAR, status);
        int32_t mon = cal.get(UCAL_MONTH, status);
        logln(calToStr(cal));
        CHECK(status, "Fail: get failed");
        int32_t dowLocal = dow - fdw;
        if (dowLocal < 0) dowLocal += 7;
        dowLocal++;
        int32_t yearWoy = year;
        if (mon == UCAL_JANUARY) {
            if (woy >= 52) --yearWoy;
        } else {
            if (woy == 1) ++yearWoy;
        }

        // Basic fields->time check y/woy/dow
        // Since Y/WOY is ambiguous, we do a check of the fields,
        // not of the specific time.
        cal.clear();
        cal.set(UCAL_YEAR, year);
        cal.set(UCAL_WEEK_OF_YEAR, woy);
        cal.set(UCAL_DAY_OF_WEEK, dow);
        t_y = cal.get(UCAL_YEAR, status);
        t_woy = cal.get(UCAL_WEEK_OF_YEAR, status);
        t_dow = cal.get(UCAL_DAY_OF_WEEK, status);
        CHECK(status, "Fail: get failed");
        if (t_y != year || t_woy != woy || t_dow != dow) {
            str = "Fail: y/woy/dow fields->time => ";
            fmt.format(cal.getTime(status), str);
            errln(str);
            logln(calToStr(cal));
            logln("[get!=set] Y%d!=%d || woy%d!=%d || dow%d!=%d\n",
                  t_y, year, t_woy, woy, t_dow, dow);
        } else {
          logln("y/woy/dow fields->time OK");
        }

        // Basic fields->time check y/woy/dow_local
        // Since Y/WOY is ambiguous, we do a check of the fields,
        // not of the specific time.
        cal.clear();
        cal.set(UCAL_YEAR, year);
        cal.set(UCAL_WEEK_OF_YEAR, woy);
        cal.set(UCAL_DOW_LOCAL, dowLocal);
        t_y = cal.get(UCAL_YEAR, status);
        t_woy = cal.get(UCAL_WEEK_OF_YEAR, status);
        t_dow = cal.get(UCAL_DOW_LOCAL, status);
        CHECK(status, "Fail: get failed");
        if (t_y != year || t_woy != woy || t_dow != dowLocal) {
            str = "Fail: y/woy/dow_local fields->time => ";
            fmt.format(cal.getTime(status), str);
            errln(str);
        }

        // Basic fields->time check y_woy/woy/dow
        cal.clear();
        cal.set(UCAL_YEAR_WOY, yearWoy);
        cal.set(UCAL_WEEK_OF_YEAR, woy);
        cal.set(UCAL_DAY_OF_WEEK, dow);
        t2 = cal.getTime(status);
        CHECK(status, "Fail: getTime failed");
        if (t != t2) {
            str = "Fail: y_woy/woy/dow fields->time => ";
            fmt.format(t2, str);
            errln(str);
            logln(calToStr(cal));
            logln("%.f != %.f\n", t, t2);
        } else {
          logln("y_woy/woy/dow OK");
        }

        // Basic fields->time check y_woy/woy/dow_local
        cal.clear();
        cal.set(UCAL_YEAR_WOY, yearWoy);
        cal.set(UCAL_WEEK_OF_YEAR, woy);
        cal.set(UCAL_DOW_LOCAL, dowLocal);
        t2 = cal.getTime(status);
        CHECK(status, "Fail: getTime failed");
        if (t != t2) {
            str = "Fail: y_woy/woy/dow_local fields->time => ";
            fmt.format(t2, str);
            errln(str);
        }

        logln("Testing DOW_LOCAL.. dow%d\n", dow);
        // Make sure DOW_LOCAL disambiguates over DOW
        int32_t wrongDow = dow - 3;
        if (wrongDow < 1) wrongDow += 7;
        cal.setTime(t, status);
        cal.set(UCAL_DAY_OF_WEEK, wrongDow);
        cal.set(UCAL_DOW_LOCAL, dowLocal);
        t2 = cal.getTime(status);
        CHECK(status, "Fail: set/getTime failed");
        if (t != t2) {
            str = "Fail: DOW_LOCAL fields->time => ";
            fmt.format(t2, str);
            errln(str);
            logln(calToStr(cal));
            logln("%.f :   DOW%d, DOW_LOCAL%d -> %.f\n",
                  t, wrongDow, dowLocal, t2);
        }

        // Make sure DOW disambiguates over DOW_LOCAL
        int32_t wrongDowLocal = dowLocal - 3;
        if (wrongDowLocal < 1) wrongDowLocal += 7;
        cal.setTime(t, status);
        cal.set(UCAL_DOW_LOCAL, wrongDowLocal);
        cal.set(UCAL_DAY_OF_WEEK, dow);
        t2 = cal.getTime(status);
        CHECK(status, "Fail: set/getTime failed");
        if (t != t2) {
            str = "Fail: DOW       fields->time => ";
            fmt.format(t2, str);
            errln(str);
        }

        // Make sure YEAR_WOY disambiguates over YEAR
        cal.setTime(t, status);
        cal.set(UCAL_YEAR, year - 2);
        cal.set(UCAL_YEAR_WOY, yearWoy);
        t2 = cal.getTime(status);
        CHECK(status, "Fail: set/getTime failed");
        if (t != t2) {
            str = "Fail: YEAR_WOY  fields->time => ";
            fmt.format(t2, str);
            errln(str);
        }

        // Make sure YEAR disambiguates over YEAR_WOY
        cal.setTime(t, status);
        cal.set(UCAL_YEAR_WOY, yearWoy - 2);
        cal.set(UCAL_YEAR, year);
        t2 = cal.getTime(status);
        CHECK(status, "Fail: set/getTime failed");
        if (t != t2) {
            str = "Fail: YEAR      fields->time => ";
            fmt.format(t2, str);
            errln(str);
        }
    }
    }

    /*
      FDW = Mon, MDFW = 4:
         Sun Dec 26 1999, WOY 51
         Mon Dec 27 1999, WOY 52
         Tue Dec 28 1999, WOY 52
         Wed Dec 29 1999, WOY 52
         Thu Dec 30 1999, WOY 52
         Fri Dec 31 1999, WOY 52
         Sat Jan 01 2000, WOY 52
         Sun Jan 02 2000, WOY 52
    */

    // Roll the DOW_LOCAL within week 52
    for (i=27; i<=33; ++i) {
        int32_t amount;
        for (amount=-7; amount<=7; ++amount) {
            str = "roll(";
            cal.set(1999, UCAL_DECEMBER, i);
            UDate t, t2;
            fmt.format(cal.getTime(status), str);
            CHECK(status, "Fail: getTime failed");
            str += UnicodeString(", ") + amount + ") = ";

            cal.roll(UCAL_DOW_LOCAL, amount, status);
            CHECK(status, "Fail: roll failed");

            t = cal.getTime(status);
            int32_t newDom = i + amount;
            while (newDom < 27) newDom += 7;
            while (newDom > 33) newDom -= 7;
            cal.set(1999, UCAL_DECEMBER, newDom);
            t2 = cal.getTime(status);
            CHECK(status, "Fail: getTime failed");
            fmt.format(t, str);

            if (t != t2) {
                str.append(", exp ");
                fmt.format(t2, str);
                errln(str);
            } else {
                logln(str);
            }
        }
    }
}

void CalendarTest::TestYWOY()
{
   UnicodeString str;
   UErrorCode status = U_ZERO_ERROR;

   GregorianCalendar cal(status);
   if (failure(status, "construct GregorianCalendar", true)) return;

   cal.setFirstDayOfWeek(UCAL_SUNDAY);
   cal.setMinimalDaysInFirstWeek(1);

   logln("Setting:  ywoy=2004, woy=1, dow=MONDAY");
   cal.clear();
   cal.set(UCAL_YEAR_WOY,2004);
   cal.set(UCAL_WEEK_OF_YEAR,1);
   cal.set(UCAL_DAY_OF_WEEK, UCAL_MONDAY);

   logln(calToStr(cal));
   if(cal.get(UCAL_YEAR, status) != 2003) {
     errln("year not 2003");
   }

   logln("+ setting DOW to THURSDAY");
   cal.clear();
   cal.set(UCAL_YEAR_WOY,2004);
   cal.set(UCAL_WEEK_OF_YEAR,1);
   cal.set(UCAL_DAY_OF_WEEK, UCAL_THURSDAY);

   logln(calToStr(cal));
   if(cal.get(UCAL_YEAR, status) != 2004) {
     errln("year not 2004");
   }

   logln("+ setting DOW_LOCAL to 1");
   cal.clear();
   cal.set(UCAL_YEAR_WOY,2004);
   cal.set(UCAL_WEEK_OF_YEAR,1);
   cal.set(UCAL_DAY_OF_WEEK, UCAL_THURSDAY);
   cal.set(UCAL_DOW_LOCAL, 1);

   logln(calToStr(cal));
   if(cal.get(UCAL_YEAR, status) != 2003) {
     errln("year not 2003");
   }

   cal.setFirstDayOfWeek(UCAL_MONDAY);
   cal.setMinimalDaysInFirstWeek(4);
   UDate t = 946713600000.;
   cal.setTime(t, status);
   cal.set(UCAL_DAY_OF_WEEK, 4);
   cal.set(UCAL_DOW_LOCAL, 6);
   if(cal.getTime(status) != t) {
     logln(calToStr(cal));
     errln("FAIL:  DOW_LOCAL did not take precedence");
   }

}

void CalendarTest::TestJD()
{
  int32_t jd;
  static const int32_t kEpochStartAsJulianDay = 2440588;
  UErrorCode status = U_ZERO_ERROR;
  GregorianCalendar cal(status);
  if (failure(status, "construct GregorianCalendar", true)) return;
  cal.setTimeZone(*TimeZone::getGMT());
  cal.clear();
  jd = cal.get(UCAL_JULIAN_DAY, status);
  if(jd != kEpochStartAsJulianDay) {
    errln("Wanted JD of %d at time=0, [epoch 1970] but got %d\n", kEpochStartAsJulianDay, jd);
  } else {
    logln("Wanted JD of %d at time=0, [epoch 1970], got %d\n", kEpochStartAsJulianDay, jd);
  }

  cal.setTime(Calendar::getNow(), status);
  cal.clear();
  cal.set(UCAL_JULIAN_DAY, kEpochStartAsJulianDay);
  UDate epochTime = cal.getTime(status);
  if(epochTime != 0) {
    errln("Wanted time of 0 at jd=%d, got %.1lf\n", kEpochStartAsJulianDay, epochTime);
  } else {
    logln("Wanted time of 0 at jd=%d, got %.1lf\n", kEpochStartAsJulianDay, epochTime);
  }

}

// make sure the ctestfw utilities are in sync with the Calendar
void CalendarTest::TestDebug()
{
    for(int32_t  t=0;t<=UDBG_ENUM_COUNT;t++) {
        int32_t count = udbg_enumCount((UDebugEnumType)t);
        if(count == -1) {
            logln("enumCount(%d) returned -1", count);
            continue;
        }
        for(int32_t i=0;i<=count;i++) {
            if(t<=UDBG_HIGHEST_CONTIGUOUS_ENUM && i<count) {
                if( i!=udbg_enumArrayValue((UDebugEnumType)t, i)) {
                    errln("FAIL: udbg_enumArrayValue(%d,%d) returned %d, expected %d", t, i, udbg_enumArrayValue((UDebugEnumType)t,i), i);
                }
            } else {
                logln("Testing count+1:");
            }
                  const char *name = udbg_enumName((UDebugEnumType)t,i);
                  if(name==nullptr) {
                          if(i==count || t>UDBG_HIGHEST_CONTIGUOUS_ENUM  ) {
                                logln(" null name - expected.\n");
                          } else {
                                errln("FAIL: udbg_enumName(%d,%d) returned nullptr", t, i);
                          }
                          name = "(null)";
                  }
          logln("udbg_enumArrayValue(%d,%d) = %s, returned %d", t, i,
                      name, udbg_enumArrayValue((UDebugEnumType)t,i));
            logln("udbg_enumString = " + udbg_enumString((UDebugEnumType)t,i));
        }
        if(udbg_enumExpectedCount((UDebugEnumType)t) != count && t<=UDBG_HIGHEST_CONTIGUOUS_ENUM) {
            errln("FAIL: udbg_enumExpectedCount(%d): %d, != UCAL_FIELD_COUNT=%d ", t, udbg_enumExpectedCount((UDebugEnumType)t), count);
        } else {
            logln("udbg_ucal_fieldCount: %d, UCAL_FIELD_COUNT=udbg_enumCount %d ", udbg_enumExpectedCount((UDebugEnumType)t), count);
        }
    }
}


#undef CHECK

// List of interesting locales
const char *CalendarTest::testLocaleID(int32_t i)
{
  switch(i) {
  case 0: return "he_IL@calendar=hebrew";
  case 1: return "en_US@calendar=hebrew";
  case 2: return "fr_FR@calendar=hebrew";
  case 3: return "fi_FI@calendar=hebrew";
  case 4: return "nl_NL@calendar=hebrew";
  case 5: return "hu_HU@calendar=hebrew";
  case 6: return "nl_BE@currency=MTL;calendar=islamic";
  case 7: return "th_TH_TRADITIONAL@calendar=gregorian";
  case 8: return "ar_JO@calendar=islamic-civil";
  case 9: return "fi_FI@calendar=islamic";
  case 10: return "fr_CH@calendar=islamic-civil";
  case 11: return "he_IL@calendar=islamic-civil";
  case 12: return "hu_HU@calendar=buddhist";
  case 13: return "hu_HU@calendar=islamic";
  case 14: return "en_US@calendar=japanese";
  default: return nullptr;
  }
}

int32_t CalendarTest::testLocaleCount()
{
  static int32_t gLocaleCount = -1;
  if(gLocaleCount < 0) {
    int32_t i;
    for(i=0;testLocaleID(i) != nullptr;i++) {
      // do nothing
    }
    gLocaleCount = i;
  }
  return gLocaleCount;
}

static UDate doMinDateOfCalendar(Calendar* adopt, UBool &isGregorian, UErrorCode& status) {
  if(U_FAILURE(status)) return 0.0;

  adopt->clear();
  adopt->set(UCAL_EXTENDED_YEAR, adopt->getActualMinimum(UCAL_EXTENDED_YEAR, status));
  UDate ret = adopt->getTime(status);
  isGregorian = dynamic_cast<GregorianCalendar*>(adopt) != nullptr;
  delete adopt;
  return ret;
}

UDate CalendarTest::minDateOfCalendar(const Locale& locale, UBool &isGregorian, UErrorCode& status) {
  if(U_FAILURE(status)) return 0.0;
  return doMinDateOfCalendar(Calendar::createInstance(locale, status), isGregorian, status);
}

UDate CalendarTest::minDateOfCalendar(const Calendar& cal, UBool &isGregorian, UErrorCode& status) {
  if(U_FAILURE(status)) return 0.0;
  return doMinDateOfCalendar(cal.clone(), isGregorian, status);
}

void CalendarTest::Test6703()
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal;

    Locale loc1("en@calendar=fubar");
    cal = Calendar::createInstance(loc1, status);
    if (failure(status, "Calendar::createInstance", true)) return;
    delete cal;

    status = U_ZERO_ERROR;
    Locale loc2("en");
    cal = Calendar::createInstance(loc2, status);
    if (failure(status, "Calendar::createInstance")) return;
    delete cal;

    status = U_ZERO_ERROR;
    Locale loc3("en@calendar=roc");
    cal = Calendar::createInstance(loc3, status);
    if (failure(status, "Calendar::createInstance")) return;
    delete cal;
}

void CalendarTest::Test3785()
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString uzone = UNICODE_STRING_SIMPLE("Europe/Paris");
    UnicodeString exp1 = UNICODE_STRING_SIMPLE("Mon 30 Jumada II 1433 AH, 01:47:03");
    UnicodeString exp2 = UNICODE_STRING_SIMPLE("Mon 1 Rajab 1433 AH, 01:47:04");

    LocalUDateFormatPointer df(udat_open(UDAT_NONE, UDAT_NONE, "en@calendar=islamic", uzone.getTerminatedBuffer(),
                                         uzone.length(), nullptr, 0, &status));
    if (df.isNull() || U_FAILURE(status)) return;

    char16_t upattern[64];
    u_uastrcpy(upattern, "EEE d MMMM y G, HH:mm:ss");
    udat_applyPattern(df.getAlias(), false, upattern, u_strlen(upattern));

    char16_t ubuffer[1024];
    UDate ud0 = 1337557623000.0;

    status = U_ZERO_ERROR;
    udat_format(df.getAlias(), ud0, ubuffer, 1024, nullptr, &status);
    if (U_FAILURE(status)) {
        errln("Error formatting date 1\n");
        return;
    }
    //printf("formatted: '%s'\n", mkcstr(ubuffer));

    UnicodeString act1(ubuffer);
    if ( act1 != exp1 ) {
        errln("Unexpected result from date 1 format\n");
    }
    ud0 += 1000.0; // add one second

    status = U_ZERO_ERROR;
    udat_format(df.getAlias(), ud0, ubuffer, 1024, nullptr, &status);
    if (U_FAILURE(status)) {
        errln("Error formatting date 2\n");
        return;
    }
    //printf("formatted: '%s'\n", mkcstr(ubuffer));
    UnicodeString act2(ubuffer);
    if ( act2 != exp2 ) {
        errln("Unexpected result from date 2 format\n");
    }
}

void CalendarTest::Test1624() {
    UErrorCode status = U_ZERO_ERROR;
    Locale loc("he_IL@calendar=hebrew");
    HebrewCalendar hc(loc,status);

    for (int32_t year = 5600; year < 5800; year++ ) {

        for (int32_t month = HebrewCalendar::TISHRI; month <= HebrewCalendar::ELUL; month++) {
            // skip the adar 1 month if year is not a leap year
            if (HebrewCalendar::isLeapYear(year) == false && month == HebrewCalendar::ADAR_1) {
                continue;
            }
            int32_t day = 15;
            hc.set(year,month,day);
            int32_t dayHC = hc.get(UCAL_DATE,status);
            int32_t monthHC = hc.get(UCAL_MONTH,status);
            int32_t yearHC = hc.get(UCAL_YEAR,status);

            if (failure(status, "HebrewCalendar.get()", true)) continue;

            if (dayHC != day) {
                errln(" ==> day %d incorrect, should be: %d\n",dayHC,day);
                break;
            }
            if (monthHC != month) {
                errln(" ==> month %d incorrect, should be: %d\n",monthHC,month);
                break;
            }
            if (yearHC != year) {
                errln(" ==> day %d incorrect, should be: %d\n",yearHC,year);
                break;
            }
        }
    }
}

void CalendarTest::TestTimeStamp() {
    UErrorCode status = U_ZERO_ERROR;
    UDate start = 0.0, time;
    Calendar *cal;

    // Create a new Gregorian Calendar.
    cal = Calendar::createInstance("en_US@calendar=gregorian", status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating Gregorian calendar.");
        return;
    }

    for (int i = 0; i < 20000; i++) {
        // Set the Gregorian Calendar to a specific date for testing.
        cal->set(2009, UCAL_JULY, 3, 0, 49, 46);

        time = cal->getTime(status);
        if (U_FAILURE(status)) {
            errln("Error calling getTime()");
            break;
        }

        if (i == 0) {
            start = time;
        } else {
            if (start != time) {
                errln("start and time not equal.");
                break;
            }
        }
    }

    delete cal;
}

void CalendarTest::TestISO8601() {
    const char* TEST_LOCALES[] = {
        "en_US@calendar=iso8601",
        "en_US@calendar=Iso8601",
        "th_TH@calendar=iso8601",
        "ar_EG@calendar=iso8601",
        nullptr
    };

    int32_t TEST_DATA[][3] = {
        {2008, 1, 2008},
        {2009, 1, 2009},
        {2010, 53, 2009},
        {2011, 52, 2010},
        {2012, 52, 2011},
        {2013, 1, 2013},
        {2014, 1, 2014},
        {0, 0, 0},
    };

    for (int i = 0; TEST_LOCALES[i] != nullptr; i++) {
        UErrorCode status = U_ZERO_ERROR;
        Calendar *cal = Calendar::createInstance(TEST_LOCALES[i], status);
        if (U_FAILURE(status)) {
            errln("Error: Failed to create a calendar for locale: %s", TEST_LOCALES[i]);
            continue;
        }
        if (uprv_strcmp(cal->getType(), "iso8601") != 0) {
            errln("Error: iso8601 calendar is not used for locale: %s", TEST_LOCALES[i]);
            continue;
        }
        for (int j = 0; TEST_DATA[j][0] != 0; j++) {
            cal->set(TEST_DATA[j][0], UCAL_JANUARY, 1);
            int32_t weekNum = cal->get(UCAL_WEEK_OF_YEAR, status);
            int32_t weekYear = cal->get(UCAL_YEAR_WOY, status);
            if (U_FAILURE(status)) {
                errln("Error: Failed to get week of year");
                break;
            }
            if (weekNum != TEST_DATA[j][1] || weekYear != TEST_DATA[j][2]) {
                errln("Error: Incorrect week of year on January 1st, %d for locale %s: Returned [weekNum=%d, weekYear=%d], Expected [weekNum=%d, weekYear=%d]",
                    TEST_DATA[j][0], TEST_LOCALES[i], weekNum, weekYear, TEST_DATA[j][1], TEST_DATA[j][2]);
            }
        }
        delete cal;
    }

}

void
CalendarTest::TestAmbiguousWallTimeAPIs() {
    UErrorCode status = U_ZERO_ERROR;
    Calendar* cal = Calendar::createInstance(status);
    if (U_FAILURE(status)) {
        errln("Fail: Error creating a calendar instance.");
        return;
    }

    if (cal->getRepeatedWallTimeOption() != UCAL_WALLTIME_LAST) {
        errln("Fail: Default repeted time option is not UCAL_WALLTIME_LAST");
    }
    if (cal->getSkippedWallTimeOption() != UCAL_WALLTIME_LAST) {
        errln("Fail: Default skipped time option is not UCAL_WALLTIME_LAST");
    }

    Calendar* cal2 = cal->clone();

    if (*cal != *cal2) {
        errln("Fail: Cloned calendar != the original");
    }
    if (!cal->equals(*cal2, status)) {
        errln("Fail: The time of cloned calendar is not equal to the original");
    } else if (U_FAILURE(status)) {
        errln("Fail: Error equals");
    }
    status = U_ZERO_ERROR;

    cal2->setRepeatedWallTimeOption(UCAL_WALLTIME_FIRST);
    cal2->setSkippedWallTimeOption(UCAL_WALLTIME_FIRST);

    if (*cal == *cal2) {
        errln("Fail: Cloned and modified calendar == the original");
    }
    if (!cal->equals(*cal2, status)) {
        errln("Fail: The time of cloned calendar is not equal to the original after changing wall time options");
    } else if (U_FAILURE(status)) {
        errln("Fail: Error equals after changing wall time options");
    }
    status = U_ZERO_ERROR;

    if (cal2->getRepeatedWallTimeOption() != UCAL_WALLTIME_FIRST) {
        errln("Fail: Repeted time option is not UCAL_WALLTIME_FIRST");
    }
    if (cal2->getSkippedWallTimeOption() != UCAL_WALLTIME_FIRST) {
        errln("Fail: Skipped time option is not UCAL_WALLTIME_FIRST");
    }

    cal2->setRepeatedWallTimeOption(UCAL_WALLTIME_NEXT_VALID);
    if (cal2->getRepeatedWallTimeOption() != UCAL_WALLTIME_FIRST) {
        errln("Fail: Repeated wall time option was updated other than UCAL_WALLTIME_FIRST");
    }

    delete cal;
    delete cal2;
}

class CalFields {
public:
    CalFields(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t min, int32_t sec, int32_t ms = 0);
    CalFields(const Calendar& cal, UErrorCode& status);
    void setTo(Calendar& cal) const;
    char* toString(char* buf, int32_t len) const;
    bool operator==(const CalFields& rhs) const;
    bool operator!=(const CalFields& rhs) const;
    UBool isEquivalentTo(const Calendar& cal, UErrorCode& status) const;

private:
    int32_t year;
    int32_t month;
    int32_t day;
    int32_t hour;
    int32_t min;
    int32_t sec;
    int32_t ms;
};

CalFields::CalFields(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t min, int32_t sec, int32_t ms)
    : year(year), month(month), day(day), hour(hour), min(min), sec(sec), ms(ms) {
}

CalFields::CalFields(const Calendar& cal, UErrorCode& status) {
    year = cal.get(UCAL_YEAR, status);
    month = cal.get(UCAL_MONTH, status) + 1;
    day = cal.get(UCAL_DAY_OF_MONTH, status);
    hour = cal.get(UCAL_HOUR_OF_DAY, status);
    min = cal.get(UCAL_MINUTE, status);
    sec = cal.get(UCAL_SECOND, status);
    ms = cal.get(UCAL_MILLISECOND, status);
}

void
CalFields::setTo(Calendar& cal) const {
    cal.clear();
    cal.set(year, month - 1, day, hour, min, sec);
    cal.set(UCAL_MILLISECOND, ms);
}

char*
CalFields::toString(char* buf, int32_t len) const {
    char local[32];
    snprintf(local, sizeof(local), "%04d-%02d-%02d %02d:%02d:%02d.%03d", year, month, day, hour, min, sec, ms);
    uprv_strncpy(buf, local, len - 1);
    buf[len - 1] = 0;
    return buf;
}

bool
CalFields::operator==(const CalFields& rhs) const {
    return year == rhs.year
        && month == rhs.month
        && day == rhs.day
        && hour == rhs.hour
        && min == rhs.min
        && sec == rhs.sec
        && ms == rhs.ms;
}

bool
CalFields::operator!=(const CalFields& rhs) const {
    return !(*this == rhs);
}

UBool
CalFields::isEquivalentTo(const Calendar& cal, UErrorCode& status) const {
    return year == cal.get(UCAL_YEAR, status)
        && month == cal.get(UCAL_MONTH, status) + 1
        && day == cal.get(UCAL_DAY_OF_MONTH, status)
        && hour == cal.get(UCAL_HOUR_OF_DAY, status)
        && min == cal.get(UCAL_MINUTE, status)
        && sec == cal.get(UCAL_SECOND, status)
        && ms == cal.get(UCAL_MILLISECOND, status);
}

typedef struct {
    const char*     tzid;
    const CalFields in;
    const CalFields expLastGMT;
    const CalFields expFirstGMT;
} RepeatedWallTimeTestData;

static const RepeatedWallTimeTestData RPDATA[] =
{
    // Time zone            Input wall time                 WALLTIME_LAST in GMT            WALLTIME_FIRST in GMT
    {"America/New_York",    CalFields(2011,11,6,0,59,59),   CalFields(2011,11,6,4,59,59),   CalFields(2011,11,6,4,59,59)},
    {"America/New_York",    CalFields(2011,11,6,1,0,0),     CalFields(2011,11,6,6,0,0),     CalFields(2011,11,6,5,0,0)},
    {"America/New_York",    CalFields(2011,11,6,1,0,1),     CalFields(2011,11,6,6,0,1),     CalFields(2011,11,6,5,0,1)},
    {"America/New_York",    CalFields(2011,11,6,1,30,0),    CalFields(2011,11,6,6,30,0),    CalFields(2011,11,6,5,30,0)},
    {"America/New_York",    CalFields(2011,11,6,1,59,59),   CalFields(2011,11,6,6,59,59),   CalFields(2011,11,6,5,59,59)},
    {"America/New_York",    CalFields(2011,11,6,2,0,0),     CalFields(2011,11,6,7,0,0),     CalFields(2011,11,6,7,0,0)},
    {"America/New_York",    CalFields(2011,11,6,2,0,1),     CalFields(2011,11,6,7,0,1),     CalFields(2011,11,6,7,0,1)},

    {"Australia/Lord_Howe", CalFields(2011,4,3,1,29,59),    CalFields(2011,4,2,14,29,59),   CalFields(2011,4,2,14,29,59)},
    {"Australia/Lord_Howe", CalFields(2011,4,3,1,30,0),     CalFields(2011,4,2,15,0,0),     CalFields(2011,4,2,14,30,0)},
    {"Australia/Lord_Howe", CalFields(2011,4,3,1,45,0),     CalFields(2011,4,2,15,15,0),    CalFields(2011,4,2,14,45,0)},
    {"Australia/Lord_Howe", CalFields(2011,4,3,1,59,59),    CalFields(2011,4,2,15,29,59),   CalFields(2011,4,2,14,59,59)},
    {"Australia/Lord_Howe", CalFields(2011,4,3,2,0,0),      CalFields(2011,4,2,15,30,0),    CalFields(2011,4,2,15,30,0)},
    {"Australia/Lord_Howe", CalFields(2011,4,3,2,0,1),      CalFields(2011,4,2,15,30,1),    CalFields(2011,4,2,15,30,1)},

    {nullptr,                  CalFields(0,0,0,0,0,0),         CalFields(0,0,0,0,0,0),          CalFields(0,0,0,0,0,0)}
};

void CalendarTest::TestRepeatedWallTime() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar calGMT((const TimeZone&)*TimeZone::getGMT(), status);
    GregorianCalendar calDefault(status);
    GregorianCalendar calLast(status);
    GregorianCalendar calFirst(status);

    if (U_FAILURE(status)) {
        errln("Fail: Failed to create a calendar object.");
        return;
    }

    calLast.setRepeatedWallTimeOption(UCAL_WALLTIME_LAST);
    calFirst.setRepeatedWallTimeOption(UCAL_WALLTIME_FIRST);

    for (int32_t i = 0; RPDATA[i].tzid != nullptr; i++) {
        char buf[32];
        TimeZone *tz = TimeZone::createTimeZone(RPDATA[i].tzid);

        // UCAL_WALLTIME_LAST
        status = U_ZERO_ERROR;
        calLast.setTimeZone(*tz);
        RPDATA[i].in.setTo(calLast);
        calGMT.setTime(calLast.getTime(status), status);
        CalFields outLastGMT(calGMT, status);
        if (U_FAILURE(status)) {
            errln(UnicodeString("Fail: Failed to get/set time calLast/calGMT (UCAL_WALLTIME_LAST) - ")
                + RPDATA[i].in.toString(buf, sizeof(buf)) + "[" + RPDATA[i].tzid + "]");
        } else {
            if (outLastGMT != RPDATA[i].expLastGMT) {
                dataerrln(UnicodeString("Fail: UCAL_WALLTIME_LAST ") + RPDATA[i].in.toString(buf, sizeof(buf)) + "[" + RPDATA[i].tzid + "] is parsed as "
                    + outLastGMT.toString(buf, sizeof(buf)) + "[GMT]. Expected: " + RPDATA[i].expLastGMT.toString(buf, sizeof(buf)) + "[GMT]");
            }
        }

        // default
        status = U_ZERO_ERROR;
        calDefault.setTimeZone(*tz);
        RPDATA[i].in.setTo(calDefault);
        calGMT.setTime(calDefault.getTime(status), status);
        CalFields outDefGMT(calGMT, status);
        if (U_FAILURE(status)) {
            errln(UnicodeString("Fail: Failed to get/set time calLast/calGMT (default) - ")
                + RPDATA[i].in.toString(buf, sizeof(buf)) + "[" + RPDATA[i].tzid + "]");
        } else {
            if (outDefGMT != RPDATA[i].expLastGMT) {
                dataerrln(UnicodeString("Fail: (default) ") + RPDATA[i].in.toString(buf, sizeof(buf)) + "[" + RPDATA[i].tzid + "] is parsed as "
                    + outDefGMT.toString(buf, sizeof(buf)) + "[GMT]. Expected: " + RPDATA[i].expLastGMT.toString(buf, sizeof(buf)) + "[GMT]");
            }
        }

        // UCAL_WALLTIME_FIRST
        status = U_ZERO_ERROR;
        calFirst.setTimeZone(*tz);
        RPDATA[i].in.setTo(calFirst);
        calGMT.setTime(calFirst.getTime(status), status);
        CalFields outFirstGMT(calGMT, status);
        if (U_FAILURE(status)) {
            errln(UnicodeString("Fail: Failed to get/set time calLast/calGMT (UCAL_WALLTIME_FIRST) - ")
                + RPDATA[i].in.toString(buf, sizeof(buf)) + "[" + RPDATA[i].tzid + "]");
        } else {
            if (outFirstGMT != RPDATA[i].expFirstGMT) {
                dataerrln(UnicodeString("Fail: UCAL_WALLTIME_FIRST ") + RPDATA[i].in.toString(buf, sizeof(buf)) + "[" + RPDATA[i].tzid + "] is parsed as "
                    + outFirstGMT.toString(buf, sizeof(buf)) + "[GMT]. Expected: " + RPDATA[i].expFirstGMT.toString(buf, sizeof(buf)) + "[GMT]");
            }
        }
        delete tz;
    }
}

typedef struct {
    const char*     tzid;
    const CalFields in;
    UBool           isValid;
    const CalFields expLastGMT;
    const CalFields expFirstGMT;
    const CalFields expNextAvailGMT;
} SkippedWallTimeTestData;

static SkippedWallTimeTestData SKDATA[] =
{
     // Time zone           Input wall time                 valid?  WALLTIME_LAST in GMT            WALLTIME_FIRST in GMT           WALLTIME_NEXT_VALID in GMT
    {"America/New_York",    CalFields(2011,3,13,1,59,59),   true,   CalFields(2011,3,13,6,59,59),   CalFields(2011,3,13,6,59,59),   CalFields(2011,3,13,6,59,59)},
    {"America/New_York",    CalFields(2011,3,13,2,0,0),     false,  CalFields(2011,3,13,7,0,0),     CalFields(2011,3,13,6,0,0),     CalFields(2011,3,13,7,0,0)},
    {"America/New_York",    CalFields(2011,3,13,2,1,0),     false,  CalFields(2011,3,13,7,1,0),     CalFields(2011,3,13,6,1,0),     CalFields(2011,3,13,7,0,0)},
    {"America/New_York",    CalFields(2011,3,13,2,30,0),    false,  CalFields(2011,3,13,7,30,0),    CalFields(2011,3,13,6,30,0),    CalFields(2011,3,13,7,0,0)},
    {"America/New_York",    CalFields(2011,3,13,2,59,59),   false,  CalFields(2011,3,13,7,59,59),   CalFields(2011,3,13,6,59,59),   CalFields(2011,3,13,7,0,0)},
    {"America/New_York",    CalFields(2011,3,13,3,0,0),     true,   CalFields(2011,3,13,7,0,0),     CalFields(2011,3,13,7,0,0),     CalFields(2011,3,13,7,0,0)},

    {"Pacific/Apia",        CalFields(2011,12,29,23,59,59), true,   CalFields(2011,12,30,9,59,59),  CalFields(2011,12,30,9,59,59),  CalFields(2011,12,30,9,59,59)},
    {"Pacific/Apia",        CalFields(2011,12,30,0,0,0),    false,  CalFields(2011,12,30,10,0,0),   CalFields(2011,12,29,10,0,0),   CalFields(2011,12,30,10,0,0)},
    {"Pacific/Apia",        CalFields(2011,12,30,12,0,0),   false,  CalFields(2011,12,30,22,0,0),   CalFields(2011,12,29,22,0,0),   CalFields(2011,12,30,10,0,0)},
    {"Pacific/Apia",        CalFields(2011,12,30,23,59,59), false,  CalFields(2011,12,31,9,59,59),  CalFields(2011,12,30,9,59,59),  CalFields(2011,12,30,10,0,0)},
    {"Pacific/Apia",        CalFields(2011,12,31,0,0,0),    true,   CalFields(2011,12,30,10,0,0),   CalFields(2011,12,30,10,0,0),   CalFields(2011,12,30,10,0,0)},

    {nullptr,                  CalFields(0,0,0,0,0,0),         true,   CalFields(0,0,0,0,0,0),         CalFields(0,0,0,0,0,0),         CalFields(0,0,0,0,0,0)}
};


void CalendarTest::TestSkippedWallTime() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar calGMT((const TimeZone&)*TimeZone::getGMT(), status);
    GregorianCalendar calDefault(status);
    GregorianCalendar calLast(status);
    GregorianCalendar calFirst(status);
    GregorianCalendar calNextAvail(status);

    if (U_FAILURE(status)) {
        errln("Fail: Failed to create a calendar object.");
        return;
    }

    calLast.setSkippedWallTimeOption(UCAL_WALLTIME_LAST);
    calFirst.setSkippedWallTimeOption(UCAL_WALLTIME_FIRST);
    calNextAvail.setSkippedWallTimeOption(UCAL_WALLTIME_NEXT_VALID);

    for (int32_t i = 0; SKDATA[i].tzid != nullptr; i++) {
        UDate d;
        char buf[32];
        TimeZone *tz = TimeZone::createTimeZone(SKDATA[i].tzid);

        for (int32_t j = 0; j < 2; j++) {
            UBool bLenient = (j == 0);

            // UCAL_WALLTIME_LAST
            status = U_ZERO_ERROR;
            calLast.setLenient(bLenient);
            calLast.setTimeZone(*tz);
            SKDATA[i].in.setTo(calLast);
            d = calLast.getTime(status);
            if (bLenient || SKDATA[i].isValid) {
                calGMT.setTime(d, status);
                CalFields outLastGMT(calGMT, status);
                if (U_FAILURE(status)) {
                    errln(UnicodeString("Fail: Failed to get/set time calLast/calGMT (UCAL_WALLTIME_LAST) - ")
                        + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "]");
                } else {
                    if (outLastGMT != SKDATA[i].expLastGMT) {
                        dataerrln(UnicodeString("Fail: UCAL_WALLTIME_LAST ") + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "] is parsed as "
                            + outLastGMT.toString(buf, sizeof(buf)) + "[GMT]. Expected: " + SKDATA[i].expLastGMT.toString(buf, sizeof(buf)) + "[GMT]");
                    }
                }
            } else if (U_SUCCESS(status)) {
                // strict, invalid wall time - must report an error
                dataerrln(UnicodeString("Fail: An error expected (UCAL_WALLTIME_LAST)") +
                    + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "]");
            }

            // default
            status = U_ZERO_ERROR;
            calDefault.setLenient(bLenient);
            calDefault.setTimeZone(*tz);
            SKDATA[i].in.setTo(calDefault);
            d = calDefault.getTime(status);
            if (bLenient || SKDATA[i].isValid) {
                calGMT.setTime(d, status);
                CalFields outDefGMT(calGMT, status);
                if (U_FAILURE(status)) {
                    errln(UnicodeString("Fail: Failed to get/set time calDefault/calGMT (default) - ")
                        + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "]");
                } else {
                    if (outDefGMT != SKDATA[i].expLastGMT) {
                        dataerrln(UnicodeString("Fail: (default) ") + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "] is parsed as "
                            + outDefGMT.toString(buf, sizeof(buf)) + "[GMT]. Expected: " + SKDATA[i].expLastGMT.toString(buf, sizeof(buf)) + "[GMT]");
                    }
                }
            } else if (U_SUCCESS(status)) {
                // strict, invalid wall time - must report an error
                dataerrln(UnicodeString("Fail: An error expected (default)") +
                    + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "]");
            }

            // UCAL_WALLTIME_FIRST
            status = U_ZERO_ERROR;
            calFirst.setLenient(bLenient);
            calFirst.setTimeZone(*tz);
            SKDATA[i].in.setTo(calFirst);
            d = calFirst.getTime(status);
            if (bLenient || SKDATA[i].isValid) {
                calGMT.setTime(d, status);
                CalFields outFirstGMT(calGMT, status);
                if (U_FAILURE(status)) {
                    errln(UnicodeString("Fail: Failed to get/set time calFirst/calGMT (UCAL_WALLTIME_FIRST) - ")
                        + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "]");
                } else {
                    if (outFirstGMT != SKDATA[i].expFirstGMT) {
                        dataerrln(UnicodeString("Fail: UCAL_WALLTIME_FIRST ") + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "] is parsed as "
                            + outFirstGMT.toString(buf, sizeof(buf)) + "[GMT]. Expected: " + SKDATA[i].expFirstGMT.toString(buf, sizeof(buf)) + "[GMT]");
                    }
                }
            } else if (U_SUCCESS(status)) {
                // strict, invalid wall time - must report an error
                dataerrln(UnicodeString("Fail: An error expected (UCAL_WALLTIME_FIRST)") +
                    + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "]");
            }

            // UCAL_WALLTIME_NEXT_VALID
            status = U_ZERO_ERROR;
            calNextAvail.setLenient(bLenient);
            calNextAvail.setTimeZone(*tz);
            SKDATA[i].in.setTo(calNextAvail);
            d = calNextAvail.getTime(status);
            if (bLenient || SKDATA[i].isValid) {
                calGMT.setTime(d, status);
                CalFields outNextAvailGMT(calGMT, status);
                if (U_FAILURE(status)) {
                    errln(UnicodeString("Fail: Failed to get/set time calNextAvail/calGMT (UCAL_WALLTIME_NEXT_VALID) - ")
                        + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "]");
                } else {
                    if (outNextAvailGMT != SKDATA[i].expNextAvailGMT) {
                        dataerrln(UnicodeString("Fail: UCAL_WALLTIME_NEXT_VALID ") + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "] is parsed as "
                            + outNextAvailGMT.toString(buf, sizeof(buf)) + "[GMT]. Expected: " + SKDATA[i].expNextAvailGMT.toString(buf, sizeof(buf)) + "[GMT]");
                    }
                }
            } else if (U_SUCCESS(status)) {
                // strict, invalid wall time - must report an error
                dataerrln(UnicodeString("Fail: An error expected (UCAL_WALLTIME_NEXT_VALID)") +
                    + SKDATA[i].in.toString(buf, sizeof(buf)) + "[" + SKDATA[i].tzid + "]");
            }
        }

        delete tz;
    }
}

void CalendarTest::TestCloneLocale() {
  UErrorCode status = U_ZERO_ERROR;
  LocalPointer<Calendar>  cal(Calendar::createInstance(TimeZone::getGMT()->clone(),
                                                       Locale::createFromName("en"), status));
  TEST_CHECK_STATUS;
  Locale l0 = cal->getLocale(ULOC_VALID_LOCALE, status);
  TEST_CHECK_STATUS;
  LocalPointer<Calendar> cal2(cal->clone());
  Locale l = cal2->getLocale(ULOC_VALID_LOCALE, status);
  if(l0!=l) {
    errln("Error: cloned locale %s != original locale %s, status %s\n", l0.getName(), l.getName(), u_errorName(status));
  }
  TEST_CHECK_STATUS;
}

void CalendarTest::TestTimeZoneInLocale() {
    const char *tests[][3]  = {
        { "en-u-tz-usden",                     "America/Denver",             "gregorian" },
        { "es-u-tz-usden",                     "America/Denver",             "gregorian" },
        { "ms-u-tz-mykul",                     "Asia/Kuala_Lumpur",          "gregorian" },
        { "zh-u-tz-mykul",                     "Asia/Kuala_Lumpur",          "gregorian" },
        { "fr-u-ca-buddhist-tz-phmnl",         "Asia/Manila",                "buddhist" },
        { "th-u-ca-chinese-tz-gblon",          "Europe/London",              "chinese" },
        { "de-u-ca-coptic-tz-ciabj",           "Africa/Abidjan",             "coptic" },
        { "ja-u-ca-dangi-tz-hkhkg",            "Asia/Hong_Kong",             "dangi" },
        { "da-u-ca-ethioaa-tz-ruunera",        "Asia/Ust-Nera",              "ethiopic-amete-alem" },
        { "ko-u-ca-ethiopic-tz-cvrai",         "Atlantic/Cape_Verde",        "ethiopic" },
        { "fil-u-ca-gregory-tz-aubne",         "Australia/Brisbane",         "gregorian" },
        { "fa-u-ca-hebrew-tz-brrbr",           "America/Rio_Branco",         "hebrew" },
        { "gr-u-ca-indian-tz-lccas",           "America/St_Lucia",           "indian" },
        { "or-u-ca-islamic-tz-cayyn",          "America/Swift_Current",      "islamic" },
        { "my-u-ca-islamic-umalqura-tz-kzala", "Asia/Almaty",                "islamic-umalqura" },
        { "lo-u-ca-islamic-tbla-tz-bmbda",     "Atlantic/Bermuda",           "islamic-tbla" },
        { "km-u-ca-islamic-civil-tz-aqplm",    "Antarctica/Palmer",          "islamic-civil" },
        { "kk-u-ca-islamic-rgsa-tz-usanc",     "America/Anchorage",          "islamic-rgsa" },
        { "ar-u-ca-iso8601-tz-bjptn",          "Africa/Porto-Novo",          "iso8601" },
        { "he-u-ca-japanese-tz-tzdar",         "Africa/Dar_es_Salaam",       "japanese" },
        { "bs-u-ca-persian-tz-etadd",          "Africa/Addis_Ababa",         "persian" },
        { "it-u-ca-roc-tz-aruaq",              "America/Argentina/San_Juan", "roc" },
    };

    for (int32_t i = 0; i < UPRV_LENGTHOF(tests); ++i) {
        UErrorCode status = U_ZERO_ERROR;
        const char **testLine = tests[i];
        Locale locale(testLine[0]);
        UnicodeString expected(testLine[1], -1, US_INV);
        UnicodeString actual;

        LocalPointer<Calendar> calendar(
                Calendar::createInstance(locale, status));
        if (failure(status, "Calendar::createInstance", true)) continue;

        assertEquals("TimeZone from Calendar::createInstance",
                     expected, calendar->getTimeZone().getID(actual));

        assertEquals("Calendar Type from Calendar::createInstance",
                     testLine[2], calendar->getType());
    }
}

void CalendarTest::AsssertCalendarFieldValue(
    Calendar* cal, double time, const char* type,
    int32_t era, int32_t year, int32_t month, int32_t week_of_year,
    int32_t week_of_month, int32_t date, int32_t day_of_year, int32_t day_of_week,
    int32_t day_of_week_in_month, int32_t am_pm, int32_t hour, int32_t hour_of_day,
    int32_t minute, int32_t second, int32_t millisecond, int32_t zone_offset,
    int32_t dst_offset, int32_t year_woy, int32_t dow_local, int32_t extended_year,
    int32_t julian_day, int32_t milliseconds_in_day, int32_t is_leap_month) {

    UErrorCode status = U_ZERO_ERROR;
    cal->setTime(time, status);
    assertEquals("getType", type, cal->getType());

    assertEquals("UCAL_ERA", era, cal->get(UCAL_ERA, status));
    assertEquals("UCAL_YEAR", year, cal->get(UCAL_YEAR, status));
    assertEquals("UCAL_MONTH", month, cal->get(UCAL_MONTH, status));
    assertEquals("UCAL_WEEK_OF_YEAR", week_of_year, cal->get(UCAL_WEEK_OF_YEAR, status));
    assertEquals("UCAL_WEEK_OF_MONTH", week_of_month, cal->get(UCAL_WEEK_OF_MONTH, status));
    assertEquals("UCAL_DATE", date, cal->get(UCAL_DATE, status));
    assertEquals("UCAL_DAY_OF_YEAR", day_of_year, cal->get(UCAL_DAY_OF_YEAR, status));
    assertEquals("UCAL_DAY_OF_WEEK", day_of_week, cal->get(UCAL_DAY_OF_WEEK, status));
    assertEquals("UCAL_DAY_OF_WEEK_IN_MONTH", day_of_week_in_month, cal->get(UCAL_DAY_OF_WEEK_IN_MONTH, status));
    assertEquals("UCAL_AM_PM", am_pm, cal->get(UCAL_AM_PM, status));
    assertEquals("UCAL_HOUR", hour, cal->get(UCAL_HOUR, status));
    assertEquals("UCAL_HOUR_OF_DAY", hour_of_day, cal->get(UCAL_HOUR_OF_DAY, status));
    assertEquals("UCAL_MINUTE", minute, cal->get(UCAL_MINUTE, status));
    assertEquals("UCAL_SECOND", second, cal->get(UCAL_SECOND, status));
    assertEquals("UCAL_MILLISECOND", millisecond, cal->get(UCAL_MILLISECOND, status));
    assertEquals("UCAL_ZONE_OFFSET", zone_offset, cal->get(UCAL_ZONE_OFFSET, status));
    assertEquals("UCAL_DST_OFFSET", dst_offset, cal->get(UCAL_DST_OFFSET, status));
    assertEquals("UCAL_YEAR_WOY", year_woy, cal->get(UCAL_YEAR_WOY, status));
    assertEquals("UCAL_DOW_LOCAL", dow_local, cal->get(UCAL_DOW_LOCAL, status));
    assertEquals("UCAL_EXTENDED_YEAR", extended_year, cal->get(UCAL_EXTENDED_YEAR, status));
    assertEquals("UCAL_JULIAN_DAY", julian_day, cal->get(UCAL_JULIAN_DAY, status));
    assertEquals("UCAL_MILLISECONDS_IN_DAY", milliseconds_in_day, cal->get(UCAL_MILLISECONDS_IN_DAY, status));
    assertEquals("UCAL_IS_LEAP_MONTH", is_leap_month, cal->get(UCAL_IS_LEAP_MONTH, status));
}

static constexpr double test_time = 1667277891323; // Nov 1, 2022 4:44:51 GMT

void CalendarTest::TestBasicConversionGregorian() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=gregorian"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Gregorian calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "gregorian",
        1, 2022, 10, 45, 1, 1, 305, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 2022, 3, 2022, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionISO8601() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=iso8601"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get ISO8601 calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "iso8601",
        1, 2022, 10, 44, 1, 1, 305, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 2022, 2, 2022, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionJapanese() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=japanese"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Japanese calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "japanese",
        236, 4, 10, 45, 1, 1, 305, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 2022, 3, 2022, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionBuddhist() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=buddhist"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Buddhist calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "buddhist",
        0, 2565, 10, 45, 1, 1, 305, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 2022, 3, 2022, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionTaiwan() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=roc"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Taiwan calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "roc",
        1, 111, 10, 45, 1, 1, 305, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 2022, 3, 2022, 2459885, 17091323, 0);

}
void CalendarTest::TestBasicConversionPersian() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=persian"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Persian calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "persian",
        0, 1401, 7, 33, 2, 10, 226, 3, 2, 0, 4, 4, 44, 51,
        323, 0, 0, 1401, 3, 1401, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionIslamic() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=islamic"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Islamic calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "islamic",
        0, 1444, 3, 15, 2, 7, 96, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 1444, 3, 1444, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionIslamicTBLA() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=islamic-tbla"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get IslamicTBLA calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "islamic-tbla",
        0, 1444, 3, 15, 2, 7, 96, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 1444, 3, 1444, 2459885, 17091323, 0);

}
void CalendarTest::TestBasicConversionIslamicCivil() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=islamic-civil"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get IslamicCivil calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "islamic-civil",
        0, 1444, 3, 15, 2, 6, 95, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 1444, 3, 1444, 2459885, 17091323, 0);

}
void CalendarTest::TestBasicConversionIslamicRGSA() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=islamic-rgsa"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get IslamicRGSA calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "islamic-rgsa",
        0, 1444, 3, 15, 2, 7, 96, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 1444, 3, 1444, 2459885, 17091323, 0);

}
void CalendarTest::TestBasicConversionIslamicUmalqura() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=islamic-umalqura"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get IslamicUmalqura calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "islamic-umalqura",
        0, 1444, 3, 15, 2, 7, 95, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 1444, 3, 1444, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionHebrew() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=hebrew"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Hebrew calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "hebrew",
        0, 5783, 1, 6, 2, 7, 37, 3, 1, 0, 4, 4, 44, 51,
        323, 0, 0, 5783, 3, 5783, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionChinese() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=chinese"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Chinese calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "chinese",
        78, 39, 9, 40, 2, 8, 274, 3, 2, 0, 4, 4, 44, 51,
        323, 0, 0, 4659, 3, 4659, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionDangi() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=dangi"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Dangi calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "dangi",
        78, 39, 9, 40, 2, 8, 274, 3, 2, 0, 4, 4, 44, 51,
        323, 0, 0, 4355, 3, 4355, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionIndian() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=indian"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Indian calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "indian",
        0, 1944, 7, 33, 2, 10, 225, 3, 2, 0, 4, 4, 44, 51,
        323, 0, 0, 1944, 3, 1944, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionCoptic() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=coptic"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Coptic calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "coptic",
        1, 1739, 1, 8, 4, 22, 52, 3, 4, 0, 4, 4, 44, 51,
        323, 0, 0, 1739, 3, 1739, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionEthiopic() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=ethiopic"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get Ethiopic calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "ethiopic",
        1, 2015, 1, 8, 4, 22, 52, 3, 4, 0, 4, 4, 44, 51,
        323, 0, 0, 2015, 3, 2015, 2459885, 17091323, 0);
}
void CalendarTest::TestBasicConversionEthiopicAmeteAlem() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(icu::Calendar::createInstance(
        *TimeZone::getGMT(), Locale("en@calendar=ethiopic-amete-alem"), status));
    if (U_FAILURE(status)) {
        errln("Fail: Cannot get EthiopicAmeteAlem calendar");
        return;
    }
    AsssertCalendarFieldValue(
        cal.getAlias(), test_time, "ethiopic-amete-alem",
        0, 7515, 1, 8, 4, 22, 52, 3, 4, 0, 4, 4, 44, 51,
        323, 0, 0, 2015, 3, 2015, 2459885, 17091323, 0);
}


void CalendarTest::setAndTestCalendar(Calendar* cal, int32_t initMonth, int32_t initDay, int32_t initYear, UErrorCode& status) {
        cal->clear();
        cal->setLenient(false);
        cal->set(initYear, initMonth, initDay);
        int32_t day = cal->get(UCAL_DAY_OF_MONTH, status);
        int32_t month = cal->get(UCAL_MONTH, status);
        int32_t year = cal->get(UCAL_YEAR, status);
        if(U_FAILURE(status))
            return;

        if(initDay != day || initMonth != month || initYear != year)
        {
            errln(" year init values:\tmonth %i\tday %i\tyear %i", initMonth, initDay, initYear);
            errln("values post set():\tmonth %i\tday %i\tyear %i",month, day, year);
        }
}

void CalendarTest::setAndTestWholeYear(Calendar* cal, int32_t startYear, UErrorCode& status) {
        for(int32_t startMonth = 0; startMonth < 12; startMonth++) {
            for(int32_t startDay = 1; startDay < 31; startDay++ ) {
                    setAndTestCalendar(cal, startMonth, startDay, startYear, status);
                    if(U_FAILURE(status) && startDay == 30) {
                        status = U_ZERO_ERROR;
                        continue;
                    }
                    TEST_CHECK_STATUS;
            }
        }
}

// =====================================================================

typedef struct {
    int16_t  gYear;
    int8_t   gMon;
    int8_t   gDay;
    int16_t  uYear;
    int8_t   uMon;
    int8_t   uDay;
} GregoUmmAlQuraMap;

// data from
// Official Umm-al-Qura calendar of SA:
// home, http://www.ummulqura.org.sa/default.aspx
// converter, http://www.ummulqura.org.sa/Index.aspx
static const GregoUmmAlQuraMap guMappings[] = {
//  gregorian,    ummAlQura
//  year mo da,   year mo da
//  (using 1-based months here)
  { 1882,11,12,   1300, 1, 1 },
  { 1892, 7,25,   1310, 1, 1 },
  { 1896, 6,12,   1314, 1, 1 },
  { 1898, 5,22,   1316, 1, 1 },
  { 1900, 4,30,   1318, 1, 1 },
  { 1901, 4,20,   1319, 1, 1 },
  { 1902, 4,10,   1320, 1, 1 },
  { 1903, 3,30,   1321, 1, 1 },
  { 1904, 3,19,   1322, 1, 1 },
  { 1905, 3, 8,   1323, 1, 1 },
  { 1906, 2,25,   1324, 1, 1 },
  { 1907, 2,14,   1325, 1, 1 },
  { 1908, 2, 4,   1326, 1, 1 },
  { 1909, 1,23,   1327, 1, 1 },
  { 1910, 1,13,   1328, 1, 1 },
  { 1911, 1, 2,   1329, 1, 1 },
  { 1911,12,22,   1330, 1, 1 },
  { 1912,12,10,   1331, 1, 1 },
  { 1913,11,30,   1332, 1, 1 },
  { 1914,11,19,   1333, 1, 1 },
  { 1915,11, 9,   1334, 1, 1 },
  { 1916,10,28,   1335, 1, 1 },
  { 1917,10,18,   1336, 1, 1 },
  { 1918,10, 7,   1337, 1, 1 },
  { 1919, 9,26,   1338, 1, 1 },
  { 1920, 9,14,   1339, 1, 1 },
  { 1921, 9, 4,   1340, 1, 1 },
  { 1922, 8,24,   1341, 1, 1 },
  { 1923, 8,14,   1342, 1, 1 },
  { 1924, 8, 2,   1343, 1, 1 },
  { 1925, 7,22,   1344, 1, 1 },
  { 1926, 7,11,   1345, 1, 1 },
  { 1927, 6,30,   1346, 1, 1 },
  { 1928, 6,19,   1347, 1, 1 },
  { 1929, 6, 9,   1348, 1, 1 },
  { 1930, 5,29,   1349, 1, 1 },
  { 1931, 5,19,   1350, 1, 1 },
  { 1932, 5, 7,   1351, 1, 1 },
  { 1933, 4,26,   1352, 1, 1 },
  { 1934, 4,15,   1353, 1, 1 },
  { 1935, 4, 5,   1354, 1, 1 },
  { 1936, 3,24,   1355, 1, 1 },
  { 1937, 3,14,   1356, 1, 1 },
  { 1938, 3, 4,   1357, 1, 1 },
  { 1939, 2,21,   1358, 1, 1 },
  { 1940, 2,10,   1359, 1, 1 },
  { 1941, 1,29,   1360, 1, 1 },
  { 1942, 1,18,   1361, 1, 1 },
  { 1943, 1, 8,   1362, 1, 1 },
  { 1943,12,28,   1363, 1, 1 },
  { 1944,12,17,   1364, 1, 1 },
  { 1945,12, 6,   1365, 1, 1 },
  { 1946,11,25,   1366, 1, 1 },
  { 1947,11,14,   1367, 1, 1 },
  { 1948,11, 3,   1368, 1, 1 },
  { 1949,10,23,   1369, 1, 1 },
  { 1950,10,13,   1370, 1, 1 },
  { 1951,10, 3,   1371, 1, 1 },
  { 1952, 9,21,   1372, 1, 1 },
  { 1953, 9,10,   1373, 1, 1 },
  { 1954, 8,30,   1374, 1, 1 },
  { 1955, 8,19,   1375, 1, 1 },
  { 1956, 8, 8,   1376, 1, 1 },
  { 1957, 7,29,   1377, 1, 1 },
  { 1958, 7,18,   1378, 1, 1 },
  { 1959, 7, 8,   1379, 1, 1 },
  { 1960, 6,26,   1380, 1, 1 },
  { 1961, 6,15,   1381, 1, 1 },
  { 1962, 6, 4,   1382, 1, 1 },
  { 1963, 5,24,   1383, 1, 1 },
  { 1964, 5,13,   1384, 1, 1 },
  { 1965, 5, 3,   1385, 1, 1 },
  { 1966, 4,22,   1386, 1, 1 },
  { 1967, 4,11,   1387, 1, 1 },
  { 1968, 3,30,   1388, 1, 1 },
  { 1969, 3,19,   1389, 1, 1 },
  { 1970, 3, 9,   1390, 1, 1 },
  { 1971, 2,27,   1391, 1, 1 },
  { 1972, 2,16,   1392, 1, 1 },
  { 1973, 2, 5,   1393, 1, 1 },
  { 1974, 1,25,   1394, 1, 1 },
  { 1975, 1,14,   1395, 1, 1 },
  { 1976, 1, 3,   1396, 1, 1 },
  { 1976,12,22,   1397, 1, 1 },
  { 1977,12,12,   1398, 1, 1 },
  { 1978,12, 1,   1399, 1, 1 },
  { 1979,11,21,   1400, 1, 1 },
  { 1980,11, 9,   1401, 1, 1 },
  { 1981,10,29,   1402, 1, 1 },
  { 1982,10,18,   1403, 1, 1 },
  { 1983,10, 8,   1404, 1, 1 },
  { 1984, 9,26,   1405, 1, 1 },
  { 1985, 9,16,   1406, 1, 1 },
  { 1986, 9, 6,   1407, 1, 1 },
  { 1987, 8,26,   1408, 1, 1 },
  { 1988, 8,14,   1409, 1, 1 },
  { 1989, 8, 3,   1410, 1, 1 },
  { 1990, 7,23,   1411, 1, 1 },
  { 1991, 7,13,   1412, 1, 1 },
  { 1992, 7, 2,   1413, 1, 1 },
  { 1993, 6,21,   1414, 1, 1 },
  { 1994, 6,11,   1415, 1, 1 },
  { 1995, 5,31,   1416, 1, 1 },
  { 1996, 5,19,   1417, 1, 1 },
  { 1997, 5, 8,   1418, 1, 1 },
  { 1998, 4,28,   1419, 1, 1 },
  { 1999, 4,17,   1420, 1, 1 },
  { 1999, 5,16,   1420, 2, 1 },
  { 1999, 6,15,   1420, 3, 1 },
  { 1999, 7,14,   1420, 4, 1 },
  { 1999, 8,12,   1420, 5, 1 },
  { 1999, 9,11,   1420, 6, 1 },
  { 1999,10,10,   1420, 7, 1 },
  { 1999,11, 9,   1420, 8, 1 },
  { 1999,12, 9,   1420, 9, 1 },
  { 2000, 1, 8,   1420,10, 1 },
  { 2000, 2, 7,   1420,11, 1 },
  { 2000, 3, 7,   1420,12, 1 },
  { 2000, 4, 6,   1421, 1, 1 },
  { 2000, 5, 5,   1421, 2, 1 },
  { 2000, 6, 3,   1421, 3, 1 },
  { 2000, 7, 3,   1421, 4, 1 },
  { 2000, 8, 1,   1421, 5, 1 },
  { 2000, 8,30,   1421, 6, 1 },
  { 2000, 9,28,   1421, 7, 1 },
  { 2000,10,28,   1421, 8, 1 },
  { 2000,11,27,   1421, 9, 1 },
  { 2000,12,27,   1421,10, 1 },
  { 2001, 1,26,   1421,11, 1 },
  { 2001, 2,24,   1421,12, 1 },
  { 2001, 3,26,   1422, 1, 1 },
  { 2001, 4,25,   1422, 2, 1 },
  { 2001, 5,24,   1422, 3, 1 },
  { 2001, 6,22,   1422, 4, 1 },
  { 2001, 7,22,   1422, 5, 1 },
  { 2001, 8,20,   1422, 6, 1 },
  { 2001, 9,18,   1422, 7, 1 },
  { 2001,10,17,   1422, 8, 1 },
  { 2001,11,16,   1422, 9, 1 },
  { 2001,12,16,   1422,10, 1 },
  { 2002, 1,15,   1422,11, 1 },
  { 2002, 2,13,   1422,12, 1 },
  { 2002, 3,15,   1423, 1, 1 },
  { 2002, 4,14,   1423, 2, 1 },
  { 2002, 5,13,   1423, 3, 1 },
  { 2002, 6,12,   1423, 4, 1 },
  { 2002, 7,11,   1423, 5, 1 },
  { 2002, 8,10,   1423, 6, 1 },
  { 2002, 9, 8,   1423, 7, 1 },
  { 2002,10, 7,   1423, 8, 1 },
  { 2002,11, 6,   1423, 9, 1 },
  { 2002,12, 5,   1423,10, 1 },
  { 2003, 1, 4,   1423,11, 1 },
  { 2003, 2, 2,   1423,12, 1 },
  { 2003, 3, 4,   1424, 1, 1 },
  { 2003, 4, 3,   1424, 2, 1 },
  { 2003, 5, 2,   1424, 3, 1 },
  { 2003, 6, 1,   1424, 4, 1 },
  { 2003, 7, 1,   1424, 5, 1 },
  { 2003, 7,30,   1424, 6, 1 },
  { 2003, 8,29,   1424, 7, 1 },
  { 2003, 9,27,   1424, 8, 1 },
  { 2003,10,26,   1424, 9, 1 },
  { 2003,11,25,   1424,10, 1 },
  { 2003,12,24,   1424,11, 1 },
  { 2004, 1,23,   1424,12, 1 },
  { 2004, 2,21,   1425, 1, 1 },
  { 2004, 3,22,   1425, 2, 1 },
  { 2004, 4,20,   1425, 3, 1 },
  { 2004, 5,20,   1425, 4, 1 },
  { 2004, 6,19,   1425, 5, 1 },
  { 2004, 7,18,   1425, 6, 1 },
  { 2004, 8,17,   1425, 7, 1 },
  { 2004, 9,15,   1425, 8, 1 },
  { 2004,10,15,   1425, 9, 1 },
  { 2004,11,14,   1425,10, 1 },
  { 2004,12,13,   1425,11, 1 },
  { 2005, 1,12,   1425,12, 1 },
  { 2005, 2,10,   1426, 1, 1 },
  { 2005, 3,11,   1426, 2, 1 },
  { 2005, 4,10,   1426, 3, 1 },
  { 2005, 5, 9,   1426, 4, 1 },
  { 2005, 6, 8,   1426, 5, 1 },
  { 2005, 7, 7,   1426, 6, 1 },
  { 2005, 8, 6,   1426, 7, 1 },
  { 2005, 9, 5,   1426, 8, 1 },
  { 2005,10, 4,   1426, 9, 1 },
  { 2005,11, 3,   1426,10, 1 },
  { 2005,12, 3,   1426,11, 1 },
  { 2006, 1, 1,   1426,12, 1 },
  { 2006, 1,31,   1427, 1, 1 },
  { 2006, 3, 1,   1427, 2, 1 },
  { 2006, 3,30,   1427, 3, 1 },
  { 2006, 4,29,   1427, 4, 1 },
  { 2006, 5,28,   1427, 5, 1 },
  { 2006, 6,27,   1427, 6, 1 },
  { 2006, 7,26,   1427, 7, 1 },
  { 2006, 8,25,   1427, 8, 1 },
  { 2006, 9,24,   1427, 9, 1 },
  { 2006,10,23,   1427,10, 1 },
  { 2006,11,22,   1427,11, 1 },
  { 2006,12,22,   1427,12, 1 },
  { 2007, 1,20,   1428, 1, 1 },
  { 2007, 2,19,   1428, 2, 1 },
  { 2007, 3,20,   1428, 3, 1 },
  { 2007, 4,18,   1428, 4, 1 },
  { 2007, 5,18,   1428, 5, 1 },
  { 2007, 6,16,   1428, 6, 1 },
  { 2007, 7,15,   1428, 7, 1 },
  { 2007, 8,14,   1428, 8, 1 },
  { 2007, 9,13,   1428, 9, 1 },
  { 2007,10,13,   1428,10, 1 },
  { 2007,11,11,   1428,11, 1 },
  { 2007,12,11,   1428,12, 1 },
  { 2008, 1,10,   1429, 1, 1 },
  { 2008, 2, 8,   1429, 2, 1 },
  { 2008, 3, 9,   1429, 3, 1 },
  { 2008, 4, 7,   1429, 4, 1 },
  { 2008, 5, 6,   1429, 5, 1 },
  { 2008, 6, 5,   1429, 6, 1 },
  { 2008, 7, 4,   1429, 7, 1 },
  { 2008, 8, 2,   1429, 8, 1 },
  { 2008, 9, 1,   1429, 9, 1 },
  { 2008,10, 1,   1429,10, 1 },
  { 2008,10,30,   1429,11, 1 },
  { 2008,11,29,   1429,12, 1 },
  { 2008,12,29,   1430, 1, 1 },
  { 2009, 1,27,   1430, 2, 1 },
  { 2009, 2,26,   1430, 3, 1 },
  { 2009, 3,28,   1430, 4, 1 },
  { 2009, 4,26,   1430, 5, 1 },
  { 2009, 5,25,   1430, 6, 1 },
  { 2009, 6,24,   1430, 7, 1 },
  { 2009, 7,23,   1430, 8, 1 },
  { 2009, 8,22,   1430, 9, 1 },
  { 2009, 9,20,   1430,10, 1 },
  { 2009,10,20,   1430,11, 1 },
  { 2009,11,18,   1430,12, 1 },
  { 2009,12,18,   1431, 1, 1 },
  { 2010, 1,16,   1431, 2, 1 },
  { 2010, 2,15,   1431, 3, 1 },
  { 2010, 3,17,   1431, 4, 1 },
  { 2010, 4,15,   1431, 5, 1 },
  { 2010, 5,15,   1431, 6, 1 },
  { 2010, 6,13,   1431, 7, 1 },
  { 2010, 7,13,   1431, 8, 1 },
  { 2010, 8,11,   1431, 9, 1 },
  { 2010, 9,10,   1431,10, 1 },
  { 2010,10, 9,   1431,11, 1 },
  { 2010,11, 7,   1431,12, 1 },
  { 2010,12, 7,   1432, 1, 1 },
  { 2011, 1, 5,   1432, 2, 1 },
  { 2011, 2, 4,   1432, 3, 1 },
  { 2011, 3, 6,   1432, 4, 1 },
  { 2011, 4, 5,   1432, 5, 1 },
  { 2011, 5, 4,   1432, 6, 1 },
  { 2011, 6, 3,   1432, 7, 1 },
  { 2011, 7, 2,   1432, 8, 1 },
  { 2011, 8, 1,   1432, 9, 1 },
  { 2011, 8,30,   1432,10, 1 },
  { 2011, 9,29,   1432,11, 1 },
  { 2011,10,28,   1432,12, 1 },
  { 2011,11,26,   1433, 1, 1 },
  { 2011,12,26,   1433, 2, 1 },
  { 2012, 1,24,   1433, 3, 1 },
  { 2012, 2,23,   1433, 4, 1 },
  { 2012, 3,24,   1433, 5, 1 },
  { 2012, 4,22,   1433, 6, 1 },
  { 2012, 5,22,   1433, 7, 1 },
  { 2012, 6,21,   1433, 8, 1 },
  { 2012, 7,20,   1433, 9, 1 },
  { 2012, 8,19,   1433,10, 1 },
  { 2012, 9,17,   1433,11, 1 },
  { 2012,10,17,   1433,12, 1 },
  { 2012,11,15,   1434, 1, 1 },
  { 2012,12,14,   1434, 2, 1 },
  { 2013, 1,13,   1434, 3, 1 },
  { 2013, 2,11,   1434, 4, 1 },
  { 2013, 3,13,   1434, 5, 1 },
  { 2013, 4,11,   1434, 6, 1 },
  { 2013, 5,11,   1434, 7, 1 },
  { 2013, 6,10,   1434, 8, 1 },
  { 2013, 7, 9,   1434, 9, 1 },
  { 2013, 8, 8,   1434,10, 1 },
  { 2013, 9, 7,   1434,11, 1 },
  { 2013,10, 6,   1434,12, 1 },
  { 2013,11, 4,   1435, 1, 1 },
  { 2013,12, 4,   1435, 2, 1 },
  { 2014, 1, 2,   1435, 3, 1 },
  { 2014, 2, 1,   1435, 4, 1 },
  { 2014, 3, 2,   1435, 5, 1 },
  { 2014, 4, 1,   1435, 6, 1 },
  { 2014, 4,30,   1435, 7, 1 },
  { 2014, 5,30,   1435, 8, 1 },
  { 2014, 6,28,   1435, 9, 1 },
  { 2014, 7,28,   1435,10, 1 },
  { 2014, 8,27,   1435,11, 1 },
  { 2014, 9,25,   1435,12, 1 },
  { 2014,10,25,   1436, 1, 1 },
  { 2014,11,23,   1436, 2, 1 },
  { 2014,12,23,   1436, 3, 1 },
  { 2015, 1,21,   1436, 4, 1 },
  { 2015, 2,20,   1436, 5, 1 },
  { 2015, 3,21,   1436, 6, 1 },
  { 2015, 4,20,   1436, 7, 1 },
  { 2015, 5,19,   1436, 8, 1 },
  { 2015, 6,18,   1436, 9, 1 },
  { 2015, 7,17,   1436,10, 1 },
  { 2015, 8,16,   1436,11, 1 },
  { 2015, 9,14,   1436,12, 1 },
  { 2015,10,14,   1437, 1, 1 },
  { 2015,11,13,   1437, 2, 1 },
  { 2015,12,12,   1437, 3, 1 },
  { 2016, 1,11,   1437, 4, 1 },
  { 2016, 2,10,   1437, 5, 1 },
  { 2016, 3,10,   1437, 6, 1 },
  { 2016, 4, 8,   1437, 7, 1 },
  { 2016, 5, 8,   1437, 8, 1 },
  { 2016, 6, 6,   1437, 9, 1 },
  { 2016, 7, 6,   1437,10, 1 },
  { 2016, 8, 4,   1437,11, 1 },
  { 2016, 9, 2,   1437,12, 1 },
  { 2016,10, 2,   1438, 1, 1 },
  { 2016,11, 1,   1438, 2, 1 },
  { 2016,11,30,   1438, 3, 1 },
  { 2016,12,30,   1438, 4, 1 },
  { 2017, 1,29,   1438, 5, 1 },
  { 2017, 2,28,   1438, 6, 1 },
  { 2017, 3,29,   1438, 7, 1 },
  { 2017, 4,27,   1438, 8, 1 },
  { 2017, 5,27,   1438, 9, 1 },
  { 2017, 6,25,   1438,10, 1 },
  { 2017, 7,24,   1438,11, 1 },
  { 2017, 8,23,   1438,12, 1 },
  { 2017, 9,21,   1439, 1, 1 },
  { 2017,10,21,   1439, 2, 1 },
  { 2017,11,19,   1439, 3, 1 },
  { 2017,12,19,   1439, 4, 1 },
  { 2018, 1,18,   1439, 5, 1 },
  { 2018, 2,17,   1439, 6, 1 },
  { 2018, 3,18,   1439, 7, 1 },
  { 2018, 4,17,   1439, 8, 1 },
  { 2018, 5,16,   1439, 9, 1 },
  { 2018, 6,15,   1439,10, 1 },
  { 2018, 7,14,   1439,11, 1 },
  { 2018, 8,12,   1439,12, 1 },
  { 2018, 9,11,   1440, 1, 1 },
  { 2019, 8,31,   1441, 1, 1 },
  { 2020, 8,20,   1442, 1, 1 },
  { 2021, 8, 9,   1443, 1, 1 },
  { 2022, 7,30,   1444, 1, 1 },
  { 2023, 7,19,   1445, 1, 1 },
  { 2024, 7, 7,   1446, 1, 1 },
  { 2025, 6,26,   1447, 1, 1 },
  { 2026, 6,16,   1448, 1, 1 },
  { 2027, 6, 6,   1449, 1, 1 },
  { 2028, 5,25,   1450, 1, 1 },
  { 2029, 5,14,   1451, 1, 1 },
  { 2030, 5, 4,   1452, 1, 1 },
  { 2031, 4,23,   1453, 1, 1 },
  { 2032, 4,11,   1454, 1, 1 },
  { 2033, 4, 1,   1455, 1, 1 },
  { 2034, 3,22,   1456, 1, 1 },
  { 2035, 3,11,   1457, 1, 1 },
  { 2036, 2,29,   1458, 1, 1 },
  { 2037, 2,17,   1459, 1, 1 },
  { 2038, 2, 6,   1460, 1, 1 },
  { 2039, 1,26,   1461, 1, 1 },
  { 2040, 1,15,   1462, 1, 1 },
  { 2041, 1, 4,   1463, 1, 1 },
  { 2041,12,25,   1464, 1, 1 },
  { 2042,12,14,   1465, 1, 1 },
  { 2043,12, 3,   1466, 1, 1 },
  { 2044,11,21,   1467, 1, 1 },
  { 2045,11,11,   1468, 1, 1 },
  { 2046,10,31,   1469, 1, 1 },
  { 2047,10,21,   1470, 1, 1 },
  { 2048,10, 9,   1471, 1, 1 },
  { 2049, 9,29,   1472, 1, 1 },
  { 2050, 9,18,   1473, 1, 1 },
  { 2051, 9, 7,   1474, 1, 1 },
  { 2052, 8,26,   1475, 1, 1 },
  { 2053, 8,15,   1476, 1, 1 },
  { 2054, 8, 5,   1477, 1, 1 },
  { 2055, 7,26,   1478, 1, 1 },
  { 2056, 7,14,   1479, 1, 1 },
  { 2057, 7, 3,   1480, 1, 1 },
  { 2058, 6,22,   1481, 1, 1 },
  { 2059, 6,11,   1482, 1, 1 },
  { 2061, 5,21,   1484, 1, 1 },
  { 2063, 4,30,   1486, 1, 1 },
  { 2065, 4, 7,   1488, 1, 1 },
  { 2067, 3,17,   1490, 1, 1 },
  { 2069, 2,23,   1492, 1, 1 },
  { 2071, 2, 2,   1494, 1, 1 },
  { 2073, 1,10,   1496, 1, 1 },
  { 2074,12,20,   1498, 1, 1 },
  { 2076,11,28,   1500, 1, 1 },
  {    0, 0, 0,      0, 0, 0 }, // terminator
};

static const char16_t zoneSA[] = {0x41,0x73,0x69,0x61,0x2F,0x52,0x69,0x79,0x61,0x64,0x68,0}; // "Asia/Riyadh"

void CalendarTest::TestIslamicUmAlQura() {

    UErrorCode status = U_ZERO_ERROR;
    Locale umalquraLoc("ar_SA@calendar=islamic-umalqura");
    Locale gregoLoc("ar_SA@calendar=gregorian");
    TimeZone* tzSA = TimeZone::createTimeZone(UnicodeString(true, zoneSA, -1));
    Calendar* tstCal = Calendar::createInstance(*tzSA, umalquraLoc, status);
    Calendar* gregCal = Calendar::createInstance(*tzSA, gregoLoc, status);

    IslamicCalendar* iCal = dynamic_cast<IslamicCalendar*>(tstCal);
    if(uprv_strcmp(iCal->getType(), "islamic-umalqura") != 0) {
        errln("wrong type of calendar created - %s", iCal->getType());
    }

    int32_t firstYear = 1318;
    int32_t lastYear = 1368;    // just enough to be pretty sure
    //int32_t lastYear = 1480;    // the whole shootin' match

    tstCal->clear();
    tstCal->setLenient(false);

    int32_t day=0, month=0, year=0, initDay = 27, initMonth = IslamicCalendar::RAJAB, initYear = 1434;

    for( int32_t startYear = firstYear; startYear <= lastYear; startYear++) {
        setAndTestWholeYear(tstCal, startYear, status);
        status = U_ZERO_ERROR;
    }

    initMonth = IslamicCalendar::RABI_2;
    initDay = 5;
    int32_t loopCnt = 25;
    tstCal->clear();
    setAndTestCalendar( tstCal, initMonth, initDay, initYear, status);
    TEST_CHECK_STATUS;

    for(int x=1; x<=loopCnt; x++) {
        day = tstCal->get(UCAL_DAY_OF_MONTH,status);
        month = tstCal->get(UCAL_MONTH,status);
        year = tstCal->get(UCAL_YEAR,status);
        TEST_CHECK_STATUS;
        tstCal->roll(UCAL_DAY_OF_MONTH, (UBool)true, status);
        TEST_CHECK_STATUS;
    }

    if(day != (initDay + loopCnt - 1) || month != IslamicCalendar::RABI_2 || year != 1434)
      errln("invalid values for RABI_2 date after roll of %d", loopCnt);

    status = U_ZERO_ERROR;
    tstCal->clear();
    initMonth = 2;
    initDay = 30;
    setAndTestCalendar( tstCal, initMonth, initDay, initYear, status);
    if(U_SUCCESS(status)) {
        errln("error NOT detected status %i",status);
        errln("      init values:\tmonth %i\tday %i\tyear %i", initMonth, initDay, initYear);
        int32_t day = tstCal->get(UCAL_DAY_OF_MONTH, status);
        int32_t month = tstCal->get(UCAL_MONTH, status);
        int32_t year = tstCal->get(UCAL_YEAR, status);
        errln("values post set():\tmonth %i\tday %i\tyear %i",month, day, year);
    }

    status = U_ZERO_ERROR;
    tstCal->clear();
    initMonth = 3;
    initDay = 30;
    setAndTestCalendar( tstCal, initMonth, initDay, initYear, status);
    TEST_CHECK_STATUS;

    SimpleDateFormat* formatter = new SimpleDateFormat("yyyy-MM-dd", Locale::getUS(), status);
    UDate date = formatter->parse("1975-05-06", status);
    Calendar* is_cal = Calendar::createInstance(umalquraLoc, status);
    is_cal->setTime(date, status);
    int32_t is_day = is_cal->get(UCAL_DAY_OF_MONTH,status);
    int32_t is_month = is_cal->get(UCAL_MONTH,status);
    int32_t is_year = is_cal->get(UCAL_YEAR,status);
    TEST_CHECK_STATUS;
    if(is_day != 24 || is_month != IslamicCalendar::RABI_2 || is_year != 1395)
        errln("unexpected conversion date month %i not %i or day %i not 24 or year %i not 1395", is_month, IslamicCalendar::RABI_2, is_day, is_year);

    UDate date2 = is_cal->getTime(status);
    TEST_CHECK_STATUS;
    if(date2 != date) {
        errln("before(%f) and after(%f) dates don't match up!",date, date2);
    }

    // check against data
    const GregoUmmAlQuraMap* guMapPtr;
    gregCal->clear();
    tstCal->clear();
    for (guMapPtr = guMappings; guMapPtr->gYear != 0; guMapPtr++) {
        status = U_ZERO_ERROR;
        gregCal->set(guMapPtr->gYear, guMapPtr->gMon - 1, guMapPtr->gDay, 12, 0);
        date = gregCal->getTime(status);
        tstCal->setTime(date, status);
        int32_t uYear = tstCal->get(UCAL_YEAR, status);
        int32_t uMon = tstCal->get(UCAL_MONTH, status) + 1;
        int32_t uDay = tstCal->get(UCAL_DATE, status);
        if(U_FAILURE(status)) {
            errln("For gregorian %4d-%02d-%02d, get status %s",
                    guMapPtr->gYear, guMapPtr->gMon, guMapPtr->gDay, u_errorName(status) );
        } else if (uYear != guMapPtr->uYear || uMon != guMapPtr->uMon || uDay != guMapPtr->uDay) {
            errln("For gregorian %4d-%02d-%02d, expect umalqura %4d-%02d-%02d, get %4d-%02d-%02d",
                    guMapPtr->gYear, guMapPtr->gMon, guMapPtr->gDay,
                    guMapPtr->uYear, guMapPtr->uMon, guMapPtr->uDay, uYear, uMon, uDay );
        }
    }

    delete is_cal;
    delete formatter;
    delete gregCal;
    delete tstCal;
    delete tzSA;
}

void CalendarTest::TestIslamicTabularDates() {
    UErrorCode status = U_ZERO_ERROR;
    Locale islamicLoc("ar_SA@calendar=islamic-civil");
    Locale tblaLoc("ar_SA@calendar=islamic-tbla");
    SimpleDateFormat* formatter = new SimpleDateFormat("yyyy-MM-dd", Locale::getUS(), status);
    UDate date = formatter->parse("1975-05-06", status);

    Calendar* tstCal = Calendar::createInstance(islamicLoc, status);
    tstCal->setTime(date, status);
    int32_t is_day = tstCal->get(UCAL_DAY_OF_MONTH,status);
    int32_t is_month = tstCal->get(UCAL_MONTH,status);
    int32_t is_year = tstCal->get(UCAL_YEAR,status);
    TEST_CHECK_STATUS;
    delete tstCal;

    tstCal = Calendar::createInstance(tblaLoc, status);
    tstCal->setTime(date, status);
    int32_t tbla_day = tstCal->get(UCAL_DAY_OF_MONTH,status);
    int32_t tbla_month = tstCal->get(UCAL_MONTH,status);
    int32_t tbla_year = tstCal->get(UCAL_YEAR,status);
    TEST_CHECK_STATUS;

    if(tbla_month != is_month || tbla_year != is_year)
        errln("unexpected difference between islamic and tbla month %d : %d and/or year %d : %d",tbla_month,is_month,tbla_year,is_year);

    if(tbla_day - is_day != 1)
        errln("unexpected day difference between islamic and tbla: %d : %d ",tbla_day,is_day);
    delete tstCal;
    delete formatter;
}

void CalendarTest::TestHebrewMonthValidation() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar>  cal(Calendar::createInstance(Locale::createFromName("he_IL@calendar=hebrew"), status));
    if (failure(status, "Calendar::createInstance, locale:he_IL@calendar=hebrew", true)) return;
    Calendar *pCal = cal.getAlias();

    UDate d;
    pCal->setLenient(false);

    // 5776 is a leap year and has month Adar I
    pCal->set(5776, HebrewCalendar::ADAR_1, 1);
    d = pCal->getTime(status);
    if (U_FAILURE(status)) {
        errln("Fail: 5776 Adar I 1 is a valid date.");
    }
    status = U_ZERO_ERROR;

    // 5777 is NOT a lear year and does not have month Adar I
    pCal->set(5777, HebrewCalendar::ADAR_1, 1);
    d = pCal->getTime(status);
    (void)d;
    if (status == U_ILLEGAL_ARGUMENT_ERROR) {
        logln("Info: U_ILLEGAL_ARGUMENT_ERROR, because 5777 Adar I 1 is not a valid date.");
    } else {
        errln("Fail: U_ILLEGAL_ARGUMENT_ERROR should be set for input date 5777 Adar I 1.");
    }
}

void CalendarTest::TestWeekData() {
    // Each line contains two locales using the same set of week rule data.
    const char* LOCALE_PAIRS[] = {
        "en",       "en_US",
        "de",       "de_DE",
        "de_DE",    "en_DE",
        "en_GB",    "und_GB",
        "ar_EG",    "en_EG",
        "ar_SA",    "fr_SA",
        nullptr
    };

    UErrorCode status;

    for (int32_t i = 0; LOCALE_PAIRS[i] != nullptr; i += 2) {
        status = U_ZERO_ERROR;
        LocalPointer<Calendar>  cal1(Calendar::createInstance(LOCALE_PAIRS[i], status));
        LocalPointer<Calendar>  cal2(Calendar::createInstance(LOCALE_PAIRS[i + 1], status));
        TEST_CHECK_STATUS_LOCALE(LOCALE_PAIRS[i]);

        // First day of week
        UCalendarDaysOfWeek dow1 = cal1->getFirstDayOfWeek(status);
        UCalendarDaysOfWeek dow2 = cal2->getFirstDayOfWeek(status);
        TEST_CHECK_STATUS;
        TEST_ASSERT(dow1 == dow2);

        // Minimum days in first week
        uint8_t minDays1 = cal1->getMinimalDaysInFirstWeek();
        uint8_t minDays2 = cal2->getMinimalDaysInFirstWeek();
        TEST_ASSERT(minDays1 == minDays2);

        // Weekdays and Weekends
        for (int32_t d = UCAL_SUNDAY; d <= UCAL_SATURDAY; d++) {
            status = U_ZERO_ERROR;
            UCalendarWeekdayType wdt1 = cal1->getDayOfWeekType((UCalendarDaysOfWeek)d, status);
            UCalendarWeekdayType wdt2 = cal2->getDayOfWeekType((UCalendarDaysOfWeek)d, status);
            TEST_CHECK_STATUS;
            TEST_ASSERT(wdt1 == wdt2);
        }
    }
}

typedef struct {
    const char* zone;
    const CalFields base;
    int32_t deltaDays;
    UCalendarWallTimeOption skippedWTOpt;
    const CalFields expected;
} TestAddAcrossZoneTransitionData;

static const TestAddAcrossZoneTransitionData AAZTDATA[] =
{
    // Time zone                Base wall time                      day(s)  Skipped time options
    //                          Expected wall time

    // Add 1 day, from the date before DST transition
    {"America/Los_Angeles",     CalFields(2014,3,8,1,59,59,999),    1,      UCAL_WALLTIME_FIRST,
                                CalFields(2014,3,9,1,59,59,999)},

    {"America/Los_Angeles",     CalFields(2014,3,8,1,59,59,999),    1,      UCAL_WALLTIME_LAST,
                                CalFields(2014,3,9,1,59,59,999)},

    {"America/Los_Angeles",     CalFields(2014,3,8,1,59,59,999),    1,      UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2014,3,9,1,59,59,999)},


    {"America/Los_Angeles",     CalFields(2014,3,8,2,0,0,0),        1,      UCAL_WALLTIME_FIRST,
                                CalFields(2014,3,9,1,0,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,8,2,0,0,0),        1,      UCAL_WALLTIME_LAST,
                                CalFields(2014,3,9,3,0,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,8,2,0,0,0),        1,      UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2014,3,9,3,0,0,0)},


    {"America/Los_Angeles",     CalFields(2014,3,8,2,30,0,0),       1,      UCAL_WALLTIME_FIRST,
                                CalFields(2014,3,9,1,30,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,8,2,30,0,0),       1,      UCAL_WALLTIME_LAST,
                                CalFields(2014,3,9,3,30,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,8,2,30,0,0),       1,      UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2014,3,9,3,0,0,0)},


    {"America/Los_Angeles",     CalFields(2014,3,8,3,0,0,0),        1,      UCAL_WALLTIME_FIRST,
                                CalFields(2014,3,9,3,0,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,8,3,0,0,0),        1,      UCAL_WALLTIME_LAST,
                                CalFields(2014,3,9,3,0,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,8,3,0,0,0),        1,      UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2014,3,9,3,0,0,0)},

    // Subtract 1 day, from one day after DST transition
    {"America/Los_Angeles",     CalFields(2014,3,10,1,59,59,999),   -1,     UCAL_WALLTIME_FIRST,
                                CalFields(2014,3,9,1,59,59,999)},

    {"America/Los_Angeles",     CalFields(2014,3,10,1,59,59,999),   -1,     UCAL_WALLTIME_LAST,
                                CalFields(2014,3,9,1,59,59,999)},

    {"America/Los_Angeles",     CalFields(2014,3,10,1,59,59,999),   -1,     UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2014,3,9,1,59,59,999)},


    {"America/Los_Angeles",     CalFields(2014,3,10,2,0,0,0),       -1,     UCAL_WALLTIME_FIRST,
                                CalFields(2014,3,9,1,0,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,10,2,0,0,0),       -1,     UCAL_WALLTIME_LAST,
                                CalFields(2014,3,9,3,0,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,10,2,0,0,0),       -1,     UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2014,3,9,3,0,0,0)},


    {"America/Los_Angeles",     CalFields(2014,3,10,2,30,0,0),      -1,     UCAL_WALLTIME_FIRST,
                                CalFields(2014,3,9,1,30,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,10,2,30,0,0),      -1,     UCAL_WALLTIME_LAST,
                                CalFields(2014,3,9,3,30,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,10,2,30,0,0),      -1,     UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2014,3,9,3,0,0,0)},


    {"America/Los_Angeles",     CalFields(2014,3,10,3,0,0,0),       -1,     UCAL_WALLTIME_FIRST,
                                CalFields(2014,3,9,3,0,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,10,3,0,0,0),       -1,     UCAL_WALLTIME_LAST,
                                CalFields(2014,3,9,3,0,0,0)},

    {"America/Los_Angeles",     CalFields(2014,3,10,3,0,0,0),       -1,     UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2014,3,9,3,0,0,0)},


    // Test case for ticket#10544
    {"America/Santiago",        CalFields(2013,4,27,0,0,0,0),       134,    UCAL_WALLTIME_FIRST,
                                CalFields(2013,9,7,23,0,0,0)},

    {"America/Santiago",        CalFields(2013,4,27,0,0,0,0),       134,    UCAL_WALLTIME_LAST,
                                CalFields(2013,9,8,1,0,0,0)},

    {"America/Santiago",        CalFields(2013,4,27,0,0,0,0),       134,    UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2013,9,8,1,0,0,0)},


    {"America/Santiago",        CalFields(2013,4,27,0,30,0,0),      134,    UCAL_WALLTIME_FIRST,
                                CalFields(2013,9,7,23,30,0,0)},

    {"America/Santiago",        CalFields(2013,4,27,0,30,0,0),      134,    UCAL_WALLTIME_LAST,
                                CalFields(2013,9,8,1,30,0,0)},

    {"America/Santiago",        CalFields(2013,4,27,0,30,0,0),      134,    UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2013,9,8,1,0,0,0)},


    // Extreme transition - Pacific/Apia completely skips 2011-12-30
    {"Pacific/Apia",            CalFields(2011,12,29,0,0,0,0),      1,      UCAL_WALLTIME_FIRST,
                                CalFields(2011,12,31,0,0,0,0)},

    {"Pacific/Apia",            CalFields(2011,12,29,0,0,0,0),      1,      UCAL_WALLTIME_LAST,
                                CalFields(2011,12,31,0,0,0,0)},

    {"Pacific/Apia",            CalFields(2011,12,29,0,0,0,0),      1,      UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2011,12,31,0,0,0,0)},


    {"Pacific/Apia",            CalFields(2011,12,31,12,0,0,0),     -1,     UCAL_WALLTIME_FIRST,
                                CalFields(2011,12,29,12,0,0,0)},

    {"Pacific/Apia",            CalFields(2011,12,31,12,0,0,0),     -1,     UCAL_WALLTIME_LAST,
                                CalFields(2011,12,29,12,0,0,0)},

    {"Pacific/Apia",            CalFields(2011,12,31,12,0,0,0),     -1,     UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2011,12,29,12,0,0,0)},


    // 30 minutes DST - Australia/Lord_Howe
    {"Australia/Lord_Howe",     CalFields(2013,10,5,2,15,0,0),      1,      UCAL_WALLTIME_FIRST,
                                CalFields(2013,10,6,1,45,0,0)},

    {"Australia/Lord_Howe",     CalFields(2013,10,5,2,15,0,0),      1,      UCAL_WALLTIME_LAST,
                                CalFields(2013,10,6,2,45,0,0)},

    {"Australia/Lord_Howe",     CalFields(2013,10,5,2,15,0,0),      1,      UCAL_WALLTIME_NEXT_VALID,
                                CalFields(2013,10,6,2,30,0,0)},

    {nullptr, CalFields(0,0,0,0,0,0,0), 0, UCAL_WALLTIME_LAST, CalFields(0,0,0,0,0,0,0)}
};

void CalendarTest::TestAddAcrossZoneTransition() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar cal(status);
    TEST_CHECK_STATUS;

    for (int32_t i = 0; AAZTDATA[i].zone; i++) {
        status = U_ZERO_ERROR;
        TimeZone *tz = TimeZone::createTimeZone(AAZTDATA[i].zone);
        cal.adoptTimeZone(tz);
        cal.setSkippedWallTimeOption(AAZTDATA[i].skippedWTOpt);
        AAZTDATA[i].base.setTo(cal);
        cal.add(UCAL_DATE, AAZTDATA[i].deltaDays, status);
        TEST_CHECK_STATUS;

        if (!AAZTDATA[i].expected.isEquivalentTo(cal, status)) {
            CalFields res(cal, status);
            TEST_CHECK_STATUS;
            char buf[32];
            const char *optDisp = AAZTDATA[i].skippedWTOpt == UCAL_WALLTIME_FIRST ? "FIRST" :
                    AAZTDATA[i].skippedWTOpt == UCAL_WALLTIME_LAST ? "LAST" : "NEXT_VALID";
            dataerrln(UnicodeString("Error: base:") + AAZTDATA[i].base.toString(buf, sizeof(buf)) + ", tz:" + AAZTDATA[i].zone
                        + ", delta:" + AAZTDATA[i].deltaDays + " day(s), opt:" + optDisp
                        + ", result:" + res.toString(buf, sizeof(buf))
                        + " - expected:" + AAZTDATA[i].expected.toString(buf, sizeof(buf)));
        }
    }
}

// Data in a separate file (Gregorian to Chinese lunar map)
#define INCLUDED_FROM_CALTEST_CPP
#include "caltestdata.h"

void CalendarTest::TestChineseCalendarMapping() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<TimeZone> zone(TimeZone::createTimeZone(UnicodeString("China")));
    Locale locEnCalGregory = Locale::createFromName("en@calendar=gregorian");
    Locale locEnCalChinese = Locale::createFromName("en@calendar=chinese");
    LocalPointer<Calendar>  calGregory(Calendar::createInstance(zone->clone(), locEnCalGregory, status));
    LocalPointer<Calendar>  calChinese(Calendar::createInstance(zone.orphan(), locEnCalChinese, status));
    if ( U_FAILURE(status) ) {
        errln("Fail: Calendar::createInstance fails for en with calendar=gregorian or calendar=chinese: %s", u_errorName(status));
    } else {
        const GregoToLunar * mapPtr = gregoToLunar; // in "caltestdata.h" included above
        calGregory->clear();
        calChinese->clear();
        for (; mapPtr->gyr != 0; mapPtr++) {
            status = U_ZERO_ERROR;
            calGregory->set(mapPtr->gyr, mapPtr->gmo - 1, mapPtr->gda, 8, 0);
            UDate date = calGregory->getTime(status);
            calChinese->setTime(date, status);
            if ( U_FAILURE(status) ) {
                errln("Fail: for Gregorian %4d-%02d-%02d, calGregory->getTime or calChinese->setTime reports: %s",
                        mapPtr->gyr, mapPtr->gmo, mapPtr->gda, u_errorName(status));
                continue;
            }
            int32_t era = calChinese->get(UCAL_ERA, status);
            int32_t yr  = calChinese->get(UCAL_YEAR, status);
            int32_t mo  = calChinese->get(UCAL_MONTH, status) + 1;
            int32_t lp  = calChinese->get(UCAL_IS_LEAP_MONTH, status);
            int32_t da  = calChinese->get(UCAL_DATE, status);
            if ( U_FAILURE(status) ) {
                errln("Fail: for Gregorian %4d-%02d-%02d, calChinese->get (for era, yr, mo, leapmo, da) reports: %s",
                        mapPtr->gyr, mapPtr->gmo, mapPtr->gda, u_errorName(status));
                continue;
            }
            if (yr != mapPtr->cyr || mo != mapPtr->cmo || lp != mapPtr->clp || da != mapPtr->cda) {
                errln("Fail: for Gregorian %4d-%02d-%02d, expected Chinese %2d-%02d(%d)-%02d, got %2d-%02d(%d)-%02d",
                        mapPtr->gyr, mapPtr->gmo, mapPtr->gda, mapPtr->cyr, mapPtr->cmo, mapPtr->clp, mapPtr->cda, yr, mo, lp, da);
                continue;
            }
            // If Grego->Chinese worked, try reverse mapping
            calChinese->set(UCAL_ERA, era);
            calChinese->set(UCAL_YEAR, mapPtr->cyr);
            calChinese->set(UCAL_MONTH, mapPtr->cmo - 1);
            calChinese->set(UCAL_IS_LEAP_MONTH, mapPtr->clp);
            calChinese->set(UCAL_DATE, mapPtr->cda);
            calChinese->set(UCAL_HOUR_OF_DAY, 8);
            date = calChinese->getTime(status);
            calGregory->setTime(date, status);
            if ( U_FAILURE(status) ) {
                errln("Fail: for Chinese %2d-%02d(%d)-%02d, calChinese->getTime or calGregory->setTime reports: %s",
                        mapPtr->cyr, mapPtr->cmo, mapPtr->clp, mapPtr->cda, u_errorName(status));
                continue;
            }
            yr  = calGregory->get(UCAL_YEAR, status);
            mo  = calGregory->get(UCAL_MONTH, status) + 1;
            da  = calGregory->get(UCAL_DATE, status);
            if ( U_FAILURE(status) ) {
                errln("Fail: for Chinese %2d-%02d(%d)-%02d, calGregory->get (for yr, mo, da) reports: %s",
                        mapPtr->cyr, mapPtr->cmo, mapPtr->clp, mapPtr->cda, u_errorName(status));
                continue;
            }
            if (yr != mapPtr->gyr || mo != mapPtr->gmo || da != mapPtr->gda) {
                errln("Fail: for Chinese %2d-%02d(%d)-%02d, Gregorian %4d-%02d-%02d, got %4d-%02d-%02d",
                        mapPtr->cyr, mapPtr->cmo, mapPtr->clp, mapPtr->cda, mapPtr->gyr, mapPtr->gmo, mapPtr->gda, yr, mo, da);
                continue;
            }
        }
    }
}

void CalendarTest::TestClearMonth() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(Calendar::createInstance(Locale::getRoot(), status));
    if (failure(status, "construct Calendar")) return;
    cal->set(2023, UCAL_JUNE, 29);
    assertEquals("Calendar::get(UCAL_MONTH)", UCAL_JUNE, cal->get(UCAL_MONTH, status));
    if (failure(status, "Calendar::get(UCAL_MONTH)")) return;
    cal->clear(UCAL_MONTH);
    assertEquals("Calendar::isSet(UCAL_MONTH) after clear(UCAL_MONTH)", false, !!cal->isSet(UCAL_MONTH));
    assertEquals("Calendar::get(UCAL_MONTH after clear(UCAL_MONTH))", UCAL_JANUARY, !!cal->get(UCAL_MONTH, status));
    if (failure(status, "Calendar::get(UCAL_MONTH)")) return;

    cal->set(UCAL_ORDINAL_MONTH, 7);
    assertEquals("Calendar::get(UCAL_MONTH) after set(UCAL_ORDINAL_MONTH, 7)", UCAL_AUGUST, cal->get(UCAL_MONTH, status));
    if (failure(status, "Calendar::get(UCAL_MONTH) after set(UCAL_ORDINAL_MONTH, 7)")) return;
    assertEquals("Calendar::get(UCAL_ORDINAL_MONTH) after set(UCAL_ORDINAL_MONTH, 7)", 7, cal->get(UCAL_ORDINAL_MONTH, status));
    if (failure(status, "Calendar::get(UCAL_ORDINAL_MONTH) after set(UCAL_ORDINAL_MONTH, 7)")) return;

    cal->clear(UCAL_ORDINAL_MONTH);
    assertEquals("Calendar::isSet(UCAL_ORDINAL_MONTH) after clear(UCAL_ORDINAL_MONTH)", false, !!cal->isSet(UCAL_ORDINAL_MONTH));
    assertEquals("Calendar::get(UCAL_MONTH) after clear(UCAL_ORDINAL_MONTH)", UCAL_JANUARY, cal->get(UCAL_MONTH, status));
    if (failure(status, "Calendar::get(UCAL_MONTH) after clear(UCAL_ORDINAL_MONTH)")) return;
    assertEquals("Calendar::get(UCAL_ORDINAL_MONTH) after clear(UCAL_ORDINAL_MONTH)", 0, cal->get(UCAL_ORDINAL_MONTH, status));
    if (failure(status, "Calendar::get(UCAL_ORDINAL_MONTH) after clear(UCAL_ORDINAL_MONTH)")) return;

}

void CalendarTest::TestGregorianCalendarInTemporalLeapYear() {
    // test from year 1800 to 2500
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    if (failure(status, "construct GregorianCalendar")) return;
    for (int32_t year = 1900; year < 2400; ++year) {
        gc.set(year, UCAL_MARCH, 7);
        assertEquals("Calendar::inTemporalLeapYear",
                 gc.isLeapYear(year) == true, gc.inTemporalLeapYear(status) == true);
        if (failure(status, "inTemporalLeapYear")) return;
    }
}

void CalendarTest::RunChineseCalendarInTemporalLeapYearTest(Calendar* cal) {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    if (failure(status, "construct GregorianCalendar")) return;
    LocalPointer<Calendar> leapTest(cal->clone());
    // Start our test from 1900, Jan 1.
    // Check every 29 days in exhausted mode.
    int32_t incrementDays = 29;
    int32_t startYear = 1900;
    int32_t stopYear = 2400;

    if (quick) {
        incrementDays = 317;
        stopYear = 2100;
    }
    int32_t yearForHasLeapMonth = -1;
    bool hasLeapMonth = false;
    for (gc.set(startYear, UCAL_JANUARY, 1);
         gc.get(UCAL_YEAR, status) <= stopYear;
         gc.add(UCAL_DATE, incrementDays, status)) {
        cal->setTime(gc.getTime(status), status);
        if (failure(status, "add/get/set/getTime/setTime incorrect")) return;

        int32_t cal_year = cal->get(UCAL_EXTENDED_YEAR, status);
        if (yearForHasLeapMonth != cal_year) {
            leapTest->set(UCAL_EXTENDED_YEAR, cal_year);
            leapTest->set(UCAL_MONTH, 0);
            leapTest->set(UCAL_DATE, 1);
            // seek any leap month
            // check any leap month in the next 12 months.
            for (hasLeapMonth = false;
                 (!hasLeapMonth) && cal_year == leapTest->get(UCAL_EXTENDED_YEAR, status);
                 leapTest->add(UCAL_MONTH, 1, status)) {
                hasLeapMonth = leapTest->get(UCAL_IS_LEAP_MONTH, status) != 0;
            }
            yearForHasLeapMonth = cal_year;
        }
        if (failure(status, "error while figure out expectation")) return;

        bool actualInLeap =  cal->inTemporalLeapYear(status);
        if (failure(status, "inTemporalLeapYear")) return;
        if (hasLeapMonth != actualInLeap) {
            logln("Gregorian y=%d m=%d d=%d => cal y=%d m=%s%d d=%d expected:%s actual:%s\n",
                   gc.get(UCAL_YEAR, status),
                   gc.get(UCAL_MONTH, status),
                   gc.get(UCAL_DATE, status),
                   cal->get(UCAL_EXTENDED_YEAR, status),
                   cal->get(UCAL_IS_LEAP_MONTH, status) == 1 ? "L" : "",
                   cal->get(UCAL_MONTH, status),
                   cal->get(UCAL_DAY_OF_MONTH, status),
                   hasLeapMonth ? "true" : "false",
                   actualInLeap ? "true" : "false");
        }
        assertEquals("inTemporalLeapYear", hasLeapMonth, actualInLeap);
    }
}

void CalendarTest::TestChineseCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "chinese", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct ChineseCalendar")) return;
    RunChineseCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestDangiCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "dangi", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct DangiCalendar")) return;
    RunChineseCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestHebrewCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "hebrew", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    LocalPointer<Calendar> leapTest(Calendar::createInstance(l, status));
    if (failure(status, "construct HebrewCalendar")) return;
    // Start our test from 1900, Jan 1.
    // Check every 29 days in exhausted mode.
    int32_t incrementDays = 29;
    int32_t startYear = 1900;
    int32_t stopYear = 2400;

    if (quick) {
        incrementDays = 317;
        stopYear = 2100;
    }
    int32_t yearForHasLeapMonth = -1;
    bool hasLeapMonth = false;
    for (gc.set(startYear, UCAL_JANUARY, 1);
         gc.get(UCAL_YEAR, status) <= stopYear;
         gc.add(UCAL_DATE, incrementDays, status)) {
        cal->setTime(gc.getTime(status), status);
        if (failure(status, "add/get/set/getTime/setTime incorrect")) return;

        int32_t cal_year = cal->get(UCAL_EXTENDED_YEAR, status);
        if (yearForHasLeapMonth != cal_year) {
            leapTest->set(UCAL_EXTENDED_YEAR, cal_year);
            leapTest->set(UCAL_MONTH, 0);
            leapTest->set(UCAL_DATE, 1);
            // If 10 months after TISHRI is TAMUZ, then it is a leap year.
            leapTest->add(UCAL_MONTH, 10, status);
            hasLeapMonth = leapTest->get(UCAL_MONTH, status) == icu::HebrewCalendar::TAMUZ;
            yearForHasLeapMonth = cal_year;
        }
        bool actualInLeap =  cal->inTemporalLeapYear(status);
        if (failure(status, "inTemporalLeapYear")) return;
        if (hasLeapMonth != actualInLeap) {
            logln("Gregorian y=%d m=%d d=7 => cal y=%d m=%d d=%d expected:%s actual:%s\n",
                   gc.get(UCAL_YEAR, status),
                   gc.get(UCAL_MONTH, status),
                   cal->get(UCAL_EXTENDED_YEAR, status),
                   cal->get(UCAL_MONTH, status),
                   cal->get(UCAL_DAY_OF_MONTH, status),
                   hasLeapMonth ? "true" : "false",
                   actualInLeap ? "true" : "false");
        }
        assertEquals("inTemporalLeapYear", hasLeapMonth, actualInLeap);
    }
}

void CalendarTest::RunIslamicCalendarInTemporalLeapYearTest(Calendar* cal) {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    if (failure(status, "construct GregorianCalendar")) return;
    // Start our test from 1900, Jan 1.
    // Check every 29 days in exhausted mode.
    int32_t incrementDays = 29;
    int32_t startYear = 1900;
    int32_t stopYear = 2400;

    if (quick) {
        incrementDays = 317;
        stopYear = 2100;
    }
    int32_t yearForHasLeapMonth = -1;
    bool hasLeapMonth = false;
    for (gc.set(startYear, UCAL_JANUARY, 1);
         gc.get(UCAL_YEAR, status) <= stopYear;
         gc.add(UCAL_DATE, incrementDays, status)) {
        cal->setTime(gc.getTime(status), status);
        if (failure(status, "set/getTime/setTime incorrect")) return;
        int32_t cal_year = cal->get(UCAL_EXTENDED_YEAR, status);
        if (yearForHasLeapMonth != cal_year) {
            // If that year has exactly 355 days, it is a leap year.
            hasLeapMonth = cal->getActualMaximum(UCAL_DAY_OF_YEAR, status) == 355;
            yearForHasLeapMonth = cal_year;
        }

        bool actualInLeap =  cal->inTemporalLeapYear(status);
        if (failure(status, "inTemporalLeapYear")) return;
        if (hasLeapMonth != actualInLeap) {
            logln("Gregorian y=%d m=%d d=%d => cal y=%d m=%s%d d=%d expected:%s actual:%s\n",
                   gc.get(UCAL_EXTENDED_YEAR, status),
                   gc.get(UCAL_MONTH, status),
                   gc.get(UCAL_DAY_OF_MONTH, status),
                   cal->get(UCAL_EXTENDED_YEAR, status),
                   cal->get(UCAL_IS_LEAP_MONTH, status) == 1 ? "L" : "",
                   cal->get(UCAL_MONTH, status),
                   cal->get(UCAL_DAY_OF_MONTH, status),
                   hasLeapMonth ? "true" : "false",
                   actualInLeap ? "true" : "false");
        }
        assertEquals("inTemporalLeapYear", hasLeapMonth, actualInLeap);
    }
}

void CalendarTest::TestIslamicCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "islamic", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct IslamicCalendar")) return;
    RunIslamicCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestIslamicCivilCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "islamic-civil", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct IslamicCivilCalendar")) return;
    RunIslamicCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestIslamicUmalquraCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "islamic-umalqura", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct IslamicUmalquraCalendar")) return;
    RunIslamicCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestIslamicRGSACalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "islamic-rgsa", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct IslamicRGSACalendar")) return;
    RunIslamicCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestIslamicTBLACalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "islamic-tbla", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct IslamicTBLACalendar")) return;
    RunIslamicCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(Calendar* cal) {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    if (failure(status, "construct GregorianCalendar")) return;
    // Start our test from 1900, Jan 1.
    // Check every 29 days in exhausted mode.
    int32_t incrementDays = 29;
    int32_t startYear = 1900;
    int32_t stopYear = 2400;

    if (quick) {
        incrementDays = 317;
        stopYear = 2100;
    }
    int32_t yearForHasLeapMonth = -1;
    bool hasLeapMonth = false;
    for (gc.set(startYear, UCAL_JANUARY, 1);
         gc.get(UCAL_YEAR, status) <= stopYear;
         gc.add(UCAL_DATE, incrementDays, status)) {
        cal->setTime(gc.getTime(status), status);
        if (failure(status, "set/getTime/setTime incorrect")) return;
        int32_t cal_year = cal->get(UCAL_EXTENDED_YEAR, status);
        if (yearForHasLeapMonth != cal_year) {
            // If that year has exactly 355 days, it is a leap year.
            hasLeapMonth = cal->getActualMaximum(UCAL_DAY_OF_YEAR, status) == 366;
            if (failure(status, "getActualMaximum incorrect")) return;
            yearForHasLeapMonth = cal_year;
        }
        bool actualInLeap =  cal->inTemporalLeapYear(status);
        if (failure(status, "inTemporalLeapYear")) return;
        if (hasLeapMonth != actualInLeap) {
            logln("Gregorian y=%d m=%d d=%d => cal y=%d m=%s%d d=%d expected:%s actual:%s\n",
                   gc.get(UCAL_EXTENDED_YEAR, status),
                   gc.get(UCAL_MONTH, status),
                   gc.get(UCAL_DAY_OF_MONTH, status),
                   cal->get(UCAL_EXTENDED_YEAR, status),
                   cal->get(UCAL_IS_LEAP_MONTH, status) == 1 ? "L" : "",
                   cal->get(UCAL_MONTH, status),
                   cal->get(UCAL_DAY_OF_MONTH, status),
                   hasLeapMonth ? "true" : "false",
                   actualInLeap ? "true" : "false");
        }
        assertEquals("inTemporalLeapYear", hasLeapMonth, actualInLeap);
    }
}

void CalendarTest::TestTaiwanCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "roc", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct TaiwanCalendar")) return;
    Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestJapaneseCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "japanese", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct JapaneseCalendar")) return;
    Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestBuddhistCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "buddhist", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct BuddhistCalendar")) return;
    Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestPersianCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "persian", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct PersianCalendar")) return;
    Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestIndianCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "indian", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct IndianCalendar")) return;
    Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestCopticCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "coptic", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct CopticCalendar")) return;
    Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestEthiopicCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "ethiopic", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct EthiopicCalendar")) return;
    Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(cal.getAlias());
}

void CalendarTest::TestEthiopicAmeteAlemCalendarInTemporalLeapYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "ethiopic-amete-alem", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct EthiopicAmeteAlemCalendar")) return;
    Run366DaysIsLeapYearCalendarInTemporalLeapYearTest(cal.getAlias());
}

std::string monthCode(int32_t month, bool leap) {
    std::string code("M");
    if (month < 10) {
        code += "0";
        code += ('0' + month);
    } else {
        code += "1";
        code += ('0' + (month % 10));
    }
    if (leap) {
        code += "L";
    }
    return code;
}
std::string hebrewMonthCode(int32_t icuMonth) {
    if (icuMonth == icu::HebrewCalendar::ADAR_1) {
        return monthCode(icuMonth, true);
    }
    return monthCode(icuMonth < icu::HebrewCalendar::ADAR_1 ? icuMonth+1 : icuMonth, false);
}

void CalendarTest::RunChineseCalendarGetTemporalMonthCode(Calendar* cal) {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    if (failure(status, "construct GregorianCalendar")) return;
    // Start our test from 1900, Jan 1.
    // Check every 29 days in exhausted mode.
    int32_t incrementDays = 29;
    int32_t startYear = 1900;
    int32_t stopYear = 2400;

    if (quick) {
        incrementDays = 317;
        startYear = 1950;
        stopYear = 2050;
    }
    for (gc.set(startYear, UCAL_JANUARY, 1);
         gc.get(UCAL_YEAR, status) <= stopYear;
         gc.add(UCAL_DATE, incrementDays, status)) {
        cal->setTime(gc.getTime(status), status);
        if (failure(status, "set/getTime/setTime incorrect")) return;
        int32_t cal_month = cal->get(UCAL_MONTH, status);
        std::string expected = monthCode(
            cal_month + 1, cal->get(UCAL_IS_LEAP_MONTH, status) != 0);
        assertEquals("getTemporalMonthCode", expected.c_str(), cal->getTemporalMonthCode(status));
        if (failure(status, "getTemporalMonthCode")) return;
    }
}

void CalendarTest::TestChineseCalendarGetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "chinese", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct ChineseCalendar")) return;
    RunChineseCalendarGetTemporalMonthCode(cal.getAlias());
}

void CalendarTest::TestDangiCalendarGetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "dangi", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct DangiCalendar")) return;
    RunChineseCalendarGetTemporalMonthCode(cal.getAlias());
}

void CalendarTest::TestHebrewCalendarGetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "hebrew", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    GregorianCalendar gc(status);
    if (failure(status, "construct Calendar")) return;
    // Start our test from 1900, Jan 1.
    // Check every 29 days in exhausted mode.
    int32_t incrementDays = 29;
    int32_t startYear = 1900;
    int32_t stopYear = 2400;

    if (quick) {
        incrementDays = 317;
        stopYear = 2100;
    }
    for (gc.set(startYear, UCAL_JANUARY, 1);
         gc.get(UCAL_YEAR, status) <= stopYear;
         gc.add(UCAL_DATE, incrementDays, status)) {
        cal->setTime(gc.getTime(status), status);
        if (failure(status, "set/getTime/setTime incorrect")) return;
        std::string expected = hebrewMonthCode(cal->get(UCAL_MONTH, status));
        if (failure(status, "get failed")) return;
        assertEquals("getTemporalMonthCode", expected.c_str(), cal->getTemporalMonthCode(status));
        if (failure(status, "getTemporalMonthCode")) return;
    }
}

void CalendarTest::RunCECalendarGetTemporalMonthCode(Calendar* cal) {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    if (failure(status, "construct Calendar")) return;
    // Start testing from 1900
    gc.set(1900, UCAL_JANUARY, 1);
    cal->setTime(gc.getTime(status), status);
    int32_t year = cal->get(UCAL_YEAR, status);
    for (int32_t m = 0; m < 13; m++) {
        std::string expected = monthCode(m+1, false);
        for (int32_t y = year; y < year + 500 ; y++) {
            cal->set(y, m, 1);
            assertEquals("getTemporalMonthCode", expected.c_str(), cal->getTemporalMonthCode(status));
            if (failure(status, "getTemporalMonthCode")) continue;
        }
    }

}

void CalendarTest::TestCopticCalendarGetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "coptic", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct CopticCalendar")) return;
    RunCECalendarGetTemporalMonthCode(cal.getAlias());
}

void CalendarTest::TestEthiopicCalendarGetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "ethiopic", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct EthiopicCalendar")) return;
    RunCECalendarGetTemporalMonthCode(cal.getAlias());
}

void CalendarTest::TestEthiopicAmeteAlemCalendarGetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "ethiopic-amete-alem", status);
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status));
    if (failure(status, "construct EthiopicAmeteAlemCalendar")) return;
    RunCECalendarGetTemporalMonthCode(cal.getAlias());
}

void CalendarTest::TestGregorianCalendarSetTemporalMonthCode() {

    struct TestCase {
      int32_t gYear;
      int32_t gMonth;
      int32_t gDate;
      const char* monthCode;
      int32_t ordinalMonth;
    } cases[] = {
      { 1911, UCAL_JANUARY, 31, "M01", 0 },
      { 1970, UCAL_FEBRUARY, 22, "M02", 1 },
      { 543, UCAL_MARCH, 3, "M03", 2 },
      { 2340, UCAL_APRIL, 21, "M04", 3 },
      { 1234, UCAL_MAY, 21, "M05", 4 },
      { 1931, UCAL_JUNE, 17, "M06", 5 },
      { 2000, UCAL_JULY, 1, "M07", 6 },
      { 2033, UCAL_AUGUST, 3, "M08", 7 },
      { 2013, UCAL_SEPTEMBER, 9, "M09", 8 },
      { 1849, UCAL_OCTOBER, 31, "M10", 9 },
      { 1433, UCAL_NOVEMBER, 30, "M11", 10 },
      { 2022, UCAL_DECEMBER, 25, "M12", 11 },
    };
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc1(status);
    GregorianCalendar gc2(status);
    if (failure(status, "construct GregorianCalendar")) return;
    for (auto& cas : cases) {
        gc1.clear();
        gc2.clear();
        gc1.set(cas.gYear, cas.gMonth, cas.gDate);

        gc2.set(UCAL_YEAR, cas.gYear);
        gc2.setTemporalMonthCode(cas.monthCode, status);
        gc2.set(UCAL_DATE, cas.gDate);
        if (failure(status, "set/setTemporalMonthCode")) return;

        assertTrue("by set and setTemporalMonthCode()", gc1.equals(gc2, status));
        const char* actualMonthCode1 = gc1.getTemporalMonthCode(status);
        const char* actualMonthCode2 = gc2.getTemporalMonthCode(status);
        if (failure(status, "getTemporalMonthCode")) continue;
        assertEquals("getTemporalMonthCode()", 0,
                     uprv_strcmp(actualMonthCode1, actualMonthCode2));
        assertEquals("getTemporalMonthCode()", 0,
                     uprv_strcmp(cas.monthCode, actualMonthCode2));
        assertEquals("ordinalMonth", cas.ordinalMonth, gc2.get(UCAL_ORDINAL_MONTH, status));
        assertEquals("ordinalMonth", gc1.get(UCAL_ORDINAL_MONTH, status),
                     gc2.get(UCAL_ORDINAL_MONTH, status));
    }
}

void CalendarTest::TestChineseCalendarSetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "chinese", status);
    LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
    if (failure(status, "construct ChineseCalendar")) return;
    LocalPointer<Calendar> cc2(cc1->clone());

    struct TestCase {
      int32_t gYear;
      int32_t gMonth;
      int32_t gDate;
      int32_t cYear;
      int32_t cMonth;
      int32_t cDate;
      const char* cMonthCode;
      bool cLeapMonth;
      int32_t cOrdinalMonth;
    } cases[] = {
      // https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2022.pdf
      { 2022, UCAL_DECEMBER, 15, 4659, UCAL_NOVEMBER, 22, "M11", false, 10},
      // M01L is very hard to find. Cannot find a year has M01L in these several
      // centuries.
      // M02L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2004.pdf
      { 2004, UCAL_MARCH, 20, 4641, UCAL_FEBRUARY, 30, "M02", false, 1},
      { 2004, UCAL_MARCH, 21, 4641, UCAL_FEBRUARY, 1, "M02L", true, 2},
      { 2004, UCAL_APRIL, 18, 4641, UCAL_FEBRUARY, 29, "M02L", true, 2},
      { 2004, UCAL_APRIL, 19, 4641, UCAL_MARCH, 1, "M03", false, 3},
      // M03L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/1995.pdf
      { 1955, UCAL_APRIL, 21, 4592, UCAL_MARCH, 29, "M03", false, 2},
      { 1955, UCAL_APRIL, 22, 4592, UCAL_MARCH, 1, "M03L", true, 3},
      { 1955, UCAL_MAY, 21, 4592, UCAL_MARCH, 30, "M03L", true, 3},
      { 1955, UCAL_MAY, 22, 4592, UCAL_APRIL, 1, "M04", false, 4},
      // M12 https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/1996.pdf
      { 1956, UCAL_FEBRUARY, 11, 4592, UCAL_DECEMBER, 30, "M12", false, 12},
      // M04L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2001.pdf
      { 2001, UCAL_MAY, 22, 4638, UCAL_APRIL, 30, "M04", false, 3},
      { 2001, UCAL_MAY, 23, 4638, UCAL_APRIL, 1, "M04L", true, 4},
      { 2001, UCAL_JUNE, 20, 4638, UCAL_APRIL, 29, "M04L", true, 4},
      { 2001, UCAL_JUNE, 21, 4638, UCAL_MAY, 1, "M05", false, 5},
      // M05L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2009.pdf
      { 2009, UCAL_JUNE, 22, 4646, UCAL_MAY, 30, "M05", false, 4},
      { 2009, UCAL_JUNE, 23, 4646, UCAL_MAY, 1, "M05L", true, 5},
      { 2009, UCAL_JULY, 21, 4646, UCAL_MAY, 29, "M05L", true, 5},
      { 2009, UCAL_JULY, 22, 4646, UCAL_JUNE, 1, "M06", false, 6},
      // M06L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2017.pdf
      { 2017, UCAL_JULY, 22, 4654, UCAL_JUNE, 29, "M06", false, 5},
      { 2017, UCAL_JULY, 23, 4654, UCAL_JUNE, 1, "M06L", true, 6},
      { 2017, UCAL_AUGUST, 21, 4654, UCAL_JUNE, 30, "M06L", true, 6},
      { 2017, UCAL_AUGUST, 22, 4654, UCAL_JULY, 1, "M07", false, 7},
      // M07L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2006.pdf
      { 2006, UCAL_AUGUST, 23, 4643, UCAL_JULY, 30, "M07", false, 6},
      { 2006, UCAL_AUGUST, 24, 4643, UCAL_JULY, 1, "M07L", true, 7},
      { 2006, UCAL_SEPTEMBER, 21, 4643, UCAL_JULY, 29, "M07L", true, 7},
      { 2006, UCAL_SEPTEMBER, 22, 4643, UCAL_AUGUST, 1, "M08", false, 8},
      // M08L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/1995.pdf
      { 1995, UCAL_SEPTEMBER, 24, 4632, UCAL_AUGUST, 30, "M08", false, 7},
      { 1995, UCAL_SEPTEMBER, 25, 4632, UCAL_AUGUST, 1, "M08L", true, 8},
      { 1995, UCAL_OCTOBER, 23, 4632, UCAL_AUGUST, 29, "M08L", true, 8},
      { 1995, UCAL_OCTOBER, 24, 4632, UCAL_SEPTEMBER, 1, "M09", false, 9},
      // M09L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2014.pdf
      { 2014, UCAL_OCTOBER, 23, 4651, UCAL_SEPTEMBER, 30, "M09", false, 8},
      { 2014, UCAL_OCTOBER, 24, 4651, UCAL_SEPTEMBER, 1, "M09L", true, 9},
      { 2014, UCAL_NOVEMBER, 21, 4651, UCAL_SEPTEMBER, 29, "M09L", true, 9},
      { 2014, UCAL_NOVEMBER, 22, 4651, UCAL_OCTOBER, 1, "M10", false, 10},
      // M10L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/1984.pdf
      { 1984, UCAL_NOVEMBER, 22, 4621, UCAL_OCTOBER, 30, "M10", false, 9},
      { 1984, UCAL_NOVEMBER, 23, 4621, UCAL_OCTOBER, 1, "M10L", true, 10},
      { 1984, UCAL_DECEMBER, 21, 4621, UCAL_OCTOBER, 29, "M10L", true, 10},
      { 1984, UCAL_DECEMBER, 22, 4621, UCAL_NOVEMBER, 1, "M11", false, 11},
      // M11L https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2033.pdf
      //      https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2034.pdf
      { 2033, UCAL_DECEMBER, 21, 4670, UCAL_NOVEMBER, 30, "M11", false, 10},
      { 2033, UCAL_DECEMBER, 22, 4670, UCAL_NOVEMBER, 1, "M11L", true, 11},
      { 2034, UCAL_JANUARY, 19, 4670, UCAL_NOVEMBER, 29, "M11L", true, 11},
      { 2034, UCAL_JANUARY, 20, 4670, UCAL_DECEMBER, 1, "M12", false, 12},
      // M12L is very hard to find. Cannot find a year has M01L in these several
      // centuries.
    };
    GregorianCalendar gc1(status);
    if (failure(status, "construct Calendar")) return;
    for (auto& cas : cases) {
        gc1.clear();
        cc1->clear();
        cc2->clear();
        gc1.set(cas.gYear, cas.gMonth, cas.gDate);
        cc1->setTime(gc1.getTime(status), status);

        cc2->set(UCAL_EXTENDED_YEAR, cas.cYear);
        cc2->setTemporalMonthCode(cas.cMonthCode, status);
        cc2->set(UCAL_DATE, cas.cDate);

        assertEquals("year", cas.cYear, cc1->get(UCAL_EXTENDED_YEAR, status));
        assertEquals("month", cas.cMonth, cc1->get(UCAL_MONTH, status));
        assertEquals("date", cas.cDate, cc1->get(UCAL_DATE, status));
        assertEquals("is_leap_month", cas.cLeapMonth ? 1 : 0, cc1->get(UCAL_IS_LEAP_MONTH, status));
        assertEquals("getTemporalMonthCode()", 0,
                     uprv_strcmp(cas.cMonthCode, cc1->getTemporalMonthCode(status)));
        if (failure(status, "getTemporalMonthCode")) continue;
        assertEquals("ordinalMonth", cas.cOrdinalMonth, cc1->get(UCAL_ORDINAL_MONTH, status));
        if (! cc2->equals(*cc1, status)) {
            printf("g=%f %f vs %f. diff = %f %d/%d%s/%d vs %d/%d%s/%d\n",
                   gc1.getTime(status), cc1->getTime(status), cc2->getTime(status),
                   cc1->getTime(status) - cc2->getTime(status),
                   cc1->get(UCAL_EXTENDED_YEAR, status),
                   cc1->get(UCAL_MONTH, status)+1,
                   cc1->get(UCAL_IS_LEAP_MONTH, status) == 0 ? "" : "L",
                   cc1->get(UCAL_DATE, status),
                   cc2->get(UCAL_EXTENDED_YEAR, status),
                   cc2->get(UCAL_MONTH, status)+1,
                   cc2->get(UCAL_IS_LEAP_MONTH, status) == 0 ? "" : "L",
                   cc2->get(UCAL_DATE, status));
        }
        assertTrue("by set() and setTemporalMonthCode()", cc2->equals(*cc1, status));
    }
}

void CalendarTest::TestHebrewCalendarSetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "hebrew", status);
    LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
    if (failure(status, "construct HebrewCalendar")) return;
    LocalPointer<Calendar> cc2(cc1->clone());

    struct TestCase {
      int32_t gYear;
      int32_t gMonth;
      int32_t gDate;
      int32_t cYear;
      int32_t cMonth;
      int32_t cDate;
      const char* cMonthCode;
      int32_t cOrdinalMonth;
    } cases[] = {
      { 2022, UCAL_JANUARY, 11, 5782, icu::HebrewCalendar::SHEVAT, 9, "M05", 4},
      { 2022, UCAL_FEBRUARY, 12, 5782, icu::HebrewCalendar::ADAR_1, 11, "M05L", 5},
      { 2022, UCAL_MARCH, 13, 5782, icu::HebrewCalendar::ADAR, 10, "M06", 6},
      { 2022, UCAL_APRIL, 14, 5782, icu::HebrewCalendar::NISAN, 13, "M07", 7},
      { 2022, UCAL_MAY, 15, 5782, icu::HebrewCalendar::IYAR, 14, "M08", 8},
      { 2022, UCAL_JUNE, 16, 5782, icu::HebrewCalendar::SIVAN, 17, "M09", 9},
      { 2022, UCAL_JULY, 17, 5782, icu::HebrewCalendar::TAMUZ, 18, "M10", 10},
      { 2022, UCAL_AUGUST, 18, 5782, icu::HebrewCalendar::AV, 21, "M11", 11},
      { 2022, UCAL_SEPTEMBER, 19, 5782, icu::HebrewCalendar::ELUL, 23, "M12", 12},
      { 2022, UCAL_OCTOBER, 20, 5783, icu::HebrewCalendar::TISHRI, 25, "M01", 0},
      { 2022, UCAL_NOVEMBER, 21, 5783, icu::HebrewCalendar::HESHVAN, 27, "M02", 1},
      { 2022, UCAL_DECEMBER, 22, 5783, icu::HebrewCalendar::KISLEV, 28, "M03", 2},
      { 2023, UCAL_JANUARY, 20, 5783, icu::HebrewCalendar::TEVET, 27, "M04", 3},
    };
    GregorianCalendar gc1(status);
    if (failure(status, "construct Calendar")) return;
    for (auto& cas : cases) {
        gc1.clear();
        cc1->clear();
        cc2->clear();
        gc1.set(cas.gYear, cas.gMonth, cas.gDate);
        cc1->setTime(gc1.getTime(status), status);

        cc2->set(UCAL_EXTENDED_YEAR, cas.cYear);
        cc2->setTemporalMonthCode(cas.cMonthCode, status);
        cc2->set(UCAL_DATE, cas.cDate);

        assertEquals("year", cas.cYear, cc1->get(UCAL_EXTENDED_YEAR, status));
        assertEquals("month", cas.cMonth, cc1->get(UCAL_MONTH, status));
        assertEquals("date", cas.cDate, cc1->get(UCAL_DATE, status));
        assertEquals("getTemporalMonthCode()", 0,
                     uprv_strcmp(cas.cMonthCode, cc1->getTemporalMonthCode(status)));
        if (failure(status, "getTemporalMonthCode")) continue;
        if (! cc2->equals(*cc1, status)) {
            printf("g=%f %f vs %f. diff = %f %d/%d/%d vs %d/%d/%d\n",
                   gc1.getTime(status), cc1->getTime(status), cc2->getTime(status),
                   cc1->getTime(status) - cc2->getTime(status),
                   cc1->get(UCAL_EXTENDED_YEAR, status),
                   cc1->get(UCAL_MONTH, status)+1,
                   cc1->get(UCAL_DATE, status),
                   cc2->get(UCAL_EXTENDED_YEAR, status),
                   cc2->get(UCAL_MONTH, status)+1,
                   cc2->get(UCAL_DATE, status));
        }
        assertTrue("by set() and setTemporalMonthCode()", cc2->equals(*cc1, status));
        assertEquals("ordinalMonth", cas.cOrdinalMonth, cc1->get(UCAL_ORDINAL_MONTH, status));
        assertEquals("ordinalMonth", cas.cOrdinalMonth, cc2->get(UCAL_ORDINAL_MONTH, status));
    }
}

void CalendarTest::TestCopticCalendarSetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "coptic", status);
    LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
    if (failure(status, "construct CopticCalendar")) return;
    LocalPointer<Calendar> cc2(cc1->clone());

    struct TestCase {
      int32_t gYear;
      int32_t gMonth;
      int32_t gDate;
      int32_t cYear;
      int32_t cMonth;
      int32_t cDate;
      const char* cMonthCode;
      int32_t cOrdinalMonth;
    } cases[] = {
      { 1900, UCAL_JANUARY, 1, 1616, icu::CopticCalendar::KIAHK, 23, "M04", 3},
      { 1900, UCAL_SEPTEMBER, 6, 1616, icu::CopticCalendar::NASIE, 1, "M13", 12},
      { 1900, UCAL_SEPTEMBER, 10, 1616, icu::CopticCalendar::NASIE, 5, "M13", 12},
      { 1900, UCAL_SEPTEMBER, 11, 1617, icu::CopticCalendar::TOUT, 1, "M01", 0},

      { 2022, UCAL_JANUARY, 11, 1738, icu::CopticCalendar::TOBA, 3, "M05", 4},
      { 2022, UCAL_FEBRUARY, 12, 1738, icu::CopticCalendar::AMSHIR, 5, "M06", 5},
      { 2022, UCAL_MARCH, 13, 1738, icu::CopticCalendar::BARAMHAT, 4, "M07", 6},
      { 2022, UCAL_APRIL, 14, 1738, icu::CopticCalendar::BARAMOUDA, 6, "M08", 7},
      { 2022, UCAL_MAY, 15, 1738, icu::CopticCalendar::BASHANS, 7, "M09", 8},
      { 2022, UCAL_JUNE, 16, 1738, icu::CopticCalendar::PAONA, 9, "M10", 9},
      { 2022, UCAL_JULY, 17, 1738, icu::CopticCalendar::EPEP, 10, "M11", 10},
      { 2022, UCAL_AUGUST, 18, 1738, icu::CopticCalendar::MESRA, 12, "M12", 11},
      { 2022, UCAL_SEPTEMBER, 6, 1738, icu::CopticCalendar::NASIE, 1, "M13", 12},
      { 2022, UCAL_SEPTEMBER, 10, 1738, icu::CopticCalendar::NASIE, 5, "M13", 12},
      { 2022, UCAL_SEPTEMBER, 11, 1739, icu::CopticCalendar::TOUT, 1, "M01", 0},
      { 2022, UCAL_SEPTEMBER, 19, 1739, icu::CopticCalendar::TOUT, 9, "M01", 0},
      { 2022, UCAL_OCTOBER, 20, 1739, icu::CopticCalendar::BABA, 10, "M02", 1},
      { 2022, UCAL_NOVEMBER, 21, 1739, icu::CopticCalendar::HATOR, 12, "M03", 2},
      { 2022, UCAL_DECEMBER, 22, 1739, icu::CopticCalendar::KIAHK, 13, "M04", 3},

      { 2023, UCAL_JANUARY, 1, 1739, icu::CopticCalendar::KIAHK, 23, "M04", 3},
      { 2023, UCAL_SEPTEMBER, 6, 1739, icu::CopticCalendar::NASIE, 1, "M13", 12},
      { 2023, UCAL_SEPTEMBER, 11, 1739, icu::CopticCalendar::NASIE, 6, "M13", 12},
      { 2023, UCAL_SEPTEMBER, 12, 1740, icu::CopticCalendar::TOUT, 1, "M01", 0},

      { 2030, UCAL_JANUARY, 1, 1746, icu::CopticCalendar::KIAHK, 23, "M04", 3},
      { 2030, UCAL_SEPTEMBER, 6, 1746, icu::CopticCalendar::NASIE, 1, "M13", 12},
      { 2030, UCAL_SEPTEMBER, 10, 1746, icu::CopticCalendar::NASIE, 5, "M13", 12},
      { 2030, UCAL_SEPTEMBER, 11, 1747, icu::CopticCalendar::TOUT, 1, "M01", 0},
    };
    GregorianCalendar gc1(status);
    if (failure(status, "construct Calendar")) return;
    for (auto& cas : cases) {
        gc1.clear();
        cc1->clear();
        cc2->clear();
        gc1.set(cas.gYear, cas.gMonth, cas.gDate);
        cc1->setTime(gc1.getTime(status), status);

        cc2->set(UCAL_EXTENDED_YEAR, cas.cYear);
        cc2->setTemporalMonthCode(cas.cMonthCode, status);
        cc2->set(UCAL_DATE, cas.cDate);

        assertEquals("year", cas.cYear, cc1->get(UCAL_EXTENDED_YEAR, status));
        assertEquals("month", cas.cMonth, cc1->get(UCAL_MONTH, status));
        assertEquals("date", cas.cDate, cc1->get(UCAL_DATE, status));
        assertEquals("getTemporalMonthCode()", 0,
                     uprv_strcmp(cas.cMonthCode, cc1->getTemporalMonthCode(status)));
        if (failure(status, "getTemporalMonthCode")) continue;
        assertTrue("by set() and setTemporalMonthCode()", cc2->equals(*cc1, status));
        if (! cc2->equals(*cc1, status)) {
            printf("g=%f %f vs %f. diff = %f %d/%d/%d vs %d/%d/%d\n",
                   gc1.getTime(status), cc1->getTime(status), cc2->getTime(status),
                   cc1->getTime(status) - cc2->getTime(status),
                   cc1->get(UCAL_EXTENDED_YEAR, status),
                   cc1->get(UCAL_MONTH, status)+1,
                   cc1->get(UCAL_DATE, status),
                   cc2->get(UCAL_EXTENDED_YEAR, status),
                   cc2->get(UCAL_MONTH, status)+1,
                   cc2->get(UCAL_DATE, status));
        }
        assertEquals("ordinalMonth", cas.cOrdinalMonth, cc1->get(UCAL_ORDINAL_MONTH, status));
        assertEquals("ordinalMonth", cas.cOrdinalMonth, cc2->get(UCAL_ORDINAL_MONTH, status));
    }
}

void CalendarTest::TestEthiopicCalendarSetTemporalMonthCode() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "ethiopic", status);
    LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
    if (failure(status, "construct EthiopicCalendar")) return;
    LocalPointer<Calendar> cc2(cc1->clone());

    struct TestCase {
      int32_t gYear;
      int32_t gMonth;
      int32_t gDate;
      int32_t cYear;
      int32_t cMonth;
      int32_t cDate;
      const char* cMonthCode;
      int32_t cOrdinalMonth;
    } cases[] = {
      { 1900, UCAL_JANUARY, 1, 1892, icu::EthiopicCalendar::TAHSAS, 23, "M04", 3},
      { 1900, UCAL_SEPTEMBER, 6, 1892, icu::EthiopicCalendar::PAGUMEN, 1, "M13", 12},
      { 1900, UCAL_SEPTEMBER, 10, 1892, icu::EthiopicCalendar::PAGUMEN, 5, "M13", 12},
      { 1900, UCAL_SEPTEMBER, 11, 1893, icu::EthiopicCalendar::MESKEREM, 1, "M01", 0},

      { 2022, UCAL_JANUARY, 11, 2014, icu::EthiopicCalendar::TER, 3, "M05", 4},
      { 2022, UCAL_FEBRUARY, 12, 2014, icu::EthiopicCalendar::YEKATIT, 5, "M06", 5},
      { 2022, UCAL_MARCH, 13, 2014, icu::EthiopicCalendar::MEGABIT, 4, "M07", 6},
      { 2022, UCAL_APRIL, 14, 2014, icu::EthiopicCalendar::MIAZIA, 6, "M08", 7},
      { 2022, UCAL_MAY, 15, 2014, icu::EthiopicCalendar::GENBOT, 7, "M09", 8},
      { 2022, UCAL_JUNE, 16, 2014, icu::EthiopicCalendar::SENE, 9, "M10", 9},
      { 2022, UCAL_JULY, 17, 2014, icu::EthiopicCalendar::HAMLE, 10, "M11", 10},
      { 2022, UCAL_AUGUST, 18, 2014, icu::EthiopicCalendar::NEHASSE, 12, "M12", 11},
      { 2022, UCAL_SEPTEMBER, 6, 2014, icu::EthiopicCalendar::PAGUMEN, 1, "M13", 12},
      { 2022, UCAL_SEPTEMBER, 10, 2014, icu::EthiopicCalendar::PAGUMEN, 5, "M13", 12},
      { 2022, UCAL_SEPTEMBER, 11, 2015, icu::EthiopicCalendar::MESKEREM, 1, "M01", 0},
      { 2022, UCAL_SEPTEMBER, 19, 2015, icu::EthiopicCalendar::MESKEREM, 9, "M01", 0},
      { 2022, UCAL_OCTOBER, 20, 2015, icu::EthiopicCalendar::TEKEMT, 10, "M02", 1},
      { 2022, UCAL_NOVEMBER, 21, 2015, icu::EthiopicCalendar::HEDAR, 12, "M03", 2},
      { 2022, UCAL_DECEMBER, 22, 2015, icu::EthiopicCalendar::TAHSAS, 13, "M04", 3},

      { 2023, UCAL_JANUARY, 1, 2015, icu::EthiopicCalendar::TAHSAS, 23, "M04", 3},
      { 2023, UCAL_SEPTEMBER, 6, 2015, icu::EthiopicCalendar::PAGUMEN, 1, "M13", 12},
      { 2023, UCAL_SEPTEMBER, 11, 2015, icu::EthiopicCalendar::PAGUMEN, 6, "M13", 12},
      { 2023, UCAL_SEPTEMBER, 12, 2016, icu::EthiopicCalendar::MESKEREM, 1, "M01", 0},

      { 2030, UCAL_JANUARY, 1, 2022, icu::EthiopicCalendar::TAHSAS, 23, "M04", 3},
      { 2030, UCAL_SEPTEMBER, 6, 2022, icu::EthiopicCalendar::PAGUMEN, 1, "M13", 12},
      { 2030, UCAL_SEPTEMBER, 10, 2022, icu::EthiopicCalendar::PAGUMEN, 5, "M13", 12},
      { 2030, UCAL_SEPTEMBER, 11, 2023, icu::EthiopicCalendar::MESKEREM, 1, "M01", 0},
    };
    GregorianCalendar gc1(status);
    if (failure(status, "construct Calendar")) return;
    for (auto& cas : cases) {
        gc1.clear();
        cc1->clear();
        cc2->clear();
        gc1.set(cas.gYear, cas.gMonth, cas.gDate);
        cc1->setTime(gc1.getTime(status), status);

        cc2->set(UCAL_EXTENDED_YEAR, cas.cYear);
        cc2->setTemporalMonthCode(cas.cMonthCode, status);
        cc2->set(UCAL_DATE, cas.cDate);

        assertEquals("year", cas.cYear, cc1->get(UCAL_EXTENDED_YEAR, status));
        assertEquals("month", cas.cMonth, cc1->get(UCAL_MONTH, status));
        assertEquals("date", cas.cDate, cc1->get(UCAL_DATE, status));
        assertEquals("getTemporalMonthCode()", 0,
                     uprv_strcmp(cas.cMonthCode, cc1->getTemporalMonthCode(status)));
        if (failure(status, "getTemporalMonthCode")) continue;
        if (! cc2->equals(*cc1, status)) {
            printf("g=%f %f vs %f. diff = %f %d/%d/%d vs %d/%d/%d\n",
                   gc1.getTime(status), cc1->getTime(status), cc2->getTime(status),
                   cc1->getTime(status) - cc2->getTime(status),
                   cc1->get(UCAL_EXTENDED_YEAR, status),
                   cc1->get(UCAL_MONTH, status)+1,
                   cc1->get(UCAL_DATE, status),
                   cc2->get(UCAL_EXTENDED_YEAR, status),
                   cc2->get(UCAL_MONTH, status)+1,
                   cc2->get(UCAL_DATE, status));
        }
        assertTrue("by set() and setTemporalMonthCode()", cc2->equals(*cc1, status));
        assertEquals("ordinalMonth", cas.cOrdinalMonth, cc1->get(UCAL_ORDINAL_MONTH, status));
        assertEquals("ordinalMonth", cas.cOrdinalMonth, cc2->get(UCAL_ORDINAL_MONTH, status));
    }
}

void VerifyMonth(CalendarTest* test, const char* message, Calendar* cc, int32_t expectedMonth,
                 int32_t expectedOrdinalMonth, bool expectedLeapMonth,
                 const char* expectedMonthCode) {
    UErrorCode status = U_ZERO_ERROR;
    std::string buf(message);
    buf.append(" get(UCAL_MONTH)");
    test->assertEquals(buf.c_str(), expectedMonth, cc->get(UCAL_MONTH, status));
    buf = message;
    buf.append(" get(UCAL_ORDINAL_MONTH)");
    test->assertEquals(buf.c_str(), expectedOrdinalMonth, cc->get(UCAL_ORDINAL_MONTH, status));
    buf = message;
    buf.append(" get(UCAL_IS_LEAP_MONTH)");
    test->assertEquals(buf.c_str(), expectedLeapMonth ? 1 : 0, cc->get(UCAL_IS_LEAP_MONTH, status));
    buf = message;
    buf.append(" getTemporalMonthCode()");
    test->assertTrue(buf.c_str(), uprv_strcmp(cc->getTemporalMonthCode(status), expectedMonthCode) == 0);
}

void CalendarTest::TestMostCalendarsOrdinalMonthSet() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    std::unique_ptr<icu::StringEnumeration> enumeration(
        Calendar::getKeywordValuesForLocale("calendar", l, false, status));
    for (const char* name = enumeration->next(nullptr, status);
        U_SUCCESS(status) && name != nullptr;
        name = enumeration->next(nullptr, status)) {

        // Test these three calendars differently.
        if (uprv_strcmp(name, "chinese") == 0) continue;
        if (uprv_strcmp(name, "dangi") == 0) continue;
        if (uprv_strcmp(name, "hebrew") == 0) continue;

        l.setKeywordValue("calendar", name, status);
        LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
        if (failure(status, "Construct Calendar")) return;

        LocalPointer<Calendar> cc2(cc1->clone());
        LocalPointer<Calendar> cc3(cc1->clone());

        cc1->set(UCAL_EXTENDED_YEAR, 2134);
        cc2->set(UCAL_EXTENDED_YEAR, 2134);
        cc3->set(UCAL_EXTENDED_YEAR, 2134);
        cc1->set(UCAL_MONTH, 5);
        cc2->set(UCAL_ORDINAL_MONTH, 5);
        cc3->setTemporalMonthCode("M06", status);
        if (failure(status, "setTemporalMonthCode failure")) return;
        cc1->set(UCAL_DATE, 23);
        cc2->set(UCAL_DATE, 23);
        cc3->set(UCAL_DATE, 23);
        assertTrue("M06 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
        assertTrue("M06 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
        if (failure(status, "equals failure")) return;
        VerifyMonth(this, "cc1", cc1.getAlias(), 5, 5, false, "M06");
        VerifyMonth(this, "cc2", cc2.getAlias(), 5, 5, false, "M06");
        VerifyMonth(this, "cc3", cc3.getAlias(), 5, 5, false, "M06");

        cc1->set(UCAL_ORDINAL_MONTH, 6);
        cc2->setTemporalMonthCode("M07", status);
        if (failure(status, "setTemporalMonthCode failure")) return;
        cc3->set(UCAL_MONTH, 6);
        assertTrue("M07 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
        assertTrue("M07 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
        if (failure(status, "equals failure")) return;
        VerifyMonth(this, "cc1", cc1.getAlias(), 6, 6, false, "M07");
        VerifyMonth(this, "cc2", cc2.getAlias(), 6, 6, false, "M07");
        VerifyMonth(this, "cc3", cc3.getAlias(), 6, 6, false, "M07");

        cc1->setTemporalMonthCode("M08", status);
        if (failure(status, "setTemporalMonthCode failure")) return;
        cc2->set(UCAL_MONTH, 7);
        cc3->set(UCAL_ORDINAL_MONTH, 7);
        assertTrue("M08 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
        assertTrue("M08 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
        if (failure(status, "equals failure")) return;
        VerifyMonth(this, "cc1", cc1.getAlias(), 7, 7, false, "M08");
        VerifyMonth(this, "cc2", cc2.getAlias(), 7, 7, false, "M08");
        VerifyMonth(this, "cc3", cc3.getAlias(), 7, 7, false, "M08");

        cc1->set(UCAL_DATE, 3);
        // For "M13", do not return error for these three calendars.
        if ((uprv_strcmp(name, "coptic") == 0) ||
            (uprv_strcmp(name, "ethiopic") == 0) ||
            (uprv_strcmp(name, "ethiopic-amete-alem") == 0)) {

            cc1->setTemporalMonthCode("M13", status);
            assertEquals("setTemporalMonthCode(\"M13\")", U_ZERO_ERROR, status);
            assertEquals("get(UCAL_MONTH) after setTemporalMonthCode(\"M13\")",
                         12, cc1->get(UCAL_MONTH, status));
            assertEquals("get(UCAL_ORDINAL_MONTH) after setTemporalMonthCode(\"M13\")",
                         12, cc1->get(UCAL_ORDINAL_MONTH, status));
            assertEquals("get", U_ZERO_ERROR, status);
        } else {
            cc1->setTemporalMonthCode("M13", status);
            assertEquals("setTemporalMonthCode(\"M13\")", U_ILLEGAL_ARGUMENT_ERROR, status);
        }
        status = U_ZERO_ERROR;

        // Out of bound monthCodes should return error.
        // These are not valid for calendar do not have a leap month
        const char* kInvalidMonthCodes[] = {
          "M00", "M14", "M01L", "M02L", "M03L", "M04L", "M05L", "M06L", "M07L",
          "M08L", "M09L", "M10L", "M11L", "M12L"};
        for (auto& cas : kInvalidMonthCodes) {
           cc1->setTemporalMonthCode(cas, status);
           assertEquals("setTemporalMonthCode(\"M13\")", U_ILLEGAL_ARGUMENT_ERROR, status);
           status = U_ZERO_ERROR;
        }

    }
}

void CalendarTest::TestChineseCalendarOrdinalMonthSet() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "chinese", status);
    LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
    if (failure(status, "Construct Calendar")) return;

    LocalPointer<Calendar> cc2(cc1->clone());
    LocalPointer<Calendar> cc3(cc1->clone());

    constexpr int32_t notLeapYear = 4591;
    constexpr int32_t leapMarchYear = 4592;

    cc1->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc2->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc3->set(UCAL_EXTENDED_YEAR, leapMarchYear);

    cc1->set(UCAL_MONTH, UCAL_MARCH); cc1->set(UCAL_IS_LEAP_MONTH, 1);
    cc2->set(UCAL_ORDINAL_MONTH, 3);
    cc3->setTemporalMonthCode("M03L", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc1->set(UCAL_DATE, 1);
    cc2->set(UCAL_DATE, 1);
    cc3->set(UCAL_DATE, 1);
    assertTrue("4592 M03L cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("4592 M03L cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "4592 M03L cc1", cc1.getAlias(), UCAL_MARCH, 3, true, "M03L");
    VerifyMonth(this, "4592 M03L cc2", cc2.getAlias(), UCAL_MARCH, 3, true, "M03L");
    VerifyMonth(this, "4592 M03L cc3", cc3.getAlias(), UCAL_MARCH, 3, true, "M03L");

    cc1->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc2->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc3->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc1->set(UCAL_ORDINAL_MONTH, 5);
    cc2->setTemporalMonthCode("M06", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc3->set(UCAL_MONTH, UCAL_JUNE); cc3->set(UCAL_IS_LEAP_MONTH, 0);
    assertTrue("4591 M06 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("4591 M06 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "4591 M06 cc1", cc1.getAlias(), UCAL_JUNE, 5, false, "M06");
    VerifyMonth(this, "4591 M06 cc2", cc2.getAlias(), UCAL_JUNE, 5, false, "M06");
    VerifyMonth(this, "4591 M06 cc3", cc3.getAlias(), UCAL_JUNE, 5, false, "M06");

    cc1->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc2->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc3->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc1->setTemporalMonthCode("M04", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc2->set(UCAL_MONTH, UCAL_APRIL); cc2->set(UCAL_IS_LEAP_MONTH, 0);
    cc3->set(UCAL_ORDINAL_MONTH, 4);
    assertTrue("4592 M04 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("4592 M04 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    // 4592 has leap March so April is the 5th month in that year.
    VerifyMonth(this, "4592 M04 cc1", cc1.getAlias(), UCAL_APRIL, 4, false, "M04");
    VerifyMonth(this, "4592 M04 cc2", cc2.getAlias(), UCAL_APRIL, 4, false, "M04");
    VerifyMonth(this, "4592 M04 cc3", cc3.getAlias(), UCAL_APRIL, 4, false, "M04");

    cc1->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc2->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc3->set(UCAL_EXTENDED_YEAR, notLeapYear);
    assertTrue("4591 M04 no leap month before cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("4591 M04 no leap month before cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    // 4592 has no leap month before April so April is the 4th month in that year.
    VerifyMonth(this, "4591 M04 cc1", cc1.getAlias(), UCAL_APRIL, 3, false, "M04");
    VerifyMonth(this, "4591 M04 cc2", cc2.getAlias(), UCAL_APRIL, 3, false, "M04");
    VerifyMonth(this, "4591 M04 cc3", cc3.getAlias(), UCAL_APRIL, 3, false, "M04");

    // Out of bound monthCodes should return error.
    // These are not valid for calendar do not have a leap month
    UErrorCode expectedStatus = U_ILLEGAL_ARGUMENT_ERROR;
    const char* kInvalidMonthCodes[] = { "M00", "M13", "M14" };


    for (auto& cas : kInvalidMonthCodes) {
       cc1->setTemporalMonthCode(cas, status);
       if (status != expectedStatus) {
           errln("setTemporalMonthCode(%s) should return U_ILLEGAL_ARGUMENT_ERROR", cas);
       }
       status = U_ZERO_ERROR;
    }
}

void CalendarTest::TestDangiCalendarOrdinalMonthSet() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    const char* name = "dangi";
    l.setKeywordValue("calendar", name, status);
    LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
    if (failure(status, "Construct Calendar")) return;

    LocalPointer<Calendar> cc2(cc1->clone());
    LocalPointer<Calendar> cc3(cc1->clone());

    constexpr int32_t notLeapYear = 4287;
    constexpr int32_t leapMarchYear = 4288;

    cc1->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc2->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc3->set(UCAL_EXTENDED_YEAR, leapMarchYear);

    cc1->set(UCAL_MONTH, UCAL_MARCH); cc1->set(UCAL_IS_LEAP_MONTH, 1);
    cc2->set(UCAL_ORDINAL_MONTH, 3);
    cc3->setTemporalMonthCode("M03L", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc1->set(UCAL_DATE, 1);
    cc2->set(UCAL_DATE, 1);
    cc3->set(UCAL_DATE, 1);
    assertTrue("4288 M03L cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("4288 M03L cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "4288 M03L cc1", cc1.getAlias(), UCAL_MARCH, 3, true, "M03L");
    VerifyMonth(this, "4288 M03L cc2", cc2.getAlias(), UCAL_MARCH, 3, true, "M03L");
    VerifyMonth(this, "4288 M03L cc3", cc3.getAlias(), UCAL_MARCH, 3, true, "M03L");

    cc1->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc2->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc3->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc1->set(UCAL_ORDINAL_MONTH, 5);
    cc2->setTemporalMonthCode("M06", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc3->set(UCAL_MONTH, UCAL_JUNE); cc3->set(UCAL_IS_LEAP_MONTH, 0);
    assertTrue("4287 M06 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("4287 M06 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "4287 M06 cc1", cc1.getAlias(), UCAL_JUNE, 5, false, "M06");
    VerifyMonth(this, "4287 M06 cc2", cc2.getAlias(), UCAL_JUNE, 5, false, "M06");
    VerifyMonth(this, "4287 M06 cc3", cc3.getAlias(), UCAL_JUNE, 5, false, "M06");

    cc1->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc2->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc3->set(UCAL_EXTENDED_YEAR, leapMarchYear);
    cc1->setTemporalMonthCode("M04", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc2->set(UCAL_MONTH, UCAL_APRIL); cc2->set(UCAL_IS_LEAP_MONTH, 0);
    cc3->set(UCAL_ORDINAL_MONTH, 4);
    assertTrue("4288 M04 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("4288 M04 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    // 4592 has leap March so April is the 5th month in that year.
    VerifyMonth(this, "4288 M04 cc1", cc1.getAlias(), UCAL_APRIL, 4, false, "M04");
    VerifyMonth(this, "4288 M04 cc2", cc2.getAlias(), UCAL_APRIL, 4, false, "M04");
    VerifyMonth(this, "4288 M04 cc3", cc3.getAlias(), UCAL_APRIL, 4, false, "M04");

    cc1->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc2->set(UCAL_EXTENDED_YEAR, notLeapYear);
    cc3->set(UCAL_EXTENDED_YEAR, notLeapYear);
    assertTrue("4287 M04 no leap month before cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("4287 M04 no leap month before cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    // 4592 has no leap month before April so April is the 4th month in that year.
    VerifyMonth(this, "4287 M04 cc1", cc1.getAlias(), UCAL_APRIL, 3, false, "M04");
    VerifyMonth(this, "4287 M04 cc2", cc2.getAlias(), UCAL_APRIL, 3, false, "M04");
    VerifyMonth(this, "4287 M04 cc3", cc3.getAlias(), UCAL_APRIL, 3, false, "M04");

    // Out of bound monthCodes should return error.
    // These are not valid for calendar do not have a leap month
    UErrorCode expectedStatus = U_ILLEGAL_ARGUMENT_ERROR;
    const char* kInvalidMonthCodes[] = { "M00", "M13", "M14" };

    for (auto& cas : kInvalidMonthCodes) {
       cc1->setTemporalMonthCode(cas, status);
       if (status != expectedStatus) {
           errln("setTemporalMonthCode(%s) should return U_ILLEGAL_ARGUMENT_ERROR", cas);
       }
       status = U_ZERO_ERROR;
    }
}

void CalendarTest::TestHebrewCalendarOrdinalMonthSet() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l(Locale::getRoot());
    l.setKeywordValue("calendar", "hebrew", status);
    LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
    if (failure(status, "Construct Calendar")) return;

    LocalPointer<Calendar> cc2(cc1->clone());
    LocalPointer<Calendar> cc3(cc1->clone());

    // 5782 is leap year, 5781 is NOT.
    cc1->set(UCAL_EXTENDED_YEAR, 5782);
    cc2->set(UCAL_EXTENDED_YEAR, 5782);
    cc3->set(UCAL_EXTENDED_YEAR, 5782);
    cc1->set(UCAL_MONTH, icu::HebrewCalendar::ADAR_1);
    cc2->set(UCAL_ORDINAL_MONTH, 5);
    cc3->setTemporalMonthCode("M05L", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc1->set(UCAL_DATE, 1);
    cc2->set(UCAL_DATE, 1);
    cc3->set(UCAL_DATE, 1);
    assertTrue("5782 M05L cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("5782 M05L cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "cc1", cc1.getAlias(), icu::HebrewCalendar::ADAR_1, 5, false, "M05L");
    VerifyMonth(this, "cc2", cc2.getAlias(), icu::HebrewCalendar::ADAR_1, 5, false, "M05L");
    VerifyMonth(this, "cc3", cc3.getAlias(), icu::HebrewCalendar::ADAR_1, 5, false, "M05L");

    cc1->set(UCAL_ORDINAL_MONTH, 4);
    cc2->setTemporalMonthCode("M05", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc3->set(UCAL_MONTH, icu::HebrewCalendar::SHEVAT);
    assertTrue("5782 M05 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("5782 M05 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "cc1", cc1.getAlias(), icu::HebrewCalendar::SHEVAT, 4, false, "M05");
    VerifyMonth(this, "cc2", cc2.getAlias(), icu::HebrewCalendar::SHEVAT, 4, false, "M05");
    VerifyMonth(this, "cc3", cc3.getAlias(), icu::HebrewCalendar::SHEVAT, 4, false, "M05");

    cc1->set(UCAL_EXTENDED_YEAR, 5781);
    cc2->set(UCAL_EXTENDED_YEAR, 5781);
    cc3->set(UCAL_EXTENDED_YEAR, 5781);
    cc1->setTemporalMonthCode("M06", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc2->set(UCAL_MONTH, icu::HebrewCalendar::ADAR);
    cc3->set(UCAL_ORDINAL_MONTH, 5);
    assertTrue("5781 M06 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("5781 M06 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "cc1", cc1.getAlias(), icu::HebrewCalendar::ADAR, 5, false, "M06");
    VerifyMonth(this, "cc2", cc2.getAlias(), icu::HebrewCalendar::ADAR, 5, false, "M06");
    VerifyMonth(this, "cc3", cc3.getAlias(), icu::HebrewCalendar::ADAR, 5, false, "M06");

    cc1->set(UCAL_EXTENDED_YEAR, 5782);
    cc2->set(UCAL_EXTENDED_YEAR, 5782);
    cc3->set(UCAL_EXTENDED_YEAR, 5782);
    assertTrue("5782 M06 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("5782 M06 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "cc1", cc1.getAlias(), icu::HebrewCalendar::ADAR, 6, false, "M06");
    VerifyMonth(this, "cc2", cc2.getAlias(), icu::HebrewCalendar::ADAR, 6, false, "M06");
    VerifyMonth(this, "cc3", cc3.getAlias(), icu::HebrewCalendar::ADAR, 6, false, "M06");

    cc1->set(UCAL_ORDINAL_MONTH, 7);
    cc2->setTemporalMonthCode("M07", status);
    if (failure(status, "setTemporalMonthCode failure")) return;
    cc3->set(UCAL_MONTH, icu::HebrewCalendar::NISAN);
    assertTrue("5782 M07 cc2==cc1 set month by UCAL_MONTH and UCAL_UCAL_ORDINAL_MONTH", cc2->equals(*cc1, status));
    assertTrue("5782 M07 cc2==cc3 set month by UCAL_MONTH and setTemporalMonthCode", cc2->equals(*cc3, status));
    if (failure(status, "equals failure")) return;
    VerifyMonth(this, "cc1", cc1.getAlias(), icu::HebrewCalendar::NISAN, 7, false, "M07");
    VerifyMonth(this, "cc2", cc2.getAlias(), icu::HebrewCalendar::NISAN, 7, false, "M07");
    VerifyMonth(this, "cc3", cc3.getAlias(), icu::HebrewCalendar::NISAN, 7, false, "M07");

    // Out of bound monthCodes should return error.
    // These are not valid for calendar do not have a leap month
    UErrorCode expectedStatus = U_ILLEGAL_ARGUMENT_ERROR;
    const char* kInvalidMonthCodes[] = { "M00", "M13", "M14",
      "M01L", "M02L", "M03L", "M04L",
      /* M05L could be legal */
      "M06L", "M07L", "M08L", "M09L", "M10L", "M11L", "M12L",
    };

    for (auto& cas : kInvalidMonthCodes) {
       cc1->setTemporalMonthCode(cas, status);
       if (status != expectedStatus) {
           errln("setTemporalMonthCode(%s) should return U_ILLEGAL_ARGUMENT_ERROR", cas);
       }
       status = U_ZERO_ERROR;
    }
}

void CalendarTest::TestCalendarAddOrdinalMonth() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    gc.set(2022, UCAL_DECEMBER, 16);
    Locale l(Locale::getRoot());
    std::unique_ptr<icu::StringEnumeration> enumeration(
        Calendar::getKeywordValuesForLocale("calendar", l, false, status));
    for (const char* name = enumeration->next(nullptr, status);
        U_SUCCESS(status) && name != nullptr;
        name = enumeration->next(nullptr, status)) {

        l.setKeywordValue("calendar", name, status);
        LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
        if (failure(status, "Construct Calendar")) return;

        cc1->setTime(gc.getTime(status), status);
        LocalPointer<Calendar> cc2(cc1->clone());

        for (int i = 0; i < 8; i++) {
            for (int j = 1; j < 8; j++) {
                cc1->add(UCAL_MONTH, j, status);
                cc2->add(UCAL_ORDINAL_MONTH, j, status);
                if (failure(status, "add j")) return;
                assertTrue("two add produce the same result", cc2->equals(*cc1, status));
            }
            for (int j = 1; j < 8; j++) {
                cc1->add(UCAL_MONTH, -j, status);
                cc2->add(UCAL_ORDINAL_MONTH, -j, status);
                if (failure(status, "add -j")) return;
                assertTrue("two add produce the same result", cc2->equals(*cc1, status));
            }
        }
    }
}

void CalendarTest::TestCalendarRollOrdinalMonth() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    gc.set(2022, UCAL_DECEMBER, 16);
    Locale l(Locale::getRoot());
    std::unique_ptr<icu::StringEnumeration> enumeration(
        Calendar::getKeywordValuesForLocale("calendar", l, false, status));
    for (const char* name = enumeration->next(nullptr, status);
        U_SUCCESS(status) && name != nullptr;
        name = enumeration->next(nullptr, status)) {

        l.setKeywordValue("calendar", name, status);
        LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
        if (failure(status, "Construct Calendar")) return;

        cc1->setTime(gc.getTime(status), status);
        LocalPointer<Calendar> cc2(cc1->clone());

        for (int i = 0; i < 8; i++) {
            for (int j = 1; j < 8; j++) {
                cc1->roll(UCAL_MONTH, j, status);
                cc2->roll(UCAL_ORDINAL_MONTH, j, status);
                if (failure(status, "roll j")) return;
                assertTrue("two add produce the same result", cc2->equals(*cc1, status));
            }
            for (int j = 1; j < 8; j++) {
                cc1->roll(UCAL_MONTH, -j, status);
                cc2->roll(UCAL_ORDINAL_MONTH, -j, status);
                if (failure(status, "roll -j")) return;
                assertTrue("two add produce the same result", cc2->equals(*cc1, status));
            }
            for (int j = 1; j < 3; j++) {
                cc1->roll(UCAL_MONTH, true, status);
                cc2->roll(UCAL_ORDINAL_MONTH, true, status);
                if (failure(status, "roll true")) return;
                assertTrue("two add produce the same result", cc2->equals(*cc1, status));
            }
            for (int j = 1; j < 3; j++) {
                cc1->roll(UCAL_MONTH, false, status);
                cc2->roll(UCAL_ORDINAL_MONTH, false, status);
                if (failure(status, "roll false")) return;
                assertTrue("two add produce the same result", cc2->equals(*cc1, status));
            }
        }
    }
}

void CalendarTest::TestLimitsOrdinalMonth() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    gc.set(2022, UCAL_DECEMBER, 16);
    Locale l(Locale::getRoot());
    std::unique_ptr<icu::StringEnumeration> enumeration(
        Calendar::getKeywordValuesForLocale("calendar", l, false, status));

    struct Expectation {
      const char* calendar;
      int32_t min;
      int32_t max;
      int32_t greatestMin;
      int32_t leastMax;
    } kExpectations[] = {
        { "gregorian", 0, 11, 0, 11 },
        { "japanese", 0, 11, 0, 11 },
        { "buddhist", 0, 11, 0, 11 },
        { "roc", 0, 11, 0, 11 },
        { "persian", 0, 11, 0, 11 },
        { "islamic-civil", 0, 11, 0, 11 },
        { "islamic", 0, 11, 0, 11 },
        { "hebrew", 0, 12, 0, 11 },
        { "chinese", 0, 12, 0, 11 },
        { "indian", 0, 11, 0, 11 },
        { "coptic", 0, 12, 0, 12 },
        { "ethiopic", 0, 12, 0, 12 },
        { "ethiopic-amete-alem", 0, 12, 0, 12 },
        { "iso8601", 0, 11, 0, 11 },
        { "dangi", 0, 12, 0, 11 },
        { "islamic-umalqura", 0, 11, 0, 11 },
        { "islamic-tbla", 0, 11, 0, 11 },
        { "islamic-rgsa", 0, 11, 0, 11 },
    };

    for (const char* name = enumeration->next(nullptr, status);
        U_SUCCESS(status) && name != nullptr;
        name = enumeration->next(nullptr, status)) {
        l.setKeywordValue("calendar", name, status);
        LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
        if (failure(status, "Construct Calendar")) return;
        bool found = false;
        for (auto& exp : kExpectations) {
            if (uprv_strcmp(exp.calendar, name) == 0) {
                found = true;
                assertEquals("getMinimum(UCAL_ORDINAL_MONTH)",
                             exp.min, cc1->getMinimum(UCAL_ORDINAL_MONTH));
                assertEquals("getMaximum(UCAL_ORDINAL_MONTH)",
                             exp.max, cc1->getMaximum(UCAL_ORDINAL_MONTH));
                assertEquals("getMinimum(UCAL_ORDINAL_MONTH)",
                             exp.greatestMin, cc1->getGreatestMinimum(UCAL_ORDINAL_MONTH));
                assertEquals("getMinimum(UCAL_ORDINAL_MONTH)",
                             exp.leastMax, cc1->getLeastMaximum(UCAL_ORDINAL_MONTH));
                break;
            }
        }
        if (!found) {
            errln("Cannot find expectation");
        }
    }
}

void CalendarTest::TestActualLimitsOrdinalMonth() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    gc.set(2022, UCAL_DECEMBER, 16);
    Locale l(Locale::getRoot());
    std::unique_ptr<icu::StringEnumeration> enumeration(
        Calendar::getKeywordValuesForLocale("calendar", l, false, status));

    struct TestCases {
      const char* calendar;
      int32_t extended_year;
      int32_t actualMinOrdinalMonth;
      int32_t actualMaxOrdinalMonth;
    } cases[] = {
        { "gregorian", 2021, 0, 11 },
        { "gregorian", 2022, 0, 11 },
        { "gregorian", 2023, 0, 11 },
        { "japanese", 2021, 0, 11 },
        { "japanese", 2022, 0, 11 },
        { "japanese", 2023, 0, 11 },
        { "buddhist", 2021, 0, 11 },
        { "buddhist", 2022, 0, 11 },
        { "buddhist", 2023, 0, 11 },
        { "roc", 2021, 0, 11 },
        { "roc", 2022, 0, 11 },
        { "roc", 2023, 0, 11 },
        { "persian", 1400, 0, 11 },
        { "persian", 1401, 0, 11 },
        { "persian", 1402, 0, 11 },
        { "hebrew", 5782, 0, 12 },
        { "hebrew", 5783, 0, 11 },
        { "hebrew", 5789, 0, 11 },
        { "hebrew", 5790, 0, 12 },
        { "chinese", 4645, 0, 11 },
        { "chinese", 4646, 0, 12 },
        { "chinese", 4647, 0, 11 },
        { "dangi", 4645 + 304, 0, 11 },
        { "dangi", 4646 + 304, 0, 12 },
        { "dangi", 4647 + 304, 0, 11 },
        { "indian", 1944, 0, 11 },
        { "indian", 1945, 0, 11 },
        { "indian", 1946, 0, 11 },
        { "coptic", 1737, 0, 12 },
        { "coptic", 1738, 0, 12 },
        { "coptic", 1739, 0, 12 },
        { "ethiopic", 2013, 0, 12 },
        { "ethiopic", 2014, 0, 12 },
        { "ethiopic", 2015, 0, 12 },
        { "ethiopic-amete-alem", 2014, 0, 12 },
        { "ethiopic-amete-alem", 2015, 0, 12 },
        { "ethiopic-amete-alem", 2016, 0, 12 },
        { "iso8601", 2022, 0, 11 },
        { "islamic-civil", 1443, 0, 11 },
        { "islamic-civil", 1444, 0, 11 },
        { "islamic-civil", 1445, 0, 11 },
        { "islamic", 1443, 0, 11 },
        { "islamic", 1444, 0, 11 },
        { "islamic", 1445, 0, 11 },
        { "islamic-umalqura", 1443, 0, 11 },
        { "islamic-umalqura", 1444, 0, 11 },
        { "islamic-umalqura", 1445, 0, 11 },
        { "islamic-tbla", 1443, 0, 11 },
        { "islamic-tbla", 1444, 0, 11 },
        { "islamic-tbla", 1445, 0, 11 },
        { "islamic-rgsa", 1443, 0, 11 },
        { "islamic-rgsa", 1444, 0, 11 },
        { "islamic-rgsa", 1445, 0, 11 },
    };

    for (auto& cas : cases) {
        l.setKeywordValue("calendar", cas.calendar, status);
        LocalPointer<Calendar> cc1(Calendar::createInstance(l, status));
        if (failure(status, "Construct Calendar")) return;
        cc1->set(UCAL_EXTENDED_YEAR, cas.extended_year);
        cc1->set(UCAL_ORDINAL_MONTH, 0);
        cc1->set(UCAL_DATE, 1);
        assertEquals("getActualMinimum(UCAL_ORDINAL_MONTH)",
                     cas.actualMinOrdinalMonth, cc1->getActualMinimum(UCAL_ORDINAL_MONTH, status));
        assertEquals("getActualMaximum(UCAL_ORDINAL_MONTH)",
                     cas.actualMaxOrdinalMonth, cc1->getActualMaximum(UCAL_ORDINAL_MONTH, status));
    }
}

// The Lunar year which majorty part fall into 1889 and the early part of 1890
// should have no leap months, but currently ICU calculate and show there is
// a Leap month after the 12th month and before the first month of the Chinese
// Calendar which overlapping most of the 1890 year in Gregorian.
//
// We use the value from
// https://ytliu0.github.io/ChineseCalendar/table_period.html?period=qing
// and https://ytliu0.github.io/ChineseCalendar/index_chinese.html
// as the expected value. The same results were given by many several other
// sites not just his one.
//
// There should be a Leap month after the 2nd month of the Chinese Calendar year
// mostly overlapping with 1890 and should have no leap month in the Chinese
// Calendar year mostly overlapping with 1889.
void CalendarTest::TestChineseCalendarMonthInSpecialYear() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar gc(status);
    ChineseCalendar cal(Locale::getRoot(), status);
    if (failure(status, "Constructor failed")) return;
    struct TestCase {
      int32_t gyear;
      int32_t gmonth;
      int32_t gdate;
      int32_t cmonth; // 0-based month number: 1st month = 0, 2nd month = 1.
      int32_t cdate;
      bool cleapmonth;
    } cases[] = {
        // Gregorian             Chinese Calendar
        // First some recent date
        // From https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2022.pdf
        { 2022, UCAL_DECEMBER, 15, 11-1, 22, false},
        //                          ^-- m-1 to convert to 0-based month from 1-based.
        // From https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2023.pdf
        { 2023, UCAL_MARCH, 21, 2-1, 30, false},
        { 2023, UCAL_MARCH, 22, 2-1, 1, true},
        // We know there are some problematic year, especially those involved
        // the rare cases of M11L and M12L.
        // Check 1890 and 2033.
        //
        // 2033 has M11L
        // From https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2033.pdf
        { 2033, UCAL_DECEMBER, 21, 11-1, 30, false},
        { 2033, UCAL_DECEMBER, 22, 11-1, 1, true},
        // Here are the date we get from multiple external sources
        // https://ytliu0.github.io/ChineseCalendar/index_chinese.html
        // https://ytliu0.github.io/ChineseCalendar/table_period.html?period=qing
        // There should have no leap 12th month in the year mostly overlapping
        // 1889 but should have a leap 2th month in the year mostly overlapping
        // with 1890.
        { 1890, UCAL_JANUARY, 1, 12-1, 11, false},
        { 1890, UCAL_JANUARY, 20, 12-1, 30, false},
        { 1890, UCAL_JANUARY, 21, 1-1, 1, false},
        { 1890, UCAL_FEBRUARY, 1, 1-1, 12, false},
        { 1890, UCAL_FEBRUARY, 19, 2-1, 1, false},
        { 1890, UCAL_MARCH, 1, 2-1, 11, false},
        { 1890, UCAL_MARCH, 21, 2-1, 1, true},
        { 1890, UCAL_APRIL, 1, 2-1, 12, true},
        { 1890, UCAL_APRIL, 18, 2-1, 29, true},
        { 1890, UCAL_APRIL, 19, 3-1, 1, false},
        { 1890, UCAL_APRIL, 20, 3-1, 2, false},
    };
    for (auto& cas : cases) {
        gc.set(cas.gyear, cas.gmonth, cas.gdate);
        cal.setTime(gc.getTime(status), status);
        if (failure(status, "getTime/setTime failed")) return;
        int32_t actual_month = cal.get(UCAL_MONTH, status);
        int32_t actual_date = cal.get(UCAL_DATE, status);
        int32_t actual_in_leap_month = cal.get(UCAL_IS_LEAP_MONTH, status);
        if (failure(status, "get failed")) return;
        if (cas.cmonth != actual_month ||
            cas.cdate != actual_date ||
            cas.cleapmonth != (actual_in_leap_month != 0)) {
            if (cas.gyear == 1890 &&
                logKnownIssue("ICU-22230", "Problem between 1890/1/21 and 1890/4/18")) {
                  continue;
            }
            errln("Fail: Gregorian(%d/%d/%d) should be Chinese %d%s/%d but got %d%s/%d",
                  cas.gyear, cas.gmonth+1, cas.gdate,
                  cas.cmonth+1, cas.cleapmonth ? "L" : "" , cas.cdate,
                  actual_month+1, ((actual_in_leap_month != 0) ? "L" : ""), actual_date );
        }
    }
}

// Test the stack will not overflow with dangi calendar during "roll".
void CalendarTest::TestDangiOverflowIsLeapMonthBetween22507() {
    Locale locale("en@calendar=dangi");
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(Calendar::createInstance(
            *TimeZone::getGMT(), locale, status));
    cal->clear();
    status = U_ZERO_ERROR;
    cal->add(UCAL_MONTH, 1242972234, status);
    status = U_ZERO_ERROR;
    cal->roll(UCAL_MONTH, 1249790538, status);
    status = U_ZERO_ERROR;
    // Without the fix, the stack will overflow during this roll().
    cal->roll(UCAL_MONTH, 1246382666, status);
}

void CalendarTest::TestFWWithISO8601() {
    // ICU UCAL_SUNDAY is 1, UCAL_MONDAY is 2, ... UCAL_SATURDAY is 7.
    const char *locales[]  = {
      "",
      "en-u-ca-iso8601-fw-sun",
      "en-u-ca-iso8601-fw-mon",
      "en-u-ca-iso8601-fw-tue",
      "en-u-ca-iso8601-fw-wed",
      "en-u-ca-iso8601-fw-thu",
      "en-u-ca-iso8601-fw-fri",
      "en-u-ca-iso8601-fw-sat"
    };
    for (int i = UCAL_SUNDAY; i <= UCAL_SATURDAY; i++) {
        UErrorCode status = U_ZERO_ERROR;
        const char* locale = locales[i];
        LocalPointer<Calendar> cal(
            Calendar::createInstance(
                Locale(locale), status), status);
        if (failure(status, "Constructor failed")) continue;
        std::string msg("Calendar::createInstance(\"");
        msg = msg + locale + "\")->getFirstDayOfWeek()";
        assertEquals(msg.c_str(), i, cal->getFirstDayOfWeek());
    }
}
void CalendarTest::TestRollWeekOfYear() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l("zh_TW@calendar=chinese");
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status), status);
    cal->set(UCAL_EXTENDED_YEAR, -1107626);
    cal->set(UCAL_MONTH, UCAL_JANUARY);
    cal->set(UCAL_DATE, 1);
    cal->roll(UCAL_WEEK_OF_YEAR, 0x7fffff, status);
    U_ASSERT(U_SUCCESS(status));
    cal->roll(UCAL_WEEK_OF_YEAR, 1, status);
}

void CalendarTest::verifyFirstDayOfWeek(const char* locale, UCalendarDaysOfWeek expected) {
    UErrorCode status = U_ZERO_ERROR;
    Locale l = Locale::forLanguageTag(locale, status);
    U_ASSERT(U_SUCCESS(status));
    LocalPointer<Calendar> cal(Calendar::createInstance(l, status), status);
    U_ASSERT(U_SUCCESS(status));
    assertEquals(locale,
                 expected, cal->getFirstDayOfWeek(status));
    U_ASSERT(U_SUCCESS(status));
}

/**
 * Test "First Day Overrides" behavior
 * https://unicode.org/reports/tr35/tr35-dates.html#first-day-overrides
 * And data in <firstDay> of
 * https://github.com/unicode-org/cldr/blob/main/common/supplemental/supplementalData.xml
 *
 * Examples of region for First Day of a week
 * Friday: MV
 * Saturday: AE AF
 * Sunday: US JP
 * Monday: GB
 */
void CalendarTest::TestFirstDayOfWeek() {
    // Test -u-fw- value
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-fw-sun-rg-mvzzzz-sd-usca", UCAL_SUNDAY);
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-fw-mon-rg-mvzzzz-sd-usca", UCAL_MONDAY);
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-fw-tue-rg-mvzzzz-sd-usca", UCAL_TUESDAY);
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-fw-wed-rg-mvzzzz-sd-usca", UCAL_WEDNESDAY);
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-fw-thu-rg-mvzzzz-sd-usca", UCAL_THURSDAY);
    verifyFirstDayOfWeek("en-AE-u-ca-iso8601-fw-fri-rg-aezzzz-sd-usca", UCAL_FRIDAY);
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-fw-sat-rg-mvzzzz-sd-usca", UCAL_SATURDAY);

    // Test -u-rg- value
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-rg-mvzzzz-sd-usca", UCAL_FRIDAY);
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-rg-aezzzz-sd-usca", UCAL_SATURDAY);
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-rg-uszzzz-sd-usca", UCAL_SUNDAY);
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-rg-gbzzzz-sd-usca", UCAL_MONDAY);

    // Test -u-ca-iso8601
    verifyFirstDayOfWeek("en-MV-u-ca-iso8601-sd-mv00", UCAL_MONDAY);
    verifyFirstDayOfWeek("en-AE-u-ca-iso8601-sd-aeaj", UCAL_MONDAY);
    verifyFirstDayOfWeek("en-US-u-ca-iso8601-sd-usca", UCAL_MONDAY);

    // Test Region Tags only
    verifyFirstDayOfWeek("en-MV", UCAL_FRIDAY);
    verifyFirstDayOfWeek("en-AE", UCAL_SATURDAY);
    verifyFirstDayOfWeek("en-US", UCAL_SUNDAY);
    verifyFirstDayOfWeek("dv-GB", UCAL_MONDAY);

    // Test -u-sd-
    verifyFirstDayOfWeek("en-u-sd-mv00", UCAL_FRIDAY);
    verifyFirstDayOfWeek("en-u-sd-aeaj", UCAL_SATURDAY);
    verifyFirstDayOfWeek("en-u-sd-usca", UCAL_SUNDAY);
    verifyFirstDayOfWeek("dv-u-sd-gbsct", UCAL_MONDAY);

    // Test Add Likely Subtags algorithm produces a region
    // dv => dv_Thaa_MV => Friday
    verifyFirstDayOfWeek("dv", UCAL_FRIDAY);
    // und_Thaa => dv_Thaa_MV => Friday
    verifyFirstDayOfWeek("und-Thaa", UCAL_FRIDAY);

    // ssh => ssh_Arab_AE => Saturday
    verifyFirstDayOfWeek("ssh", UCAL_SATURDAY);
    // wbl_Arab => wbl_Arab_AF => Saturday
    verifyFirstDayOfWeek("wbl-Arab", UCAL_SATURDAY);

    // en => en_Latn_US => Sunday
    verifyFirstDayOfWeek("en", UCAL_SUNDAY);
    // und_Hira => ja_Hira_JP => Sunday
    verifyFirstDayOfWeek("und-Hira", UCAL_SUNDAY);

    verifyFirstDayOfWeek("zxx", UCAL_MONDAY);
}

void CalendarTest::Test22633ChineseOverflow() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(Calendar::createInstance(Locale("en@calendar=chinese"), status), status);
    U_ASSERT(U_SUCCESS(status));
    cal->setTime(2043071457431218011677338081118001787485161156097100985923226601036925437809699842362992455895409920480414647512899096575018732258582416938813614617757317338664031880042592085084690242819214720523061081124318514531466365480449329351434046537728.000000, status);
    U_ASSERT(U_SUCCESS(status));
    cal->set(UCAL_EXTENDED_YEAR, -1594662558);
    cal->get(UCAL_YEAR, status);
    assertTrue("Should return success", U_SUCCESS(status));

    cal->setTime(17000065021099877464213620139773683829419175940649608600213244013003611130029599692535053209683880603725167923910423116397083334648012657787978113960494455603744210944.000000, status);
    cal->add(UCAL_YEAR, 1935762034, status);
    assertTrue("Should return falure", U_FAILURE(status));

    status = U_ZERO_ERROR;
    cal->set(UCAL_ERA, 1651667877);
    cal->add(UCAL_YEAR, 1935762034, status);
    assertTrue("Should return falure", U_FAILURE(status));
}
void CalendarTest::Test22633IndianOverflow() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(Calendar::createInstance(Locale("en@calendar=indian"), status), status);
    U_ASSERT(U_SUCCESS(status));
    cal->roll(UCAL_EXTENDED_YEAR, -2120158417, status);
    assertTrue("Should return success", U_SUCCESS(status));
}
void CalendarTest::Test22633IslamicUmalquraOverflow() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(Calendar::createInstance(Locale("en@calendar=islamic-umalqura"), status), status);
    U_ASSERT(U_SUCCESS(status));
    cal->roll(UCAL_YEAR, -134404585, status);
    assertTrue("Should return success", U_SUCCESS(status));
}

void CalendarTest::Test22633PersianOverflow() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(Calendar::createInstance(Locale("en@calendar=persian"), status), status);
    U_ASSERT(U_SUCCESS(status));
    cal->add(UCAL_ORDINAL_MONTH, 1594095615, status);
    assertTrue("Should return success", U_SUCCESS(status));

    cal->clear();
    cal->fieldDifference(
        -874417153152678003697180890506448687181167523704194267774844295805672585701302166100950793070884718009504322601688549650298776623158701367393457997817732662883592665106020013730689242515513560464852918376875667091108609655859551000798163265126400.000000,
        UCAL_YEAR, status);
    assertFalse("Should not return success", U_SUCCESS(status));
}

void CalendarTest::Test22633HebrewOverflow() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(Calendar::createInstance(Locale("en@calendar=hebrew"), status), status);
    U_ASSERT(U_SUCCESS(status));
    cal->clear();
    cal->roll(UCAL_JULIAN_DAY, -335544321, status);
    assertTrue("Should return success", U_SUCCESS(status));
    cal->roll(UCAL_JULIAN_DAY, -1812424430, status);
    assertEquals("Should return U_ILLEGAL_ARGUMENT_ERROR",
                 U_ILLEGAL_ARGUMENT_ERROR, status);
}

void CalendarTest::Test22633AMPMOverflow() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> cal(Calendar::createInstance(Locale("en"), status), status);
    U_ASSERT(U_SUCCESS(status));
    cal->setTimeZone(*TimeZone::getGMT());
    cal->clear();
    // Test to set a value > limit should not cause internal overflow.
    cal->set(UCAL_AM_PM, 370633137);
    assertEquals("set large odd value for UCAL_AM_PM should be treated as PM",
                 12.0 * 60.0 * 60.0 *1000.0, cal->getTime(status));
    assertTrue("Should return success", U_SUCCESS(status));

    cal->set(UCAL_AM_PM, 370633138);
    assertEquals("set large even value for UCAL_AM_PM should be treated as AM",
                 0.0, cal->getTime(status));
    assertTrue("Should return success", U_SUCCESS(status));
}

void CalendarTest::RunTestOnCalendars(void(TestFunc)(Calendar*, UCalendarDateFields)) {
    UErrorCode status = U_ZERO_ERROR;
    Locale locale = Locale::getEnglish();
    LocalPointer<StringEnumeration> values(
        Calendar::getKeywordValuesForLocale("calendar", locale, false, status),
        status);
    assertTrue("Should return success", U_SUCCESS(status));
    if (U_FAILURE(status)) {
      return;
    }
    const char* value = nullptr;
    while ((value = values->next(nullptr, status)) != nullptr && U_SUCCESS(status)) {
        locale.setKeywordValue("calendar", value, status);
        assertTrue("Should return success", U_SUCCESS(status));

        LocalPointer<Calendar> cal(Calendar::createInstance(*TimeZone::getGMT(), locale, status), status);
        assertTrue("Should return success", U_SUCCESS(status));
        for (int32_t i = 0; i < UCAL_FIELD_COUNT; i++) {
            TestFunc(cal.getAlias(), static_cast<UCalendarDateFields>(i));
        }
    }
}

// This test is designed to work with undefined behavior sanitizer UBSAN to
// ensure we do not have math operation overflow int32_t.
void CalendarTest::Test22633SetGetTimeOverflow() {
    RunTestOnCalendars([](Calendar* cal, UCalendarDateFields field) {
        auto f = [](Calendar* cal, UCalendarDateFields field, int32_t value) {
            UErrorCode status = U_ZERO_ERROR;
            cal->clear();
            cal->set(field, value);
            cal->getTime(status);
        };
        f(cal, field, INT32_MAX);
        f(cal, field, INT32_MIN);
    });
}

void CalendarTest::Test22633Set2FieldsGetTimeOverflow() {
    RunTestOnCalendars([](Calendar* cal, UCalendarDateFields field) {
        auto f = [](Calendar* cal, UCalendarDateFields field, int32_t value) {
            for (int32_t j = 0; j < UCAL_FIELD_COUNT; j++) {
                UCalendarDateFields field2 = static_cast<UCalendarDateFields>(j);
                UErrorCode status = U_ZERO_ERROR;
                cal->clear();
                cal->set(field, value);
                cal->set(field2, value);
                cal->getTime(status);
            }
        };
        f(cal, field, INT32_MAX);
        f(cal, field, INT32_MIN);
    });
}

void CalendarTest::Test22633SetAddGetTimeOverflow() {
    RunTestOnCalendars([](Calendar* cal, UCalendarDateFields field) {
        auto f = [](Calendar* cal, UCalendarDateFields field, int32_t value) {
            UErrorCode status = U_ZERO_ERROR;
            cal->clear();
            cal->set(field, value);
            cal->add(field, value, status);
            status = U_ZERO_ERROR;
            cal->getTime(status);
        };
        f(cal, field, INT32_MAX);
        f(cal, field, INT32_MIN);
    });
}

void CalendarTest::Test22633AddTwiceGetTimeOverflow() {
    RunTestOnCalendars([](Calendar* cal, UCalendarDateFields field) {
        auto f = [](Calendar* cal, UCalendarDateFields field, int32_t value) {
            UErrorCode status = U_ZERO_ERROR;
            cal->clear();
            cal->add(field, value, status);
            status = U_ZERO_ERROR;
            cal->add(field, value, status);
            status = U_ZERO_ERROR;
            cal->getTime(status);
        };
        f(cal, field, INT32_MAX);
        f(cal, field, INT32_MIN);
    });
}

void CalendarTest::Test22633SetRollGetTimeOverflow() {
    RunTestOnCalendars([](Calendar* cal, UCalendarDateFields field) {
        auto f = [](Calendar* cal, UCalendarDateFields field, int32_t value) {
            UErrorCode status = U_ZERO_ERROR;
            cal->clear();
            cal->set(field, value);
            cal->roll(field, value, status);
            status = U_ZERO_ERROR;
            cal->getTime(status);
        };
        f(cal, field, INT32_MAX);
        f(cal, field, INT32_MIN);
    });
}

void CalendarTest::Test22633RollTwiceGetTimeOverflow() {
    RunTestOnCalendars([](Calendar* cal, UCalendarDateFields field) {
        auto f = [](Calendar* cal, UCalendarDateFields field, int32_t value) {
            UErrorCode status = U_ZERO_ERROR;
            cal->clear();
            cal->roll(field, value, status);
            status = U_ZERO_ERROR;
            cal->roll(field, value, status);
            status = U_ZERO_ERROR;
            cal->getTime(status);
        };
        f(cal, field, INT32_MAX);
        f(cal, field, INT32_MIN);
    });
}

void CalendarTest::TestChineseCalendarComputeMonthStart() {  // ICU-22639
    UErrorCode status = U_ZERO_ERROR;

    // An extended year for which hasLeapMonthBetweenWinterSolstices is true.
    constexpr int32_t eyear = 4643;
    constexpr int64_t monthStart = 2453764;

    LocalPointer<Calendar> calendar(
        Calendar::createInstance(Locale("en_US@calendar=chinese"), status),
        status);
    if (failure(status, "Calendar::createInstance")) return;

    // This test case is a friend of ChineseCalendar and may access internals.
    const ChineseCalendar& chinese =
        *dynamic_cast<ChineseCalendar*>(calendar.getAlias());

    // The initial value of hasLeapMonthBetweenWinterSolstices should be false.
    assertFalse("hasLeapMonthBetweenWinterSolstices [#1]",
                chinese.hasLeapMonthBetweenWinterSolstices);

    assertEquals("monthStart", monthStart,
                 chinese.handleComputeMonthStart(eyear, 0, false, status));

    // Calling a const method must not haved changed the state of the object.
    assertFalse("hasLeapMonthBetweenWinterSolstices [#2]",
                chinese.hasLeapMonthBetweenWinterSolstices);
}

void CalendarTest::Test22633HebrewLargeNegativeDay() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> calendar(
        Calendar::createInstance(Locale("en-u-ca-hebrew"), status),
        status);
    calendar->clear();
    calendar->set(UCAL_DAY_OF_YEAR, -2147483648);
    calendar->get(UCAL_HOUR, status);
    assertEquals("status return without hang", status, U_ILLEGAL_ARGUMENT_ERROR);
}

void CalendarTest::Test22730JapaneseOverflow() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> calendar(
        Calendar::createInstance(Locale("en-u-ca-japanese"), status),
        status);
    calendar->clear();
    calendar->roll(UCAL_EXTENDED_YEAR, -1946156856, status);
    assertEquals("status return without overflow", status, U_ILLEGAL_ARGUMENT_ERROR);
}

void CalendarTest::TestAddOverflow() {
    UErrorCode status = U_ZERO_ERROR;

    LocalPointer<Calendar> calendar(
        Calendar::createInstance(Locale("en"), status),
        status);
    if (failure(status, "Calendar::createInstance")) return;
    for (int32_t i = 0; i < UCAL_FIELD_COUNT; i++) {
        status = U_ZERO_ERROR;
        calendar->setTime(0, status);
        calendar->add(static_cast<UCalendarDateFields>(i), INT32_MAX / 2, status);
        calendar->add(static_cast<UCalendarDateFields>(i), INT32_MAX, status);
        if ((i == UCAL_ERA) ||
            (i == UCAL_YEAR) ||
            (i == UCAL_YEAR_WOY) ||
            (i == UCAL_EXTENDED_YEAR) ||
            (i == UCAL_IS_LEAP_MONTH) ||
            (i == UCAL_MONTH) ||
            (i == UCAL_ORDINAL_MONTH) ||
            (i == UCAL_ZONE_OFFSET) ||
            (i == UCAL_DST_OFFSET)) {
            assertTrue("add INT32_MAX should fail", U_FAILURE(status));
        } else {
            assertTrue("add INT32_MAX should still success", U_SUCCESS(status));
        }

        status = U_ZERO_ERROR;
        calendar->setTime(0, status);
        calendar->add(static_cast<UCalendarDateFields>(i), INT32_MIN / 2, status);
        calendar->add(static_cast<UCalendarDateFields>(i), INT32_MIN, status);
        if ((i == UCAL_YEAR) ||
            (i == UCAL_YEAR_WOY) ||
            (i == UCAL_EXTENDED_YEAR) ||
            (i == UCAL_IS_LEAP_MONTH) ||
            (i == UCAL_ZONE_OFFSET) ||
            (i == UCAL_DST_OFFSET)) {
            assertTrue("add INT32_MIN should fail", U_FAILURE(status));
        } else {
            assertTrue("add INT32_MIN should still success", U_SUCCESS(status));
        }
    }
}
#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
