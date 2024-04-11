// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/***********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2016, International Business Machines Corporation
 * and others. All Rights Reserved.
 ***********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/timezone.h"
#include "unicode/simpletz.h"
#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/localpointer.h"
#include "unicode/resbund.h"
#include "unicode/strenum.h"
#include "unicode/ustring.h"
#include "unicode/uversion.h"
#include "tztest.h"
#include "cmemory.h"
#include "putilimp.h"
#include "cstring.h"
#include "olsontz.h"

#define CASE(id,test) case id:                               \
                          name = #test;                      \
                          if (exec) {                        \
                              logln(#test "---"); logln(""); \
                              test();                        \
                          }                                  \
                          break

// *****************************************************************************
// class TimeZoneTest
// *****************************************************************************

// Some test case data is current date/tzdata version sensitive and producing errors
// when year/rule are changed. Although we want to keep our eyes on test failures
// caused by tzdata changes while development, keep maintaining test data in maintenance
// stream is a little bit hassle. ICU 49 or later versions are using minor version field
// to indicate a development build (0) or official release build (others). For development
// builds, a test failure triggers an error, while release builds only report them in
// verbose mode with logln.
static UBool isDevelopmentBuild = (U_ICU_VERSION_MINOR_NUM == 0);

void TimeZoneTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) {
        logln("TestSuite TestTimeZone");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestPRTOffset);
    TESTCASE_AUTO(TestVariousAPI518);
    TESTCASE_AUTO(TestGetAvailableIDs913);
    TESTCASE_AUTO(TestGenericAPI);
    TESTCASE_AUTO(TestRuleAPI);
    TESTCASE_AUTO(TestShortZoneIDs);
    TESTCASE_AUTO(TestCustomParse);
    TESTCASE_AUTO(TestDisplayName);
    TESTCASE_AUTO(TestDSTSavings);
    TESTCASE_AUTO(TestAlternateRules);
    TESTCASE_AUTO(TestCountries); 
    TESTCASE_AUTO(TestHistorical);
    TESTCASE_AUTO(TestEquivalentIDs);
    TESTCASE_AUTO(TestAliasedNames);
    TESTCASE_AUTO(TestFractionalDST);
    TESTCASE_AUTO(TestFebruary);
    TESTCASE_AUTO(TestCanonicalIDAPI);
    TESTCASE_AUTO(TestCanonicalID);
    TESTCASE_AUTO(TestDisplayNamesMeta);
    TESTCASE_AUTO(TestGetRegion);
    TESTCASE_AUTO(TestGetAvailableIDsNew);
    TESTCASE_AUTO(TestGetUnknown);
    TESTCASE_AUTO(TestGetGMT);
    TESTCASE_AUTO(TestGetWindowsID);
    TESTCASE_AUTO(TestGetIDForWindowsID);
    TESTCASE_AUTO(TestCasablancaNameAndOffset22041);
    TESTCASE_AUTO(TestRawOffsetAndOffsetConsistency22041);
    TESTCASE_AUTO(TestGetIanaID);
    TESTCASE_AUTO(TestGMTMinus24ICU22526);
    TESTCASE_AUTO_END;
}

const int32_t TimeZoneTest::millisPerHour = 3600000;

// ---------------------------------------------------------------------------------

/**
 * Generic API testing for API coverage.
 */
void
TimeZoneTest::TestGenericAPI()
{
    UnicodeString id("NewGMT");
    int32_t offset = 12345;

    SimpleTimeZone *zone = new SimpleTimeZone(offset, id);
    if (zone->useDaylightTime()) errln("FAIL: useDaylightTime should return false");

    TimeZone* zoneclone = zone->clone();
    if (!(*zoneclone == *zone)) errln("FAIL: clone or operator== failed");
    zoneclone->setID("abc");
    if (!(*zoneclone != *zone)) errln("FAIL: clone or operator!= failed");
    delete zoneclone;

    zoneclone = zone->clone();
    if (!(*zoneclone == *zone)) errln("FAIL: clone or operator== failed");
    zoneclone->setRawOffset(45678);
    if (!(*zoneclone != *zone)) errln("FAIL: clone or operator!= failed");

    SimpleTimeZone copy(*zone);
    if (!(copy == *zone)) errln("FAIL: copy constructor or operator== failed");
    copy = *dynamic_cast<SimpleTimeZone*>(zoneclone);
    if (!(copy == *zoneclone)) errln("FAIL: assignment operator or operator== failed");

    TimeZone* saveDefault = TimeZone::createDefault();
    logln((UnicodeString)"TimeZone::createDefault() => " + saveDefault->getID(id));

    TimeZone::adoptDefault(zone);
    TimeZone* defaultzone = TimeZone::createDefault();
    if (defaultzone == zone ||
        !(*defaultzone == *zone))
        errln("FAIL: createDefault failed");
    TimeZone::adoptDefault(saveDefault);
    delete defaultzone;
    delete zoneclone;

    logln("call uprv_timezone() which uses the host");
    logln("to get the difference in seconds between coordinated universal");
    logln("time and local time. E.g., -28,800 for PST (GMT-8hrs)");

    int32_t tzoffset = uprv_timezone();
    if ((tzoffset % 900) != 0) {
        /*
         * Ticket#6364 and #7648
         * A few time zones are using GMT offests not a multiple of 15 minutes.
         * Therefore, we should not interpret such case as an error.
         * We downgrade this from errln to infoln. When we see this message,
         * we should examine if it is ignorable or not.
         */
        infoln("WARNING: t_timezone may be incorrect. It is not a multiple of 15min.", tzoffset);
    }

    TimeZone* hostZone = TimeZone::detectHostTimeZone();
    int32_t hostZoneRawOffset = hostZone->getRawOffset();
    logln("hostZone->getRawOffset() = %d , tzoffset = %d", hostZoneRawOffset, tzoffset * (-1000));

    /* Host time zone's offset should match the offset returned by uprv_timezone() */
    if (hostZoneRawOffset != tzoffset * (-1000)) {
        UnicodeString id;
        hostZone->getID(id);
        // Known issues in ICU-22274 we have issues in time zone
        // Africa/Casablanca Europe/Dublin America/Godthab America/Nuuk
        if (id == u"Africa/Casablanca" || id == u"Europe/Dublin" ||
            id == u"America/Godthab" || id == u"America/Nuuk" ||
            id == u"Africa/El_Aaiun" ||
            id == u"Asia/Qostanay" ||  // Due to changes in tz2024a
            id == u"Asia/Almaty" ||  // Due to changes in tz2024a
            id == u"America/Scoresbysund"  // break after the update of tz2023d
            ) {
          logKnownIssue( "ICU-22274", "detectHostTimeZone()'s raw offset != host timezone's offset in TimeZone " + id);
        } else {
          errln("FAIL: detectHostTimeZone()'s raw offset != host timezone's offset.\n"
                "hostZone->getRawOffset()=%d\n"
                "but uprv_timezone() return %d and "
                "uprv_timezone() * -1000=%d",
                hostZoneRawOffset, tzoffset, tzoffset * -1000);
        }
    }
    delete hostZone;

    UErrorCode status = U_ZERO_ERROR;
    const char* tzver = TimeZone::getTZDataVersion(status);
    if (U_FAILURE(status)) {
        errcheckln(status, "FAIL: getTZDataVersion failed - %s", u_errorName(status));
    } else {
        int32_t tzverLen = uprv_strlen(tzver);
        if (tzverLen == 5 || tzverLen == 6 /* 4 digits + 1 or 2 letters */) {
            logln((UnicodeString)"tzdata version: " + tzver);
        } else {
            errln((UnicodeString)"FAIL: getTZDataVersion returned " + tzver);
        }
    }
}

// ---------------------------------------------------------------------------------

/**
 * Test the setStartRule/setEndRule API calls.
 */
void
TimeZoneTest::TestRuleAPI()
{
    UErrorCode status = U_ZERO_ERROR;

    UDate offset = 60*60*1000*1.75; // Pick a weird offset
    SimpleTimeZone *zone = new SimpleTimeZone((int32_t)offset, "TestZone");
    if (zone->useDaylightTime()) errln("FAIL: useDaylightTime should return false");

    // Establish our expected transition times.  Do this with a non-DST
    // calendar with the (above) declared local offset.
    GregorianCalendar *gc = new GregorianCalendar(*zone, status);
    if (failure(status, "new GregorianCalendar", true)) return;
    gc->clear();
    gc->set(1990, UCAL_MARCH, 1);
    UDate marchOneStd = gc->getTime(status); // Local Std time midnight
    gc->clear();
    gc->set(1990, UCAL_JULY, 1);
    UDate julyOneStd = gc->getTime(status); // Local Std time midnight
    if (failure(status, "GregorianCalendar::getTime")) return;

    // Starting and ending hours, WALL TIME
    int32_t startHour = (int32_t)(2.25 * 3600000);
    int32_t endHour   = (int32_t)(3.5  * 3600000);

    zone->setStartRule(UCAL_MARCH, 1, 0, startHour, status);
    zone->setEndRule  (UCAL_JULY,  1, 0, endHour, status);

    delete gc;
    gc = new GregorianCalendar(*zone, status);
    if (failure(status, "new GregorianCalendar")) return;

    UDate marchOne = marchOneStd + startHour;
    UDate julyOne = julyOneStd + endHour - 3600000; // Adjust from wall to Std time

    UDate expMarchOne = 636251400000.0;
    if (marchOne != expMarchOne)
    {
        errln((UnicodeString)"FAIL: Expected start computed as " + marchOne +
          " = " + dateToString(marchOne));
        logln((UnicodeString)"      Should be                  " + expMarchOne +
          " = " + dateToString(expMarchOne));
    }

    UDate expJulyOne = 646793100000.0;
    if (julyOne != expJulyOne)
    {
        errln((UnicodeString)"FAIL: Expected start computed as " + julyOne +
          " = " + dateToString(julyOne));
        logln((UnicodeString)"      Should be                  " + expJulyOne +
          " = " + dateToString(expJulyOne));
    }

    testUsingBinarySearch(*zone, date(90, UCAL_JANUARY, 1), date(90, UCAL_JUNE, 15), marchOne);
    testUsingBinarySearch(*zone, date(90, UCAL_JUNE, 1), date(90, UCAL_DECEMBER, 31), julyOne);

    if (zone->inDaylightTime(marchOne - 1000, status) ||
        !zone->inDaylightTime(marchOne, status))
        errln("FAIL: Start rule broken");
    if (!zone->inDaylightTime(julyOne - 1000, status) ||
        zone->inDaylightTime(julyOne, status))
        errln("FAIL: End rule broken");

    zone->setStartYear(1991);
    if (zone->inDaylightTime(marchOne, status) ||
        zone->inDaylightTime(julyOne - 1000, status))
        errln("FAIL: Start year broken");

    failure(status, "TestRuleAPI");
    delete gc;
    delete zone;
}

void
TimeZoneTest::findTransition(const TimeZone& tz,
                             UDate min, UDate max) {
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeString id,s;
    UBool startsInDST = tz.inDaylightTime(min, ec);
    if (failure(ec, "TimeZone::inDaylightTime")) return;
    if (tz.inDaylightTime(max, ec) == startsInDST) {
        logln("Error: " + tz.getID(id) + ".inDaylightTime(" + dateToString(min) + ") = " + (startsInDST?"true":"false") +
              ", inDaylightTime(" + dateToString(max) + ") = " + (startsInDST?"true":"false"));
        return;
    }
    if (failure(ec, "TimeZone::inDaylightTime")) return;
    while ((max - min) > INTERVAL) {
        UDate mid = (min + max) / 2;
        if (tz.inDaylightTime(mid, ec) == startsInDST) {
            min = mid;
        } else {
            max = mid;
        }
        if (failure(ec, "TimeZone::inDaylightTime")) return;
    }
    min = 1000.0 * uprv_floor(min/1000.0);
    max = 1000.0 * uprv_floor(max/1000.0);
    logln(tz.getID(id) + " Before: " + min/1000 + " = " +
          dateToString(min,s,tz));
    logln(tz.getID(id) + " After:  " + max/1000 + " = " +
          dateToString(max,s,tz));
}

void
TimeZoneTest::testUsingBinarySearch(const TimeZone& tz,
                                    UDate min, UDate max,
                                    UDate expectedBoundary)
{
    UErrorCode status = U_ZERO_ERROR;
    UBool startsInDST = tz.inDaylightTime(min, status);
    if (failure(status, "TimeZone::inDaylightTime")) return;
    if (tz.inDaylightTime(max, status) == startsInDST) {
        logln("Error: inDaylightTime(" + dateToString(max) + ") != " + ((!startsInDST)?"true":"false"));
        return;
    }
    if (failure(status, "TimeZone::inDaylightTime")) return;
    while ((max - min) > INTERVAL) {
        UDate mid = (min + max) / 2;
        if (tz.inDaylightTime(mid, status) == startsInDST) {
            min = mid;
        } else {
            max = mid;
        }
        if (failure(status, "TimeZone::inDaylightTime")) return;
    }
    logln(UnicodeString("Binary Search Before: ") + uprv_floor(0.5 + min) + " = " + dateToString(min));
    logln(UnicodeString("Binary Search After:  ") + uprv_floor(0.5 + max) + " = " + dateToString(max));
    UDate mindelta = expectedBoundary - min;
    UDate maxdelta = max - expectedBoundary;
    if (mindelta >= 0 &&
        mindelta <= INTERVAL &&
        maxdelta >= 0 &&
        maxdelta <= INTERVAL)
        logln(UnicodeString("PASS: Expected bdry:  ") + expectedBoundary + " = " + dateToString(expectedBoundary));
    else
        errln(UnicodeString("FAIL: Expected bdry:  ") + expectedBoundary + " = " + dateToString(expectedBoundary));
}

const UDate TimeZoneTest::INTERVAL = 100;

// ---------------------------------------------------------------------------------

// -------------------------------------

/**
 * Test the offset of the PRT timezone.
 */
void
TimeZoneTest::TestPRTOffset()
{
    TimeZone* tz = TimeZone::createTimeZone("PRT");
    if (tz == nullptr) {
        errln("FAIL: TimeZone(PRT) is null");
    }
    else {
      int32_t expectedHour = -4;
      double expectedOffset = (((double)expectedHour) * millisPerHour);
      double foundOffset = tz->getRawOffset();
      int32_t foundHour = (int32_t)foundOffset / millisPerHour;
      if (expectedOffset != foundOffset) {
        dataerrln("FAIL: Offset for PRT should be %d, found %d", expectedHour, foundHour);
      } else {
        logln("PASS: Offset for PRT should be %d, found %d", expectedHour, foundHour);
      }
    }
    delete tz;
}

// -------------------------------------

/**
 * Regress a specific bug with a sequence of API calls.
 */
