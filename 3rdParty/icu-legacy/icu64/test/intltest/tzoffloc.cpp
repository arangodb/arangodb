// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2007-2011, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "tzoffloc.h"

#include "unicode/ucal.h"
#include "unicode/timezone.h"
#include "unicode/calendar.h"
#include "unicode/dtrule.h"
#include "unicode/tzrule.h"
#include "unicode/rbtz.h"
#include "unicode/simpletz.h"
#include "unicode/tzrule.h"
#include "unicode/smpdtfmt.h"
#include "unicode/gregocal.h"

void
TimeZoneOffsetLocalTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) {
        logln("TestSuite TimeZoneOffsetLocalTest");
    }
    switch (index) {
        TESTCASE(0, TestGetOffsetAroundTransition);
        default: name = ""; break;
    }
}

/*
 * Testing getOffset APIs around rule transition by local standard/wall time.
 */
void
TimeZoneOffsetLocalTest::TestGetOffsetAroundTransition() {
    const int32_t NUM_DATES = 10;
    const int32_t NUM_TIMEZONES = 3;

    const int32_t HOUR = 60*60*1000;
    const int32_t MINUTE = 60*1000;

    const int32_t DATES[NUM_DATES][6] = {
        {2006, UCAL_APRIL, 2, 1, 30, 1*HOUR+30*MINUTE},
        {2006, UCAL_APRIL, 2, 2, 00, 2*HOUR},
        {2006, UCAL_APRIL, 2, 2, 30, 2*HOUR+30*MINUTE},
        {2006, UCAL_APRIL, 2, 3, 00, 3*HOUR},
        {2006, UCAL_APRIL, 2, 3, 30, 3*HOUR+30*MINUTE},
        {2006, UCAL_OCTOBER, 29, 0, 30, 0*HOUR+30*MINUTE},
        {2006, UCAL_OCTOBER, 29, 1, 00, 1*HOUR},
        {2006, UCAL_OCTOBER, 29, 1, 30, 1*HOUR+30*MINUTE},
        {2006, UCAL_OCTOBER, 29, 2, 00, 2*HOUR},
        {2006, UCAL_OCTOBER, 29, 2, 30, 2*HOUR+30*MINUTE},
    };

    // Expected offsets by int32_t getOffset(uint8_t era, int32_t year, int32_t month, int32_t day,
    // uint8_t dayOfWeek, int32_t millis, UErrorCode& status)
    const int32_t OFFSETS1[NUM_DATES] = {
        // April 2, 2006
        -8*HOUR,
        -7*HOUR,
        -7*HOUR,
        -7*HOUR,
        -7*HOUR,

        // October 29, 2006
        -7*HOUR,
        -8*HOUR,
        -8*HOUR,
        -8*HOUR,
        -8*HOUR,
    };

    // Expected offsets by void getOffset(UDate date, UBool local, int32_t& rawOffset,
    // int32_t& dstOffset, UErrorCode& ec) with local=true
    // or void getOffsetFromLocal(UDate date, UTimeZoneLocalOption nonExistingTimeOpt, UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t& rawOffset, int32_t& dstOffset, UErrorCode& status) with
    // nonExistingTimeOpt=STANDARD_*/duplicatedTimeOpt=STANDARD_*
    const int32_t OFFSETS2[NUM_DATES][2] = {
        // April 2, 2006
        {-8*HOUR, 0},
        {-8*HOUR, 0},
        {-8*HOUR, 0},
        {-8*HOUR, 1*HOUR},
        {-8*HOUR, 1*HOUR},

        // Oct 29, 2006
        {-8*HOUR, 1*HOUR},
        {-8*HOUR, 0},
        {-8*HOUR, 0},
        {-8*HOUR, 0},
        {-8*HOUR, 0},
    };

    // Expected offsets by void getOffsetFromLocal(UDate date, UTimeZoneLocalOption nonExistingTimeOpt,
    // UTimeZoneLocalOption duplicatedTimeOpt, int32_t& rawOffset, int32_t& dstOffset, UErrorCode& status) with
    // nonExistingTimeOpt=DAYLIGHT_*/duplicatedTimeOpt=DAYLIGHT_*
    const int32_t OFFSETS3[][2] = {
        // April 2, 2006
        {-8*HOUR, 0},
        {-8*HOUR, 1*HOUR},
        {-8*HOUR, 1*HOUR},
        {-8*HOUR, 1*HOUR},
        {-8*HOUR, 1*HOUR},

        // October 29, 2006
        {-8*HOUR, 1*HOUR},
        {-8*HOUR, 1*HOUR},
        {-8*HOUR, 1*HOUR},
        {-8*HOUR, 0},
        {-8*HOUR, 0},
    };

    UErrorCode status = U_ZERO_ERROR;

    int32_t rawOffset, dstOffset;
    TimeZone* utc = TimeZone::createTimeZone("UTC");
    Calendar* cal = Calendar::createInstance(*utc, status);
    if (U_FAILURE(status)) {
        dataerrln("Calendar::createInstance failed: %s", u_errorName(status));
        return;
    }
    cal->clear();

    // Set up TimeZone objects - OlsonTimeZone, SimpleTimeZone and RuleBasedTimeZone
    BasicTimeZone *TESTZONES[NUM_TIMEZONES];

    TESTZONES[0] = dynamic_cast<BasicTimeZone*>(TimeZone::createTimeZone("America/Los_Angeles"));
    TESTZONES[1] = new SimpleTimeZone(-8*HOUR, "Simple Pacific Time",
                                        UCAL_APRIL, 1, UCAL_SUNDAY, 2*HOUR,
                                        UCAL_OCTOBER, -1, UCAL_SUNDAY, 2*HOUR, status);
    if (U_FAILURE(status)) {
        errln("SimpleTimeZone constructor failed");
        return;
    }

    InitialTimeZoneRule *ir = new InitialTimeZoneRule(
            "Pacific Standard Time", // Initial time Name
            -8*HOUR,        // Raw offset
            0*HOUR);        // DST saving amount

    RuleBasedTimeZone *rbPT = new RuleBasedTimeZone("Rule based Pacific Time", ir);

    DateTimeRule *dtr;
    AnnualTimeZoneRule *atzr;
    const int32_t STARTYEAR = 2000;

    dtr = new DateTimeRule(UCAL_APRIL, 1, UCAL_SUNDAY,
                        2*HOUR, DateTimeRule::WALL_TIME); // 1st Sunday in April, at 2AM wall time
    atzr = new AnnualTimeZoneRule("Pacific Daylight Time",
            -8*HOUR /* rawOffset */, 1*HOUR /* dstSavings */, dtr,
            STARTYEAR, AnnualTimeZoneRule::MAX_YEAR);
    rbPT->addTransitionRule(atzr, status);
    if (U_FAILURE(status)) {
        errln("Could not add DST start rule to the RuleBasedTimeZone rbPT");
        return;
    }

    dtr = new DateTimeRule(UCAL_OCTOBER, -1, UCAL_SUNDAY,
                        2*HOUR, DateTimeRule::WALL_TIME); // last Sunday in October, at 2AM wall time
    atzr = new AnnualTimeZoneRule("Pacific Standard Time",
            -8*HOUR /* rawOffset */, 0 /* dstSavings */, dtr,
            STARTYEAR, AnnualTimeZoneRule::MAX_YEAR);
    rbPT->addTransitionRule(atzr, status);
    if (U_FAILURE(status)) {
        errln("Could not add STD start rule to the RuleBasedTimeZone rbPT");
        return;
    }

    rbPT->complete(status);
    if (U_FAILURE(status)) {
        errln("complete() failed for RuleBasedTimeZone rbPT");
        return;
    }

    TESTZONES[2] = rbPT;

    // Calculate millis
    UDate MILLIS[NUM_DATES];
    for (int32_t i = 0; i < NUM_DATES; i++) {
        cal->clear();
        cal->set(DATES[i][0], DATES[i][1], DATES[i][2], DATES[i][3], DATES[i][4]);
        MILLIS[i] = cal->getTime(status);
        if (U_FAILURE(status)) {
            errln("cal->getTime failed");
            return;
        }
    }

    SimpleDateFormat df(UnicodeString("yyyy-MM-dd HH:mm:ss"), status);
    if (U_FAILURE(status)) {
        dataerrln("Failed to initialize a SimpleDateFormat - %s", u_errorName(status));
    }
    df.setTimeZone(*utc);
    UnicodeString dateStr;

    // Test getOffset(uint8_t era, int32_t year, int32_t month, int32_t day,
    // uint8_t dayOfWeek, int32_t millis, UErrorCode& status)
    for (int32_t i = 0; i < NUM_TIMEZONES; i++) {
        for (int32_t d = 0; d < NUM_DATES; d++) {
            status = U_ZERO_ERROR;
            int32_t offset = TESTZONES[i]->getOffset(GregorianCalendar::AD, DATES[d][0], DATES[d][1], DATES[d][2],
                                                UCAL_SUNDAY, DATES[d][5], status);
            if (U_FAILURE(status)) {
                errln((UnicodeString)"getOffset(era,year,month,day,dayOfWeek,millis,status) failed for TESTZONES[" + i + "]");
            } else if (offset != OFFSETS1[d]) {
                dateStr.remove();
                df.format(MILLIS[d], dateStr);
                dataerrln((UnicodeString)"Bad offset returned by TESTZONES[" + i + "] at "
                        + dateStr + "(standard) - Got: " + offset + " Expected: " + OFFSETS1[d]);
            }
        }
    }

    // Test getOffset(UDate date, UBool local, int32_t& rawOffset,
    // int32_t& dstOffset, UErrorCode& ec) with local = true
    for (int32_t i = 0; i < NUM_TIMEZONES; i++) {
        for (int32_t m = 0; m < NUM_DATES; m++) {
            status = U_ZERO_ERROR;
            TESTZONES[i]->getOffset(MILLIS[m], true, rawOffset, dstOffset, status);
            if (U_FAILURE(status)) {
                errln((UnicodeString)"getOffset(date,local,rawOfset,dstOffset,ec) failed for TESTZONES[" + i + "]");
            } else if (rawOffset != OFFSETS2[m][0] || dstOffset != OFFSETS2[m][1]) {
                dateStr.remove();
                df.format(MILLIS[m], dateStr);
                dataerrln((UnicodeString)"Bad offset returned by TESTZONES[" + i + "] at "
                        + dateStr + "(wall) - Got: "
                        + rawOffset + "/" + dstOffset
                        + " Expected: " + OFFSETS2[m][0] + "/" + OFFSETS2[m][1]);
            }
        }
    }

    // Test getOffsetFromLocal(UDate date, UTimeZoneLocalOption nonExistingTimeOpt, UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t& rawOffset, int32_t& dstOffset, UErroCode& status)
    // with nonExistingTimeOpt=STANDARD_FORMER/duplicatedTimeOpt=STANDARD_LATTER
    for (int32_t i = 0; i < NUM_TIMEZONES; i++) {
        for (int m = 0; m < NUM_DATES; m++) {
            status = U_ZERO_ERROR;
            TESTZONES[i]->getOffsetFromLocal(MILLIS[m], UCAL_TZ_LOCAL_STANDARD_FORMER, UCAL_TZ_LOCAL_STANDARD_LATTER,
                rawOffset, dstOffset, status);
            if (U_FAILURE(status)) {
                errln((UnicodeString)"getOffsetFromLocal with UCAL_TZ_LOCAL_STANDARD_FORMER/UCAL_TZ_LOCAL_STANDARD_LATTER failed for TESTZONES[" + i + "]");
            } else if (rawOffset != OFFSETS2[m][0] || dstOffset != OFFSETS2[m][1]) {
                dateStr.remove();
                df.format(MILLIS[m], dateStr);
                dataerrln((UnicodeString)"Bad offset returned by TESTZONES[" + i + "] at "
                        + dateStr + "(wall/STANDARD_FORMER/STANDARD_LATTER) - Got: "
                        + rawOffset + "/" + dstOffset
                        + " Expected: " + OFFSETS2[m][0] + "/" + OFFSETS2[m][1]);
            }
        }
    }

    // Test getOffsetFromLocal(UDate date, UTimeZoneLocalOption nonExistingTimeOpt, UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t& rawOffset, int32_t& dstOffset, UErroCode& status)
    // with nonExistingTimeOpt=DAYLIGHT_LATTER/duplicatedTimeOpt=DAYLIGHT_FORMER
    for (int32_t i = 0; i < NUM_TIMEZONES; i++) {
        for (int m = 0; m < NUM_DATES; m++) {
            status = U_ZERO_ERROR;
            TESTZONES[i]->getOffsetFromLocal(MILLIS[m], UCAL_TZ_LOCAL_DAYLIGHT_LATTER, UCAL_TZ_LOCAL_DAYLIGHT_FORMER,
                rawOffset, dstOffset, status);
            if (U_FAILURE(status)) {
                errln((UnicodeString)"getOffsetFromLocal with UCAL_TZ_LOCAL_DAYLIGHT_LATTER/UCAL_TZ_LOCAL_DAYLIGHT_FORMER failed for TESTZONES[" + i + "]");
            } else if (rawOffset != OFFSETS3[m][0] || dstOffset != OFFSETS3[m][1]) {
                dateStr.remove();
                df.format(MILLIS[m], dateStr);
                dataerrln((UnicodeString)"Bad offset returned by TESTZONES[" + i + "] at "
                        + dateStr + "(wall/DAYLIGHT_LATTER/DAYLIGHT_FORMER) - Got: "
                        + rawOffset + "/" + dstOffset
                        + " Expected: " + OFFSETS3[m][0] + "/" + OFFSETS3[m][1]);
            }
        }
    }

    // Test getOffsetFromLocal(UDate date, UTimeZoneLocalOption nonExistingTimeOpt, UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t& rawOffset, int32_t& dstOffset, UErroCode& status)
    // with nonExistingTimeOpt=FORMER/duplicatedTimeOpt=LATTER
    for (int32_t i = 0; i < NUM_TIMEZONES; i++) {
        for (int m = 0; m < NUM_DATES; m++) {
            status = U_ZERO_ERROR;
            TESTZONES[i]->getOffsetFromLocal(MILLIS[m], UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_LATTER,
                rawOffset, dstOffset, status);
            if (U_FAILURE(status)) {
                errln((UnicodeString)"getOffsetFromLocal with UCAL_TZ_LOCAL_FORMER/UCAL_TZ_LOCAL_LATTER failed for TESTZONES[" + i + "]");
            } else if (rawOffset != OFFSETS2[m][0] || dstOffset != OFFSETS2[m][1]) {
                dateStr.remove();
                df.format(MILLIS[m], dateStr);
                dataerrln((UnicodeString)"Bad offset returned by TESTZONES[" + i + "] at "
                        + dateStr + "(wall/FORMER/LATTER) - Got: "
                        + rawOffset + "/" + dstOffset
                        + " Expected: " + OFFSETS2[m][0] + "/" + OFFSETS2[m][1]);
            }
        }
    }

    // Test getOffsetFromLocal(UDate date, UTimeZoneLocalOption nonExistingTimeOpt, UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t& rawOffset, int32_t& dstOffset, UErroCode& status)
    // with nonExistingTimeOpt=LATTER/duplicatedTimeOpt=FORMER
    for (int32_t i = 0; i < NUM_TIMEZONES; i++) {
        for (int m = 0; m < NUM_DATES; m++) {
            status = U_ZERO_ERROR;
            TESTZONES[i]->getOffsetFromLocal(MILLIS[m], UCAL_TZ_LOCAL_LATTER, UCAL_TZ_LOCAL_FORMER,
                rawOffset, dstOffset, status);
            if (U_FAILURE(status)) {
                errln((UnicodeString)"getOffsetFromLocal with UCAL_TZ_LOCAL_LATTER/UCAL_TZ_LOCAL_FORMER failed for TESTZONES[" + i + "]");
            } else if (rawOffset != OFFSETS3[m][0] || dstOffset != OFFSETS3[m][1]) {
                dateStr.remove();
                df.format(MILLIS[m], dateStr);
                dataerrln((UnicodeString)"Bad offset returned by TESTZONES[" + i + "] at "
                        + dateStr + "(wall/LATTER/FORMER) - Got: "
                        + rawOffset + "/" + dstOffset
                        + " Expected: " + OFFSETS3[m][0] + "/" + OFFSETS3[m][1]);
            }
        }
    }

    for (int32_t i = 0; i < NUM_TIMEZONES; i++) {
        delete TESTZONES[i];
    }
    delete utc;
    delete cal;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
