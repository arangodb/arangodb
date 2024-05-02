// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/***********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2015, International Business Machines Corporation
 * and others. All Rights Reserved.
 ***********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "callimts.h"
#include "caltest.h"
#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/datefmt.h"
#include "unicode/smpdtfmt.h"
#include "cstring.h"
#include "mutex.h"
#include "putilimp.h"
#include "simplethread.h"

U_NAMESPACE_USE
void CalendarLimitTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite TestCalendarLimit");
    switch (index) {
        // Re-enable this later
        case 0:
            name = "TestCalendarExtremeLimit";
            if (exec) {
                logln("TestCalendarExtremeLimit---"); logln("");
                TestCalendarExtremeLimit();
            }
            break;
        case 1:
            name = "TestLimits";
            if (exec) {
                logln("TestLimits---"); logln("");
                TestLimits();
            }
            break;

        default: name = ""; break;
    }
}


// *****************************************************************************
// class CalendarLimitTest
// *****************************************************************************

// -------------------------------------
void
CalendarLimitTest::test(UDate millis, icu::Calendar* cal, icu::DateFormat* fmt)
{
  static const UDate kDrift = 1e-10;
    UErrorCode exception = U_ZERO_ERROR;
    UnicodeString theDate;
    UErrorCode status = U_ZERO_ERROR;
    cal->setTime(millis, exception);
    if (U_SUCCESS(exception)) {
        fmt->format(millis, theDate);
        UDate dt = fmt->parse(theDate, status);
        // allow a small amount of error (drift)
        if(! withinErr(dt, millis, kDrift)) {
          errln("FAIL:round trip for large milli, got: %.1lf wanted: %.1lf. (delta %.2lf greater than %.2lf)",
                dt, millis, uprv_fabs(millis-dt), uprv_fabs(dt*kDrift));
          logln(UnicodeString("   ") + theDate + " " + CalendarTest::calToStr(*cal));
          } else {
            logln(UnicodeString("OK: got ") + dt + ", wanted " + millis);
            logln(UnicodeString("    ") + theDate);
        }
    }        
}
 
// -------------------------------------

// bug 986c: deprecate nextDouble/previousDouble
//|double
//|CalendarLimitTest::nextDouble(double a)
//|{
//|    return uprv_nextDouble(a, TRUE);
//|}
//|
//|double
//|CalendarLimitTest::previousDouble(double a)
//|{
//|    return uprv_nextDouble(a, FALSE);
//|}

UBool
CalendarLimitTest::withinErr(double a, double b, double err)
{
    return ( uprv_fabs(a - b) < uprv_fabs(a * err) ); 
}

void
CalendarLimitTest::TestCalendarExtremeLimit()
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = Calendar::createInstance(status);
    if (failure(status, "Calendar::createInstance", TRUE)) return;
    cal->adoptTimeZone(TimeZone::createTimeZone("GMT"));
    DateFormat *fmt = DateFormat::createDateTimeInstance();
    if(!fmt || !cal) {
       dataerrln("can't open cal and/or fmt");
       return;
    }
    fmt->adoptCalendar(cal);
    ((SimpleDateFormat*) fmt)->applyPattern("HH:mm:ss.SSS Z, EEEE, MMMM d, yyyy G");


    // This test used to test the algorithmic limits of the dates that
    // GregorianCalendar could handle.  However, the algorithm has
    // been rewritten completely since then and the prior limits no
    // longer apply.  Instead, we now do basic round-trip testing of
    // some extreme (but still manageable) dates.
    UDate m;
    logln("checking 1e16..1e17");
    for ( m = 1e16; m < 1e17; m *= 1.1) {
        test(m, cal, fmt);
    }
    logln("checking -1e14..-1e15");
    for ( m = -1e14; m > -1e15; m *= 1.1) {
        test(m, cal, fmt);
    }

    // This is 2^52 - 1, the largest allowable mantissa with a 0
    // exponent in a 64-bit double
    UDate VERY_EARLY_MILLIS = - 4503599627370495.0;
    UDate VERY_LATE_MILLIS  =   4503599627370495.0;

    // I am removing the previousDouble and nextDouble calls below for
    // two reasons: 1. As part of jitterbug 986, I am deprecating
    // these methods and removing calls to them.  2. This test is a
    // non-critical boundary behavior test.
    test(VERY_EARLY_MILLIS, cal, fmt);
    //test(previousDouble(VERY_EARLY_MILLIS), cal, fmt);
    test(VERY_LATE_MILLIS, cal, fmt);
    //test(nextDouble(VERY_LATE_MILLIS), cal, fmt);
    delete fmt;
}