void
TimeZoneTest::TestVariousAPI518()
{
    UErrorCode status = U_ZERO_ERROR;
    TimeZone* time_zone = TimeZone::createTimeZone("PST");
    UDate d = date(97, UCAL_APRIL, 30);
    UnicodeString str;
    logln("The timezone is " + time_zone->getID(str));
    if (!time_zone->inDaylightTime(d, status)) dataerrln("FAIL: inDaylightTime returned false");
    if (failure(status, "TimeZone::inDaylightTime", true)) return;
    if (!time_zone->useDaylightTime()) dataerrln("FAIL: useDaylightTime returned false");
    if (time_zone->getRawOffset() != - 8 * millisPerHour) dataerrln("FAIL: getRawOffset returned wrong value");
    GregorianCalendar *gc = new GregorianCalendar(status);
    if (U_FAILURE(status)) { errln("FAIL: Couldn't create GregorianCalendar"); return; }
    gc->setTime(d, status);
    if (U_FAILURE(status)) { errln("FAIL: GregorianCalendar::setTime failed"); return; }
    if (time_zone->getOffset(gc->AD, gc->get(UCAL_YEAR, status), gc->get(UCAL_MONTH, status),
        gc->get(UCAL_DATE, status), (uint8_t)gc->get(UCAL_DAY_OF_WEEK, status), 0, status) != - 7 * millisPerHour)
        dataerrln("FAIL: getOffset returned wrong value");
    if (U_FAILURE(status)) { errln("FAIL: GregorianCalendar::set failed"); return; }
    delete gc;
    delete time_zone;
}

// -------------------------------------

/**
 * Test the call which retrieves the available IDs.
 */
void
TimeZoneTest::TestGetAvailableIDs913()
{
    UErrorCode ec = U_ZERO_ERROR;
    int32_t i;

#ifdef U_USE_TIMEZONE_OBSOLETE_2_8
    // Test legacy API -- remove these tests when the corresponding API goes away (duh)
    int32_t numIDs = -1;
    const UnicodeString** ids = TimeZone::createAvailableIDs(numIDs);
    if (ids == 0 || numIDs < 1) {
        errln("FAIL: createAvailableIDs()");
    } else {
        UnicodeString buf("TimeZone::createAvailableIDs() = { ");
        for(i=0; i<numIDs; ++i) {
            if (i) buf.append(", ");
            buf.append(*ids[i]);
        }
        buf.append(" } ");
        logln(buf + numIDs);
        // we own the array; the caller owns the contained strings (yuck)
        uprv_free(ids);
    }

    numIDs = -1;
    ids = TimeZone::createAvailableIDs(-8*U_MILLIS_PER_HOUR, numIDs);
    if (ids == 0 || numIDs < 1) {
        errln("FAIL: createAvailableIDs(-8:00)");
    } else {
        UnicodeString buf("TimeZone::createAvailableIDs(-8:00) = { ");
        for(i=0; i<numIDs; ++i) {
            if (i) buf.append(", ");
            buf.append(*ids[i]);
        }
        buf.append(" } ");
        logln(buf + numIDs);
        // we own the array; the caller owns the contained strings (yuck)
        uprv_free(ids);
    }
    numIDs = -1;
    ids = TimeZone::createAvailableIDs("US", numIDs);
    if (ids == 0 || numIDs < 1) {
      errln("FAIL: createAvailableIDs(US) ids=%d, numIDs=%d", ids, numIDs);
    } else {
        UnicodeString buf("TimeZone::createAvailableIDs(US) = { ");
        for(i=0; i<numIDs; ++i) {
            if (i) buf.append(", ");
            buf.append(*ids[i]);
        }
        buf.append(" } ");
        logln(buf + numIDs);
        // we own the array; the caller owns the contained strings (yuck)
        uprv_free(ids);
    }
#endif

    UnicodeString str;
    UnicodeString buf(u"TimeZone::createEnumeration() = { ");
    int32_t s_length;
    StringEnumeration* s = TimeZone::createEnumeration(ec);
    LocalPointer<StringEnumeration> tmp1(TimeZone::createEnumeration(), ec);
    if (U_FAILURE(ec) || s == nullptr) {
        dataerrln("Unable to create TimeZone enumeration");
        return;
    }
    s_length = s->count(ec);
    if (s_length != tmp1->count(ec)) {
        errln("TimeZone::createEnumeration() with no status args returns a different count.");
    }
    for (i = 0; i < s_length;++i) {
        if (i > 0) buf += ", ";
        if ((i & 1) == 0) {
            buf += *s->snext(ec);
        } else {
            buf += UnicodeString(s->next(nullptr, ec), "");
        }

        if((i % 5) == 4) {
            // replace s with a clone of itself
            StringEnumeration *s2 = s->clone();
            if(s2 == nullptr || s_length != s2->count(ec)) {
                errln("TimezoneEnumeration.clone() failed");
            } else {
                delete s;
                s = s2;
            }
        }
    }
    buf += " };";
    logln(buf);

    /* Confirm that the following zones can be retrieved: The first
     * zone, the last zone, and one in-between.  This tests the binary
     * search through the system zone data.
     */
    s->reset(ec);
    int32_t middle = s_length/2;
    for (i=0; i<s_length; ++i) {
        const UnicodeString* id = s->snext(ec);
        if (i==0 || i==middle || i==(s_length-1)) {
        TimeZone *z = TimeZone::createTimeZone(*id);
        if (z == nullptr) {
            errln(UnicodeString("FAIL: createTimeZone(") +
                  *id + ") -> 0");
        } else if (z->getID(str) != *id) {
            errln(UnicodeString("FAIL: createTimeZone(") +
                  *id + ") -> zone " + str);
        } else {
            logln(UnicodeString("OK: createTimeZone(") +
                  *id + ") succeeded");
        }
        delete z;
        }
    }
    delete s;

    buf.truncate(0);
    buf += "TimeZone::createEnumeration(GMT+01:00) = { ";

    s = TimeZone::createEnumerationForRawOffset(1 * U_MILLIS_PER_HOUR, ec);
    LocalPointer<StringEnumeration> tmp2(TimeZone::createEnumeration(1 * U_MILLIS_PER_HOUR), ec);
    if (U_FAILURE(ec)) {
        dataerrln("Unable to create TimeZone enumeration for GMT+1");
        return;
    }
    s_length = s->count(ec);
    if (s_length != tmp2->count(ec)) {
        errln("TimeZone::createEnumeration(GMT+01:00) with no status args returns a different count.");
    }
    for (i = 0; i < s_length;++i) {
        if (i > 0) buf += ", ";
        buf += *s->snext(ec);
    }
    delete s;
    buf += " };";
    logln(buf);


    buf.truncate(0);
    buf += "TimeZone::createEnumeration(US) = { ";

    s = TimeZone::createEnumerationForRegion("US", ec);
    LocalPointer<StringEnumeration> tmp3(TimeZone::createEnumeration("US"), ec);
    if (U_FAILURE(ec)) {
        dataerrln("Unable to create TimeZone enumeration for US");
        return;
    }
    s_length = s->count(ec);
    if (s_length != tmp3->count(ec)) {
        errln("TimeZone::createEnumeration(\"US\") with no status args returns a different count.");
    }
    for (i = 0; i < s_length; ++i) {
        if (i > 0) buf += ", ";
        buf += *s->snext(ec);
    }
    buf += " };";
    logln(buf);

    TimeZone *tz = TimeZone::createTimeZone("PST");
    if (tz != nullptr) logln("getTimeZone(PST) = " + tz->getID(str));
    else errln("FAIL: getTimeZone(PST) = null");
    delete tz;
    tz = TimeZone::createTimeZone("America/Los_Angeles");
    if (tz != nullptr) logln("getTimeZone(America/Los_Angeles) = " + tz->getID(str));
    else errln("FAIL: getTimeZone(PST) = null");
    delete tz;

    // @bug 4096694
    tz = TimeZone::createTimeZone("NON_EXISTENT");
    UnicodeString temp;
    if (tz == nullptr)
        errln("FAIL: getTimeZone(NON_EXISTENT) = null");
    else if (tz->getID(temp) != UCAL_UNKNOWN_ZONE_ID)
        errln("FAIL: getTimeZone(NON_EXISTENT) = " + temp);
    delete tz;

    delete s;
}

void
TimeZoneTest::TestGetAvailableIDsNew()
{
    UErrorCode ec = U_ZERO_ERROR;
    StringEnumeration *any, *canonical, *canonicalLoc;
    StringEnumeration *any_US, *canonical_US, *canonicalLoc_US;
    StringEnumeration *any_W5, *any_CA_W5;
    StringEnumeration *any_US_E14;
    int32_t rawOffset;
    const UnicodeString *id1, *id2;
    UnicodeString canonicalID;
    UBool isSystemID;
    char region[4] = {0};
    int32_t zoneCount;

    any = canonical = canonicalLoc = any_US = canonical_US = canonicalLoc_US = any_W5 = any_CA_W5 = any_US_E14 = nullptr;
    
    any = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, nullptr, nullptr, ec);
    if (U_FAILURE(ec)) {
        dataerrln("Failed to create enumeration for ANY");
        goto cleanup;
    }

    canonical = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, nullptr, nullptr, ec);
    if (U_FAILURE(ec)) {
        errln("Failed to create enumeration for CANONICAL");
        goto cleanup;
    }

    canonicalLoc = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL_LOCATION, nullptr, nullptr, ec);
    if (U_FAILURE(ec)) {
        errln("Failed to create enumeration for CANONICALLOC");
        goto cleanup;
    }

    any_US = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, "US", nullptr, ec);
    if (U_FAILURE(ec)) {
        errln("Failed to create enumeration for ANY_US");
        goto cleanup;
    }

    canonical_US = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, "US", nullptr, ec);
    if (U_FAILURE(ec)) {
        errln("Failed to create enumeration for CANONICAL_US");
        goto cleanup;
    }

    canonicalLoc_US = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL_LOCATION, "US", nullptr, ec);
    if (U_FAILURE(ec)) {
        errln("Failed to create enumeration for CANONICALLOC_US");
        goto cleanup;
    }

    rawOffset = (-5)*60*60*1000;
    any_W5 = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, nullptr, &rawOffset, ec);
    if (U_FAILURE(ec)) {
        errln("Failed to create enumeration for ANY_W5");
        goto cleanup;
    }

    any_CA_W5 = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, "CA", &rawOffset, ec);
    if (U_FAILURE(ec)) {
        errln("Failed to create enumeration for ANY_CA_W5");
        goto cleanup;
    }

    rawOffset = 14*60*60*1000;
    any_US_E14 = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, "US", &rawOffset, ec);
    if (U_FAILURE(ec)) {
        errln("Failed to create enumeration for ANY_US_E14");
        goto cleanup;
    }

    checkContainsAll(any, "ANY", canonical, "CANONICAL");
    checkContainsAll(canonical, "CANONICAL", canonicalLoc, "CANONICALLOC");

    checkContainsAll(any, "ANY", any_US, "ANY_US");
    checkContainsAll(canonical, "CANONICAL", canonical_US, "CANONICAL_US");
    checkContainsAll(canonicalLoc, "CANONICALLOC", canonicalLoc_US, "CANONICALLOC_US");

    checkContainsAll(any_US, "ANY_US", canonical_US, "CANONICAL_US");
    checkContainsAll(canonical_US, "CANONICAL_US", canonicalLoc_US, "CANONICALLOC_US");

    checkContainsAll(any, "ANY", any_W5, "ANY_W5");
    checkContainsAll(any_W5, "ANY_W5", any_CA_W5, "ANY_CA_W5");

    // And ID in any set, but not in canonical set must not be a canonical ID
    any->reset(ec);
    while ((id1 = any->snext(ec)) != nullptr) {
        UBool found = false;
        canonical->reset(ec);
        while ((id2 = canonical->snext(ec)) != nullptr) {
            if (*id1 == *id2) {
                found = true;
                break;
            }
        }
        if (U_FAILURE(ec)) {
            break;
        }
        if (!found) {
            TimeZone::getCanonicalID(*id1, canonicalID, isSystemID, ec);
            if (U_FAILURE(ec)) {
                break;
            }
            if (*id1 == canonicalID) {
                errln((UnicodeString)"FAIL: canonicalID [" + *id1 + "] is not in CANONICAL");
            }
            if (!isSystemID) {
                errln((UnicodeString)"FAIL: ANY contains non-system ID: " + *id1);
            }
        }
    }
    if (U_FAILURE(ec)) {
        errln("Error checking IDs in ANY, but not in CANONICAL");
        ec = U_ZERO_ERROR;
    }

    // canonical set must contains only canonical IDs
    canonical->reset(ec);
    while ((id1 = canonical->snext(ec)) != nullptr) {
        TimeZone::getCanonicalID(*id1, canonicalID, isSystemID, ec);
        if (U_FAILURE(ec)) {
            break;
        }
        if (*id1 != canonicalID) {
            errln((UnicodeString)"FAIL: CANONICAL contains non-canonical ID: " + *id1);
        }
        if (!isSystemID) {
            errln((UnicodeString)"FAILE: CANONICAL contains non-system ID: " + *id1);
        }
    }
    if (U_FAILURE(ec)) {
        errln("Error checking IDs in CANONICAL");
        ec = U_ZERO_ERROR;
    }

    // canonicalLoc set must contain only canonical location IDs
    canonicalLoc->reset(ec);
    while ((id1 = canonicalLoc->snext(ec)) != nullptr) {
        TimeZone::getRegion(*id1, region, sizeof(region), ec);
        if (U_FAILURE(ec)) {
            break;
        }
        if (uprv_strcmp(region, "001") == 0) {
            errln((UnicodeString)"FAIL: CANONICALLOC contains non location zone: " + *id1);
        }
    }
    if (U_FAILURE(ec)) {
        errln("Error checking IDs in CANONICALLOC");
        ec = U_ZERO_ERROR;
    }

    // any_US must contain only US zones
    any_US->reset(ec);
    while ((id1 = any_US->snext(ec)) != nullptr) {
        TimeZone::getRegion(*id1, region, sizeof(region), ec);
        if (U_FAILURE(ec)) {
            break;
        }
        if (uprv_strcmp(region, "US") != 0) {
            errln((UnicodeString)"FAIL: ANY_US contains non-US zone ID: " + *id1);
        }
    }
    if (U_FAILURE(ec)) {
        errln("Error checking IDs in ANY_US");
        ec = U_ZERO_ERROR;
    }

    // any_W5 must contain only GMT-05:00 zones
    any_W5->reset(ec);
    while ((id1 = any_W5->snext(ec)) != nullptr) {
        TimeZone *tz = TimeZone::createTimeZone(*id1);
        if (tz->getRawOffset() != (-5)*60*60*1000) {
            errln((UnicodeString)"FAIL: ANY_W5 contains a zone whose offset is not -05:00: " + *id1);
        }
        delete tz;
    }
    if (U_FAILURE(ec)) {
        errln("Error checking IDs in ANY_W5");
        ec = U_ZERO_ERROR;
    }

    // No US zone swith GMT+14:00
    zoneCount = any_US_E14->count(ec);
    if (U_FAILURE(ec)) {
        errln("Error checking IDs in ANY_US_E14");
        ec = U_ZERO_ERROR;
    } else if (zoneCount != 0) {
        errln("FAIL: ANY_US_E14 must be empty");
    }

