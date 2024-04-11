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
#include "islamcal.h"

#define mkcstr(U) u_austrcpy(calloc(8, u_strlen(U) + 1), U)

#define TEST_CHECK_STATUS { \
    if (U_FAILURE(status)) { \
        if (status == U_MISSING_RESOURCE_ERROR) { \
            dataerrln("%s:%d: Test failure.  status=%s", __FILE__, __LINE__, u_errorName(status)); \
        } else { \
            errln("%s:%d: Test failure.  status=%s", __FILE__, __LINE__, u_errorName(status)); \
        } return;}}

#define TEST_CHECK_STATUS_LOCALE(testlocale) { \
    if (U_FAILURE(status)) { \
        if (status == U_MISSING_RESOURCE_ERROR) { \
            dataerrln("%s:%d: Test failure, locale %s.  status=%s", __FILE__, __LINE__, testlocale, u_errorName(status)); \
        } else { \
            errln("%s:%d: Test failure, locale %s.  status=%s", __FILE__, __LINE__, testlocale, u_errorName(status)); \
        } return;}}

#define TEST_ASSERT(expr) {if ((expr)==FALSE) {errln("%s:%d: Test failure \n", __FILE__, __LINE__);};}

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
    switch (index) {
        case 0:
            name = "TestDOW943";
            if (exec) {
                logln("TestDOW943---"); logln("");
                TestDOW943();
            }
            break;
        case 1:
            name = "TestClonesUnique908";
            if (exec) {
                logln("TestClonesUnique908---"); logln("");
                TestClonesUnique908();
            }
            break;
        case 2:
            name = "TestGregorianChange768";
            if (exec) {
                logln("TestGregorianChange768---"); logln("");
                TestGregorianChange768();
            }
            break;
        case 3:
            name = "TestDisambiguation765";
            if (exec) {
                logln("TestDisambiguation765---"); logln("");
                TestDisambiguation765();
            }
            break;
        case 4:
            name = "TestGMTvsLocal4064654";
            if (exec) {
                logln("TestGMTvsLocal4064654---"); logln("");
                TestGMTvsLocal4064654();
            }
            break;
        case 5:
            name = "TestAddSetOrder621";
            if (exec) {
                logln("TestAddSetOrder621---"); logln("");
                TestAddSetOrder621();
            }
            break;
        case 6:
            name = "TestAdd520";
            if (exec) {
                logln("TestAdd520---"); logln("");
                TestAdd520();
            }
            break;
        case 7:
            name = "TestFieldSet4781";
            if (exec) {
                logln("TestFieldSet4781---"); logln("");
                TestFieldSet4781();
            }
            break;
        case 8:
            name = "TestSerialize337";
            if (exec) {
                logln("TestSerialize337---"); logln("");
            //  TestSerialize337();
            }
            break;
        case 9:
            name = "TestSecondsZero121";
            if (exec) {
                logln("TestSecondsZero121---"); logln("");
                TestSecondsZero121();
            }
            break;
        case 10:
            name = "TestAddSetGet0610";
            if (exec) {
                logln("TestAddSetGet0610---"); logln("");
                TestAddSetGet0610();
            }
            break;
        case 11:
            name = "TestFields060";
            if (exec) {
                logln("TestFields060---"); logln("");
                TestFields060();
            }
            break;
        case 12:
            name = "TestEpochStartFields";
            if (exec) {
                logln("TestEpochStartFields---"); logln("");
                TestEpochStartFields();
            }
            break;
        case 13:
            name = "TestDOWProgression";
            if (exec) {
                logln("TestDOWProgression---"); logln("");
                TestDOWProgression();
            }
            break;
        case 14:
            name = "TestGenericAPI";
            if (exec) {
                logln("TestGenericAPI---"); logln("");
                TestGenericAPI();
            }
            break;
        case 15:
            name = "TestAddRollExtensive";
            if (exec) {
                logln("TestAddRollExtensive---"); logln("");
                TestAddRollExtensive();
            }
            break;
        case 16:
            name = "TestDOW_LOCALandYEAR_WOY";
            if (exec) {
                logln("TestDOW_LOCALandYEAR_WOY---"); logln("");
                TestDOW_LOCALandYEAR_WOY();
            }
            break;
        case 17:
            name = "TestWOY";
            if (exec) {
                logln("TestWOY---"); logln("");
                TestWOY();
            }
            break;
        case 18:
            name = "TestRog";
            if (exec) {
                logln("TestRog---"); logln("");
                TestRog();
            }
            break;
        case 19:
           name = "TestYWOY";
            if (exec) {
                logln("TestYWOY---"); logln("");
                TestYWOY();
            }
            break;
        case 20:
          name = "TestJD";
          if(exec) {
            logln("TestJD---"); logln("");
            TestJD();
          }
          break;
        case 21:
          name = "TestDebug";
          if(exec) {
            logln("TestDebug---"); logln("");
            TestDebug();
          }
          break;
        case 22:
          name = "Test6703";
          if(exec) {
            logln("Test6703---"); logln("");
            Test6703();
          }
          break;
        case 23:
          name = "Test3785";
          if(exec) {
            logln("Test3785---"); logln("");
            Test3785();
          }
          break;
        case 24:
          name = "Test1624";
          if(exec) {
            logln("Test1624---"); logln("");
            Test1624();
          }
          break;
        case 25:
          name = "TestTimeStamp";
          if(exec) {
            logln("TestTimeStamp---"); logln("");
            TestTimeStamp();
          }
          break;
        case 26:
          name = "TestISO8601";
          if(exec) {
            logln("TestISO8601---"); logln("");
            TestISO8601();
          }
          break;
        case 27:
          name = "TestAmbiguousWallTimeAPIs";
          if(exec) {
            logln("TestAmbiguousWallTimeAPIs---"); logln("");
            TestAmbiguousWallTimeAPIs();
          }
          break;
        case 28:
          name = "TestRepeatedWallTime";
          if(exec) {
            logln("TestRepeatedWallTime---"); logln("");
            TestRepeatedWallTime();
          }
          break;
        case 29:
          name = "TestSkippedWallTime";
          if(exec) {
            logln("TestSkippedWallTime---"); logln("");
            TestSkippedWallTime();
          }
          break;
        case 30:
          name = "TestCloneLocale";
          if(exec) {
            logln("TestCloneLocale---"); logln("");
            TestCloneLocale();
          }
          break;
        case 31:
          name = "TestIslamicUmAlQura";
          if(exec) {
            logln("TestIslamicUmAlQura---"); logln("");
            TestIslamicUmAlQura();
          }
          break;
        case 32:
          name = "TestIslamicTabularDates";
          if(exec) {
            logln("TestIslamicTabularDates---"); logln("");
            TestIslamicTabularDates();
          }
          break;
        case 33:
          name = "TestHebrewMonthValidation";
          if(exec) {
            logln("TestHebrewMonthValidation---"); logln("");
            TestHebrewMonthValidation();
          }
          break;
        case 34:
          name = "TestWeekData";
          if(exec) {
            logln("TestWeekData---"); logln("");
            TestWeekData();
          }
          break;
        case 35:
          name = "TestAddAcrossZoneTransition";
          if(exec) {
            logln("TestAddAcrossZoneTransition---"); logln("");
            TestAddAcrossZoneTransition();
          }
          break;
        case 36:
          name = "TestChineseCalendarMapping";
          if(exec) {
            logln("TestChineseCalendarMapping---"); logln("");
            TestChineseCalendarMapping();
          }
          break;
        default: name = ""; break;
    }
}