namespace {

struct TestCase {
    const char *type;
    UBool hasLeapMonth;
    UDate actualTestStart;
    int32_t actualTestEnd;
};
    
const UDate DEFAULT_START = 944006400000.0; // 1999-12-01T00:00Z
const int32_t DEFAULT_END = -120; // Default for non-quick is run 2 minutes

TestCase TestCases[] = {
        {"gregorian",       FALSE,      DEFAULT_START, DEFAULT_END},
        {"japanese",        FALSE,      596937600000.0, DEFAULT_END}, // 1988-12-01T00:00Z, Showa 63
        {"buddhist",        FALSE,      DEFAULT_START, DEFAULT_END},
        {"roc",             FALSE,      DEFAULT_START, DEFAULT_END},
        {"persian",         FALSE,      DEFAULT_START, DEFAULT_END},
        {"islamic-civil",   FALSE,      DEFAULT_START, DEFAULT_END},
        {"islamic",         FALSE,      DEFAULT_START, 800000}, // Approx. 2250 years from now, after which 
                                                                // some rounding errors occur in Islamic calendar
        {"hebrew",          TRUE,       DEFAULT_START, DEFAULT_END},
        {"chinese",         TRUE,       DEFAULT_START, DEFAULT_END},
        {"dangi",           TRUE,       DEFAULT_START, DEFAULT_END},
        {"indian",          FALSE,      DEFAULT_START, DEFAULT_END},
        {"coptic",          FALSE,      DEFAULT_START, DEFAULT_END},
        {"ethiopic",        FALSE,      DEFAULT_START, DEFAULT_END},
        {"ethiopic-amete-alem", FALSE,  DEFAULT_START, DEFAULT_END}
};
    
struct {
    int32_t fIndex;
    UBool next (int32_t &rIndex) {
        Mutex lock;
        if (fIndex >= UPRV_LENGTHOF(TestCases)) {
            return FALSE;
        }
        rIndex = fIndex++;
        return TRUE;
    }
    void reset() {
        fIndex = 0;
    }
} gTestCaseIterator;

}  // anonymous name space

void
CalendarLimitTest::TestLimits(void) {
    gTestCaseIterator.reset();

    ThreadPool<CalendarLimitTest> threads(this, threadCount, &CalendarLimitTest::TestLimitsThread);
    threads.start();
    threads.join();
}