cleanup:
    delete any;
    delete canonical;
    delete canonicalLoc;
    delete any_US;
    delete canonical_US;
    delete canonicalLoc_US;
    delete any_W5;
    delete any_CA_W5;
    delete any_US_E14;
}

void
TimeZoneTest::checkContainsAll(StringEnumeration *s1, const char *name1,
        StringEnumeration *s2, const char *name2)
{
    UErrorCode ec = U_ZERO_ERROR;
    const UnicodeString *id1, *id2;

    s2->reset(ec);

    while ((id2 = s2->snext(ec)) != nullptr) {
        UBool found = false;
        s1->reset(ec);
        while ((id1 = s1->snext(ec)) != nullptr) {
            if (*id1 == *id2) {
                found = true;
                break;
            }
        }
        if (!found) {
            errln((UnicodeString)"FAIL: " + name1 + "does not contain "
                + *id2 + " in " + name2);
        }
    }

    if (U_FAILURE(ec)) {
        errln((UnicodeString)"Error checkContainsAll for " + name1 + " - " + name2);
    }
}

/**
 * NOTE: As of ICU 2.8, this test confirms that the "tz.alias"
 * file, used to build ICU alias zones, is working.  It also
 * looks at some genuine Olson compatibility IDs. [aliu]
 *
 * This test is problematic. It should really just confirm that
 * the list of compatibility zone IDs exist and are somewhat
 * meaningful (that is, they aren't all aliases of GMT). It goes a
 * bit further -- it hard-codes expectations about zone behavior,
 * when in fact zones are redefined quite frequently. ICU's build
 * process means that it is easy to update ICU to contain the
 * latest Olson zone data, but if a zone tested here changes, then
 * this test will fail.  I have updated the test for 1999j data,
 * but further updates will probably be required. Note that some
 * of the concerts listed below no longer apply -- in particular,
 * we do NOT overwrite real UNIX zones with 3-letter IDs. There
 * are two points of overlap as of 1999j: MET and EET. These are
 * both real UNIX zones, so we just use the official
 * definition. This test has been updated to reflect this.
 * 12/3/99 aliu
 *
 * Added tests for additional zones and aliases from the icuzones file.
 * Markus Scherer 2006-nov-06
 *
 * [srl - from java - 7/5/1998]
 * @bug 4130885
 * Certain short zone IDs, used since 1.1.x, are incorrect.
 *
 * The worst of these is:
 *
 * "CAT" (Central African Time) should be GMT+2:00, but instead returns a
 * zone at GMT-1:00. The zone at GMT-1:00 should be called EGT, CVT, EGST,
 * or AZOST, depending on which zone is meant, but in no case is it CAT.
 *
 * Other wrong zone IDs:
 *
 * ECT (European Central Time) GMT+1:00: ECT is Ecuador Time,
 * GMT-5:00. European Central time is abbreviated CEST.
 *
 * SST (Solomon Island Time) GMT+11:00. SST is actually Samoa Standard Time,
 * GMT-11:00. Solomon Island time is SBT.
 *
 * NST (New Zealand Time) GMT+12:00. NST is the abbreviation for
 * Newfoundland Standard Time, GMT-3:30. New Zealanders use NZST.
 *
 * AST (Alaska Standard Time) GMT-9:00. [This has already been noted in
 * another bug.] It should be "AKST". AST is Atlantic Standard Time,
 * GMT-4:00.
 *
 * PNT (Phoenix Time) GMT-7:00. PNT usually means Pitcairn Time,
 * GMT-8:30. There is no standard abbreviation for Phoenix time, as distinct
 * from MST with daylight savings.
 *
 * In addition to these problems, a number of zones are FAKE. That is, they
 * don't match what people use in the real world.
 *
 * FAKE zones:
 *
 * EET (should be EEST)
 * ART (should be EEST)
 * MET (should be IRST)
 * NET (should be AMST)
 * PLT (should be PKT)
 * BST (should be BDT)
 * VST (should be ICT)
 * CTT (should be CST) +
 * ACT (should be CST) +
 * AET (should be EST) +
 * MIT (should be WST) +
 * IET (should be EST) +
 * PRT (should be AST) +
 * CNT (should be NST)
 * AGT (should be ARST)
 * BET (should be EST) +
 *
 * + A zone with the correct name already exists and means something
 * else. E.g., EST usually indicates the US Eastern zone, so it cannot be
 * used for Brazil (BET).
 */
void TimeZoneTest::TestShortZoneIDs()
{
    int32_t i;
    // Create a small struct to hold the array
    struct
    {
        const char *id;
        int32_t    offset;
        UBool      daylight;
    }
    kReferenceList [] =
    {
        {"HST", -600, false}, // Olson northamerica -10:00
        {"AST", -540, true},  // ICU Link - America/Anchorage
        {"PST", -480, true},  // ICU Link - America/Los_Angeles
        {"PNT", -420, false}, // ICU Link - America/Phoenix
        {"MST", -420, false}, // updated Aug 2003 aliu
        {"CST", -360, true},  // Olson northamerica -7:00
        {"IET", -300, true},  // ICU Link - America/Indiana/Indianapolis
        {"EST", -300, false}, // Olson northamerica -5:00
        {"PRT", -240, false}, // ICU Link - America/Puerto_Rico
        {"CNT", -210, true},  // ICU Link - America/St_Johns
        {"AGT", -180, false}, // ICU Link - America/Argentina/Buenos_Aires
        // Per https://mm.icann.org/pipermail/tz-announce/2019-July/000056.html
        //      Brazil has canceled DST and will stay on standard time indefinitely.
        {"BET", -180, false},  // ICU Link - America/Sao_Paulo
        {"GMT", 0, false},    // Olson etcetera Link - Etc/GMT
        {"UTC", 0, false},    // Olson etcetera 0
        {"ECT", 60, true},    // ICU Link - Europe/Paris
        {"MET", 60, true},    // Olson europe 1:00 C-Eur
        {"CAT", 120, false},  // ICU Link - Africa/Maputo
        {"ART", 120, false},  // ICU Link - Africa/Cairo
        {"EET", 120, true},   // Olson europe 2:00 EU
        {"EAT", 180, false},  // ICU Link - Africa/Addis_Ababa
        {"NET", 240, false},  // ICU Link - Asia/Yerevan
        {"PLT", 300, false},  // ICU Link - Asia/Karachi
        {"IST", 330, false},  // ICU Link - Asia/Kolkata
        {"BST", 360, false},  // ICU Link - Asia/Dhaka
        {"VST", 420, false},  // ICU Link - Asia/Ho_Chi_Minh
        {"CTT", 480, false},  // ICU Link - Asia/Shanghai
        {"JST", 540, false},  // ICU Link - Asia/Tokyo
        {"ACT", 570, false},  // ICU Link - Australia/Darwin
        {"AET", 600, true},   // ICU Link - Australia/Sydney
        {"SST", 660, false},  // ICU Link - Pacific/Guadalcanal
        {"NST", 720, true},   // ICU Link - Pacific/Auckland
        {"MIT", 780, false},  // ICU Link - Pacific/Apia

        {"Etc/Unknown", 0, false},  // CLDR

        {"SystemV/AST4ADT", -240, true},
        {"SystemV/EST5EDT", -300, true},
        {"SystemV/CST6CDT", -360, true},
        {"SystemV/MST7MDT", -420, true},
        {"SystemV/PST8PDT", -480, true},
        {"SystemV/YST9YDT", -540, true},
        {"SystemV/AST4", -240, false},
        {"SystemV/EST5", -300, false},
        {"SystemV/CST6", -360, false},
        {"SystemV/MST7", -420, false},
        {"SystemV/PST8", -480, false},
        {"SystemV/YST9", -540, false},
        {"SystemV/HST10", -600, false},

        {"",0,false}
    };

    for(i=0;kReferenceList[i].id[0];i++) {
        UnicodeString itsID(kReferenceList[i].id);
        UBool ok = true;
        // Check existence.
        TimeZone *tz = TimeZone::createTimeZone(itsID);
        if (!tz || (kReferenceList[i].offset != 0 && *tz == *TimeZone::getGMT())) {
            errln("FAIL: Time Zone " + itsID + " does not exist!");
            continue;
        }

        // Check daylight usage.
        UBool usesDaylight = tz->useDaylightTime();
        if (usesDaylight != kReferenceList[i].daylight) {
            if (!isDevelopmentBuild) {
                logln("Warning: Time Zone " + itsID + " use daylight is " +
                      (usesDaylight?"true":"false") +
                      " but it should be " +
                      ((kReferenceList[i].daylight)?"true":"false"));
            } else if (!(itsID==UnicodeString(u"ART",-1) && logKnownIssue("ICU-22436", "Wrong DST status for time zone ART"))) {
                dataerrln("FAIL: Time Zone " + itsID + " use daylight is " +
                      (usesDaylight?"true":"false") +
                      " but it should be " +
                      ((kReferenceList[i].daylight)?"true":"false"));
            }
            ok = false;
        }

        // Check offset
        int32_t offsetInMinutes = tz->getRawOffset()/60000;
        if (offsetInMinutes != kReferenceList[i].offset) {
            if (!isDevelopmentBuild) {
                logln("FAIL: Time Zone " + itsID + " raw offset is " +
                      offsetInMinutes +
                      " but it should be " + kReferenceList[i].offset);
            } else {
                dataerrln("FAIL: Time Zone " + itsID + " raw offset is " +
                      offsetInMinutes +
                      " but it should be " + kReferenceList[i].offset);
            }
            ok = false;
        }

        if (ok) {
            logln("OK: " + itsID +
                  " useDaylightTime() & getRawOffset() as expected");
        }
        delete tz;
    }


    // OK now test compat
    logln("Testing for compatibility zones");

    const char* compatibilityMap[] = {
        // This list is copied from tz.alias.  If tz.alias
        // changes, this list must be updated.  Current as of Mar 2007
        "ACT", "Australia/Darwin",
        "AET", "Australia/Sydney",
        "AGT", "America/Buenos_Aires",
        "ART", "Africa/Cairo",
        "AST", "America/Anchorage",
        "BET", "America/Sao_Paulo",
        "BST", "Asia/Dhaka", // # spelling changed in 2000h; was Asia/Dacca
        "CAT", "Africa/Maputo",
        "CNT", "America/St_Johns",
        "CST", "America/Chicago",
        "CTT", "Asia/Shanghai",
        "EAT", "Africa/Addis_Ababa",
        "ECT", "Europe/Paris",
        // EET Europe/Istanbul # EET is a standard UNIX zone
        // "EST", "America/New_York", # Defined as -05:00
        // "HST", "Pacific/Honolulu", # Defined as -10:00
        "IET", "America/Indianapolis",
        "IST", "Asia/Calcutta",
        "JST", "Asia/Tokyo",
        // MET Asia/Tehran # MET is a standard UNIX zone
        "MIT", "Pacific/Apia",
        // "MST", "America/Denver", # Defined as -07:00
        "NET", "Asia/Yerevan",
        "NST", "Pacific/Auckland",
        "PLT", "Asia/Karachi",
        "PNT", "America/Phoenix",
        "PRT", "America/Puerto_Rico",
        "PST", "America/Los_Angeles",
        "SST", "Pacific/Guadalcanal",
        "UTC", "Etc/GMT",
        "VST", "Asia/Saigon",
         "","",""
    };

    for (i=0;*compatibilityMap[i];i+=2) {
        UnicodeString itsID;

        const char *zone1 = compatibilityMap[i];
        const char *zone2 = compatibilityMap[i+1];

        TimeZone *tz1 = TimeZone::createTimeZone(zone1);
        TimeZone *tz2 = TimeZone::createTimeZone(zone2);

        if (!tz1) {
            errln(UnicodeString("FAIL: Could not find short ID zone ") + zone1);
        }
        if (!tz2) {
            errln(UnicodeString("FAIL: Could not find long ID zone ") + zone2);
        }

        if (tz1 && tz2) {
            // make NAME same so comparison will only look at the rest
            tz2->setID(tz1->getID(itsID));

            if (*tz1 != *tz2) {
                errln("FAIL: " + UnicodeString(zone1) +
                      " != " + UnicodeString(zone2));
            } else {
                logln("OK: " + UnicodeString(zone1) +
                      " == " + UnicodeString(zone2));
            }
        }

        delete tz1;
        delete tz2;
    }
}


/**
 * Utility function for TestCustomParse
 */
UnicodeString& TimeZoneTest::formatOffset(int32_t offset, UnicodeString &rv) {
    rv.remove();
    char16_t sign = 0x002B;
    if (offset < 0) {
        sign = 0x002D;
        offset = -offset;
    }

    int32_t s = offset % 60;
    offset /= 60;
    int32_t m = offset % 60;
    int32_t h = offset / 60;

    rv += (char16_t)(sign);
    if (h >= 10) {
        rv += (char16_t)(0x0030 + (h/10));
    } else {
        rv += (char16_t)0x0030;
    }
    rv += (char16_t)(0x0030 + (h%10));

    rv += (char16_t)0x003A; /* ':' */
    if (m >= 10) {
        rv += (char16_t)(0x0030 + (m/10));
    } else {
        rv += (char16_t)0x0030;
    }
    rv += (char16_t)(0x0030 + (m%10));

    if (s) {
        rv += (char16_t)0x003A; /* ':' */
        if (s >= 10) {
            rv += (char16_t)(0x0030 + (s/10));
        } else {
            rv += (char16_t)0x0030;
        }
        rv += (char16_t)(0x0030 + (s%10));
    }
    return rv;
}

/**
 * Utility function for TestCustomParse, generating time zone ID
 * string for the give offset.
 */
UnicodeString& TimeZoneTest::formatTZID(int32_t offset, UnicodeString &rv) {
    rv.remove();
    char16_t sign = 0x002B;
    if (offset < 0) {
        sign = 0x002D;
        offset = -offset;
    }

    int32_t s = offset % 60;
    offset /= 60;
    int32_t m = offset % 60;
    int32_t h = offset / 60;

    rv += "GMT";
    rv += (char16_t)(sign);
    if (h >= 10) {
        rv += (char16_t)(0x0030 + (h/10));
    } else {
        rv += (char16_t)0x0030;
    }
    rv += (char16_t)(0x0030 + (h%10));
    rv += (char16_t)0x003A;
    if (m >= 10) {
        rv += (char16_t)(0x0030 + (m/10));
    } else {
        rv += (char16_t)0x0030;
    }
    rv += (char16_t)(0x0030 + (m%10));

    if (s) {
        rv += (char16_t)0x003A;
        if (s >= 10) {
            rv += (char16_t)(0x0030 + (s/10));
        } else {
            rv += (char16_t)0x0030;
        }
        rv += (char16_t)(0x0030 + (s%10));
    }
    return rv;
}