// ---------------------------------------------------------------------------------

UnicodeString CalendarTest::fieldName(UCalendarDateFields f) {
    switch (f) {
#define FIELD_NAME_STR(x) case x: return (#x+5)
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
    UBool eq = FALSE,b4 = FALSE,af = FALSE;

    UDate when = date(90, UCAL_APRIL, 15);

    UnicodeString tzid("TestZone");
    int32_t tzoffset = 123400;

    SimpleTimeZone *zone = new SimpleTimeZone(tzoffset, tzid);
    Calendar *cal = Calendar::createInstance(zone->clone(), status);
    if (failure(status, "Calendar::createInstance #1", TRUE)) return;

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
    cal->roll(UCAL_SECOND, (UBool) TRUE, status);
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
    if (count < 1 || loc == 0)
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
        ((Calendar *)cal)->roll(UCAL_HOUR, (int32_t)100, status);
        ((Calendar *)cal)->clear(UCAL_HOUR);
#if !UCONFIG_NO_SERVICE
        URegistryKey key = cal->registerFactory(NULL, status);
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
        Calendar::getKeywordValuesForLocale("calendar", Locale("he"), FALSE, status));
    if (values.isNull() || U_FAILURE(status)) {
        dataerrln("FAIL: Calendar::getKeywordValuesForLocale(he): %s", u_errorName(status));
    } else {
        UBool containsHebrew = FALSE;
        const char *charValue;
        int32_t valueLength;
        while ((charValue = values->next(&valueLength, status)) != NULL) {
            if (valueLength == 6 && strcmp(charValue, "hebrew") == 0) {
                containsHebrew = TRUE;
            }
        }
        if (!containsHebrew) {
            errln("Calendar::getKeywordValuesForLocale(he)->next() does not contain \"hebrew\"");
        }

        values->reset(status);
        containsHebrew = FALSE;
        UnicodeString hebrew = UNICODE_STRING_SIMPLE("hebrew");
        const UChar *ucharValue;
        while ((ucharValue = values->unext(&valueLength, status)) != NULL) {
            UnicodeString value(FALSE, ucharValue, valueLength);
            if (value == hebrew) {
                containsHebrew = TRUE;
            }
        }
        if (!containsHebrew) {
            errln("Calendar::getKeywordValuesForLocale(he)->unext() does not contain \"hebrew\"");
        }

        values->reset(status);
        containsHebrew = FALSE;
        const UnicodeString *stringValue;
        while ((stringValue = values->snext(status)) != NULL) {
            if (*stringValue == hebrew) {
                containsHebrew = TRUE;
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
    if (failure(status, "new GregorianCalendar", TRUE)) return;
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
    dowTest(FALSE);
    dowTest(TRUE);
}

void CalendarTest::dowTest(UBool lenient)
{
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar* cal = new GregorianCalendar(status);
    if (failure(status, "new GregorianCalendar", TRUE)) return;
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
    if (failure(status, "Calendar::createInstance", TRUE)) return;
    Calendar *d = (Calendar*) c->clone();
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
    if (failure(status, "new GregorianCalendar", TRUE)) return;
    logln(UnicodeString("With cutoff ") + dateToString(c->getGregorianChange(), str));
    b = c->isLeapYear(1800);
    logln(UnicodeString(" isLeapYear(1800) = ") + (b ? "true" : "false"));
    logln(UnicodeString(" (should be FALSE)"));
    if (b) errln("FAIL");
    c->setGregorianChange(date(0, 0, 1), status);
    if (U_FAILURE(status)) { errln("GregorianCalendar::setGregorianChange failed"); return; }
    logln(UnicodeString("With cutoff ") + dateToString(c->getGregorianChange(), str));
    b = c->isLeapYear(1800);
    logln(UnicodeString(" isLeapYear(1800) = ") + (b ? "true" : "false"));
    logln(UnicodeString(" (should be TRUE)"));
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
    if (failure(status, "Calendar::createInstance", TRUE)) return;
    c->setLenient(FALSE);
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
     * it propertly resolves to Tue Dec 30 1997, which has a WOY value
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
    if (failure(status, "Calendar::createInstance", TRUE)) return;
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
    if (failure(status, "Calendar::createInstance", TRUE)) return;

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
    if (failure(status, "new GregorianCalendar", TRUE)) return;
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
    if (failure(status, "new GregorianCalendar", TRUE)) return;

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
            if (U_FAILURE(status)) { limit = i; status = U_ZERO_ERROR; }
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
        if (failure(status, "new GregorianCalendar", TRUE)) return;
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
    UBool ok = FALSE;
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
    if (failure(status, "new GregorianCalendar", TRUE)) return;
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
        if (failure(status, "new GregorianCalendar", TRUE)) return;
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
    GregorianCalendar *calendar = 0;
    calendar = new GregorianCalendar(year, month, dDate, status);
    if (failure(status, "new GregorianCalendar", TRUE)) return;
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
    if (failure(status, "Calendar::createInstance", TRUE)) return;
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
    if (failure(status, "new GregorianCalendar", TRUE)) return;
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
    if (failure(status, "Calendar::createInstance", TRUE)) return;
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

    return;
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
    Calendar *cur = (Calendar*) cal->clone();
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

#define CHECK(status, msg) \
    if (U_FAILURE(status)) { \
        errcheckln(status, msg); \
        return; \
    }

void CalendarTest::TestWOY(void) {
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
    if (failure(status, "Cannot construct calendar/format", TRUE)) return;

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
   if (failure(status, "construct GregorianCalendar", TRUE)) return;

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
  if (failure(status, "construct GregorianCalendar", TRUE)) return;
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
                  if(name==NULL) {
                          if(i==count || t>UDBG_HIGHEST_CONTIGUOUS_ENUM  ) {
                                logln(" null name - expected.\n");
                          } else {
                                errln("FAIL: udbg_enumName(%d,%d) returned NULL", t, i);
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
  default: return NULL;
  }
}

int32_t CalendarTest::testLocaleCount()
{
  static int32_t gLocaleCount = -1;
  if(gLocaleCount < 0) {
    int32_t i;
    for(i=0;testLocaleID(i) != NULL;i++) {
      ;
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
  isGregorian = dynamic_cast<GregorianCalendar*>(adopt) != NULL;
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
    if (failure(status, "Calendar::createInstance", TRUE)) return;
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

    return;
}

void CalendarTest::Test3785()
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString uzone = UNICODE_STRING_SIMPLE("Europe/Paris");
    UnicodeString exp1 = UNICODE_STRING_SIMPLE("Mon 30 Jumada II 1433 AH, 01:47:03");
    UnicodeString exp2 = UNICODE_STRING_SIMPLE("Mon 1 Rajab 1433 AH, 01:47:04");

    LocalUDateFormatPointer df(udat_open(UDAT_NONE, UDAT_NONE, "en@calendar=islamic", uzone.getTerminatedBuffer(),
                                         uzone.length(), NULL, 0, &status));
    if (df.isNull() || U_FAILURE(status)) return;

    UChar upattern[64];
    u_uastrcpy(upattern, "EEE d MMMM y G, HH:mm:ss");
    udat_applyPattern(df.getAlias(), FALSE, upattern, u_strlen(upattern));

    UChar ubuffer[1024];
    UDate ud0 = 1337557623000.0;

    status = U_ZERO_ERROR;
    udat_format(df.getAlias(), ud0, ubuffer, 1024, NULL, &status);
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
    udat_format(df.getAlias(), ud0, ubuffer, 1024, NULL, &status);
    if (U_FAILURE(status)) {
        errln("Error formatting date 2\n");
        return;
    }
    //printf("formatted: '%s'\n", mkcstr(ubuffer));
    UnicodeString act2(ubuffer);
    if ( act2 != exp2 ) {
        errln("Unexpected result from date 2 format\n");
    }

    return;
}

void CalendarTest::Test1624() {
    UErrorCode status = U_ZERO_ERROR;
    Locale loc("he_IL@calendar=hebrew");
    HebrewCalendar hc(loc,status);

    for (int32_t year = 5600; year < 5800; year++ ) {

        for (int32_t month = HebrewCalendar::TISHRI; month <= HebrewCalendar::ELUL; month++) {
            // skip the adar 1 month if year is not a leap year
            if (HebrewCalendar::isLeapYear(year) == FALSE && month == HebrewCalendar::ADAR_1) {
                continue;
            }
            int32_t day = 15;
            hc.set(year,month,day);
            int32_t dayHC = hc.get(UCAL_DATE,status);
            int32_t monthHC = hc.get(UCAL_MONTH,status);
            int32_t yearHC = hc.get(UCAL_YEAR,status);

            if (failure(status, "HebrewCalendar.get()", TRUE)) continue;

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
    return;
}

void CalendarTest::TestTimeStamp() {
    UErrorCode status = U_ZERO_ERROR;
    UDate start = 0.0, time;
    Calendar *cal;

    // Create a new Gregorian Calendar.
    cal = Calendar::createInstance("en_US@calender=gregorian", status);
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
        NULL
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

    for (int i = 0; TEST_LOCALES[i] != NULL; i++) {
        UErrorCode status = U_ZERO_ERROR;
        Calendar *cal = Calendar::createInstance(TEST_LOCALES[i], status);
        if (U_FAILURE(status)) {
            errln("Error: Failed to create a calendar for locale: %s", TEST_LOCALES[i]);
            continue;
        }
        if (uprv_strcmp(cal->getType(), "gregorian") != 0) {
            errln("Error: Gregorian calendar is not used for locale: %s", TEST_LOCALES[i]);
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
CalendarTest::TestAmbiguousWallTimeAPIs(void) {
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
    UBool operator==(const CalFields& rhs) const;
    UBool operator!=(const CalFields& rhs) const;
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
    sprintf(local, "%04d-%02d-%02d %02d:%02d:%02d.%03d", year, month, day, hour, min, sec, ms);
    uprv_strncpy(buf, local, len - 1);
    buf[len - 1] = 0;
    return buf;
}

UBool
CalFields::operator==(const CalFields& rhs) const {
    return year == rhs.year
        && month == rhs.month
        && day == rhs.day
        && hour == rhs.hour
        && min == rhs.min
        && sec == rhs.sec
        && ms == rhs.ms;
}

UBool
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

    {NULL,                  CalFields(0,0,0,0,0,0),         CalFields(0,0,0,0,0,0),          CalFields(0,0,0,0,0,0)}
};

void CalendarTest::TestRepeatedWallTime(void) {
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

    for (int32_t i = 0; RPDATA[i].tzid != NULL; i++) {
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
    {"America/New_York",    CalFields(2011,3,13,1,59,59),   TRUE,   CalFields(2011,3,13,6,59,59),   CalFields(2011,3,13,6,59,59),   CalFields(2011,3,13,6,59,59)},
    {"America/New_York",    CalFields(2011,3,13,2,0,0),     FALSE,  CalFields(2011,3,13,7,0,0),     CalFields(2011,3,13,6,0,0),     CalFields(2011,3,13,7,0,0)},
    {"America/New_York",    CalFields(2011,3,13,2,1,0),     FALSE,  CalFields(2011,3,13,7,1,0),     CalFields(2011,3,13,6,1,0),     CalFields(2011,3,13,7,0,0)},
    {"America/New_York",    CalFields(2011,3,13,2,30,0),    FALSE,  CalFields(2011,3,13,7,30,0),    CalFields(2011,3,13,6,30,0),    CalFields(2011,3,13,7,0,0)},
    {"America/New_York",    CalFields(2011,3,13,2,59,59),   FALSE,  CalFields(2011,3,13,7,59,59),   CalFields(2011,3,13,6,59,59),   CalFields(2011,3,13,7,0,0)},
    {"America/New_York",    CalFields(2011,3,13,3,0,0),     TRUE,   CalFields(2011,3,13,7,0,0),     CalFields(2011,3,13,7,0,0),     CalFields(2011,3,13,7,0,0)},

    {"Pacific/Apia",        CalFields(2011,12,29,23,59,59), TRUE,   CalFields(2011,12,30,9,59,59),  CalFields(2011,12,30,9,59,59),  CalFields(2011,12,30,9,59,59)},
    {"Pacific/Apia",        CalFields(2011,12,30,0,0,0),    FALSE,  CalFields(2011,12,30,10,0,0),   CalFields(2011,12,29,10,0,0),   CalFields(2011,12,30,10,0,0)},
    {"Pacific/Apia",        CalFields(2011,12,30,12,0,0),   FALSE,  CalFields(2011,12,30,22,0,0),   CalFields(2011,12,29,22,0,0),   CalFields(2011,12,30,10,0,0)},
    {"Pacific/Apia",        CalFields(2011,12,30,23,59,59), FALSE,  CalFields(2011,12,31,9,59,59),  CalFields(2011,12,30,9,59,59),  CalFields(2011,12,30,10,0,0)},
    {"Pacific/Apia",        CalFields(2011,12,31,0,0,0),    TRUE,   CalFields(2011,12,30,10,0,0),   CalFields(2011,12,30,10,0,0),   CalFields(2011,12,30,10,0,0)},

    {NULL,                  CalFields(0,0,0,0,0,0),         TRUE,   CalFields(0,0,0,0,0,0),         CalFields(0,0,0,0,0,0),         CalFields(0,0,0,0,0,0)}
};


void CalendarTest::TestSkippedWallTime(void) {
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

    for (int32_t i = 0; SKDATA[i].tzid != NULL; i++) {
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

void CalendarTest::TestCloneLocale(void) {
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

void CalendarTest::setAndTestCalendar(Calendar* cal, int32_t initMonth, int32_t initDay, int32_t initYear, UErrorCode& status) {
        cal->clear();
        cal->setLenient(FALSE);
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

static const UChar zoneSA[] = {0x41,0x73,0x69,0x61,0x2F,0x52,0x69,0x79,0x61,0x64,0x68,0}; // "Asia/Riyadh"

void CalendarTest::TestIslamicUmAlQura() {

    UErrorCode status = U_ZERO_ERROR;
    Locale umalquraLoc("ar_SA@calendar=islamic-umalqura");
    Locale gregoLoc("ar_SA@calendar=gregorian");
    TimeZone* tzSA = TimeZone::createTimeZone(UnicodeString(TRUE, zoneSA, -1));
    Calendar* tstCal = Calendar::createInstance(*((const TimeZone *)tzSA), umalquraLoc, status);
    Calendar* gregCal = Calendar::createInstance(*((const TimeZone *)tzSA), gregoLoc, status);

    IslamicCalendar* iCal = (IslamicCalendar*)tstCal;
    if(strcmp(iCal->getType(), "islamic-umalqura") != 0) {
        errln("wrong type of calendar created - %s", iCal->getType());
    }

    int32_t firstYear = 1318;
    int32_t lastYear = 1368;    // just enough to be pretty sure
    //int32_t lastYear = 1480;    // the whole shootin' match

    tstCal->clear();
    tstCal->setLenient(FALSE);

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
        tstCal->roll(UCAL_DAY_OF_MONTH, (UBool)TRUE, status);
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
    if (failure(status, "Calendar::createInstance, locale:he_IL@calendar=hebrew", TRUE)) return;
    Calendar *pCal = cal.getAlias();

    UDate d;
    pCal->setLenient(FALSE);

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
        0
    };

    UErrorCode status;

    for (int32_t i = 0; LOCALE_PAIRS[i] != 0; i += 2) {
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

    {NULL, CalFields(0,0,0,0,0,0,0), 0, UCAL_WALLTIME_LAST, CalFields(0,0,0,0,0,0,0)}
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

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