void CalendarLimitTest::TestLimitsThread(int32_t threadNum) {
    logln("thread %d starting", threadNum);
    int32_t testIndex = 0;
    LocalPointer<Calendar> cal;
    while (gTestCaseIterator.next(testIndex)) {
        TestCase &testCase = TestCases[testIndex];
        logln("begin test of %s calendar.", testCase.type);
        UErrorCode status = U_ZERO_ERROR;
        char buf[64];
        uprv_strcpy(buf, "root@calendar=");
        strcat(buf, testCase.type);
        cal.adoptInstead(Calendar::createInstance(buf, status));
        if (failure(status, "Calendar::createInstance", TRUE)) {
            continue;
        }
        if (uprv_strcmp(cal->getType(), testCase.type) != 0) {
            errln((UnicodeString)"FAIL: Wrong calendar type: " + cal->getType()
                + " Requested: " + testCase.type);
            continue;
        }
        doTheoreticalLimitsTest(*(cal.getAlias()), testCase.hasLeapMonth);
        doLimitsTest(*(cal.getAlias()), testCase.actualTestStart, testCase.actualTestEnd);
        logln("end test of %s calendar.", testCase.type);
    }
}

    
void
CalendarLimitTest::doTheoreticalLimitsTest(Calendar& cal, UBool leapMonth) {
    const char* calType = cal.getType();

    int32_t nDOW = cal.getMaximum(UCAL_DAY_OF_WEEK);
    int32_t maxDOY = cal.getMaximum(UCAL_DAY_OF_YEAR);
    int32_t lmaxDOW = cal.getLeastMaximum(UCAL_DAY_OF_YEAR);
    int32_t maxWOY = cal.getMaximum(UCAL_WEEK_OF_YEAR);
    int32_t lmaxWOY = cal.getLeastMaximum(UCAL_WEEK_OF_YEAR);
    int32_t maxM = cal.getMaximum(UCAL_MONTH) + 1;
    int32_t lmaxM = cal.getLeastMaximum(UCAL_MONTH) + 1;
    int32_t maxDOM = cal.getMaximum(UCAL_DAY_OF_MONTH);
    int32_t lmaxDOM = cal.getLeastMaximum(UCAL_DAY_OF_MONTH);
    int32_t maxDOWIM = cal.getMaximum(UCAL_DAY_OF_WEEK_IN_MONTH);
    int32_t lmaxDOWIM = cal.getLeastMaximum(UCAL_DAY_OF_WEEK_IN_MONTH);
    int32_t maxWOM = cal.getMaximum(UCAL_WEEK_OF_MONTH);
    int32_t lmaxWOM = cal.getLeastMaximum(UCAL_WEEK_OF_MONTH);
    int32_t minDaysInFirstWeek = cal.getMinimalDaysInFirstWeek();

    // Day of year
    int32_t expected;
    if (!leapMonth) {
        expected = maxM*maxDOM;
        if (maxDOY > expected) {
            errln((UnicodeString)"FAIL: [" + calType + "] Maximum value of DAY_OF_YEAR is too big: "
                + maxDOY + "/expected: <=" + expected);
        }
        expected = lmaxM*lmaxDOM;
        if (lmaxDOW < expected) {
            errln((UnicodeString)"FAIL: [" + calType + "] Least maximum value of DAY_OF_YEAR is too small: "
                + lmaxDOW + "/expected: >=" + expected);
        }
    }

    // Week of year
    expected = maxDOY/nDOW + 1;
    if (maxWOY > expected) {
        errln((UnicodeString)"FAIL: [" + calType + "] Maximum value of WEEK_OF_YEAR is too big: "
            + maxWOY + "/expected: <=" + expected);
    }
    expected = lmaxDOW/nDOW;
    if (lmaxWOY < expected) {
        errln((UnicodeString)"FAIL: [" + calType + "] Least maximum value of WEEK_OF_YEAR is too small: "
            + lmaxWOY + "/expected >=" + expected);
    }

    // Day of week in month
    expected = (maxDOM + nDOW - 1)/nDOW;
    if (maxDOWIM != expected) {
        errln((UnicodeString)"FAIL: [" + calType + "] Maximum value of DAY_OF_WEEK_IN_MONTH is incorrect: "
            + maxDOWIM + "/expected: " + expected);
    }
    expected = (lmaxDOM + nDOW - 1)/nDOW;
    if (lmaxDOWIM != expected) {
        errln((UnicodeString)"FAIL: [" + calType + "] Least maximum value of DAY_OF_WEEK_IN_MONTH is incorrect: "
            + lmaxDOWIM + "/expected: " + expected);
    }

    // Week of month
    expected = (maxDOM + (nDOW - 1) + (nDOW - minDaysInFirstWeek)) / nDOW;
    if (maxWOM != expected) {
        errln((UnicodeString)"FAIL: [" + calType + "] Maximum value of WEEK_OF_MONTH is incorrect: "
            + maxWOM + "/expected: " + expected);
    }
    expected = (lmaxDOM + (nDOW - minDaysInFirstWeek)) / nDOW;
    if (lmaxWOM != expected) {
        errln((UnicodeString)"FAIL: [" + calType + "] Least maximum value of WEEK_OF_MONTH is incorrect: "
            + lmaxWOM + "/expected: " + expected);
    }
}

void
CalendarLimitTest::doLimitsTest(Calendar& cal, UDate startDate, int32_t endTime) {
    int32_t testTime = quick ? ( endTime / 40 ) : endTime;
    doLimitsTest(cal, NULL /*default fields*/, startDate, testTime);
}