/**
 * As part of the VM fix (see CCC approved RFE 4028006, bug
 * 4044013), TimeZone.getTimeZone() has been modified to recognize
 * generic IDs of the form GMT[+-]hh:mm, GMT[+-]hhmm, and
 * GMT[+-]hh.  Test this behavior here.
 *
 * @bug 4044013
 */
void TimeZoneTest::TestCustomParse()
{
    int32_t i;
    const int32_t kUnparseable = 604800; // the number of seconds in a week. More than any offset should be.

    struct
    {
        const char16_t *customId;
        int32_t expectedOffset;
    }
    kData[] =
    {
        // ID        Expected offset in seconds
        {u"GMT",       kUnparseable},   //Isn't custom. [returns normal GMT]
        {u"GMT-YOUR.AD.HERE", kUnparseable},
        {u"GMT0",      kUnparseable},
        {u"GMT+0",     (0)},
        {u"GMT+1",     (1*60*60)},
        {u"GMT-0030",  (-30*60)},
        {u"GMT+15:99", kUnparseable},
        {u"GMT+",      kUnparseable},
        {u"GMT-",      kUnparseable},
        {u"GMT+0:",    kUnparseable},
        {u"GMT-:",     kUnparseable},
        {u"GMT-YOUR.AD.HERE",    kUnparseable},
        {u"GMT+0010",  (10*60)}, // Interpret this as 00:10
        {u"GMT-10",    (-10*60*60)},
        {u"GMT+30",    kUnparseable},
        {u"GMT-3:30",  (-(3*60+30)*60)},
        {u"GMT-230",   (-(2*60+30)*60)},
        {u"GMT+05:13:05",    ((5*60+13)*60+5)},
        {u"GMT-71023",       (-((7*60+10)*60+23))},
        {u"GMT+01:23:45:67", kUnparseable},
        {u"GMT+01:234",      kUnparseable},
        {u"GMT-2:31:123",    kUnparseable},
        {u"GMT+3:75",        kUnparseable},
        {u"GMT-01010101",    kUnparseable},
        {u"GMT-4E58",        kUnparseable}, // ICU-22637
        {u"GMT-4e58",        kUnparseable}, // ICU-22637
        {u"GMT-1E01",        kUnparseable}, // ICU-22637
        {u"GMT-2E01",        kUnparseable}, // ICU-22637
        {u"GMT-2e01",        kUnparseable}, // ICU-22637
        {u"GMT-9e02",        kUnparseable}, // ICU-22637
        {u"GMT-1e03",        kUnparseable}, // ICU-22637
        {u"GMT-2e03",        kUnparseable}, // ICU-22637
        {u"GMT-500M",        kUnparseable}, // ICU-22637
        {u"GMT-500T",        kUnparseable}, // ICU-22637
        {u"GMT-9E00",        kUnparseable}, // ICU-22637
        {u"GMT-0X0F",        kUnparseable}, // ICU-22637
        {u"GMT-0x0F",        kUnparseable}, // ICU-22637
        {u"GMT-0x12",        kUnparseable}, // ICU-22637
        {u"GMT-B111",        kUnparseable}, // ICU-22637
        {u"GMT-b111",        kUnparseable}, // ICU-22637
        {u"GMT-0b11",        kUnparseable}, // ICU-22637
        {u"GMT-๑๒",          (-((12*60)*60))}, // ICU-22637
        {u"GMT-๑๒:๓๔",       (-((12*60+34)*60))}, // ICU-22637
        {u"GMT+๑๒:๓๔:๕๖",    ((12*60+34)*60+56)}, // ICU-22637
        {nullptr, 0}
    };

    for (i = 0; kData[i].customId != nullptr; i++) {
        UnicodeString id(kData[i].customId);
        int32_t exp = kData[i].expectedOffset;
        TimeZone *zone = TimeZone::createTimeZone(id);
        UnicodeString   itsID, temp;

        OlsonTimeZone *ozone = dynamic_cast<OlsonTimeZone *>(zone);
        if (ozone != nullptr) {
            logln(id + " -> Olson time zone");
            ozone->operator=(*ozone);  // self-assignment should be a no-op
        } else {
            zone->getID(itsID);
            int32_t ioffset = zone->getRawOffset()/1000;
            UnicodeString offset, expectedID;
            formatOffset(ioffset, offset);
            formatTZID(ioffset, expectedID);
            logln(id + " -> " + itsID + " " + offset);
            if (exp == kUnparseable && itsID != UCAL_UNKNOWN_ZONE_ID) {
                errln("Expected parse failure for " + id +
                      ", got offset of " + offset +
                      ", id " + itsID);
            }
            // JDK 1.3 creates custom zones with the ID "Custom"
            // JDK 1.4 creates custom zones with IDs of the form "GMT+02:00"
            // ICU creates custom zones with IDs of the form "GMT+02:00"
            else if (exp != kUnparseable && (ioffset != exp || itsID != expectedID)) {
                dataerrln("Expected offset of " + formatOffset(exp, temp) +
                      ", id " + expectedID +
                      ", for " + id +
                      ", got offset of " + offset +
                      ", id " + itsID);
            }
        }
        delete zone;
    }
}

void
TimeZoneTest::TestAliasedNames()
{
    struct {
        const char *from;
        const char *to;
    } kData[] = {
        /* Generated by org.unicode.cldr.tool.CountItems */

        /* zoneID, canonical zoneID */
        {"Africa/Asmara", "Africa/Addis_Ababa"},
        {"Africa/Timbuktu", "Africa/Abidjan"},
        {"America/Argentina/Buenos_Aires", "America/Buenos_Aires"},
        {"America/Argentina/Catamarca", "America/Catamarca"},
        {"America/Argentina/ComodRivadavia", "America/Catamarca"},
        {"America/Argentina/Cordoba", "America/Cordoba"},
        {"America/Argentina/Jujuy", "America/Jujuy"},
        {"America/Argentina/Mendoza", "America/Mendoza"},
        {"America/Atka", "America/Adak"},
        {"America/Ensenada", "America/Tijuana"},
        {"America/Fort_Wayne", "America/Indianapolis"},
        {"America/Indiana/Indianapolis", "America/Indianapolis"},
        {"America/Kentucky/Louisville", "America/Louisville"},
        {"America/Knox_IN", "America/Indiana/Knox"},
        {"America/Porto_Acre", "America/Rio_Branco"},
        {"America/Rosario", "America/Cordoba"},
        {"America/Shiprock", "America/Denver"},
        {"America/Virgin", "America/Anguilla"},
        {"Antarctica/South_Pole", "Antarctica/McMurdo"},
        {"Asia/Ashkhabad", "Asia/Ashgabat"},
        {"Asia/Chongqing", "Asia/Shanghai"},
        {"Asia/Chungking", "Asia/Shanghai"},
        {"Asia/Dacca", "Asia/Dhaka"},
        {"Asia/Harbin", "Asia/Shanghai"},
        {"Asia/Ho_Chi_Minh", "Asia/Saigon"},
        {"Asia/Istanbul", "Europe/Istanbul"},
        {"Asia/Kashgar", "Asia/Urumqi"},
        {"Asia/Kathmandu", "Asia/Katmandu"},
        {"Asia/Kolkata", "Asia/Calcutta"},
        {"Asia/Macao", "Asia/Macau"},
        {"Asia/Tel_Aviv", "Asia/Jerusalem"},
        {"Asia/Thimbu", "Asia/Thimphu"},
        {"Asia/Ujung_Pandang", "Asia/Makassar"},
        {"Asia/Ulan_Bator", "Asia/Ulaanbaatar"},
        {"Atlantic/Faroe", "Atlantic/Faeroe"},
        {"Atlantic/Jan_Mayen", "Arctic/Longyearbyen"},
        {"Australia/ACT", "Australia/Sydney"},
        {"Australia/Canberra", "Australia/Sydney"},
        {"Australia/LHI", "Australia/Lord_Howe"},
        {"Australia/NSW", "Australia/Sydney"},
        {"Australia/North", "Australia/Darwin"},
        {"Australia/Queensland", "Australia/Brisbane"},
        {"Australia/South", "Australia/Adelaide"},
        {"Australia/Tasmania", "Australia/Hobart"},
        {"Australia/Victoria", "Australia/Melbourne"},
        {"Australia/West", "Australia/Perth"},
        {"Australia/Yancowinna", "Australia/Broken_Hill"},
        {"Brazil/Acre", "America/Rio_Branco"},
        {"Brazil/DeNoronha", "America/Noronha"},
        {"Brazil/East", "America/Sao_Paulo"},
        {"Brazil/West", "America/Manaus"},
        {"Canada/Atlantic", "America/Halifax"},
        {"Canada/Central", "America/Winnipeg"},
        {"Canada/East-Saskatchewan", "America/Regina"},
        {"Canada/Eastern", "America/Toronto"},
        {"Canada/Mountain", "America/Edmonton"},
        {"Canada/Newfoundland", "America/St_Johns"},
        {"Canada/Pacific", "America/Vancouver"},
        {"Canada/Saskatchewan", "America/Regina"},
        {"Canada/Yukon", "America/Whitehorse"},
        {"Chile/Continental", "America/Santiago"},
        {"Chile/EasterIsland", "Pacific/Easter"},
        {"Cuba", "America/Havana"},
        {"EST", "Etc/GMT+5"},
        {"Egypt", "Africa/Cairo"},
        {"Eire", "Europe/Dublin"},
        {"Etc/GMT+0", "Etc/GMT"},
        {"Etc/GMT-0", "Etc/GMT"},
        {"Etc/GMT0", "Etc/GMT"},
        {"Etc/Greenwich", "Etc/GMT"},
        {"Etc/UCT", "Etc/UTC"},
        {"Etc/Universal", "Etc/UTC"},
        {"Etc/Zulu", "Etc/UTC"},
        {"Europe/Belfast", "Europe/London"},
        {"Europe/Nicosia", "Asia/Nicosia"},
        {"Europe/Tiraspol", "Europe/Chisinau"},
        {"GB", "Europe/London"},
        {"GB-Eire", "Europe/London"},
        {"GMT", "Etc/GMT"},
        {"GMT+0", "Etc/GMT"},
        {"GMT-0", "Etc/GMT"},
        {"GMT0", "Etc/GMT"},
        {"Greenwich", "Etc/GMT"},
        {"HST", "Etc/GMT+10"},
        {"Hongkong", "Asia/Hong_Kong"},
        {"Iceland", "Atlantic/Reykjavik"},
        {"Iran", "Asia/Tehran"},
        {"Israel", "Asia/Jerusalem"},
        {"Jamaica", "America/Jamaica"},
        {"Japan", "Asia/Tokyo"},
        {"Kwajalein", "Pacific/Kwajalein"},
        {"Libya", "Africa/Tripoli"},
        {"MST", "Etc/GMT+7"},
        {"Mexico/BajaNorte", "America/Tijuana"},
        {"Mexico/BajaSur", "America/Mazatlan"},
        {"Mexico/General", "America/Mexico_City"},
        {"NZ", "Antarctica/McMurdo"},
        {"NZ-CHAT", "Pacific/Chatham"},
        {"Navajo", "America/Denver"},
        {"PRC", "Asia/Shanghai"},
        {"Pacific/Chuuk", "Pacific/Truk"},
        {"Pacific/Pohnpei", "Pacific/Ponape"},
        {"Pacific/Samoa", "Pacific/Midway"},
        {"Pacific/Yap", "Pacific/Truk"},
        {"Poland", "Europe/Warsaw"},
        {"Portugal", "Europe/Lisbon"},
        {"ROC", "Asia/Taipei"},
        {"ROK", "Asia/Seoul"},
        {"Singapore", "Asia/Singapore"},
        {"SystemV/AST4", "Etc/GMT+4"},
        {"SystemV/CST6", "Etc/GMT+6"},
        {"SystemV/EST5", "Etc/GMT+5"},
        {"SystemV/HST10", "Etc/GMT+10"},
        {"SystemV/MST7", "Etc/GMT+7"},
        {"SystemV/PST8", "Etc/GMT+8"},
        {"SystemV/YST9", "Etc/GMT+9"},
        {"Turkey", "Europe/Istanbul"},
        {"UCT", "Etc/UTC"},
        {"US/Alaska", "America/Anchorage"},
        {"US/Aleutian", "America/Adak"},
        {"US/Arizona", "America/Phoenix"},
        {"US/Central", "America/Chicago"},
        {"US/East-Indiana", "America/Indianapolis"},
        {"US/Eastern", "America/New_York"},
        {"US/Hawaii", "Pacific/Honolulu"},
        {"US/Indiana-Starke", "America/Indiana/Knox"},
        {"US/Michigan", "America/Detroit"},
        {"US/Mountain", "America/Denver"},
        {"US/Pacific", "America/Los_Angeles"},
        {"US/Pacific-New", "America/Los_Angeles"},
        {"US/Samoa", "Pacific/Midway"},
        {"UTC", "Etc/UTC"},
        {"Universal", "Etc/UTC"},
        {"W-SU", "Europe/Moscow"},
        {"Zulu", "Etc/UTC"},
        /* Total: 136 */
    };

    TimeZone::EDisplayType styles[] = { TimeZone::SHORT, TimeZone::LONG };
    UBool useDst[] = { false, true };
    int32_t noLoc = uloc_countAvailable();

    int32_t i, j, k, loc;
    UnicodeString fromName, toName;
    TimeZone *from = nullptr, *to = nullptr;
    for(i = 0; i < UPRV_LENGTHOF(kData); i++) {
        from = TimeZone::createTimeZone(kData[i].from);
        to = TimeZone::createTimeZone(kData[i].to);
        if(!from->hasSameRules(*to)) {
            errln("different at %i\n", i);
        }
        if(!quick) {
            for(loc = 0; loc < noLoc; loc++) {
                const char* locale = uloc_getAvailable(loc); 
                for(j = 0; j < UPRV_LENGTHOF(styles); j++) {
                    for(k = 0; k < UPRV_LENGTHOF(useDst); k++) {
                        fromName.remove();
                        toName.remove();
                        from->getDisplayName(useDst[k], styles[j],locale, fromName);
                        to->getDisplayName(useDst[k], styles[j], locale, toName);
                        if(fromName.compare(toName) != 0) {
                            errln("Fail: Expected "+toName+" but got " + prettify(fromName) 
                                + " for locale: " + locale + " index: "+ loc 
                                + " to id "+ kData[i].to
                                + " from id " + kData[i].from);
                        }
                    }
                }
            }
        } else {
            fromName.remove();
            toName.remove();
            from->getDisplayName(fromName);
            to->getDisplayName(toName);
            if(fromName.compare(toName) != 0) {
                errln("Fail: Expected "+toName+" but got " + fromName);
            }
        }
        delete from;
        delete to;
    }
}