void
CalendarLimitTest::doLimitsTest(Calendar& cal,
                                const int32_t* fieldsToTest,
                                UDate startDate,
                                int32_t testDuration) {
    static const int32_t FIELDS[] = {
        UCAL_ERA,
        UCAL_YEAR,
        UCAL_MONTH,
        UCAL_WEEK_OF_YEAR,
        UCAL_WEEK_OF_MONTH,
        UCAL_DAY_OF_MONTH,
        UCAL_DAY_OF_YEAR,
        UCAL_DAY_OF_WEEK_IN_MONTH,
        UCAL_YEAR_WOY,
        UCAL_EXTENDED_YEAR,
        -1,
    };

    static const char* FIELD_NAME[] = {
        "ERA", "YEAR", "MONTH", "WEEK_OF_YEAR", "WEEK_OF_MONTH",
        "DAY_OF_MONTH", "DAY_OF_YEAR", "DAY_OF_WEEK",
        "DAY_OF_WEEK_IN_MONTH", "AM_PM", "HOUR", "HOUR_OF_DAY",
        "MINUTE", "SECOND", "MILLISECOND", "ZONE_OFFSET",
        "DST_OFFSET", "YEAR_WOY", "DOW_LOCAL", "EXTENDED_YEAR",
        "JULIAN_DAY", "MILLISECONDS_IN_DAY",
        "IS_LEAP_MONTH"
    };

    UErrorCode status = U_ZERO_ERROR;
    int32_t i, j;
    UnicodeString ymd;

    GregorianCalendar greg(status);
    if (failure(status, "new GregorianCalendar")) {
        return;
    }
    greg.setTime(startDate, status);
    if (failure(status, "GregorianCalendar::setTime")) {
        return;
    }
    logln((UnicodeString)"Start: " + startDate);

    if (fieldsToTest == NULL) {
        fieldsToTest = FIELDS;
    }


    // Keep a record of minima and maxima that we actually see.
    // These are kept in an array of arrays of hashes.
    int32_t limits[UCAL_FIELD_COUNT][4];
    for (j = 0; j < UCAL_FIELD_COUNT; j++) {
        limits[j][0] = INT32_MAX;
        limits[j][1] = INT32_MIN;
        limits[j][2] = INT32_MAX;
        limits[j][3] = INT32_MIN;
    }

    // This test can run for a long time; show progress.
    UDate millis = ucal_getNow();
    UDate mark = millis + 5000; // 5 sec
    millis -= testDuration * 1000; // stop time if testDuration<0

    for (i = 0;
         testDuration > 0 ? i < testDuration
                        : ucal_getNow() < millis;
         ++i) {
        if (ucal_getNow() >= mark) {
            logln((UnicodeString)"(" + i + " days)");
            mark += 5000; // 5 sec
        }
        UDate testMillis = greg.getTime(status);
        cal.setTime(testMillis, status);
        cal.setMinimalDaysInFirstWeek(1);
        if (failure(status, "Calendar set/getTime")) {
            return;
        }
        for (j = 0; fieldsToTest[j] >= 0; ++j) {
            UCalendarDateFields f = (UCalendarDateFields)fieldsToTest[j];
            int32_t v = cal.get(f, status);
            int32_t minActual = cal.getActualMinimum(f, status);
            int32_t maxActual = cal.getActualMaximum(f, status);
            int32_t minLow = cal.getMinimum(f);
            int32_t minHigh = cal.getGreatestMinimum(f);
            int32_t maxLow = cal.getLeastMaximum(f);
            int32_t maxHigh = cal.getMaximum(f);

            if (limits[j][0] > minActual) {
                // the minimum
                limits[j][0] = minActual;
            }
            if (limits[j][1] < minActual) {
                // the greatest minimum
                limits[j][1] = minActual;
            }
            if (limits[j][2] > maxActual) {
                // the least maximum
                limits[j][2] = maxActual;
            }
            if (limits[j][3] < maxActual) {
                // the maximum
                limits[j][3] = maxActual;
            }

            if (minActual < minLow || minActual > minHigh) {
                errln((UnicodeString)"Fail: [" + cal.getType() + "] " +
                      ymdToString(cal, ymd) +
                      " Range for min of " + FIELD_NAME[f] + "(" + f +
                      ")=" + minLow + ".." + minHigh +
                      ", actual_min=" + minActual);
            }
            if (maxActual < maxLow || maxActual > maxHigh) {
                if ( uprv_strcmp(cal.getType(), "chinese") == 0 &&
                        testMillis >= 1802044800000.0 &&
                     logKnownIssue("12620", "chinese calendar failures for some actualMax tests")) {
                    logln((UnicodeString)"KnownFail: [" + cal.getType() + "] " +
                          ymdToString(cal, ymd) +
                          " Range for max of " + FIELD_NAME[f] + "(" + f +
                          ")=" + maxLow + ".." + maxHigh +
                          ", actual_max=" + maxActual);
                } else {
                    errln((UnicodeString)"Fail: [" + cal.getType() + "] " +
                          ymdToString(cal, ymd) +
                          " Range for max of " + FIELD_NAME[f] + "(" + f +
                          ")=" + maxLow + ".." + maxHigh +
                          ", actual_max=" + maxActual);
                }
            }
            if (v < minActual || v > maxActual) {
                // timebomb per #9967, fix with #9972
                if ( uprv_strcmp(cal.getType(), "dangi") == 0 &&
                        testMillis >= 1865635198000.0  &&
                     logKnownIssue("9972", "as per #9967")) { // Feb 2029 gregorian, end of dangi 4361
                    logln((UnicodeString)"KnownFail: [" + cal.getType() + "] " +
                          ymdToString(cal, ymd) +
                          " " + FIELD_NAME[f] + "(" + f + ")=" + v +
                          ", actual=" + minActual + ".." + maxActual +
                          ", allowed=(" + minLow + ".." + minHigh + ")..(" +
                          maxLow + ".." + maxHigh + ")");
                } else if ( uprv_strcmp(cal.getType(), "chinese") == 0 &&
                        testMillis >= 1832544000000.0 &&
                     logKnownIssue("12620", "chinese calendar failures for some actualMax tests")) {
                    logln((UnicodeString)"KnownFail: [" + cal.getType() + "] " +
                          ymdToString(cal, ymd) +
                          " " + FIELD_NAME[f] + "(" + f + ")=" + v +
                          ", actual=" + minActual + ".." + maxActual +
                          ", allowed=(" + minLow + ".." + minHigh + ")..(" +
                          maxLow + ".." + maxHigh + ")");
                } else {
                    errln((UnicodeString)"Fail: [" + cal.getType() + "] " +
                          ymdToString(cal, ymd) +
                          " " + FIELD_NAME[f] + "(" + f + ")=" + v +
                          ", actual=" + minActual + ".." + maxActual +
                          ", allowed=(" + minLow + ".." + minHigh + ")..(" +
                          maxLow + ".." + maxHigh + ")");
                }
            }
        }
        greg.add(UCAL_DAY_OF_YEAR, 1, status);
        if (failure(status, "Calendar::add")) {
            return;
        }
    }

    // Check actual maxima and minima seen against ranges returned
    // by API.
    UnicodeString buf;
    for (j = 0; fieldsToTest[j] >= 0; ++j) {
        int32_t rangeLow, rangeHigh;
        UBool fullRangeSeen = TRUE;
        UCalendarDateFields f = (UCalendarDateFields)fieldsToTest[j];

        buf.remove();
        buf.append((UnicodeString)"[" + cal.getType() + "] " + FIELD_NAME[f]);

        // Minumum
        rangeLow = cal.getMinimum(f);
        rangeHigh = cal.getGreatestMinimum(f);
        if (limits[j][0] != rangeLow || limits[j][1] != rangeHigh) {
            fullRangeSeen = FALSE;
        }
        buf.append((UnicodeString)" minima range=" + rangeLow + ".." + rangeHigh);
        buf.append((UnicodeString)" minima actual=" + limits[j][0] + ".." + limits[j][1]);

        // Maximum
        rangeLow = cal.getLeastMaximum(f);
        rangeHigh = cal.getMaximum(f);
        if (limits[j][2] != rangeLow || limits[j][3] != rangeHigh) {
            fullRangeSeen = FALSE;
        }
        buf.append((UnicodeString)" maxima range=" + rangeLow + ".." + rangeHigh);
        buf.append((UnicodeString)" maxima actual=" + limits[j][2] + ".." + limits[j][3]);

        if (fullRangeSeen) {
            logln((UnicodeString)"OK: " + buf);
        } else {
            // This may or may not be an error -- if the range of dates
            // we scan over doesn't happen to contain a minimum or
            // maximum, it doesn't mean some other range won't.
            logln((UnicodeString)"Warning: " + buf);
        }
    }

    logln((UnicodeString)"End: " + greg.getTime(status));
}

UnicodeString&
CalendarLimitTest::ymdToString(const Calendar& cal, UnicodeString& str) {
    UErrorCode status = U_ZERO_ERROR;
    str.remove();
    str.append((UnicodeString)"" + cal.get(UCAL_EXTENDED_YEAR, status)
        + "/" + (cal.get(UCAL_MONTH, status) + 1)
        + (cal.get(UCAL_IS_LEAP_MONTH, status) == 1 ? "(leap)" : "")
        + "/" + cal.get(UCAL_DATE, status)
        + " " + cal.get(UCAL_HOUR_OF_DAY, status)
        + ":" + cal.get(UCAL_MINUTE, status)
        + " zone(hrs) " + cal.get(UCAL_ZONE_OFFSET, status)/(60.0*60.0*1000.0)
        + " dst(hrs) " + cal.get(UCAL_DST_OFFSET, status)/(60.0*60.0*1000.0)
        + ", time(millis)=" + cal.getTime(status));
    return str;
}

#endif /* #if !UCONFIG_NO_FORMATTING */

// eof