/**
 * Test the basic functionality of the getDisplayName() API.
 *
 * @bug 4112869
 * @bug 4028006
 *
 * See also API change request A41.
 *
 * 4/21/98 - make smarter, so the test works if the ext resources
 * are present or not.
 */
void
TimeZoneTest::TestDisplayName()
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t i;
    TimeZone *zone = TimeZone::createTimeZone("PST");
    UnicodeString name;
    zone->getDisplayName(Locale::getEnglish(), name);
    logln("PST->" + name);
    if (name.compare("Pacific Standard Time") != 0)
        dataerrln("Fail: Expected \"Pacific Standard Time\" but got " + name);

    //*****************************************************************
    // THE FOLLOWING LINES MUST BE UPDATED IF THE LOCALE DATA CHANGES
    // THE FOLLOWING LINES MUST BE UPDATED IF THE LOCALE DATA CHANGES
    // THE FOLLOWING LINES MUST BE UPDATED IF THE LOCALE DATA CHANGES
    //*****************************************************************
    struct
    {
        UBool useDst;
        TimeZone::EDisplayType style;
        const char *expect;
    } kData[] = {
        {false, TimeZone::SHORT, "PST"},
        {true,  TimeZone::SHORT, "PDT"},
        {false, TimeZone::LONG,  "Pacific Standard Time"},
        {true,  TimeZone::LONG,  "Pacific Daylight Time"},

        {false, TimeZone::SHORT_GENERIC, "PT"},
        {true,  TimeZone::SHORT_GENERIC, "PT"},
        {false, TimeZone::LONG_GENERIC,  "Pacific Time"},
        {true,  TimeZone::LONG_GENERIC,  "Pacific Time"},

        {false, TimeZone::SHORT_GMT, "-0800"},
        {true,  TimeZone::SHORT_GMT, "-0700"},
        {false, TimeZone::LONG_GMT,  "GMT-08:00"},
        {true,  TimeZone::LONG_GMT,  "GMT-07:00"},

        {false, TimeZone::SHORT_COMMONLY_USED, "PST"},
        {true,  TimeZone::SHORT_COMMONLY_USED, "PDT"},
        {false, TimeZone::GENERIC_LOCATION,  "Los Angeles Time"},
        {true,  TimeZone::GENERIC_LOCATION,  "Los Angeles Time"},

        {false, TimeZone::LONG, ""}
    };

    for (i=0; kData[i].expect[0] != '\0'; i++)
    {
        name.remove();
        name = zone->getDisplayName(kData[i].useDst,
                                   kData[i].style,
                                   Locale::getEnglish(), name);
        if (name.compare(kData[i].expect) != 0)
            dataerrln("Fail: Expected " + UnicodeString(kData[i].expect) + "; got " + name);
        logln("PST [with options]->" + name);
    }
    for (i=0; kData[i].expect[0] != '\0'; i++)
    {
        name.remove();
        name = zone->getDisplayName(kData[i].useDst,
                                   kData[i].style, name);
        if (name.compare(kData[i].expect) != 0)
            dataerrln("Fail: Expected " + UnicodeString(kData[i].expect) + "; got " + name);
        logln("PST [with options]->" + name);
    }


    // Make sure that we don't display the DST name by constructing a fake
    // PST zone that has DST all year long.
    SimpleTimeZone *zone2 = new SimpleTimeZone(0, "PST");

    zone2->setStartRule(UCAL_JANUARY, 1, 0, 0, status);
    zone2->setEndRule(UCAL_DECEMBER, 31, 0, 0, status);

    UnicodeString inDaylight;
    if (zone2->inDaylightTime(UDate(0), status)) {
        inDaylight = UnicodeString("true");
    } else {
        inDaylight = UnicodeString("false");
    }
    logln(UnicodeString("Modified PST inDaylightTime->") + inDaylight );
    if(U_FAILURE(status))
    {
        dataerrln("Some sort of error..." + UnicodeString(u_errorName(status))); // REVISIT
    }
    name.remove();
    name = zone2->getDisplayName(Locale::getEnglish(),name);
    logln("Modified PST->" + name);
    if (name.compare("Pacific Standard Time") != 0)
        dataerrln("Fail: Expected \"Pacific Standard Time\"");

    // Make sure we get the default display format for Locales
    // with no display name data.
    Locale mt_MT("mt_MT");
    name.remove();
    name = zone->getDisplayName(mt_MT,name);
    //*****************************************************************
    // THE FOLLOWING LINE MUST BE UPDATED IF THE LOCALE DATA CHANGES
    // THE FOLLOWING LINE MUST BE UPDATED IF THE LOCALE DATA CHANGES
    // THE FOLLOWING LINE MUST BE UPDATED IF THE LOCALE DATA CHANGES
    //*****************************************************************
    logln("PST(mt_MT)->" + name);

    // *** REVISIT SRL how in the world do I check this? looks java specific.
    // Now be smart -- check to see if zh resource is even present.
    // If not, we expect the en fallback behavior.
    ResourceBundle enRB(nullptr,
                            Locale::getEnglish(), status);
    if(U_FAILURE(status))
        dataerrln("Couldn't get ResourceBundle for en - %s", u_errorName(status));

    ResourceBundle mtRB(nullptr,
                         mt_MT, status);
    //if(U_FAILURE(status))
    //    errln("Couldn't get ResourceBundle for mt_MT");

    UBool noZH = U_FAILURE(status);

    if (noZH) {
        logln("Warning: Not testing the mt_MT behavior because resource is absent");
        if (name != "Pacific Standard Time")
            dataerrln("Fail: Expected Pacific Standard Time");
    }


    if      (name.compare("GMT-08:00") &&
             name.compare("GMT-8:00") &&
             name.compare("GMT-0800") &&
             name.compare("GMT-800")) {
      dataerrln(UnicodeString("Fail: Expected GMT-08:00 or something similar for PST in mt_MT but got ") + name );
        dataerrln("************************************************************");
        dataerrln("THE ABOVE FAILURE MAY JUST MEAN THE LOCALE DATA HAS CHANGED");
        dataerrln("************************************************************");
    }

    // Now try a non-existent zone
    delete zone2;
    zone2 = new SimpleTimeZone(90*60*1000, "xyzzy");
    name.remove();
    name = zone2->getDisplayName(Locale::getEnglish(),name);
    logln("GMT+90min->" + name);
    if (name.compare("GMT+01:30") &&
        name.compare("GMT+1:30") &&
        name.compare("GMT+0130") &&
        name.compare("GMT+130"))
        dataerrln("Fail: Expected GMT+01:30 or something similar");
    name.truncate(0);
    zone2->getDisplayName(name);
    logln("GMT+90min->" + name);
    if (name.compare("GMT+01:30") &&
        name.compare("GMT+1:30") &&
        name.compare("GMT+0130") &&
        name.compare("GMT+130"))
        dataerrln("Fail: Expected GMT+01:30 or something similar");
    // clean up
    delete zone;
    delete zone2;
}

/**
 * @bug 4107276
 */
void
TimeZoneTest::TestDSTSavings()
{
    UErrorCode status = U_ZERO_ERROR;
    // It might be better to find a way to integrate this test into the main TimeZone
    // tests above, but I don't have time to figure out how to do this (or if it's
    // even really a good idea).  Let's consider that a future.  --rtg 1/27/98
    SimpleTimeZone *tz = new SimpleTimeZone(-5 * U_MILLIS_PER_HOUR, "dstSavingsTest",
                                           UCAL_MARCH, 1, 0, 0, UCAL_SEPTEMBER, 1, 0, 0,
                                           (int32_t)(0.5 * U_MILLIS_PER_HOUR), status);
    if(U_FAILURE(status))
        errln("couldn't create TimeZone");

    if (tz->getRawOffset() != -5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("Got back a raw offset of ") + (tz->getRawOffset() / U_MILLIS_PER_HOUR) +
              " hours instead of -5 hours.");
    if (!tz->useDaylightTime())
        errln("Test time zone should use DST but claims it doesn't.");
    if (tz->getDSTSavings() != 0.5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("Set DST offset to 0.5 hour, but got back ") + (tz->getDSTSavings() /
                                                             U_MILLIS_PER_HOUR) + " hours instead.");

    int32_t offset = tz->getOffset(GregorianCalendar::AD, 1998, UCAL_JANUARY, 1,
                              UCAL_THURSDAY, 10 * U_MILLIS_PER_HOUR,status);
    if (offset != -5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10 AM, 1/1/98 should have been -5 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    offset = tz->getOffset(GregorianCalendar::AD, 1998, UCAL_JUNE, 1, UCAL_MONDAY,
                          10 * U_MILLIS_PER_HOUR,status);
    if (offset != -4.5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10 AM, 6/1/98 should have been -4.5 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    tz->setDSTSavings(U_MILLIS_PER_HOUR, status);
    offset = tz->getOffset(GregorianCalendar::AD, 1998, UCAL_JANUARY, 1,
                          UCAL_THURSDAY, 10 * U_MILLIS_PER_HOUR,status);
    if (offset != -5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10 AM, 1/1/98 should have been -5 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    offset = tz->getOffset(GregorianCalendar::AD, 1998, UCAL_JUNE, 1, UCAL_MONDAY,
                          10 * U_MILLIS_PER_HOUR,status);
    if (offset != -4 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10 AM, 6/1/98 (with a 1-hour DST offset) should have been -4 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    delete tz;
}

/**
 * @bug 4107570
 */
void
TimeZoneTest::TestAlternateRules()
{
    // Like TestDSTSavings, this test should probably be integrated somehow with the main
    // test at the top of this class, but I didn't have time to figure out how to do that.
    //                      --rtg 1/28/98

    SimpleTimeZone tz(-5 * U_MILLIS_PER_HOUR, "alternateRuleTest");

    // test the day-of-month API
    UErrorCode status = U_ZERO_ERROR;
    tz.setStartRule(UCAL_MARCH, 10, 12 * U_MILLIS_PER_HOUR, status);
    if(U_FAILURE(status))
        errln("tz.setStartRule failed");
    tz.setEndRule(UCAL_OCTOBER, 20, 12 * U_MILLIS_PER_HOUR, status);
    if(U_FAILURE(status))
        errln("tz.setStartRule failed");

    int32_t offset = tz.getOffset(GregorianCalendar::AD, 1998, UCAL_MARCH, 5,
                              UCAL_THURSDAY, 10 * U_MILLIS_PER_HOUR,status);
    if (offset != -5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10AM, 3/5/98 should have been -5 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    offset = tz.getOffset(GregorianCalendar::AD, 1998, UCAL_MARCH, 15,
                          UCAL_SUNDAY, 10 * millisPerHour,status);
    if (offset != -4 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10AM, 3/15/98 should have been -4 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    offset = tz.getOffset(GregorianCalendar::AD, 1998, UCAL_OCTOBER, 15,
                          UCAL_THURSDAY, 10 * millisPerHour,status);
    if (offset != -4 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10AM, 10/15/98 should have been -4 hours, but we got ")              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    offset = tz.getOffset(GregorianCalendar::AD, 1998, UCAL_OCTOBER, 25,
                          UCAL_SUNDAY, 10 * millisPerHour,status);
    if (offset != -5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10AM, 10/25/98 should have been -5 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    // test the day-of-week-after-day-in-month API
    tz.setStartRule(UCAL_MARCH, 10, UCAL_FRIDAY, 12 * millisPerHour, true, status);
    if(U_FAILURE(status))
        errln("tz.setStartRule failed");
    tz.setEndRule(UCAL_OCTOBER, 20, UCAL_FRIDAY, 12 * millisPerHour, false, status);
    if(U_FAILURE(status))
        errln("tz.setStartRule failed");

    offset = tz.getOffset(GregorianCalendar::AD, 1998, UCAL_MARCH, 11,
                          UCAL_WEDNESDAY, 10 * millisPerHour,status);
    if (offset != -5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10AM, 3/11/98 should have been -5 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    offset = tz.getOffset(GregorianCalendar::AD, 1998, UCAL_MARCH, 14,
                          UCAL_SATURDAY, 10 * millisPerHour,status);
    if (offset != -4 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10AM, 3/14/98 should have been -4 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    offset = tz.getOffset(GregorianCalendar::AD, 1998, UCAL_OCTOBER, 15,
                          UCAL_THURSDAY, 10 * millisPerHour,status);
    if (offset != -4 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10AM, 10/15/98 should have been -4 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");

    offset = tz.getOffset(GregorianCalendar::AD, 1998, UCAL_OCTOBER, 17,
                          UCAL_SATURDAY, 10 * millisPerHour,status);
    if (offset != -5 * U_MILLIS_PER_HOUR)
        errln(UnicodeString("The offset for 10AM, 10/17/98 should have been -5 hours, but we got ")
              + (offset / U_MILLIS_PER_HOUR) + " hours.");
}

void TimeZoneTest::TestFractionalDST() {
    const char* tzName = "Australia/Lord_Howe"; // 30 min offset
    TimeZone* tz_icu = TimeZone::createTimeZone(tzName);
	int dst_icu = tz_icu->getDSTSavings();
    UnicodeString id;
    int32_t expected = 1800000;
	if (expected != dst_icu) {
	    dataerrln(UnicodeString("java reports dst savings of ") + expected +
	        " but icu reports " + dst_icu + 
	        " for tz " + tz_icu->getID(id));
	} else {
	    logln(UnicodeString("both java and icu report dst savings of ") + expected + " for tz " + tz_icu->getID(id));
	}
    delete tz_icu;
}

/**
 * Test country code support.  Jitterbug 776.
 */
void TimeZoneTest::TestCountries() {
    // Make sure America/Los_Angeles is in the "US" group, and
    // Asia/Tokyo isn't.  Vice versa for the "JP" group.
    UErrorCode ec = U_ZERO_ERROR;
    int32_t n;
    StringEnumeration* s = TimeZone::createEnumerationForRegion("US", ec);
    if (U_FAILURE(ec)) {
        dataerrln("Unable to create TimeZone enumeration for US");
        return;
    }
    n = s->count(ec);
    UBool la = false, tokyo = false;
    UnicodeString laZone("America/Los_Angeles", "");
    UnicodeString tokyoZone("Asia/Tokyo", "");
    int32_t i;

    if (n <= 0) {
        dataerrln("FAIL: TimeZone::createEnumeration() returned nothing");
        return;
    }
    for (i=0; i<n; ++i) {
        const UnicodeString* id = s->snext(ec);
        if (*id == (laZone)) {
            la = true;
        }
        if (*id == (tokyoZone)) {
            tokyo = true;
        }
    }
    if (!la || tokyo) {
        errln("FAIL: " + laZone + " in US = " + la);
        errln("FAIL: " + tokyoZone + " in US = " + tokyo);
    }
    delete s;
    
    s = TimeZone::createEnumerationForRegion("JP", ec);
    if (U_FAILURE(ec)) {
        dataerrln("Unable to create TimeZone enumeration for JP");
        return;
    }
    n = s->count(ec);
    la = false; tokyo = false;
    
    for (i=0; i<n; ++i) {
        const UnicodeString* id = s->snext(ec);
        if (*id == (laZone)) {
            la = true;
        }
        if (*id == (tokyoZone)) {
            tokyo = true;
        }
    }
    if (la || !tokyo) {
        errln("FAIL: " + laZone + " in JP = " + la);
        errln("FAIL: " + tokyoZone + " in JP = " + tokyo);
    }
    StringEnumeration* s1 = TimeZone::createEnumerationForRegion("US", ec);
    StringEnumeration* s2 = TimeZone::createEnumerationForRegion("US", ec);
    if (U_FAILURE(ec)) {
        dataerrln("Unable to create TimeZone enumeration for US");
        return;
    }
    for(i=0;i<n;++i){
        const UnicodeString* id1 = s1->snext(ec);
        if(id1==nullptr || U_FAILURE(ec)){
            errln("Failed to fetch next from TimeZone enumeration. Length returned : %i Current Index: %i", n,i);
        }
        TimeZone* tz1 = TimeZone::createTimeZone(*id1);
        for(int j=0; j<n;++j){
            const UnicodeString* id2 = s2->snext(ec);
            if(id2==nullptr || U_FAILURE(ec)){
                errln("Failed to fetch next from TimeZone enumeration. Length returned : %i Current Index: %i", n,i);
            }
            TimeZone* tz2 = TimeZone::createTimeZone(*id2);
            if(tz1->hasSameRules(*tz2)){
                logln("ID1 : " + *id1+" == ID2 : " +*id2);
            }
            delete tz2;
        }
        delete tz1;
    }
    delete s1;
    delete s2;
    delete s;
}

void TimeZoneTest::TestHistorical() {
    const int32_t H = U_MILLIS_PER_HOUR;
    struct {
        const char* id;
        int32_t time; // epoch seconds
        int32_t offset; // total offset (millis)
    } DATA[] = {
        // Add transition points (before/after) as desired to test historical
        // behavior.
        {"America/Los_Angeles", 638963999, -8*H}, // Sun Apr 01 01:59:59 GMT-08:00 1990
        {"America/Los_Angeles", 638964000, -7*H}, // Sun Apr 01 03:00:00 GMT-07:00 1990
        {"America/Los_Angeles", 657104399, -7*H}, // Sun Oct 28 01:59:59 GMT-07:00 1990
        {"America/Los_Angeles", 657104400, -8*H}, // Sun Oct 28 01:00:00 GMT-08:00 1990
        {"America/Goose_Bay", -116445601, -4*H}, // Sun Apr 24 01:59:59 GMT-04:00 1966
        {"America/Goose_Bay", -116445600, -3*H}, // Sun Apr 24 03:00:00 GMT-03:00 1966
        {"America/Goose_Bay", -100119601, -3*H}, // Sun Oct 30 01:59:59 GMT-03:00 1966
        {"America/Goose_Bay", -100119600, -4*H}, // Sun Oct 30 01:00:00 GMT-04:00 1966
        {"America/Goose_Bay", -84391201, -4*H}, // Sun Apr 30 01:59:59 GMT-04:00 1967
        {"America/Goose_Bay", -84391200, -3*H}, // Sun Apr 30 03:00:00 GMT-03:00 1967
        {"America/Goose_Bay", -68670001, -3*H}, // Sun Oct 29 01:59:59 GMT-03:00 1967
        {"America/Goose_Bay", -68670000, -4*H}, // Sun Oct 29 01:00:00 GMT-04:00 1967
        {nullptr, 0, 0}
    };

    for (int32_t i = 0; DATA[i].id != nullptr; ++i) {
        const char* id = DATA[i].id;
        TimeZone *tz = TimeZone::createTimeZone(id);
        UnicodeString s;
        if (tz == nullptr) {
            errln("FAIL: Cannot create %s", id);
        } else if (tz->getID(s) != UnicodeString(id)) {
            dataerrln((UnicodeString)"FAIL: createTimeZone(" + id + ") => " + s);
        } else {
            UErrorCode ec = U_ZERO_ERROR;
            int32_t raw, dst;
            UDate when = (double) DATA[i].time * U_MILLIS_PER_SECOND;
            tz->getOffset(when, false, raw, dst, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: getOffset");
            } else if ((raw+dst) != DATA[i].offset) {
                errln((UnicodeString)"FAIL: " + DATA[i].id + ".getOffset(" +
                      //when + " = " +
                      dateToString(when) + ") => " +
                      raw + ", " + dst);
            } else {
                logln((UnicodeString)"Ok: " + DATA[i].id + ".getOffset(" +
                      //when + " = " +
                      dateToString(when) + ") => " +
                      raw + ", " + dst);
            }
        }
        delete tz;
    }
}

void TimeZoneTest::TestEquivalentIDs() {
    int32_t n = TimeZone::countEquivalentIDs("PST");
    if (n < 2) {
        dataerrln((UnicodeString)"FAIL: countEquivalentIDs(PST) = " + n);
    } else {
        UBool sawLA = false;
        for (int32_t i=0; i<n; ++i) {
            UnicodeString id = TimeZone::getEquivalentID("PST", i);
            logln((UnicodeString)"" + i + " : " + id);
            if (id == UnicodeString("America/Los_Angeles")) {
                sawLA = true;
            }
        }
        if (!sawLA) {
            errln("FAIL: America/Los_Angeles should be in the list");
        }
    }
}

// Test that a transition at the end of February is handled correctly.
void TimeZoneTest::TestFebruary() {
    UErrorCode status = U_ZERO_ERROR;

    // Time zone with daylight savings time from the first Sunday in November
    // to the last Sunday in February.
    // Similar to the new rule for Brazil (Sao Paulo) in tzdata2006n.
    //
    // Note: In tzdata2007h, the rule had changed, so no actual zones uses
    // lastSun in Feb anymore.
    SimpleTimeZone tz1(-3 * U_MILLIS_PER_HOUR,          // raw offset: 3h before (west of) GMT
                       UNICODE_STRING("nov-feb", 7),
                       UCAL_NOVEMBER, 1, UCAL_SUNDAY,   // start: November, first, Sunday
                       0,                               //        midnight wall time
                       UCAL_FEBRUARY, -1, UCAL_SUNDAY,  // end:   February, last, Sunday
                       0,                               //        midnight wall time
                       status);
    if (U_FAILURE(status)) {
        errln("Unable to create the SimpleTimeZone(nov-feb): %s", u_errorName(status));
        return;
    }

    // Now hardcode the same rules as for Brazil in tzdata 2006n, so that
    // we cover the intended code even when in the future zoneinfo hardcodes
    // these transition dates.
    SimpleTimeZone tz2(-3 * U_MILLIS_PER_HOUR,          // raw offset: 3h before (west of) GMT
                       UNICODE_STRING("nov-feb2", 8),
                       UCAL_NOVEMBER, 1, -UCAL_SUNDAY,  // start: November, 1 or after, Sunday
                       0,                               //        midnight wall time
                       UCAL_FEBRUARY, -29, -UCAL_SUNDAY,// end:   February, 29 or before, Sunday
                       0,                               //        midnight wall time
                       status);
    if (U_FAILURE(status)) {
        errln("Unable to create the SimpleTimeZone(nov-feb2): %s", u_errorName(status));
        return;
    }

    // Gregorian calendar with the UTC time zone for getting sample test date/times.
    GregorianCalendar gc(*TimeZone::getGMT(), status);
    if (U_FAILURE(status)) {
        dataerrln("Unable to create the UTC calendar: %s", u_errorName(status));
        return;
    }

    struct {
        // UTC time.
        int32_t year, month, day, hour, minute, second;
        // Expected time zone offset in hours after GMT (negative=before GMT).
        int32_t offsetHours;
    } data[] = {
        { 2006, UCAL_NOVEMBER,  5, 02, 59, 59, -3 },
        { 2006, UCAL_NOVEMBER,  5, 03, 00, 00, -2 },
        { 2007, UCAL_FEBRUARY, 25, 01, 59, 59, -2 },
        { 2007, UCAL_FEBRUARY, 25, 02, 00, 00, -3 },

        { 2007, UCAL_NOVEMBER,  4, 02, 59, 59, -3 },
        { 2007, UCAL_NOVEMBER,  4, 03, 00, 00, -2 },
        { 2008, UCAL_FEBRUARY, 24, 01, 59, 59, -2 },
        { 2008, UCAL_FEBRUARY, 24, 02, 00, 00, -3 },

        { 2008, UCAL_NOVEMBER,  2, 02, 59, 59, -3 },
        { 2008, UCAL_NOVEMBER,  2, 03, 00, 00, -2 },
        { 2009, UCAL_FEBRUARY, 22, 01, 59, 59, -2 },
        { 2009, UCAL_FEBRUARY, 22, 02, 00, 00, -3 },

        { 2009, UCAL_NOVEMBER,  1, 02, 59, 59, -3 },
        { 2009, UCAL_NOVEMBER,  1, 03, 00, 00, -2 },
        { 2010, UCAL_FEBRUARY, 28, 01, 59, 59, -2 },
        { 2010, UCAL_FEBRUARY, 28, 02, 00, 00, -3 }
    };

    TimeZone *timezones[] = { &tz1, &tz2 };

    TimeZone *tz;
    UDate dt;
    int32_t t, i, raw, dst;
    for (t = 0; t < UPRV_LENGTHOF(timezones); ++t) {
        tz = timezones[t];
        for (i = 0; i < UPRV_LENGTHOF(data); ++i) {
            gc.set(data[i].year, data[i].month, data[i].day,
                   data[i].hour, data[i].minute, data[i].second);
            dt = gc.getTime(status);
            if (U_FAILURE(status)) {
                errln("test case %d.%d: bad date/time %04d-%02d-%02d %02d:%02d:%02d",
                      t, i,
                      data[i].year, data[i].month + 1, data[i].day,
                      data[i].hour, data[i].minute, data[i].second);
                status = U_ZERO_ERROR;
                continue;
            }
            tz->getOffset(dt, false, raw, dst, status);
            if (U_FAILURE(status)) {
                errln("test case %d.%d: tz.getOffset(%04d-%02d-%02d %02d:%02d:%02d) fails: %s",
                      t, i,
                      data[i].year, data[i].month + 1, data[i].day,
                      data[i].hour, data[i].minute, data[i].second,
                      u_errorName(status));
                status = U_ZERO_ERROR;
            } else if ((raw + dst) != data[i].offsetHours * U_MILLIS_PER_HOUR) {
                errln("test case %d.%d: tz.getOffset(%04d-%02d-%02d %02d:%02d:%02d) returns %d+%d != %d",
                      t, i,
                      data[i].year, data[i].month + 1, data[i].day,
                      data[i].hour, data[i].minute, data[i].second,
                      raw, dst, data[i].offsetHours * U_MILLIS_PER_HOUR);
            }
        }
    }
}

void TimeZoneTest::TestCanonicalIDAPI() {
    // Bogus input string.
    UnicodeString bogus;
    bogus.setToBogus();
    UnicodeString canonicalID;
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeString *pResult = &TimeZone::getCanonicalID(bogus, canonicalID, ec);
    assertEquals("TimeZone::getCanonicalID(bogus) should fail", (int32_t)U_ILLEGAL_ARGUMENT_ERROR, ec);
    assertTrue("TimeZone::getCanonicalID(bogus) should return the dest string", pResult == &canonicalID);

    // U_FAILURE on input.
    UnicodeString berlin("Europe/Berlin");
    ec = U_MEMORY_ALLOCATION_ERROR;
    pResult = &TimeZone::getCanonicalID(berlin, canonicalID, ec);
    assertEquals("TimeZone::getCanonicalID(failure) should fail", (int32_t)U_MEMORY_ALLOCATION_ERROR, ec);
    assertTrue("TimeZone::getCanonicalID(failure) should return the dest string", pResult == &canonicalID);

    // Valid input should un-bogus the dest string.
    canonicalID.setToBogus();
    ec = U_ZERO_ERROR;
    pResult = &TimeZone::getCanonicalID(berlin, canonicalID, ec);
    assertSuccess("TimeZone::getCanonicalID(bogus dest) should succeed", ec, true);
    assertTrue("TimeZone::getCanonicalID(bogus dest) should return the dest string", pResult == &canonicalID);
    assertFalse("TimeZone::getCanonicalID(bogus dest) should un-bogus the dest string", canonicalID.isBogus());
    assertEquals("TimeZone::getCanonicalID(bogus dest) unexpected result", canonicalID, berlin, true);
}

void TimeZoneTest::TestCanonicalID() {

    // Olson (IANA) tzdata used to have very few "Link"s long time ago.
    // This test case was written when most of CLDR canonical time zones are
    // defined as independent "Zone" in the TZ database.
    // Since then, the TZ maintainer found some historic rules in mid 20th century
    // were not really reliable, and many zones are now sharing rules.
    // As of TZ database release 2022a, there are quite a lot of zones defined
    // by "Link" to another zone, so the exception table below becomes really
    // big. It might be still useful to make sure CLDR zone aliases are consistent
    // with zone rules.
    static const struct {
        const char *alias;  // link-from
        const char *zone;   // link-to (A zone ID with "Zone" rule)
    } excluded1[] = {
        {"Africa/Accra", "Africa/Abidjan"},
        {"Africa/Addis_Ababa", "Africa/Nairobi"},
        {"Africa/Asmera", "Africa/Nairobi"},
        {"Africa/Bamako", "Africa/Abidjan"},
        {"Africa/Bangui", "Africa/Lagos"},
        {"Africa/Banjul", "Africa/Abidjan"},
        {"Africa/Blantyre", "Africa/Maputo"},
        {"Africa/Brazzaville", "Africa/Lagos"},
        {"Africa/Bujumbura", "Africa/Maputo"},
        {"Africa/Conakry", "Africa/Abidjan"},
        {"Africa/Dakar", "Africa/Abidjan"},
        {"Africa/Dar_es_Salaam", "Africa/Nairobi"},
        {"Africa/Djibouti", "Africa/Nairobi"},
        {"Africa/Douala", "Africa/Lagos"},
        {"Africa/Freetown", "Africa/Abidjan"},
        {"Africa/Gaborone", "Africa/Maputo"},
        {"Africa/Harare", "Africa/Maputo"},
        {"Africa/Kampala", "Africa/Nairobi"},
        {"Africa/Khartoum", "Africa/Juba"},
        {"Africa/Kigali", "Africa/Maputo"},
        {"Africa/Kinshasa", "Africa/Lagos"},
        {"Africa/Libreville", "Africa/Lagos"},
        {"Africa/Lome", "Africa/Abidjan"},
        {"Africa/Luanda", "Africa/Lagos"},
        {"Africa/Lubumbashi", "Africa/Maputo"},
        {"Africa/Lusaka", "Africa/Maputo"},
        {"Africa/Maseru", "Africa/Johannesburg"},
        {"Africa/Malabo", "Africa/Lagos"},
        {"Africa/Mbabane", "Africa/Johannesburg"},
        {"Africa/Mogadishu", "Africa/Nairobi"},
        {"Africa/Niamey", "Africa/Lagos"},
        {"Africa/Nouakchott", "Africa/Abidjan"},
        {"Africa/Ouagadougou", "Africa/Abidjan"},
        {"Africa/Porto-Novo", "Africa/Lagos"},
        {"Africa/Sao_Tome", "Africa/Abidjan"},
        {"America/Antigua", "America/Puerto_Rico"},
        {"America/Anguilla", "America/Puerto_Rico"},
        {"America/Aruba", "America/Puerto_Rico"},
        {"America/Atikokan", "America/Panama"},
        {"America/Blanc-Sablon", "America/Puerto_Rico"},
        {"America/Cayman", "America/Panama"},
        {"America/Coral_Harbour", "America/Panama"},
        {"America/Creston", "America/Phoenix"},
        {"America/Curacao", "America/Puerto_Rico"},
        {"America/Dominica", "America/Puerto_Rico"},
        {"America/Grenada", "America/Puerto_Rico"},
        {"America/Guadeloupe", "America/Puerto_Rico"},
        {"America/Kralendijk", "America/Puerto_Rico"},
        {"America/Lower_Princes", "America/Puerto_Rico"},
        {"America/Marigot", "America/Puerto_Rico"},
        {"America/Montreal", "America/Toronto"},
        {"America/Montserrat", "America/Puerto_Rico"},
        {"America/Nassau", "America/Toronto"},
        {"America/Nipigon", "America/Toronto"},
        {"America/Pangnirtung", "America/Iqaluit"},
        {"America/Port_of_Spain", "America/Puerto_Rico"},
        {"America/Rainy_River", "America/Winnipeg"},
        {"America/Santa_Isabel", "America/Tijuana"},
        {"America/Shiprock", "America/Denver"},
        {"America/St_Barthelemy", "America/Puerto_Rico"},
        {"America/St_Kitts", "America/Puerto_Rico"},
        {"America/St_Lucia", "America/Puerto_Rico"},
        {"America/St_Thomas", "America/Puerto_Rico"},
        {"America/St_Vincent", "America/Puerto_Rico"},
        {"America/Thunder_Bay", "America/Toronto"},
        {"America/Tortola", "America/Puerto_Rico"},
        {"America/Virgin", "America/Puerto_Rico"},
        {"America/Yellowknife", "America/Edmonton"},
        {"Antarctica/DumontDUrville", "Pacific/Port_Moresby"},
        {"Antarctica/South_Pole", "Antarctica/McMurdo"},
        {"Antarctica/Syowa", "Asia/Riyadh"},
        {"Arctic/Longyearbyen", "Europe/Berlin"},
        {"Asia/Aden", "Asia/Riyadh"},
        {"Asia/Brunei", "Asia/Kuching"},
        {"Asia/Kuala_Lumpur", "Asia/Singapore"},
        {"Asia/Kuwait", "Asia/Riyadh"},
        {"Asia/Muscat", "Asia/Dubai"},
        {"Asia/Phnom_Penh", "Asia/Bangkok"},
        {"Asia/Qatar", "Asia/Bahrain"},
        {"Asia/Vientiane", "Asia/Bangkok"},
        {"Atlantic/Jan_Mayen", "Europe/Berlin"},
        {"Atlantic/Reykjavik", "Africa/Abidjan"},
        {"Atlantic/St_Helena", "Africa/Abidjan"},
        {"Australia/Currie", "Australia/Hobart"},
        {"Australia/Tasmania", "Australia/Hobart"},
        {"Europe/Bratislava", "Europe/Prague"},
        {"Europe/Brussels", "Europe/Amsterdam"},
        {"Europe/Busingen", "Europe/Zurich"},
        {"Europe/Copenhagen", "Europe/Berlin"},
        {"Europe/Guernsey", "Europe/London"},
        {"Europe/Isle_of_Man", "Europe/London"},
        {"Europe/Jersey", "Europe/London"},
        {"Europe/Ljubljana", "Europe/Belgrade"},
        {"Europe/Luxembourg", "Europe/Amsterdam"},
        {"Europe/Mariehamn", "Europe/Helsinki"},
        {"Europe/Monaco", "Europe/Paris"},
        {"Europe/Oslo", "Europe/Berlin"},
        {"Europe/Podgorica", "Europe/Belgrade"},
        {"Europe/San_Marino", "Europe/Rome"},
        {"Europe/Sarajevo", "Europe/Belgrade"},
        {"Europe/Skopje", "Europe/Belgrade"},
        {"Europe/Stockholm", "Europe/Berlin"},
        {"Europe/Uzhgorod", "Europe/Kiev"},
        {"Europe/Vaduz", "Europe/Zurich"},
        {"Europe/Vatican", "Europe/Rome"},
        {"Europe/Zagreb", "Europe/Belgrade"},
        {"Europe/Zaporozhye", "Europe/Kiev"},
        {"Indian/Antananarivo", "Africa/Nairobi"},
        {"Indian/Christmas", "Asia/Bangkok"},
        {"Indian/Cocos", "Asia/Rangoon"},
        {"Indian/Comoro", "Africa/Nairobi"},
        {"Indian/Mahe", "Asia/Dubai"},
        {"Indian/Maldives", "Indian/Kerguelen"},
        {"Indian/Mayotte", "Africa/Nairobi"},
        {"Indian/Reunion", "Asia/Dubai"},
        {"Pacific/Auckland", "Antarctica/McMurdo"},
        {"Pacific/Johnston", "Pacific/Honolulu"},
        {"Pacific/Majuro", "Pacific/Funafuti"},
        {"Pacific/Midway", "Pacific/Pago_Pago"},
        {"Pacific/Ponape", "Pacific/Guadalcanal"},
        {"Pacific/Saipan", "Pacific/Guam"},
        {"Pacific/Tarawa", "Pacific/Funafuti"},
        {"Pacific/Truk", "Pacific/Port_Moresby"},
        {"Pacific/Wake", "Pacific/Funafuti"},
        {"Pacific/Wallis", "Pacific/Funafuti"},
        {nullptr, nullptr}
    };

    // Following IDs are aliases of Etc/GMT in CLDR,
    // but Olson tzdata has 3 independent definitions
    // for Etc/GMT, Etc/UTC, Etc/UCT.
    // Until we merge them into one equivalent group
    // in zoneinfo.res, we exclude them in the test
    // below.
    static const char* excluded2[] = {
        "Etc/UCT", "UCT",
        "Etc/UTC", "UTC",
        "Etc/Universal", "Universal",
        "Etc/Zulu", "Zulu", nullptr
    };

    // Walk through equivalency groups
    UErrorCode ec = U_ZERO_ERROR;
    int32_t s_length, i, j, k;
    StringEnumeration* s = TimeZone::createEnumeration(ec);
    if (U_FAILURE(ec)) {
        dataerrln("Unable to create TimeZone enumeration");
        return;
    }
    UnicodeString canonicalID, tmpCanonical;
    s_length = s->count(ec);
    for (i = 0; i < s_length;++i) {
        const UnicodeString *tzid = s->snext(ec);
        int32_t nEquiv = TimeZone::countEquivalentIDs(*tzid);
        if (nEquiv == 0) {
            continue;
        }
        UBool bFoundCanonical = false;
        // Make sure getCanonicalID returns the exact same result
        // for all entries within a same equivalency group with some
        // exceptions listed in exluded1.
        // Also, one of them must be canonical id.
        for (j = 0; j < nEquiv; j++) {
            UnicodeString tmp = TimeZone::getEquivalentID(*tzid, j);
            TimeZone::getCanonicalID(tmp, tmpCanonical, ec);
            if (U_FAILURE(ec)) {
                errln((UnicodeString)"FAIL: getCanonicalID(" + tmp + ") failed.");
                ec = U_ZERO_ERROR;
                continue;
            }
            // Some exceptional cases
            for (k = 0; excluded1[k].alias != nullptr; k++) {
                if (tmpCanonical == excluded1[k].alias) {
                    tmpCanonical = excluded1[k].zone;
                    break;
                }
            }
            if (j == 0) {
                canonicalID = tmpCanonical;
            } else if (canonicalID != tmpCanonical) {
                errln("FAIL: getCanonicalID(" + tmp + ") returned " + tmpCanonical + " expected:" + canonicalID);
            }

            if (canonicalID == tmp) {
                bFoundCanonical = true;
            }
        }
        // At least one ID in an equvalency group must match the
        // canonicalID
        if (bFoundCanonical == false) {
            // test exclusion because of differences between Olson tzdata and CLDR
            UBool isExcluded = false;
            for (k = 0; excluded2[k] != nullptr; k++) {
                if (*tzid == UnicodeString(excluded2[k])) {
                    isExcluded = true;
                    break;
                }
            }
            if (isExcluded) {
                continue;
            }
            errln((UnicodeString)"FAIL: No timezone ids match the canonical ID " + canonicalID);
        }
    }
    delete s;

    // Testing some special cases
    static const struct {
        const char *id;
        const char *expected;
        UBool isSystem;
    } data[] = {
        {"GMT-03", "GMT-03:00", false},
        {"GMT+4", "GMT+04:00", false},
        {"GMT-055", "GMT-00:55", false},
        {"GMT+430", "GMT+04:30", false},
        {"GMT-12:15", "GMT-12:15", false},
        {"GMT-091015", "GMT-09:10:15", false},
        {"GMT+1:90", nullptr, false},
        {"America/Argentina/Buenos_Aires", "America/Buenos_Aires", true},
        {"Etc/Unknown", "Etc/Unknown", false},
        {"bogus", nullptr, false},
        {"", nullptr, false},
        {"America/Marigot", "America/Marigot", true},     // Olson link, but CLDR canonical (#8953)
        {"Europe/Bratislava", "Europe/Bratislava", true}, // Same as above
        {nullptr, nullptr, false}
    };

    UBool isSystemID;
    for (i = 0; data[i].id != nullptr; i++) {
        TimeZone::getCanonicalID(UnicodeString(data[i].id), canonicalID, isSystemID, ec);
        if (U_FAILURE(ec)) {
            if (ec != U_ILLEGAL_ARGUMENT_ERROR || data[i].expected != nullptr) {
                errln((UnicodeString)"FAIL: getCanonicalID(\"" + data[i].id
                    + "\") returned status U_ILLEGAL_ARGUMENT_ERROR");
            }
            ec = U_ZERO_ERROR;
            continue;
        }
        if (canonicalID != data[i].expected) {
            dataerrln((UnicodeString)"FAIL: getCanonicalID(\"" + data[i].id
                + "\") returned " + canonicalID + " - expected: " + data[i].expected);
        }
        if (isSystemID != data[i].isSystem) {
            dataerrln((UnicodeString)"FAIL: getCanonicalID(\"" + data[i].id
                + "\") set " + isSystemID + " to isSystemID");
        }
    }
}

//
//  Test Display Names, choosing zones and lcoales where there are multiple
//                      meta-zones defined.
//
static struct   {
    const char            *zoneName;
    const char            *localeName;
    UBool                  summerTime;
    TimeZone::EDisplayType style;
    const char            *expectedDisplayName; } 
 zoneDisplayTestData [] =  {
     //  zone id         locale   summer   format          expected display name
      {"Europe/London",     "en", false, TimeZone::SHORT, "GMT"},
      {"Europe/London",     "en", false, TimeZone::LONG,  "Greenwich Mean Time"},
      {"Europe/London",     "en", true,  TimeZone::SHORT, "GMT+1" /*"BST"*/},
      {"Europe/London",     "en", true,  TimeZone::LONG,  "British Summer Time"},
      
      {"America/Anchorage", "en", false, TimeZone::SHORT, "AKST"},
      {"America/Anchorage", "en", false, TimeZone::LONG,  "Alaska Standard Time"},
      {"America/Anchorage", "en", true,  TimeZone::SHORT, "AKDT"},
      {"America/Anchorage", "en", true,  TimeZone::LONG,  "Alaska Daylight Time"},
      
      // Southern Hemisphere, all data from meta:Australia_Western
      {"Australia/Perth",   "en", false, TimeZone::SHORT, "GMT+8"/*"AWST"*/},
      {"Australia/Perth",   "en", false, TimeZone::LONG,  "Australian Western Standard Time"},
      // Note: Perth does not observe DST currently. When display name is missing,
      // the localized GMT format with the current offset is used even daylight name was
      // requested. See #9350.
      {"Australia/Perth",   "en", true,  TimeZone::SHORT, "GMT+8"/*"AWDT"*/},
      {"Australia/Perth",   "en", true,  TimeZone::LONG,  "Australian Western Daylight Time"},
       
      {"America/Sao_Paulo",  "en", false, TimeZone::SHORT, "GMT-3"/*"BRT"*/},
      {"America/Sao_Paulo",  "en", false, TimeZone::LONG,  "Brasilia Standard Time"},

      // Per https://mm.icann.org/pipermail/tz-announce/2019-July/000056.html
      //      Brazil has canceled DST and will stay on standard time indefinitely.
      // {"America/Sao_Paulo",  "en", true,  TimeZone::SHORT, "GMT-2"/*"BRST"*/},
      // {"America/Sao_Paulo",  "en", true,  TimeZone::LONG,  "Brasilia Summer Time"},
       
      // No Summer Time, but had it before 1983.
      {"Pacific/Honolulu",   "en", false, TimeZone::SHORT, "HST"},
      {"Pacific/Honolulu",   "en", false, TimeZone::LONG,  "Hawaii-Aleutian Standard Time"},
      {"Pacific/Honolulu",   "en", true,  TimeZone::SHORT, "HDT"},
      {"Pacific/Honolulu",   "en", true,  TimeZone::LONG,  "Hawaii-Aleutian Daylight Time"},
       
      // Northern, has Summer, not commonly used.
      {"Europe/Helsinki",    "en", false, TimeZone::SHORT, "GMT+2"/*"EET"*/},
      {"Europe/Helsinki",    "en", false, TimeZone::LONG,  "Eastern European Standard Time"},
      {"Europe/Helsinki",    "en", true,  TimeZone::SHORT, "GMT+3"/*"EEST"*/},
      {"Europe/Helsinki",    "en", true,  TimeZone::LONG,  "Eastern European Summer Time"},

      // Repeating the test data for DST.  The test data below trigger the problem reported
      // by Ticket#6644
      {"Europe/London",       "en", true, TimeZone::SHORT, "GMT+1" /*"BST"*/},
      {"Europe/London",       "en", true, TimeZone::LONG,  "British Summer Time"},

      {nullptr, nullptr, false, TimeZone::SHORT, nullptr}   // nullptr values terminate list
    };

void TimeZoneTest::TestDisplayNamesMeta() {
    UErrorCode status = U_ZERO_ERROR;
    GregorianCalendar cal(*TimeZone::getGMT(), status);
    if (failure(status, "GregorianCalendar", true)) return;

    UBool sawAnError = false;
    for (int testNum   = 0; zoneDisplayTestData[testNum].zoneName != nullptr; testNum++) {
        Locale locale  = Locale::createFromName(zoneDisplayTestData[testNum].localeName);
        TimeZone *zone = TimeZone::createTimeZone(zoneDisplayTestData[testNum].zoneName);
        UnicodeString displayName;
        zone->getDisplayName(zoneDisplayTestData[testNum].summerTime,
                             zoneDisplayTestData[testNum].style,
                             locale,
                             displayName);
        if (displayName != zoneDisplayTestData[testNum].expectedDisplayName) {
            char  name[100];
            UErrorCode status = U_ZERO_ERROR;
            displayName.extract(name, 100, nullptr, status);
            if (isDevelopmentBuild) {
                sawAnError = true;
                dataerrln("Incorrect time zone display name.  zone = \"%s\",\n"
                      "   locale = \"%s\",   style = %s,  Summertime = %d\n"
                      "   Expected \"%s\", "
                      "   Got \"%s\"\n   Error: %s", zoneDisplayTestData[testNum].zoneName,
                                         zoneDisplayTestData[testNum].localeName,
                                         zoneDisplayTestData[testNum].style==TimeZone::SHORT ?
                                            "SHORT" : "LONG",
                                         zoneDisplayTestData[testNum].summerTime,
                                         zoneDisplayTestData[testNum].expectedDisplayName,
                                         name,
                                         u_errorName(status));
            } else {
                logln("Incorrect time zone display name.  zone = \"%s\",\n"
                      "   locale = \"%s\",   style = %s,  Summertime = %d\n"
                      "   Expected \"%s\", "
                      "   Got \"%s\"\n", zoneDisplayTestData[testNum].zoneName,
                                         zoneDisplayTestData[testNum].localeName,
                                         zoneDisplayTestData[testNum].style==TimeZone::SHORT ?
                                            "SHORT" : "LONG",
                                         zoneDisplayTestData[testNum].summerTime,
                                         zoneDisplayTestData[testNum].expectedDisplayName,
                                         name);
            }
        }
        delete zone;
    }
    if (sawAnError) {
        dataerrln("***Note: Errors could be the result of changes to zoneStrings locale data");
    }
}

void TimeZoneTest::TestGetRegion()
{
    static const struct {
        const char *id;
        const char *region;
    } data[] = {
        {"America/Los_Angeles",             "US"},
        {"America/Indianapolis",            "US"},  // CLDR canonical, Olson backward
        {"America/Indiana/Indianapolis",    "US"},  // CLDR alias
        {"Mexico/General",                  "MX"},  // Link America/Mexico_City, Olson backward
        {"Etc/UTC",                         "001"},
        {"EST5EDT",                         "001"},
        {"PST",                             "US"},  // Link America/Los_Angeles
        {"Europe/Helsinki",                 "FI"},
        {"Europe/Mariehamn",                "AX"},  // Link Europe/Helsinki, but in zone.tab
        {"Asia/Riyadh",                     "SA"},
        // tz file solar87 was removed from tzdata2013i
        // {"Asia/Riyadh87",                   "001"}, // this should be "SA" actually, but not in zone.tab
        {"Atlantic/Jan_Mayen",              "SJ"},
        {"Pacific/Truk",                    "FM"},
        {"Etc/Unknown",                     nullptr},  // CLDR canonical, but not a sysmte zone ID
        {"bogus",                           nullptr},  // bogus
        {"GMT+08:00",                       nullptr},  // a custom ID, not a system zone ID
        {nullptr, nullptr}
    };

    int32_t i;
    char region[4];
    UErrorCode sts;
    for (i = 0; data[i].id; i++) {
        sts = U_ZERO_ERROR;
        TimeZone::getRegion(data[i].id, region, sizeof(region), sts);
        if (U_SUCCESS(sts)) {
            if (data[i].region == nullptr) {
                errln((UnicodeString)"Fail: getRegion(\"" + data[i].id + "\") returns "
                    + region + " [expected: U_ILLEGAL_ARGUMENT_ERROR]");
            } else if (uprv_strcmp(region, data[i].region) != 0) {
                errln((UnicodeString)"Fail: getRegion(\"" + data[i].id + "\") returns "
                    + region + " [expected: " + data[i].region + "]");
            }
        } else if (sts == U_ILLEGAL_ARGUMENT_ERROR) {
            if (data[i].region != nullptr) {
                dataerrln((UnicodeString)"Fail: getRegion(\"" + data[i].id
                    + "\") returns error status U_ILLEGAL_ARGUMENT_ERROR [expected: "
                    + data[i].region + "]");
            }
        } else {
                errln((UnicodeString)"Fail: getRegion(\"" + data[i].id
                    + "\") returns an unexpected error status");
        }
    }

    // Extra test cases for short buffer
    int32_t len;
    char region2[2];
    sts = U_ZERO_ERROR;

    len = TimeZone::getRegion("America/New_York", region2, sizeof(region2), sts);
    if (sts == U_ILLEGAL_ARGUMENT_ERROR) {
        dataerrln("Error calling TimeZone::getRegion");
    } else {
        if (sts != U_STRING_NOT_TERMINATED_WARNING) {
            errln("Expected U_STRING_NOT_TERMINATED_WARNING");
        }
        if (len != 2) { // length of "US"
            errln("Incorrect result length");
        }
        if (uprv_strncmp(region2, "US", 2) != 0) {
            errln("Incorrect result");
        }
    }

    char region1[1];
    sts = U_ZERO_ERROR;

    len = TimeZone::getRegion("America/Chicago", region1, sizeof(region1), sts);
    if (sts == U_ILLEGAL_ARGUMENT_ERROR) {
        dataerrln("Error calling TimeZone::getRegion");
    } else {
        if (sts != U_BUFFER_OVERFLOW_ERROR) {
            errln("Expected U_BUFFER_OVERFLOW_ERROR");
        }
        if (len != 2) { // length of "US"
            errln("Incorrect result length");
        }
    }
}

void TimeZoneTest::TestGetUnknown() {
    const TimeZone &unknown = TimeZone::getUnknown();
    UnicodeString expectedID = UNICODE_STRING_SIMPLE("Etc/Unknown");
    UnicodeString id;
    assertEquals("getUnknown() wrong ID", expectedID, unknown.getID(id));
    assertTrue("getUnknown() wrong offset", 0 == unknown.getRawOffset());
    assertFalse("getUnknown() uses DST", unknown.useDaylightTime());
}

void TimeZoneTest::TestGetGMT() {
    const TimeZone *gmt = TimeZone::getGMT();
    UnicodeString expectedID = UNICODE_STRING_SIMPLE("GMT");
    UnicodeString id;
    assertEquals("getGMT() wrong ID", expectedID, gmt->getID(id));
    assertTrue("getGMT() wrong offset", 0 == gmt->getRawOffset());
    assertFalse("getGMT() uses DST", gmt->useDaylightTime());
}

void TimeZoneTest::TestGetWindowsID() {
    static const struct {
        const char *id;
        const char *winid;
    } TESTDATA[] = {
        {"America/New_York",        "Eastern Standard Time"},
        {"America/Montreal",        "Eastern Standard Time"},
        {"America/Los_Angeles",     "Pacific Standard Time"},
        {"America/Vancouver",       "Pacific Standard Time"},
        {"Asia/Shanghai",           "China Standard Time"},
        {"Asia/Chongqing",          "China Standard Time"},
        {"America/Indianapolis",    "US Eastern Standard Time"},            // CLDR canonical name
        {"America/Indiana/Indianapolis",    "US Eastern Standard Time"},    // tzdb canonical name
        {"Asia/Khandyga",           "Yakutsk Standard Time"},
        {"Australia/Eucla",         "Aus Central W. Standard Time"}, // formerly no Windows ID mapping, now has one
        {"Bogus",                   ""},
        {nullptr,                   nullptr},
    };

    for (int32_t i = 0; TESTDATA[i].id != nullptr; i++) {
        UErrorCode sts = U_ZERO_ERROR;
        UnicodeString windowsID;

        TimeZone::getWindowsID(UnicodeString(TESTDATA[i].id), windowsID, sts);
        assertSuccess(TESTDATA[i].id, sts);
        assertEquals(TESTDATA[i].id, UnicodeString(TESTDATA[i].winid), windowsID, true);
    }
}

void TimeZoneTest::TestGetIDForWindowsID() {
    static const struct {
        const char *winid;
        const char *region;
        const char *id;
    } TESTDATA[] = {
        {"Eastern Standard Time",      nullptr, "America/New_York"},
        {"Eastern Standard Time",      "US",    "America/New_York"},
        {"Eastern Standard Time",      "CA",    "America/Toronto"},
        {"Eastern Standard Time",      "CN",    "America/New_York"},
        {"China Standard Time",        nullptr, "Asia/Shanghai"},
        {"China Standard Time",        "CN",    "Asia/Shanghai"},
        {"China Standard Time",        "HK",    "Asia/Hong_Kong"},
        {"Mid-Atlantic Standard Time", nullptr, ""}, // No tz database mapping
        {"Bogus",                      nullptr, ""},
        {nullptr,                      nullptr, nullptr},
    };

    for (int32_t i = 0; TESTDATA[i].winid != nullptr; i++) {
        UErrorCode sts = U_ZERO_ERROR;
        UnicodeString id;

        TimeZone::getIDForWindowsID(UnicodeString(TESTDATA[i].winid), TESTDATA[i].region,
                                    id, sts);
        assertSuccess(UnicodeString(TESTDATA[i].winid) + "/" + TESTDATA[i].region, sts);
        assertEquals(UnicodeString(TESTDATA[i].winid) + "/" + TESTDATA[i].region, TESTDATA[i].id, id, true);
    }
}

void TimeZoneTest::TestCasablancaNameAndOffset22041() {
    std::unique_ptr<TimeZone> zone(TimeZone::createTimeZone("Africa/Casablanca"));
    UnicodeString standardName, summerName;
    zone->getDisplayName(false, TimeZone::LONG, Locale::getEnglish(), standardName);
    zone->getDisplayName(true, TimeZone::LONG, Locale::getEnglish(), summerName);
    int32_t raw, dst;
    UErrorCode status = U_ZERO_ERROR;
    zone->getOffset(Calendar::getNow(), false, raw, dst, status);
    assertEquals(u"TimeZone name for Africa/Casablanca should not contain '+02' since it is located in UTC, but got "
                 + standardName, -1, standardName.indexOf("+02"));
    assertEquals(u"TimeZone name for Africa/Casablanca should not contain '+02' since it is located in UTC, but got "
                 + summerName, -1, summerName.indexOf("+02"));
    assertEquals("getRawOffset() and the raw from getOffset(now, false, raw, dst, status) should not be different but got",
                 zone->getRawOffset(), raw);
}

void TimeZoneTest::TestRawOffsetAndOffsetConsistency22041() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<StringEnumeration> s(TimeZone::createEnumeration(status));
    if (U_FAILURE(status)) {
        dataerrln("Unable to create TimeZone enumeration");
        return;
    }
    const char* tz;
    UDate now = Calendar::getNow();
    while ((tz = s->next(nullptr, status)) != nullptr && U_SUCCESS(status)) {
        std::unique_ptr<TimeZone> zone(TimeZone::createTimeZone(tz));
        int32_t raw, dst;
        zone->getOffset(now, false, raw, dst, status);
        if (U_FAILURE(status)) {
           errln("TimeZone '%s' getOffset() return error", tz);
        }
        assertEquals(u"TimeZone '" + UnicodeString(tz) +
                     u"' getRawOffset() and the raw from getOffset(now, false, raw, dst, status) should not be different but got",
                     zone->getRawOffset(), raw);
    }
}

void TimeZoneTest::TestGetIanaID() {
    const char16_t* UNKNOWN = u"Etc/Unknown";
    static const struct {
        const char16_t* id;
        const char16_t* expected;
    } TESTDATA[] = {
        {u"",                   UNKNOWN},
        {nullptr,               UNKNOWN},
        {UNKNOWN,               UNKNOWN},
        {u"America/New_York",   u"America/New_York"},
        {u"Asia/Calcutta",      u"Asia/Kolkata"},
        {u"Europe/Kiev",        u"Europe/Kyiv"},
        {u"Europe/Zaporozhye",  u"Europe/Kyiv"},
        {u"Etc/GMT-1",          u"Etc/GMT-1"},
        {u"Etc/GMT+20",         UNKNOWN},
        {u"PST8PDT",            u"PST8PDT"},
        {u"GMT-08:00",          UNKNOWN},
        {nullptr,               nullptr}
    };

    for (int32_t i = 0; TESTDATA[i].expected != nullptr; i++) {
        UErrorCode sts = U_ZERO_ERROR;
        UnicodeString inputID(TESTDATA[i].id);
        UnicodeString ianaID;

        TimeZone::getIanaID(inputID, ianaID, sts);
        if (u_strcmp(TESTDATA[i].expected, UNKNOWN) == 0) {
            assertEquals(inputID + " should fail", (int32_t)U_ILLEGAL_ARGUMENT_ERROR, sts);
            assertTrue(inputID + " should set bogus", ianaID.isBogus());
        } else {
            assertEquals(inputID, UnicodeString(TESTDATA[i].expected), ianaID);
            // Calling getIanaID with an IANA ID should return the same
            UnicodeString ianaID2;
            TimeZone::getIanaID(ianaID, ianaID2, sts);
            assertEquals(ianaID, ianaID, ianaID2);
        }
    }
}

void TimeZoneTest::TestGMTMinus24ICU22526() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<TimeZone> tz(TimeZone::createTimeZone("GMT-23:59"), status);
    U_ASSERT(U_SUCCESS(status));
    GregorianCalendar gc(tz.orphan(), status);
    gc.setTime(123456789, status);
    gc.get(UCAL_MONTH, status);
}
#endif /* #if !UCONFIG_NO_FORMATTING */
