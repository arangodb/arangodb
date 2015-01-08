/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2013, International Business Machines
 * Corporation and others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "dtfmttst.h"
#include "unicode/localpointer.h"
#include "unicode/timezone.h"
#include "unicode/gregocal.h"
#include "unicode/smpdtfmt.h"
#include "unicode/datefmt.h"
#include "unicode/dtptngen.h"
#include "unicode/simpletz.h"
#include "unicode/strenum.h"
#include "unicode/dtfmtsym.h"
#include "cmemory.h"
#include "cstring.h"
#include "caltest.h"  // for fieldName
#include <stdio.h> // for sprintf

#if U_PLATFORM_HAS_WIN32_API
#include "windttst.h"
#endif

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

#define ARRAY_SIZE(array) (sizeof array / sizeof array[0])

#define ASSERT_OK(status)  if(U_FAILURE(status)) {errcheckln(status, #status " = %s @ %s:%d", u_errorName(status), __FILE__, __LINE__); return; }

// *****************************************************************************
// class DateFormatTest
// *****************************************************************************

void DateFormatTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if(exec) {
        logln("TestSuite DateFormatTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestPatterns);
    TESTCASE_AUTO(TestEquals);
    TESTCASE_AUTO(TestTwoDigitYearDSTParse);
    TESTCASE_AUTO(TestFieldPosition);
    TESTCASE_AUTO(TestPartialParse994);
    TESTCASE_AUTO(TestRunTogetherPattern985);
    TESTCASE_AUTO(TestRunTogetherPattern917);
    TESTCASE_AUTO(TestCzechMonths459);
    TESTCASE_AUTO(TestLetterDPattern212);
    TESTCASE_AUTO(TestDayOfYearPattern195);
    TESTCASE_AUTO(TestQuotePattern161);
    TESTCASE_AUTO(TestBadInput135);
    TESTCASE_AUTO(TestBadInput135a);
    TESTCASE_AUTO(TestTwoDigitYear);
    TESTCASE_AUTO(TestDateFormatZone061);
    TESTCASE_AUTO(TestDateFormatZone146);
    TESTCASE_AUTO(TestLocaleDateFormat);
    TESTCASE_AUTO(TestWallyWedel);
    TESTCASE_AUTO(TestDateFormatCalendar);
    TESTCASE_AUTO(TestSpaceParsing);
    TESTCASE_AUTO(TestExactCountFormat);
    TESTCASE_AUTO(TestWhiteSpaceParsing);
    TESTCASE_AUTO(TestInvalidPattern);
    TESTCASE_AUTO(TestGeneral);
    TESTCASE_AUTO(TestGreekMay);
    TESTCASE_AUTO(TestGenericTime);
    TESTCASE_AUTO(TestGenericTimeZoneOrder);
    TESTCASE_AUTO(TestHost);
    TESTCASE_AUTO(TestEras);
    TESTCASE_AUTO(TestNarrowNames);
    TESTCASE_AUTO(TestShortDays);
    TESTCASE_AUTO(TestStandAloneDays);
    TESTCASE_AUTO(TestStandAloneMonths);
    TESTCASE_AUTO(TestQuarters);
    TESTCASE_AUTO(TestZTimeZoneParsing);
    TESTCASE_AUTO(TestRelative);
    TESTCASE_AUTO(TestRelativeClone);
    TESTCASE_AUTO(TestHostClone);
    TESTCASE_AUTO(TestTimeZoneDisplayName);
    TESTCASE_AUTO(TestRoundtripWithCalendar);
    TESTCASE_AUTO(Test6338);
    TESTCASE_AUTO(Test6726);
    TESTCASE_AUTO(TestGMTParsing);
    TESTCASE_AUTO(Test6880);
    TESTCASE_AUTO(TestISOEra);
    TESTCASE_AUTO(TestFormalChineseDate);
    TESTCASE_AUTO(TestNumberAsStringParsing);
    TESTCASE_AUTO(TestStandAloneGMTParse);
    TESTCASE_AUTO(TestParsePosition);
    TESTCASE_AUTO(TestMonthPatterns);
    TESTCASE_AUTO(TestContext);
    TESTCASE_AUTO(TestNonGregoFmtParse);
    /*
    TESTCASE_AUTO(TestRelativeError);
    TESTCASE_AUTO(TestRelativeOther);
    */
    TESTCASE_AUTO(TestDotAndAtLeniency);
    TESTCASE_AUTO(TestDateFormatLeniency);
    TESTCASE_AUTO_END;
}

void DateFormatTest::TestPatterns() {
    static const struct {
        const char *actualPattern;
        const char *expectedPattern;
        const char *localeID;
        const char *expectedLocalPattern;
    } EXPECTED[] = {
        {UDAT_YEAR, "y","en","y"},

        {UDAT_QUARTER, "QQQQ", "en", "QQQQ"},
        {UDAT_ABBR_QUARTER, "QQQ", "en", "QQQ"},
        {UDAT_YEAR_QUARTER, "yQQQQ", "en", "QQQQ y"}, 
        {UDAT_YEAR_ABBR_QUARTER, "yQQQ", "en", "QQQ y"},

        {UDAT_NUM_MONTH, "M", "en", "L"},
        {UDAT_ABBR_MONTH, "MMM", "en", "LLL"},
        {UDAT_MONTH, "MMMM", "en", "LLLL"},
        {UDAT_YEAR_NUM_MONTH, "yM","en","M/y"}, 
        {UDAT_YEAR_ABBR_MONTH, "yMMM","en","MMM y"},
        {UDAT_YEAR_MONTH, "yMMMM","en","MMMM y"},

        {UDAT_DAY, "d","en","d"},
        {UDAT_YEAR_NUM_MONTH_DAY, "yMd", "en", "M/d/y"}, 
        {UDAT_YEAR_ABBR_MONTH_DAY, "yMMMd", "en", "MMM d, y"},
        {UDAT_YEAR_MONTH_DAY, "yMMMMd", "en", "MMMM d, y"},
        {UDAT_YEAR_NUM_MONTH_WEEKDAY_DAY, "yMEd", "en", "EEE, M/d/y"}, 
        {UDAT_YEAR_ABBR_MONTH_WEEKDAY_DAY, "yMMMEd", "en", "EEE, MMM d, y"},
        {UDAT_YEAR_MONTH_WEEKDAY_DAY, "yMMMMEEEEd", "en", "EEEE, MMMM d, y"},

        {UDAT_NUM_MONTH_DAY, "Md","en","M/d"},
        {UDAT_ABBR_MONTH_DAY, "MMMd","en","MMM d"},
        {UDAT_MONTH_DAY, "MMMMd","en","MMMM d"},
        {UDAT_NUM_MONTH_WEEKDAY_DAY, "MEd","en","EEE, M/d"},
        {UDAT_ABBR_MONTH_WEEKDAY_DAY, "MMMEd","en","EEE, MMM d"},
        {UDAT_MONTH_WEEKDAY_DAY, "MMMMEEEEd","en","EEEE, MMMM d"},

        {UDAT_HOUR, "j", "en", "h a"}, // (fixed expected result per ticket 6872<-6626)
        {UDAT_HOUR24, "H", "en", "HH"}, // (fixed expected result per ticket 6872<-6626

        {UDAT_MINUTE, "m", "en", "m"},
        {UDAT_HOUR_MINUTE, "jm","en","h:mm a"}, // (fixed expected result per ticket 6872<-7180)
        {UDAT_HOUR24_MINUTE, "Hm", "en", "HH:mm"}, // (fixed expected result per ticket 6872<-6626)

        {UDAT_SECOND, "s", "en", "s"},
        {UDAT_HOUR_MINUTE_SECOND, "jms","en","h:mm:ss a"}, // (fixed expected result per ticket 6872<-7180)
        {UDAT_HOUR24_MINUTE_SECOND, "Hms","en","HH:mm:ss"}, // (fixed expected result per ticket 6872<-6626)
        {UDAT_MINUTE_SECOND, "ms", "en", "mm:ss"}, // (fixed expected result per ticket 6872<-6626)

        {UDAT_LOCATION_TZ, "VVVV", "en", "VVVV"},
        {UDAT_GENERIC_TZ, "vvvv", "en", "vvvv"},
        {UDAT_ABBR_GENERIC_TZ, "v", "en", "v"},
        {UDAT_SPECIFIC_TZ, "zzzz", "en", "zzzz"},
        {UDAT_ABBR_SPECIFIC_TZ, "z", "en", "z"},
        {UDAT_ABBR_UTC_TZ, "ZZZZ", "en", "ZZZZ"},

        {UDAT_YEAR_NUM_MONTH_DAY UDAT_ABBR_UTC_TZ, "yMdZZZZ", "en", "M/d/y, ZZZZ"},
        {UDAT_MONTH_DAY UDAT_LOCATION_TZ, "MMMMdVVVV", "en", "MMMM d, VVVV"}
    };

    IcuTestErrorCode errorCode(*this, "TestPatterns()");
    for (int32_t i = 0; i < LENGTHOF(EXPECTED); i++) {
        // Verify that patterns have the correct values
        UnicodeString actualPattern(EXPECTED[i].actualPattern, -1, US_INV);
        UnicodeString expectedPattern(EXPECTED[i].expectedPattern, -1, US_INV);
        Locale locale(EXPECTED[i].localeID);
        if (actualPattern != expectedPattern) {
            errln("FAILURE! Expected pattern: " + expectedPattern + 
                    " but was: " + actualPattern);
        }

        // Verify that DataFormat instances produced contain the correct 
        // localized patterns
        // TODO: use DateFormat::getInstanceForSkeleton(), ticket #9029
        // Java test code:
        // DateFormat date1 = DateFormat.getPatternInstance(actualPattern, 
        //         locale);
        // DateFormat date2 = DateFormat.getPatternInstance(Calendar.getInstance(locale),
        //         actualPattern, locale);
        LocalPointer<DateTimePatternGenerator> generator(
                DateTimePatternGenerator::createInstance(locale, errorCode));
        if(errorCode.logDataIfFailureAndReset("DateTimePatternGenerator::createInstance() failed for locale ID \"%s\"", EXPECTED[i].localeID)) {
            continue;
        }
        UnicodeString pattern = generator->getBestPattern(actualPattern, errorCode);
        SimpleDateFormat date1(pattern, locale, errorCode);
        SimpleDateFormat date2(pattern, locale, errorCode);
        date2.adoptCalendar(Calendar::createInstance(locale, errorCode));
        if(errorCode.logIfFailureAndReset("DateFormat::getInstanceForSkeleton() failed")) {
            errln("  for actualPattern \"%s\" & locale ID \"%s\"",
                  EXPECTED[i].actualPattern, EXPECTED[i].localeID);
            continue;
        }

        UnicodeString expectedLocalPattern(EXPECTED[i].expectedLocalPattern, -1, US_INV);
        UnicodeString actualLocalPattern1;
        UnicodeString actualLocalPattern2;
        date1.toLocalizedPattern(actualLocalPattern1, errorCode);
        date2.toLocalizedPattern(actualLocalPattern2, errorCode);
        if (actualLocalPattern1 != expectedLocalPattern) {
            errln("FAILURE! Expected local pattern: " + expectedLocalPattern 
                    + " but was: " + actualLocalPattern1);
        }
        if (actualLocalPattern2 != expectedLocalPattern) {
            errln("FAILURE! Expected local pattern: " + expectedLocalPattern 
                    + " but was: " + actualLocalPattern2);
        }
    }
}

// Test written by Wally Wedel and emailed to me.
void DateFormatTest::TestWallyWedel()
{
    UErrorCode status = U_ZERO_ERROR;
    /*
     * Instantiate a TimeZone so we can get the ids.
     */
    TimeZone *tz = new SimpleTimeZone(7,"");
    /*
     * Computational variables.
     */
    int32_t offset, hours, minutes, seconds;
    /*
     * Instantiate a SimpleDateFormat set up to produce a full time
     zone name.
     */
    SimpleDateFormat *sdf = new SimpleDateFormat((UnicodeString)"zzzz", status);
    /*
     * A String array for the time zone ids.
     */
    int32_t ids_length;
    StringEnumeration* ids = TimeZone::createEnumeration();
    if (ids == NULL) {
        dataerrln("Unable to create TimeZone enumeration.");
        if (sdf != NULL) {
            delete sdf;
        }
        return;
    }
    ids_length = ids->count(status);
    /*
     * How many ids do we have?
     */
    logln("Time Zone IDs size: %d", ids_length);
    /*
     * Column headings (sort of)
     */
    logln("Ordinal ID offset(h:m) name");
    /*
     * Loop through the tzs.
     */
    UDate today = Calendar::getNow();
    Calendar *cal = Calendar::createInstance(status);
    for (int32_t i = 0; i < ids_length; i++) {
        // logln(i + " " + ids[i]);
        const UnicodeString* id = ids->snext(status);
        TimeZone *ttz = TimeZone::createTimeZone(*id);
        // offset = ttz.getRawOffset();
        cal->setTimeZone(*ttz);
        cal->setTime(today, status);
        offset = cal->get(UCAL_ZONE_OFFSET, status) + cal->get(UCAL_DST_OFFSET, status);
        // logln(i + " " + ids[i] + " offset " + offset);
        const char* sign = "+";
        if (offset < 0) {
            sign = "-";
            offset = -offset;
        }
        hours = offset/3600000;
        minutes = (offset%3600000)/60000;
        seconds = (offset%60000)/1000;
        UnicodeString dstOffset = (UnicodeString)"" + sign + (hours < 10 ? "0" : "") +
            (int32_t)hours + ":" + (minutes < 10 ? "0" : "") + (int32_t)minutes;
        if (seconds != 0) {
            dstOffset = dstOffset + ":" + (seconds < 10 ? "0" : "") + seconds;
        }
        /*
         * Instantiate a date so we can display the time zone name.
         */
        sdf->setTimeZone(*ttz);
        /*
         * Format the output.
         */
        UnicodeString fmtOffset;
        FieldPosition pos(0);
        sdf->format(today,fmtOffset, pos);
        // UnicodeString fmtOffset = tzS.toString();
        UnicodeString *fmtDstOffset = 0;
        if (fmtOffset.startsWith("GMT") && fmtOffset.length() != 3)
        {
            //fmtDstOffset = fmtOffset->substring(3);
            fmtDstOffset = new UnicodeString();
            fmtOffset.extract(3, fmtOffset.length(), *fmtDstOffset);
        }
        /*
         * Show our result.
         */
        UBool ok = fmtDstOffset == 0 || *fmtDstOffset == dstOffset;
        if (ok)
        {
            logln(UnicodeString() + i + " " + *id + " " + dstOffset +
                  " " + fmtOffset +
                  (fmtDstOffset != 0 ? " ok" : " ?"));
        }
        else
        {
            errln(UnicodeString() + i + " " + *id + " " + dstOffset +
                  " " + fmtOffset + " *** FAIL ***");
        }
        delete ttz;
        delete fmtDstOffset;
    }
    delete cal;
    //  delete ids;   // TODO:  BAD API
    delete ids;
    delete sdf;
    delete tz;
}

// -------------------------------------

/**
 * Test operator==
 */
void
DateFormatTest::TestEquals()
{
    DateFormat* fmtA = DateFormat::createDateTimeInstance(DateFormat::MEDIUM, DateFormat::FULL);
    DateFormat* fmtB = DateFormat::createDateTimeInstance(DateFormat::MEDIUM, DateFormat::FULL);
    if ( fmtA == NULL || fmtB == NULL){
        dataerrln("Error calling DateFormat::createDateTimeInstance");
        delete fmtA;
        delete fmtB;
        return;
    }

    if (!(*fmtA == *fmtB)) errln((UnicodeString)"FAIL");
    delete fmtA;
    delete fmtB;

    TimeZone* test = TimeZone::createTimeZone("PDT");
    delete test;
}

// -------------------------------------

/**
 * Test the parsing of 2-digit years.
 */
void
DateFormatTest::TestTwoDigitYearDSTParse(void)
{
    UErrorCode status = U_ZERO_ERROR;
    SimpleDateFormat* fullFmt = new SimpleDateFormat((UnicodeString)"EEE MMM dd HH:mm:ss.SSS zzz yyyy G", status);
    SimpleDateFormat *fmt = new SimpleDateFormat((UnicodeString)"dd-MMM-yy h:mm:ss 'o''clock' a z", Locale::getEnglish(), status);
    //DateFormat* fmt = DateFormat::createDateTimeInstance(DateFormat::MEDIUM, DateFormat::FULL, Locale::ENGLISH);
    UnicodeString* s = new UnicodeString("03-Apr-04 2:20:47 o'clock AM PST", "");
    TimeZone* defaultTZ = TimeZone::createDefault();
    TimeZone* PST = TimeZone::createTimeZone("PST");
    int32_t defaultOffset = defaultTZ->getRawOffset();
    int32_t PSTOffset = PST->getRawOffset();
    int32_t hour = 2 + (defaultOffset - PSTOffset) / (60*60*1000);
    // hour is the expected hour of day, in units of seconds
    hour = ((hour < 0) ? hour + 24 : hour) * 60*60;

    UnicodeString str;

    if(U_FAILURE(status)) {
        dataerrln("Could not set up test. exitting - %s", u_errorName(status));
        return;
    }

    UDate d = fmt->parse(*s, status);
    logln(*s + " P> " + ((DateFormat*)fullFmt)->format(d, str));
    int32_t y, m, day, hr, min, sec;
    dateToFields(d, y, m, day, hr, min, sec);
    hour += defaultTZ->inDaylightTime(d, status) ? 1 : 0;
    hr = hr*60*60;
    if (hr != hour)
        errln((UnicodeString)"FAIL: Should parse to hour " + hour + " but got " + hr);

    if (U_FAILURE(status))
        errln((UnicodeString)"FAIL: " + (int32_t)status);

    delete s;
    delete fmt;
    delete fullFmt;
    delete PST;
    delete defaultTZ;
}

// -------------------------------------

UChar toHexString(int32_t i) { return (UChar)(i + (i < 10 ? 0x30 : (0x41 - 10))); }

UnicodeString&
DateFormatTest::escape(UnicodeString& s)
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

/**
 * This MUST be kept in sync with DateFormatSymbols.gPatternChars.
 */
static const char* PATTERN_CHARS = "GyMdkHmsSEDFwWahKzYeugAZvcLQqVUOXx";

/**
 * A list of the names of all the fields in DateFormat.
 * This MUST be kept in sync with DateFormat.
 */
static const char* DATEFORMAT_FIELD_NAMES[] = {
    "ERA_FIELD",
    "YEAR_FIELD",
    "MONTH_FIELD",
    "DATE_FIELD",
    "HOUR_OF_DAY1_FIELD",
    "HOUR_OF_DAY0_FIELD",
    "MINUTE_FIELD",
    "SECOND_FIELD",
    "MILLISECOND_FIELD",
    "DAY_OF_WEEK_FIELD",
    "DAY_OF_YEAR_FIELD",
    "DAY_OF_WEEK_IN_MONTH_FIELD",
    "WEEK_OF_YEAR_FIELD",
    "WEEK_OF_MONTH_FIELD",
    "AM_PM_FIELD",
    "HOUR1_FIELD",
    "HOUR0_FIELD",
    "TIMEZONE_FIELD",
    "YEAR_WOY_FIELD",
    "DOW_LOCAL_FIELD",
    "EXTENDED_YEAR_FIELD",
    "JULIAN_DAY_FIELD",
    "MILLISECONDS_IN_DAY_FIELD",
    "TIMEZONE_RFC_FIELD",
    "GENERIC_TIMEZONE_FIELD",
    "STAND_ALONE_DAY_FIELD",
    "STAND_ALONE_MONTH_FIELD",
    "QUARTER_FIELD",
    "STAND_ALONE_QUARTER_FIELD",
    "TIMEZONE_SPECIAL_FIELD",
    "YEAR_NAME_FIELD",
    "TIMEZONE_LOCALIZED_GMT_OFFSET_FIELD",
    "TIMEZONE_ISO_FIELD",
    "TIMEZONE_ISO_LOCAL_FIELD",
};

static const int32_t DATEFORMAT_FIELD_NAMES_LENGTH =
    sizeof(DATEFORMAT_FIELD_NAMES) / sizeof(DATEFORMAT_FIELD_NAMES[0]);

/**
 * Verify that returned field position indices are correct.
 */
void DateFormatTest::TestFieldPosition() {
    UErrorCode ec = U_ZERO_ERROR;
    int32_t i, j, exp;
    UnicodeString buf;

    // Verify data
    DateFormatSymbols rootSyms(Locale(""), ec);
    if (U_FAILURE(ec)) {
        dataerrln("Unable to create DateFormatSymbols - %s", u_errorName(ec));
        return;
    }

    // local pattern chars data is not longer loaded
    // from icu locale bundle
    assertEquals("patternChars", PATTERN_CHARS, rootSyms.getLocalPatternChars(buf));
    assertEquals("patternChars", PATTERN_CHARS, DateFormatSymbols::getPatternUChars());
    assertTrue("DATEFORMAT_FIELD_NAMES", DATEFORMAT_FIELD_NAMES_LENGTH == UDAT_FIELD_COUNT);
    assertTrue("Data", UDAT_FIELD_COUNT == uprv_strlen(PATTERN_CHARS));

    // Create test formatters
    const int32_t COUNT = 4;
    DateFormat* dateFormats[COUNT];
    dateFormats[0] = DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull, Locale::getUS());
    dateFormats[1] = DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull, Locale::getFrance());
    // Make the pattern "G y M d..."
    buf.remove().append(PATTERN_CHARS);
    for (j=buf.length()-1; j>=0; --j) buf.insert(j, (UChar)32/*' '*/);
    dateFormats[2] = new SimpleDateFormat(buf, Locale::getUS(), ec);
    // Make the pattern "GGGG yyyy MMMM dddd..."
    for (j=buf.length()-1; j>=0; j-=2) {
        for (i=0; i<3; ++i) {
            buf.insert(j, buf.charAt(j));
        }
    }
    dateFormats[3] = new SimpleDateFormat(buf, Locale::getUS(), ec);
    if(U_FAILURE(ec)){
        errln(UnicodeString("Could not create SimpleDateFormat object for locale en_US. Error: " )+ UnicodeString(u_errorName(ec)));
        return;
    }
    UDate aug13 = 871508052513.0;

    // Expected output field values for above DateFormats on aug13
    // Fields are given in order of DateFormat field number
    const char* EXPECTED[] = {
        "", "1997", "August", "13", "", "", "34", "12", "", "Wednesday",
        "", "", "", "", "PM", "2", "", "Pacific Daylight Time", "", "",
        "", "", "", "", "", "", "", "", "", "",
        "", "", "", "",

        "", "1997", "ao\\u00FBt", "13", "", "14", "34", "12", "", "mercredi",
        "", "", "", "", "", "", "", "heure avanc\\u00e9e du Pacifique", "", "",
        "", "", "", "", "",  "", "", "", "", "",
        "", "", "", "",

        "AD", "1997", "8", "13", "14", "14", "34", "12", "5", "Wed",
        "225", "2", "33", "3", "PM", "2", "2", "PDT", "1997", "4",
        "1997", "2450674", "52452513", "-0700", "PT",  "4", "8", "3", "3", "uslax",
        "1997", "GMT-7", "-07", "-07",

        "Anno Domini", "1997", "August", "0013", "0014", "0014", "0034", "0012", "5130", "Wednesday",
        "0225", "0002", "0033", "0003", "PM", "0002", "0002", "Pacific Daylight Time", "1997", "Wednesday",
        "1997", "2450674", "52452513", "GMT-07:00", "Pacific Time",  "Wednesday", "August", "3rd quarter", "3rd quarter", "Los Angeles Time",
        "1997", "GMT-07:00", "-0700", "-0700",
    };

    const int32_t EXPECTED_LENGTH = sizeof(EXPECTED)/sizeof(EXPECTED[0]);

    assertTrue("data size", EXPECTED_LENGTH == COUNT * UDAT_FIELD_COUNT);

    TimeZone* PT = TimeZone::createTimeZone("America/Los_Angeles");
    for (j = 0, exp = 0; j < COUNT; ++j) {
        //  String str;
        DateFormat* df = dateFormats[j];
        df->setTimeZone(*PT);
        SimpleDateFormat* sdtfmt = dynamic_cast<SimpleDateFormat*>(df);
        if (sdtfmt != NULL) {
            logln(" Pattern = " + sdtfmt->toPattern(buf.remove()));
        } else {
            logln(" Pattern = ? (not a SimpleDateFormat)");
        }
        logln((UnicodeString)"  Result = " + df->format(aug13, buf.remove()));

        int32_t expBase = exp; // save for later
        for (i = 0; i < UDAT_FIELD_COUNT; ++i, ++exp) {
            FieldPosition pos(i);
            buf.remove();
            df->format(aug13, buf, pos);
            UnicodeString field;
            buf.extractBetween(pos.getBeginIndex(), pos.getEndIndex(), field);
            assertEquals((UnicodeString)"field #" + i + " " + DATEFORMAT_FIELD_NAMES[i],
                         ctou(EXPECTED[exp]), field);
        }

        // test FieldPositionIterator API
        logln("FieldPositionIterator");
        {
          UErrorCode status = U_ZERO_ERROR;
          FieldPositionIterator posIter;
          FieldPosition fp;

          buf.remove();
          df->format(aug13, buf, &posIter, status);
          while (posIter.next(fp)) {
            int32_t i = fp.getField();
            UnicodeString field;
            buf.extractBetween(fp.getBeginIndex(), fp.getEndIndex(), field);
            assertEquals((UnicodeString)"field #" + i + " " + DATEFORMAT_FIELD_NAMES[i],
                         ctou(EXPECTED[expBase + i]), field);
          }

        }
    }


    // test null posIter
    buf.remove();
    UErrorCode status = U_ZERO_ERROR;
    dateFormats[0]->format(aug13, buf, NULL, status);
    // if we didn't crash, we succeeded.

    for (i=0; i<COUNT; ++i) {
        delete dateFormats[i];
    }
    delete PT;
}

// -------------------------------------

/**
 * General parse/format tests.  Add test cases as needed.
 */
void DateFormatTest::TestGeneral() {
    const char* DATA[] = {
        "yyyy MM dd HH:mm:ss.SSS",

        // Milliseconds are left-justified, since they format as fractions of a second
        "y/M/d H:mm:ss.S", "fp", "2004 03 10 16:36:31.567", "2004/3/10 16:36:31.5", "2004 03 10 16:36:31.500",
        "y/M/d H:mm:ss.SS", "fp", "2004 03 10 16:36:31.567", "2004/3/10 16:36:31.56", "2004 03 10 16:36:31.560",
        "y/M/d H:mm:ss.SSS", "F", "2004 03 10 16:36:31.567", "2004/3/10 16:36:31.567",
        "y/M/d H:mm:ss.SSSS", "pf", "2004/3/10 16:36:31.5679", "2004 03 10 16:36:31.567", "2004/3/10 16:36:31.5670",
    };
    expect(DATA, ARRAY_SIZE(DATA), Locale("en", "", ""));
}

// -------------------------------------

/**
 * Verify that strings which contain incomplete specifications are parsed
 * correctly.  In some instances, this means not being parsed at all, and
 * returning an appropriate error.
 */
void
DateFormatTest::TestPartialParse994()
{
    UErrorCode status = U_ZERO_ERROR;
    SimpleDateFormat* f = new SimpleDateFormat(status);
    if (U_FAILURE(status)) {
        dataerrln("Fail new SimpleDateFormat: %s", u_errorName(status));
        delete f;
        return;
    }
    UDate null = 0;
    tryPat994(f, "yy/MM/dd HH:mm:ss", "97/01/17 10:11:42", date(97, 1 - 1, 17, 10, 11, 42));
    tryPat994(f, "yy/MM/dd HH:mm:ss", "97/01/17 10:", null);
    tryPat994(f, "yy/MM/dd HH:mm:ss", "97/01/17 10", null);
    tryPat994(f, "yy/MM/dd HH:mm:ss", "97/01/17 ", null);
    tryPat994(f, "yy/MM/dd HH:mm:ss", "97/01/17", null);
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
    delete f;
}

// -------------------------------------

void
DateFormatTest::tryPat994(SimpleDateFormat* format, const char* pat, const char* str, UDate expected)
{
    UErrorCode status = U_ZERO_ERROR;
    UDate null = 0;
    logln(UnicodeString("Pattern \"") + pat + "\"   String \"" + str + "\"");
    //try {
        format->applyPattern(pat);
        UDate date = format->parse(str, status);
        if (U_FAILURE(status) || date == null)
        {
            logln((UnicodeString)"ParseException: " + (int32_t)status);
            if (expected != null) errln((UnicodeString)"FAIL: Expected " + dateToString(expected));
        }
        else
        {
            UnicodeString f;
            ((DateFormat*)format)->format(date, f);
            logln(UnicodeString(" parse(") + str + ") -> " + dateToString(date));
            logln((UnicodeString)" format -> " + f);
            if (expected == null ||
                !(date == expected)) errln((UnicodeString)"FAIL: Expected null");//" + expected);
            if (!(f == str)) errln(UnicodeString("FAIL: Expected ") + str);
        }
    //}
    //catch(ParseException e) {
    //    logln((UnicodeString)"ParseException: " + e.getMessage());
    //    if (expected != null) errln((UnicodeString)"FAIL: Expected " + dateToString(expected));
    //}
    //catch(Exception e) {
    //    errln((UnicodeString)"*** Exception:");
    //    e.printStackTrace();
    //}
}

// -------------------------------------

/**
 * Verify the behavior of patterns in which digits for different fields run together
 * without intervening separators.
 */
void
DateFormatTest::TestRunTogetherPattern985()
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString format("yyyyMMddHHmmssSSS");
    UnicodeString now, then;
    //UBool flag;
    SimpleDateFormat *formatter = new SimpleDateFormat(format, status);
    if (U_FAILURE(status)) {
        dataerrln("Fail new SimpleDateFormat: %s", u_errorName(status));
        delete formatter;
        return;
    }
    UDate date1 = Calendar::getNow();
    ((DateFormat*)formatter)->format(date1, now);
    logln(now);
    ParsePosition pos(0);
    UDate date2 = formatter->parse(now, pos);
    if (date2 == 0) then = UnicodeString("Parse stopped at ") + pos.getIndex();
    else ((DateFormat*)formatter)->format(date2, then);
    logln(then);
    if (!(date2 == date1)) errln((UnicodeString)"FAIL");
    delete formatter;
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
}

// -------------------------------------

/**
 * Verify the behavior of patterns in which digits for different fields run together
 * without intervening separators.
 */
void
DateFormatTest::TestRunTogetherPattern917()
{
    UErrorCode status = U_ZERO_ERROR;
    SimpleDateFormat* fmt;
    UnicodeString myDate;
    fmt = new SimpleDateFormat((UnicodeString)"yyyy/MM/dd", status);
    if (U_FAILURE(status)) {
        dataerrln("Fail new SimpleDateFormat: %s", u_errorName(status));
        delete fmt;
        return;
    }
    myDate = "1997/02/03";
    testIt917(fmt, myDate, date(97, 2 - 1, 3));
    delete fmt;
    fmt = new SimpleDateFormat((UnicodeString)"yyyyMMdd", status);
    myDate = "19970304";
    testIt917(fmt, myDate, date(97, 3 - 1, 4));
    delete fmt;
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
}

// -------------------------------------

void
DateFormatTest::testIt917(SimpleDateFormat* fmt, UnicodeString& str, UDate expected)
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString pattern;
    logln((UnicodeString)"pattern=" + fmt->toPattern(pattern) + "   string=" + str);
    Formattable o;
    //try {
        ((Format*)fmt)->parseObject(str, o, status);
    //}
    if (U_FAILURE(status)) return;
    //catch(ParseException e) {
    //    e.printStackTrace();
    //    return;
    //}
    logln((UnicodeString)"Parsed object: " + dateToString(o.getDate()));
    if (!(o.getDate() == expected)) errln((UnicodeString)"FAIL: Expected " + dateToString(expected));
    UnicodeString formatted; ((Format*)fmt)->format(o, formatted, status);
    logln((UnicodeString)"Formatted string: " + formatted);
    if (!(formatted == str)) errln((UnicodeString)"FAIL: Expected " + str);
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
}

// -------------------------------------

/**
 * Verify the handling of Czech June and July, which have the unique attribute that
 * one is a proper prefix substring of the other.
 */
void
DateFormatTest::TestCzechMonths459()
{
    UErrorCode status = U_ZERO_ERROR;
    DateFormat* fmt = DateFormat::createDateInstance(DateFormat::FULL, Locale("cs", "", ""));
    if (fmt == NULL){
        dataerrln("Error calling DateFormat::createDateInstance()");
        return;
    }

    UnicodeString pattern;
    logln((UnicodeString)"Pattern " + ((SimpleDateFormat*) fmt)->toPattern(pattern));
    UDate june = date(97, UCAL_JUNE, 15);
    UDate july = date(97, UCAL_JULY, 15);
    UnicodeString juneStr; fmt->format(june, juneStr);
    UnicodeString julyStr; fmt->format(july, julyStr);
    //try {
        logln((UnicodeString)"format(June 15 1997) = " + juneStr);
        UDate d = fmt->parse(juneStr, status);
        UnicodeString s; fmt->format(d, s);
        int32_t month,yr,day,hr,min,sec; dateToFields(d,yr,month,day,hr,min,sec);
        logln((UnicodeString)"  -> parse -> " + s + " (month = " + month + ")");
        if (month != UCAL_JUNE) errln((UnicodeString)"FAIL: Month should be June");
        logln((UnicodeString)"format(July 15 1997) = " + julyStr);
        d = fmt->parse(julyStr, status);
        fmt->format(d, s);
        dateToFields(d,yr,month,day,hr,min,sec);
        logln((UnicodeString)"  -> parse -> " + s + " (month = " + month + ")");
        if (month != UCAL_JULY) errln((UnicodeString)"FAIL: Month should be July");
    //}
    //catch(ParseException e) {
    if (U_FAILURE(status))
        errln((UnicodeString)"Exception: " + (int32_t)status);
    //}
    delete fmt;
}

// -------------------------------------

/**
 * Test the handling of 'D' in patterns.
 */
void
DateFormatTest::TestLetterDPattern212()
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString dateString("1995-040.05:01:29");
    UnicodeString bigD("yyyy-DDD.hh:mm:ss");
    UnicodeString littleD("yyyy-ddd.hh:mm:ss");
    UDate expLittleD = date(95, 0, 1, 5, 1, 29);
    UDate expBigD = expLittleD + 39 * 24 * 3600000.0;
    expLittleD = expBigD; // Expect the same, with default lenient parsing
    logln((UnicodeString)"dateString= " + dateString);
    SimpleDateFormat *formatter = new SimpleDateFormat(bigD, status);
    if (U_FAILURE(status)) {
        dataerrln("Fail new SimpleDateFormat: %s", u_errorName(status));
        delete formatter;
        return;
    }
    ParsePosition pos(0);
    UDate myDate = formatter->parse(dateString, pos);
    logln((UnicodeString)"Using " + bigD + " -> " + myDate);
    if (myDate != expBigD) errln((UnicodeString)"FAIL: bigD - Expected " + dateToString(expBigD));
    delete formatter;
    formatter = new SimpleDateFormat(littleD, status);
    ASSERT_OK(status);
    pos = ParsePosition(0);
    myDate = formatter->parse(dateString, pos);
    logln((UnicodeString)"Using " + littleD + " -> " + dateToString(myDate));
    if (myDate != expLittleD) errln((UnicodeString)"FAIL: littleD - Expected " + dateToString(expLittleD));
    delete formatter;
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
}

// -------------------------------------

/**
 * Test the day of year pattern.
 */
void
DateFormatTest::TestDayOfYearPattern195()
{
    UErrorCode status = U_ZERO_ERROR;
    UDate today = Calendar::getNow();
    int32_t year,month,day,hour,min,sec; dateToFields(today,year,month,day,hour,min,sec);
    UDate expected = date(year, month, day);
    logln((UnicodeString)"Test Date: " + dateToString(today));
    SimpleDateFormat* sdf = (SimpleDateFormat*)DateFormat::createDateInstance();
    if (sdf == NULL){
        dataerrln("Error calling DateFormat::createDateInstance()");
        return;
    }
    tryPattern(*sdf, today, 0, expected);
    tryPattern(*sdf, today, "G yyyy DDD", expected);
    delete sdf;
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
}

// -------------------------------------

void
DateFormatTest::tryPattern(SimpleDateFormat& sdf, UDate d, const char* pattern, UDate expected)
{
    UErrorCode status = U_ZERO_ERROR;
    if (pattern != 0) sdf.applyPattern(pattern);
    UnicodeString thePat;
    logln((UnicodeString)"pattern: " + sdf.toPattern(thePat));
    UnicodeString formatResult; (*(DateFormat*)&sdf).format(d, formatResult);
    logln((UnicodeString)" format -> " + formatResult);
    // try {
        UDate d2 = sdf.parse(formatResult, status);
        logln((UnicodeString)" parse(" + formatResult + ") -> " + dateToString(d2));
        if (d2 != expected) errln((UnicodeString)"FAIL: Expected " + dateToString(expected));
        UnicodeString format2; (*(DateFormat*)&sdf).format(d2, format2);
        logln((UnicodeString)" format -> " + format2);
        if (!(formatResult == format2)) errln((UnicodeString)"FAIL: Round trip drift");
    //}
    //catch(Exception e) {
    if (U_FAILURE(status))
        errln((UnicodeString)"Error: " + (int32_t)status);
    //}
}

// -------------------------------------

/**
 * Test the handling of single quotes in patterns.
 */
void
DateFormatTest::TestQuotePattern161()
{
    UErrorCode status = U_ZERO_ERROR;
    SimpleDateFormat* formatter = new SimpleDateFormat((UnicodeString)"MM/dd/yyyy 'at' hh:mm:ss a zzz", status);
    if (U_FAILURE(status)) {
        dataerrln("Fail new SimpleDateFormat: %s", u_errorName(status));
        delete formatter;
        return;
    }
    UDate currentTime_1 = date(97, UCAL_AUGUST, 13, 10, 42, 28);
    UnicodeString dateString; ((DateFormat*)formatter)->format(currentTime_1, dateString);
    UnicodeString exp("08/13/1997 at 10:42:28 AM ");
    logln((UnicodeString)"format(" + dateToString(currentTime_1) + ") = " + dateString);
    if (0 != dateString.compareBetween(0, exp.length(), exp, 0, exp.length())) errln((UnicodeString)"FAIL: Expected " + exp);
    delete formatter;
    if (U_FAILURE(status)) errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
}

// -------------------------------------

/**
 * Verify the correct behavior when handling invalid input strings.
 */
void
DateFormatTest::TestBadInput135()
{
    UErrorCode status = U_ZERO_ERROR;
    DateFormat::EStyle looks[] = {
        DateFormat::SHORT, DateFormat::MEDIUM, DateFormat::LONG, DateFormat::FULL
    };
    int32_t looks_length = (int32_t)(sizeof(looks) / sizeof(looks[0]));
    const char* strings[] = {
        "Mar 15", "Mar 15 1997", "asdf", "3/1/97 1:23:", "3/1/00 1:23:45 AM"
    };
    int32_t strings_length = (int32_t)(sizeof(strings) / sizeof(strings[0]));
    DateFormat *full = DateFormat::createDateTimeInstance(DateFormat::LONG, DateFormat::LONG);
    if(full==NULL) {
      dataerrln("could not create date time instance");
      return;
    }
    UnicodeString expected("March 1, 2000 at 1:23:45 AM ");
    for (int32_t i = 0; i < strings_length;++i) {
        const char* text = strings[i];
        for (int32_t j = 0; j < looks_length;++j) {
            DateFormat::EStyle dateLook = looks[j];
            for (int32_t k = 0; k < looks_length;++k) {
                DateFormat::EStyle timeLook = looks[k];
                DateFormat *df = DateFormat::createDateTimeInstance(dateLook, timeLook);
                if (df == NULL){
                    dataerrln("Error calling DateFormat::createDateTimeInstance()");
                    continue;
                }
                UnicodeString prefix = UnicodeString(text) + ", " + dateLook + "/" + timeLook + ": ";
                //try {
                    UDate when = df->parse(text, status);
                    if (when == 0 && U_SUCCESS(status)) {
                        errln(prefix + "SHOULD NOT HAPPEN: parse returned 0.");
                        continue;
                    }
                    if (U_SUCCESS(status))
                    {
                        UnicodeString format;
                        UnicodeString pattern;
                        SimpleDateFormat* sdtfmt = dynamic_cast<SimpleDateFormat*>(df);
                        if (sdtfmt != NULL) {
                            sdtfmt->toPattern(pattern);
                        }
                        full->format(when, format);
                        logln(prefix + "OK: " + format);
                        if (0!=format.compareBetween(0, expected.length(), expected, 0, expected.length()))
                            errln((UnicodeString)"FAIL: Parse \"" + text + "\", pattern \"" + pattern + "\", expected " + expected + " got " + format);
                    }
                //}
                //catch(ParseException e) {
                    else
                        status = U_ZERO_ERROR;
                //}
                //catch(StringIndexOutOfBoundsException e) {
                //    errln(prefix + "SHOULD NOT HAPPEN: " + (int)status);
                //}
                delete df;
            }
        }
    }
    delete full;
    if (U_FAILURE(status))
        errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
}

static const char* const parseFormats[] = {
    "MMMM d, yyyy",
    "MMMM d yyyy",
    "M/d/yy",
    "d MMMM, yyyy",
    "d MMMM yyyy",
    "d MMMM",
    "MMMM d",
    "yyyy",
    "h:mm a MMMM d, yyyy"
};

#if 0
// strict inputStrings
static const char* const inputStrings[] = {
    "bogus string", 0, 0, 0, 0, 0, 0, 0, 0, 0,
    "April 1, 1997", "April 1, 1997", 0, 0, 0, 0, 0, "April 1", 0, 0,
    "Jan 1, 1970", "January 1, 1970", 0, 0, 0, 0, 0, "January 1", 0, 0,
    "Jan 1 2037", 0, "January 1 2037", 0, 0, 0, 0, "January 1", 0, 0,
    "1/1/70", 0, 0, "1/1/70", 0, 0, 0, 0, "0001", 0,
    "5 May 1997", 0, 0, 0, 0, "5 May 1997", "5 May", 0, "0005", 0,
    "16 May", 0, 0, 0, 0, 0, "16 May", 0, "0016", 0,
    "April 30", 0, 0, 0, 0, 0, 0, "April 30", 0, 0,
    "1998", 0, 0, 0, 0, 0, 0, 0, "1998", 0,
    "1", 0, 0, 0, 0, 0, 0, 0, "0001", 0,
    "3:00 pm Jan 1, 1997", 0, 0, 0, 0, 0, 0, 0, "0003", "3:00 PM January 1, 1997",
};
#else
// lenient inputStrings
static const char* const inputStrings[] = {
    "bogus string", 0, 0, 0, 0, 0, 0, 0, 0, 0,
    "April 1, 1997", "April 1, 1997", "April 1 1997", "4/1/97", 0, 0, 0, "April 1", 0, 0,
    "Jan 1, 1970", "January 1, 1970", "January 1 1970", "1/1/70", 0, 0, 0, "January 1", 0, 0,
    "Jan 1 2037", "January 1, 2037", "January 1 2037", "1/1/37", 0, 0, 0, "January 1", 0, 0,
    "1/1/70", "January 1, 0070", "January 1 0070", "1/1/70", "1 January, 0070", "1 January 0070", "1 January", "January 1", "0001", 0,
    "5 May 1997", 0, 0, 0, "5 May, 1997", "5 May 1997", "5 May", 0, "0005", 0,
    "16 May", 0, 0, 0, 0, 0, "16 May", 0, "0016", 0,
    "April 30", 0, 0, 0, 0, 0, 0, "April 30", 0, 0,
    "1998", 0, 0, 0, 0, 0, 0, 0, "1998", 0,
    "1", 0, 0, 0, 0, 0, 0, 0, "0001", 0,
    "3:00 pm Jan 1, 1997", 0, 0, 0, 0, 0, 0, 0, "0003", "3:00 PM January 1, 1997",
};
#endif
 
// -------------------------------------

/**
 * Verify the correct behavior when parsing an array of inputs against an
 * array of patterns, with known results.  The results are encoded after
 * the input strings in each row.
 */
void
DateFormatTest::TestBadInput135a()
{
  UErrorCode status = U_ZERO_ERROR;
  SimpleDateFormat* dateParse = new SimpleDateFormat(status);
  if(U_FAILURE(status)) {
    dataerrln("Failed creating SimpleDateFormat with %s. Quitting test", u_errorName(status));
    delete dateParse;
    return;
  }
  const char* s;
  UDate date;
  const uint32_t PF_LENGTH = (int32_t)(sizeof(parseFormats)/sizeof(parseFormats[0]));
  const uint32_t INPUT_LENGTH = (int32_t)(sizeof(inputStrings)/sizeof(inputStrings[0]));

  dateParse->applyPattern("d MMMM, yyyy");
  dateParse->adoptTimeZone(TimeZone::createDefault());
  s = "not parseable";
  UnicodeString thePat;
  logln(UnicodeString("Trying to parse \"") + s + "\" with " + dateParse->toPattern(thePat));
  //try {
  date = dateParse->parse(s, status);
  if (U_SUCCESS(status))
    errln((UnicodeString)"FAIL: Expected exception during parse");
  //}
  //catch(Exception ex) {
  else
    logln((UnicodeString)"Exception during parse: " + (int32_t)status);
  status = U_ZERO_ERROR;
  //}
  for (uint32_t i = 0; i < INPUT_LENGTH; i += (PF_LENGTH + 1)) {
    ParsePosition parsePosition(0);
    UnicodeString s( inputStrings[i]);
    for (uint32_t index = 0; index < PF_LENGTH;++index) {
      const char* expected = inputStrings[i + 1 + index];
      dateParse->applyPattern(parseFormats[index]);
      dateParse->adoptTimeZone(TimeZone::createDefault());
      //try {
      parsePosition.setIndex(0);
      date = dateParse->parse(s, parsePosition);
      if (parsePosition.getIndex() != 0) {
        UnicodeString s1, s2;
        s.extract(0, parsePosition.getIndex(), s1);
        s.extract(parsePosition.getIndex(), s.length(), s2);
        if (date == 0) {
          errln((UnicodeString)"ERROR: null result fmt=\"" +
                     parseFormats[index] +
                     "\" pos=" + parsePosition.getIndex() + " " +
                     s1 + "|" + s2);
        }
        else {
          UnicodeString result;
          ((DateFormat*)dateParse)->format(date, result);
          logln((UnicodeString)"Parsed \"" + s + "\" using \"" + dateParse->toPattern(thePat) + "\" to: " + result);
          if (expected == 0)
            errln((UnicodeString)"FAIL: Expected parse failure, got " + result);
          else if (!(result == expected))
            errln(UnicodeString("FAIL: Parse \"") + s + UnicodeString("\", expected ") + expected + UnicodeString(", got ") + result);
        }
      }
      else if (expected != 0) {
        errln(UnicodeString("FAIL: Expected ") + expected + " from \"" +
                     s + "\" with \"" + dateParse->toPattern(thePat) + "\"");
      }
      //}
      //catch(Exception ex) {
      if (U_FAILURE(status))
        errln((UnicodeString)"An exception was thrown during parse: " + (int32_t)status);
      //}
    }
  }
  delete dateParse;
  if (U_FAILURE(status))
    errln((UnicodeString)"FAIL: UErrorCode received during test: " + (int32_t)status);
}

// -------------------------------------

/**
 * Test the parsing of two-digit years.
 */
void
DateFormatTest::TestTwoDigitYear()
{
    UErrorCode ec = U_ZERO_ERROR;
    SimpleDateFormat fmt("dd/MM/yy", Locale::getUK(), ec);
    if (U_FAILURE(ec)) {
        dataerrln("FAIL: SimpleDateFormat constructor - %s", u_errorName(ec));
        return;
    }
    parse2DigitYear(fmt, "5/6/17", date(117, UCAL_JUNE, 5));
    parse2DigitYear(fmt, "4/6/34", date(34, UCAL_JUNE, 4));
}

// -------------------------------------

void
DateFormatTest::parse2DigitYear(DateFormat& fmt, const char* str, UDate expected)
{
    UErrorCode status = U_ZERO_ERROR;
    //try {
        UDate d = fmt.parse(str, status);
        UnicodeString thePat;
        logln(UnicodeString("Parsing \"") + str + "\" with " + ((SimpleDateFormat*)&fmt)->toPattern(thePat) +
            "  => " + dateToString(d));
        if (d != expected) errln((UnicodeString)"FAIL: Expected " + expected);
    //}
    //catch(ParseException e) {
        if (U_FAILURE(status))
        errln((UnicodeString)"FAIL: Got exception");
    //}
}

// -------------------------------------

/**
 * Test the formatting of time zones.
 */
void
DateFormatTest::TestDateFormatZone061()
{
    UErrorCode status = U_ZERO_ERROR;
    UDate date;
    DateFormat *formatter;
    date= 859248000000.0;
    logln((UnicodeString)"Date 1997/3/25 00:00 GMT: " + date);
    formatter = new SimpleDateFormat((UnicodeString)"dd-MMM-yyyyy HH:mm", Locale::getUK(), status);
    if(U_FAILURE(status)) {
      dataerrln("Failed creating SimpleDateFormat with %s. Quitting test", u_errorName(status));
      delete formatter;
      return;
    }
    formatter->adoptTimeZone(TimeZone::createTimeZone("GMT"));
    UnicodeString temp; formatter->format(date, temp);
    logln((UnicodeString)"Formatted in GMT to: " + temp);
    //try {
        UDate tempDate = formatter->parse(temp, status);
        logln((UnicodeString)"Parsed to: " + dateToString(tempDate));
        if (tempDate != date) errln((UnicodeString)"FAIL: Expected " + dateToString(date));
    //}
    //catch(Throwable t) {
    if (U_FAILURE(status))
        errln((UnicodeString)"Date Formatter throws: " + (int32_t)status);
    //}
    delete formatter;
}

// -------------------------------------

/**
 * Test the formatting of time zones.
 */
void
DateFormatTest::TestDateFormatZone146()
{
    TimeZone *saveDefault = TimeZone::createDefault();

        //try {
    TimeZone *thedefault = TimeZone::createTimeZone("GMT");
    TimeZone::setDefault(*thedefault);
            // java.util.Locale.setDefault(new java.util.Locale("ar", "", ""));

            // check to be sure... its GMT all right
        TimeZone *testdefault = TimeZone::createDefault();
        UnicodeString testtimezone;
        testdefault->getID(testtimezone);
        if (testtimezone == "GMT")
            logln("Test timezone = " + testtimezone);
        else
            dataerrln("Test timezone should be GMT, not " + testtimezone);

        UErrorCode status = U_ZERO_ERROR;
        // now try to use the default GMT time zone
        GregorianCalendar *greenwichcalendar =
            new GregorianCalendar(1997, 3, 4, 23, 0, status);
        if (U_FAILURE(status)) {
            dataerrln("Fail new GregorianCalendar: %s", u_errorName(status));
        } else {
            //*****************************greenwichcalendar.setTimeZone(TimeZone.getDefault());
            //greenwichcalendar.set(1997, 3, 4, 23, 0);
            // try anything to set hour to 23:00 !!!
            greenwichcalendar->set(UCAL_HOUR_OF_DAY, 23);
            // get time
            UDate greenwichdate = greenwichcalendar->getTime(status);
            // format every way
            UnicodeString DATA [] = {
                UnicodeString("simple format:  "), UnicodeString("04/04/97 23:00 GMT"),
                    UnicodeString("MM/dd/yy HH:mm z"),
                UnicodeString("full format:    "), UnicodeString("Friday, April 4, 1997 11:00:00 o'clock PM GMT"),
                    UnicodeString("EEEE, MMMM d, yyyy h:mm:ss 'o''clock' a z"),
                UnicodeString("long format:    "), UnicodeString("April 4, 1997 11:00:00 PM GMT"),
                    UnicodeString("MMMM d, yyyy h:mm:ss a z"),
                UnicodeString("default format: "), UnicodeString("04-Apr-97 11:00:00 PM"),
                    UnicodeString("dd-MMM-yy h:mm:ss a"),
                UnicodeString("short format:   "), UnicodeString("4/4/97 11:00 PM"),
                    UnicodeString("M/d/yy h:mm a")
            };
            int32_t DATA_length = (int32_t)(sizeof(DATA) / sizeof(DATA[0]));

            for (int32_t i=0; i<DATA_length; i+=3) {
                DateFormat *fmt = new SimpleDateFormat(DATA[i+2], Locale::getEnglish(), status);
                if (U_FAILURE(status)) {
                    dataerrln("Unable to create SimpleDateFormat - %s", u_errorName(status));
                    break;
                }
                fmt->setCalendar(*greenwichcalendar);
                UnicodeString result;
                result = fmt->format(greenwichdate, result);
                logln(DATA[i] + result);
                if (result != DATA[i+1])
                    errln("FAIL: Expected " + DATA[i+1] + ", got " + result);
                delete fmt;
            }
        }
    //}
    //finally {
        TimeZone::adoptDefault(saveDefault);
    //}
        delete testdefault;
        delete greenwichcalendar;
        delete thedefault;


}

// -------------------------------------

/**
 * Test the formatting of dates in different locales.
 */
void
DateFormatTest::TestLocaleDateFormat() // Bug 495
{
    UDate testDate = date(97, UCAL_SEPTEMBER, 15);
    DateFormat *dfFrench = DateFormat::createDateTimeInstance(DateFormat::FULL,
        DateFormat::FULL, Locale::getFrench());
    DateFormat *dfUS = DateFormat::createDateTimeInstance(DateFormat::FULL,
        DateFormat::FULL, Locale::getUS());
    UnicodeString expectedFRENCH ( "lundi 15 septembre 1997 00:00:00 heure avanc\\u00E9e du Pacifique", -1, US_INV );
    expectedFRENCH = expectedFRENCH.unescape();
    UnicodeString expectedUS ( "Monday, September 15, 1997 at 12:00:00 AM Pacific Daylight Time" );
    logln((UnicodeString)"Date set to : " + dateToString(testDate));
    UnicodeString out;
    if (dfUS == NULL || dfFrench == NULL){
        dataerrln("Error calling DateFormat::createDateTimeInstance)");
        delete dfUS;
        delete dfFrench;
        return;
    }

    dfFrench->format(testDate, out);
    logln((UnicodeString)"Date Formated with French Locale " + out);
    if (!(out == expectedFRENCH))
        errln((UnicodeString)"FAIL: Expected " + expectedFRENCH);
    out.truncate(0);
    dfUS->format(testDate, out);
    logln((UnicodeString)"Date Formated with US Locale " + out);
    if (!(out == expectedUS))
        errln((UnicodeString)"FAIL: Expected " + expectedUS);
    delete dfUS;
    delete dfFrench;
}

/**
 * Test DateFormat(Calendar) API
 */
void DateFormatTest::TestDateFormatCalendar() {
    DateFormat *date=0, *time=0, *full=0;
    Calendar *cal=0;
    UnicodeString str;
    ParsePosition pos;
    UDate when;
    UErrorCode ec = U_ZERO_ERROR;

    /* Create a formatter for date fields. */
    date = DateFormat::createDateInstance(DateFormat::kShort, Locale::getUS());
    if (date == NULL) {
        dataerrln("FAIL: createDateInstance failed");
        goto FAIL;
    }

    /* Create a formatter for time fields. */
    time = DateFormat::createTimeInstance(DateFormat::kShort, Locale::getUS());
    if (time == NULL) {
        errln("FAIL: createTimeInstance failed");
        goto FAIL;
    }

    /* Create a full format for output */
    full = DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull,
                                              Locale::getUS());
    if (full == NULL) {
        errln("FAIL: createInstance failed");
        goto FAIL;
    }

    /* Create a calendar */
    cal = Calendar::createInstance(Locale::getUS(), ec);
    if (cal == NULL || U_FAILURE(ec)) {
        errln((UnicodeString)"FAIL: Calendar::createInstance failed with " +
              u_errorName(ec));
        goto FAIL;
    }

    /* Parse the date */
    cal->clear();
    str = UnicodeString("4/5/2001", "");
    pos.setIndex(0);
    date->parse(str, *cal, pos);
    if (pos.getIndex() != str.length()) {
        errln((UnicodeString)"FAIL: DateFormat::parse(4/5/2001) failed at " +
              pos.getIndex());
        goto FAIL;
    }

    /* Parse the time */
    str = UnicodeString("5:45 PM", "");
    pos.setIndex(0);
    time->parse(str, *cal, pos);
    if (pos.getIndex() != str.length()) {
        errln((UnicodeString)"FAIL: DateFormat::parse(17:45) failed at " +
              pos.getIndex());
        goto FAIL;
    }

    /* Check result */
    when = cal->getTime(ec);
    if (U_FAILURE(ec)) {
        errln((UnicodeString)"FAIL: cal->getTime() failed with " + u_errorName(ec));
        goto FAIL;
    }
    str.truncate(0);
    full->format(when, str);
    // Thursday, April 5, 2001 5:45:00 PM PDT 986517900000
    if (when == 986517900000.0) {
        logln("Ok: Parsed result: " + str);
    } else {
        errln("FAIL: Parsed result: " + str + ", exp 4/5/2001 5:45 PM");
    }

 FAIL:
    delete date;
    delete time;
    delete full;
    delete cal;
}

/**
 * Test DateFormat's parsing of space characters.  See jitterbug 1916.
 */
void DateFormatTest::TestSpaceParsing() {
    const char* DATA[] = {
        "yyyy MM dd HH:mm:ss",

        // pattern, input, expected parse or NULL if expect parse failure
        "MMMM d yy", " 04 05 06",  "2006 04 05 00:00:00",
        NULL,        "04 05 06",   "2006 04 05 00:00:00",

        "MM d yy",   " 04 05 06",    "2006 04 05 00:00:00",
        NULL,        "04 05 06",     "2006 04 05 00:00:00",
        NULL,        "04/05/06",     "2006 04 05 00:00:00",
        NULL,        "04-05-06",     "2006 04 05 00:00:00",
        NULL,        "04.05.06",     "2006 04 05 00:00:00",
        NULL,        "04 / 05 / 06", "2006 04 05 00:00:00",
        NULL,        "Apr / 05/ 06", "2006 04 05 00:00:00",
        NULL,        "Apr-05-06",    "2006 04 05 00:00:00",
        NULL,        "Apr 05, 2006", "2006 04 05 00:00:00",

        "MMMM d yy", " Apr 05 06", "2006 04 05 00:00:00",
        NULL,        "Apr 05 06",  "2006 04 05 00:00:00",
        NULL,        "Apr05 06",   "2006 04 05 00:00:00",

        "hh:mm:ss a", "12:34:56 PM", "1970 01 01 12:34:56",
        NULL,         "12:34:56PM",  "1970 01 01 12:34:56",
        NULL,         "12.34.56PM",  "1970 01 01 12:34:56",
        NULL,         "12-34-56 PM", "1970 01 01 12:34:56",
        NULL,         "12 : 34 : 56  PM", "1970 01 01 12:34:56",
        
        "MM d yy 'at' hh:mm:ss a", "04/05/06 12:34:56 PM", "2006 04 05 12:34:56",
        
        "MMMM dd yyyy hh:mm a", "September 27, 1964 21:56 PM", "1964 09 28 09:56:00",
        NULL,                   "November 4, 2008 0:13 AM",    "2008 11 04 00:13:00",
        
        "HH'h'mm'min'ss's'", "12h34min56s", "1970 01 01 12:34:56",
        NULL,                "12h34mi56s",  "1970 01 01 12:34:56",
        NULL,                "12h34m56s",   "1970 01 01 12:34:56",
        NULL,                "12:34:56",    "1970 01 01 12:34:56"
    };
    const int32_t DATA_len = sizeof(DATA)/sizeof(DATA[0]);

    expectParse(DATA, DATA_len, Locale("en"));
}

/**
 * Test handling of "HHmmss" pattern.
 */
void DateFormatTest::TestExactCountFormat() {
    const char* DATA[] = {
        "yyyy MM dd HH:mm:ss",

        // pattern, input, expected parse or NULL if expect parse failure
        "HHmmss", "123456", "1970 01 01 12:34:56",
        NULL,     "12345",  "1970 01 01 01:23:45",
        NULL,     "1234",   NULL,
        NULL,     "00-05",  NULL,
        NULL,     "12-34",  NULL,
        NULL,     "00+05",  NULL,
        "ahhmm",  "PM730",  "1970 01 01 19:30:00",
    };
    const int32_t DATA_len = sizeof(DATA)/sizeof(DATA[0]);

    expectParse(DATA, DATA_len, Locale("en"));
}

/**
 * Test handling of white space.
 */
void DateFormatTest::TestWhiteSpaceParsing() {
    const char* DATA[] = {
        "yyyy MM dd",

        // pattern, input, expected parse or null if expect parse failure

        // Pattern space run should parse input text space run
        "MM   d yy",   " 04 01 03",    "2003 04 01",
        NULL,          " 04  01   03 ", "2003 04 01",
    };
    const int32_t DATA_len = sizeof(DATA)/sizeof(DATA[0]);

    expectParse(DATA, DATA_len, Locale("en"));
}


void DateFormatTest::TestInvalidPattern() {
    UErrorCode ec = U_ZERO_ERROR;
    SimpleDateFormat f(UnicodeString("Yesterday"), ec);
    if (U_FAILURE(ec)) {
        dataerrln("Fail construct SimpleDateFormat: %s", u_errorName(ec));
        return;
    }
    UnicodeString out;
    FieldPosition pos;
    f.format((UDate)0, out, pos);
    logln(out);
    // The bug is that the call to format() will crash.  By not
    // crashing, the test passes.
}

void DateFormatTest::TestGreekMay() {
    UErrorCode ec = U_ZERO_ERROR;
    UDate date = -9896080848000.0;
    SimpleDateFormat fmt("EEEE, dd MMMM yyyy h:mm:ss a", Locale("el", "", ""), ec);
    if (U_FAILURE(ec)) {
        dataerrln("Fail construct SimpleDateFormat: %s", u_errorName(ec));
        return;
    }
    UnicodeString str;
    fmt.format(date, str);
    ParsePosition pos(0);
    UDate d2 = fmt.parse(str, pos);
    if (date != d2) {
        errln("FAIL: unable to parse strings where case-folding changes length");
    }
}

void DateFormatTest::TestStandAloneMonths()
{
    const char *EN_DATA[] = {
        "yyyy MM dd HH:mm:ss",

        "yyyy LLLL dd H:mm:ss", "fp", "2004 03 10 16:36:31", "2004 March 10 16:36:31", "2004 03 10 16:36:31",
        "yyyy LLL dd H:mm:ss",  "fp", "2004 03 10 16:36:31", "2004 Mar 10 16:36:31",   "2004 03 10 16:36:31",
        "yyyy LLLL dd H:mm:ss", "F",  "2004 03 10 16:36:31", "2004 March 10 16:36:31",
        "yyyy LLL dd H:mm:ss",  "pf", "2004 Mar 10 16:36:31", "2004 03 10 16:36:31", "2004 Mar 10 16:36:31",

        "LLLL", "fp", "1970 01 01 0:00:00", "January",   "1970 01 01 0:00:00",
        "LLLL", "fp", "1970 02 01 0:00:00", "February",  "1970 02 01 0:00:00",
        "LLLL", "fp", "1970 03 01 0:00:00", "March",     "1970 03 01 0:00:00",
        "LLLL", "fp", "1970 04 01 0:00:00", "April",     "1970 04 01 0:00:00",
        "LLLL", "fp", "1970 05 01 0:00:00", "May",       "1970 05 01 0:00:00",
        "LLLL", "fp", "1970 06 01 0:00:00", "June",      "1970 06 01 0:00:00",
        "LLLL", "fp", "1970 07 01 0:00:00", "July",      "1970 07 01 0:00:00",
        "LLLL", "fp", "1970 08 01 0:00:00", "August",    "1970 08 01 0:00:00",
        "LLLL", "fp", "1970 09 01 0:00:00", "September", "1970 09 01 0:00:00",
        "LLLL", "fp", "1970 10 01 0:00:00", "October",   "1970 10 01 0:00:00",
        "LLLL", "fp", "1970 11 01 0:00:00", "November",  "1970 11 01 0:00:00",
        "LLLL", "fp", "1970 12 01 0:00:00", "December",  "1970 12 01 0:00:00",

        "LLL", "fp", "1970 01 01 0:00:00", "Jan", "1970 01 01 0:00:00",
        "LLL", "fp", "1970 02 01 0:00:00", "Feb", "1970 02 01 0:00:00",
        "LLL", "fp", "1970 03 01 0:00:00", "Mar", "1970 03 01 0:00:00",
        "LLL", "fp", "1970 04 01 0:00:00", "Apr", "1970 04 01 0:00:00",
        "LLL", "fp", "1970 05 01 0:00:00", "May", "1970 05 01 0:00:00",
        "LLL", "fp", "1970 06 01 0:00:00", "Jun", "1970 06 01 0:00:00",
        "LLL", "fp", "1970 07 01 0:00:00", "Jul", "1970 07 01 0:00:00",
        "LLL", "fp", "1970 08 01 0:00:00", "Aug", "1970 08 01 0:00:00",
        "LLL", "fp", "1970 09 01 0:00:00", "Sep", "1970 09 01 0:00:00",
        "LLL", "fp", "1970 10 01 0:00:00", "Oct", "1970 10 01 0:00:00",
        "LLL", "fp", "1970 11 01 0:00:00", "Nov", "1970 11 01 0:00:00",
        "LLL", "fp", "1970 12 01 0:00:00", "Dec", "1970 12 01 0:00:00",
    };

    const char *CS_DATA[] = {
        "yyyy MM dd HH:mm:ss",

        "yyyy LLLL dd H:mm:ss", "fp", "2004 04 10 16:36:31", "2004 duben 10 16:36:31", "2004 04 10 16:36:31",
        "yyyy MMMM dd H:mm:ss", "fp", "2004 04 10 16:36:31", "2004 dubna 10 16:36:31", "2004 04 10 16:36:31",
        "yyyy LLL dd H:mm:ss",  "fp", "2004 04 10 16:36:31", "2004 dub 10 16:36:31",   "2004 04 10 16:36:31",
        "yyyy LLLL dd H:mm:ss", "F",  "2004 04 10 16:36:31", "2004 duben 10 16:36:31",
        "yyyy MMMM dd H:mm:ss", "F",  "2004 04 10 16:36:31", "2004 dubna 10 16:36:31",
        "yyyy LLLL dd H:mm:ss", "pf", "2004 duben 10 16:36:31", "2004 04 10 16:36:31", "2004 duben 10 16:36:31",
        "yyyy MMMM dd H:mm:ss", "pf", "2004 dubna 10 16:36:31", "2004 04 10 16:36:31", "2004 dubna 10 16:36:31",

        "LLLL", "fp", "1970 01 01 0:00:00", "leden",               "1970 01 01 0:00:00",
        "LLLL", "fp", "1970 02 01 0:00:00", "\\u00FAnor",           "1970 02 01 0:00:00",
        "LLLL", "fp", "1970 03 01 0:00:00", "b\\u0159ezen",         "1970 03 01 0:00:00",
        "LLLL", "fp", "1970 04 01 0:00:00", "duben",               "1970 04 01 0:00:00",
        "LLLL", "fp", "1970 05 01 0:00:00", "kv\\u011Bten",         "1970 05 01 0:00:00",
        "LLLL", "fp", "1970 06 01 0:00:00", "\\u010Derven",         "1970 06 01 0:00:00",
        "LLLL", "fp", "1970 07 01 0:00:00", "\\u010Dervenec",       "1970 07 01 0:00:00",
        "LLLL", "fp", "1970 08 01 0:00:00", "srpen",               "1970 08 01 0:00:00",
        "LLLL", "fp", "1970 09 01 0:00:00", "z\\u00E1\\u0159\\u00ED", "1970 09 01 0:00:00",
        "LLLL", "fp", "1970 10 01 0:00:00", "\\u0159\\u00EDjen",     "1970 10 01 0:00:00",
        "LLLL", "fp", "1970 11 01 0:00:00", "listopad",            "1970 11 01 0:00:00",
        "LLLL", "fp", "1970 12 01 0:00:00", "prosinec",            "1970 12 01 0:00:00",

        "LLL", "fp", "1970 01 01 0:00:00", "led",  "1970 01 01 0:00:00",
        "LLL", "fp", "1970 02 01 0:00:00", "\\u00FAno",  "1970 02 01 0:00:00",
        "LLL", "fp", "1970 03 01 0:00:00", "b\\u0159e",  "1970 03 01 0:00:00",
        "LLL", "fp", "1970 04 01 0:00:00", "dub",  "1970 04 01 0:00:00",
        "LLL", "fp", "1970 05 01 0:00:00", "kv\\u011B",  "1970 05 01 0:00:00",
        "LLL", "fp", "1970 06 01 0:00:00", "\\u010Dvn",  "1970 06 01 0:00:00",
        "LLL", "fp", "1970 07 01 0:00:00", "\\u010Dvc",  "1970 07 01 0:00:00",
        "LLL", "fp", "1970 08 01 0:00:00", "srp",  "1970 08 01 0:00:00",
        "LLL", "fp", "1970 09 01 0:00:00", "z\\u00E1\\u0159",  "1970 09 01 0:00:00",
        "LLL", "fp", "1970 10 01 0:00:00", "\\u0159\\u00EDj", "1970 10 01 0:00:00",
        "LLL", "fp", "1970 11 01 0:00:00", "lis", "1970 11 01 0:00:00",
        "LLL", "fp", "1970 12 01 0:00:00", "pro", "1970 12 01 0:00:00",
    };

    expect(EN_DATA, ARRAY_SIZE(EN_DATA), Locale("en", "", ""));
    expect(CS_DATA, ARRAY_SIZE(CS_DATA), Locale("cs", "", ""));
}

void DateFormatTest::TestStandAloneDays()
{
    const char *EN_DATA[] = {
        "yyyy MM dd HH:mm:ss",

        "cccc", "fp", "1970 01 04 0:00:00", "Sunday",    "1970 01 04 0:00:00",
        "cccc", "fp", "1970 01 05 0:00:00", "Monday",    "1970 01 05 0:00:00",
        "cccc", "fp", "1970 01 06 0:00:00", "Tuesday",   "1970 01 06 0:00:00",
        "cccc", "fp", "1970 01 07 0:00:00", "Wednesday", "1970 01 07 0:00:00",
        "cccc", "fp", "1970 01 01 0:00:00", "Thursday",  "1970 01 01 0:00:00",
        "cccc", "fp", "1970 01 02 0:00:00", "Friday",    "1970 01 02 0:00:00",
        "cccc", "fp", "1970 01 03 0:00:00", "Saturday",  "1970 01 03 0:00:00",

        "ccc", "fp", "1970 01 04 0:00:00", "Sun", "1970 01 04 0:00:00",
        "ccc", "fp", "1970 01 05 0:00:00", "Mon", "1970 01 05 0:00:00",
        "ccc", "fp", "1970 01 06 0:00:00", "Tue", "1970 01 06 0:00:00",
        "ccc", "fp", "1970 01 07 0:00:00", "Wed", "1970 01 07 0:00:00",
        "ccc", "fp", "1970 01 01 0:00:00", "Thu", "1970 01 01 0:00:00",
        "ccc", "fp", "1970 01 02 0:00:00", "Fri", "1970 01 02 0:00:00",
        "ccc", "fp", "1970 01 03 0:00:00", "Sat", "1970 01 03 0:00:00",
    };

    const char *CS_DATA[] = {
        "yyyy MM dd HH:mm:ss",

        "cccc", "fp", "1970 01 04 0:00:00", "ned\\u011Ble",       "1970 01 04 0:00:00",
        "cccc", "fp", "1970 01 05 0:00:00", "pond\\u011Bl\\u00ED", "1970 01 05 0:00:00",
        "cccc", "fp", "1970 01 06 0:00:00", "\\u00FAter\\u00FD",   "1970 01 06 0:00:00",
        "cccc", "fp", "1970 01 07 0:00:00", "st\\u0159eda",       "1970 01 07 0:00:00",
        "cccc", "fp", "1970 01 01 0:00:00", "\\u010Dtvrtek",      "1970 01 01 0:00:00",
        "cccc", "fp", "1970 01 02 0:00:00", "p\\u00E1tek",        "1970 01 02 0:00:00",
        "cccc", "fp", "1970 01 03 0:00:00", "sobota",            "1970 01 03 0:00:00",

        "ccc", "fp", "1970 01 04 0:00:00", "ne",      "1970 01 04 0:00:00",
        "ccc", "fp", "1970 01 05 0:00:00", "po",      "1970 01 05 0:00:00",
        "ccc", "fp", "1970 01 06 0:00:00", "\\u00FAt", "1970 01 06 0:00:00",
        "ccc", "fp", "1970 01 07 0:00:00", "st",      "1970 01 07 0:00:00",
        "ccc", "fp", "1970 01 01 0:00:00", "\\u010Dt", "1970 01 01 0:00:00",
        "ccc", "fp", "1970 01 02 0:00:00", "p\\u00E1", "1970 01 02 0:00:00",
        "ccc", "fp", "1970 01 03 0:00:00", "so",      "1970 01 03 0:00:00",
    };

    expect(EN_DATA, ARRAY_SIZE(EN_DATA), Locale("en", "", ""));
    expect(CS_DATA, ARRAY_SIZE(CS_DATA), Locale("cs", "", ""));
}

void DateFormatTest::TestShortDays()
{
    const char *EN_DATA[] = {
        "yyyy MM dd HH:mm:ss",

        "EEEEEE, MMM d y", "fp", "2013 01 13 0:00:00", "Su, Jan 13 2013", "2013 01 13 0:00:00",
        "EEEEEE, MMM d y", "fp", "2013 01 16 0:00:00", "We, Jan 16 2013", "2013 01 16 0:00:00",
        "EEEEEE d",        "fp", "1970 01 17 0:00:00", "Sa 17",           "1970 01 17 0:00:00",
        "cccccc d",        "fp", "1970 01 17 0:00:00", "Sa 17",           "1970 01 17 0:00:00",
        "cccccc",          "fp", "1970 01 03 0:00:00", "Sa",              "1970 01 03 0:00:00",
    };
    const char *SV_DATA[] = {
        "yyyy MM dd HH:mm:ss",

        "EEEEEE d MMM y",  "fp", "2013 01 13 0:00:00", "s\\u00F6 13 jan 2013", "2013 01 13 0:00:00",
        "EEEEEE d MMM y",  "fp", "2013 01 16 0:00:00", "on 16 jan 2013",       "2013 01 16 0:00:00",
        "EEEEEE d",        "fp", "1970 01 17 0:00:00", "l\\u00F6 17",          "1970 01 17 0:00:00",
        "cccccc d",        "fp", "1970 01 17 0:00:00", "L\\u00F6 17",          "1970 01 17 0:00:00",
        "cccccc",          "fp", "1970 01 03 0:00:00", "L\\u00F6",             "1970 01 03 0:00:00",
    };
    expect(EN_DATA, ARRAY_SIZE(EN_DATA), Locale("en", "", ""));
    expect(SV_DATA, ARRAY_SIZE(SV_DATA), Locale("sv", "", ""));
}

void DateFormatTest::TestNarrowNames()
{
    const char *EN_DATA[] = {
            "yyyy MM dd HH:mm:ss",

            "yyyy MMMMM dd H:mm:ss", "2004 03 10 16:36:31", "2004 M 10 16:36:31",
            "yyyy LLLLL dd H:mm:ss",  "2004 03 10 16:36:31", "2004 M 10 16:36:31",

            "MMMMM", "1970 01 01 0:00:00", "J",
            "MMMMM", "1970 02 01 0:00:00", "F",
            "MMMMM", "1970 03 01 0:00:00", "M",
            "MMMMM", "1970 04 01 0:00:00", "A",
            "MMMMM", "1970 05 01 0:00:00", "M",
            "MMMMM", "1970 06 01 0:00:00", "J",
            "MMMMM", "1970 07 01 0:00:00", "J",
            "MMMMM", "1970 08 01 0:00:00", "A",
            "MMMMM", "1970 09 01 0:00:00", "S",
            "MMMMM", "1970 10 01 0:00:00", "O",
            "MMMMM", "1970 11 01 0:00:00", "N",
            "MMMMM", "1970 12 01 0:00:00", "D",

            "LLLLL", "1970 01 01 0:00:00", "J",
            "LLLLL", "1970 02 01 0:00:00", "F",
            "LLLLL", "1970 03 01 0:00:00", "M",
            "LLLLL", "1970 04 01 0:00:00", "A",
            "LLLLL", "1970 05 01 0:00:00", "M",
            "LLLLL", "1970 06 01 0:00:00", "J",
            "LLLLL", "1970 07 01 0:00:00", "J",
            "LLLLL", "1970 08 01 0:00:00", "A",
            "LLLLL", "1970 09 01 0:00:00", "S",
            "LLLLL", "1970 10 01 0:00:00", "O",
            "LLLLL", "1970 11 01 0:00:00", "N",
            "LLLLL", "1970 12 01 0:00:00", "D",

            "EEEEE", "1970 01 04 0:00:00", "S",
            "EEEEE", "1970 01 05 0:00:00", "M",
            "EEEEE", "1970 01 06 0:00:00", "T",
            "EEEEE", "1970 01 07 0:00:00", "W",
            "EEEEE", "1970 01 01 0:00:00", "T",
            "EEEEE", "1970 01 02 0:00:00", "F",
            "EEEEE", "1970 01 03 0:00:00", "S",

            "ccccc", "1970 01 04 0:00:00", "S",
            "ccccc", "1970 01 05 0:00:00", "M",
            "ccccc", "1970 01 06 0:00:00", "T",
            "ccccc", "1970 01 07 0:00:00", "W",
            "ccccc", "1970 01 01 0:00:00", "T",
            "ccccc", "1970 01 02 0:00:00", "F",
            "ccccc", "1970 01 03 0:00:00", "S",
        };

        const char *CS_DATA[] = {
            "yyyy MM dd HH:mm:ss",

            "yyyy LLLLL dd H:mm:ss", "2004 04 10 16:36:31", "2004 d 10 16:36:31",
            "yyyy MMMMM dd H:mm:ss", "2004 04 10 16:36:31", "2004 4 10 16:36:31",

            "MMMMM", "1970 01 01 0:00:00", "1",
            "MMMMM", "1970 02 01 0:00:00", "2",
            "MMMMM", "1970 03 01 0:00:00", "3",
            "MMMMM", "1970 04 01 0:00:00", "4",
            "MMMMM", "1970 05 01 0:00:00", "5",
            "MMMMM", "1970 06 01 0:00:00", "6",
            "MMMMM", "1970 07 01 0:00:00", "7",
            "MMMMM", "1970 08 01 0:00:00", "8",
            "MMMMM", "1970 09 01 0:00:00", "9",
            "MMMMM", "1970 10 01 0:00:00", "10",
            "MMMMM", "1970 11 01 0:00:00", "11",
            "MMMMM", "1970 12 01 0:00:00", "12",

            "LLLLL", "1970 01 01 0:00:00", "l",
            "LLLLL", "1970 02 01 0:00:00", "\\u00FA",
            "LLLLL", "1970 03 01 0:00:00", "b",
            "LLLLL", "1970 04 01 0:00:00", "d",
            "LLLLL", "1970 05 01 0:00:00", "k",
            "LLLLL", "1970 06 01 0:00:00", "\\u010D",
            "LLLLL", "1970 07 01 0:00:00", "\\u010D",
            "LLLLL", "1970 08 01 0:00:00", "s",
            "LLLLL", "1970 09 01 0:00:00", "z",
            "LLLLL", "1970 10 01 0:00:00", "\\u0159",
            "LLLLL", "1970 11 01 0:00:00", "l",
            "LLLLL", "1970 12 01 0:00:00", "p",

            "EEEEE", "1970 01 04 0:00:00", "N",
            "EEEEE", "1970 01 05 0:00:00", "P",
            "EEEEE", "1970 01 06 0:00:00", "\\u00DA",
            "EEEEE", "1970 01 07 0:00:00", "S",
            "EEEEE", "1970 01 01 0:00:00", "\\u010C",
            "EEEEE", "1970 01 02 0:00:00", "P",
            "EEEEE", "1970 01 03 0:00:00", "S",

            "ccccc", "1970 01 04 0:00:00", "N",
            "ccccc", "1970 01 05 0:00:00", "P",
            "ccccc", "1970 01 06 0:00:00", "\\u00DA",
            "ccccc", "1970 01 07 0:00:00", "S",
            "ccccc", "1970 01 01 0:00:00", "\\u010C",
            "ccccc", "1970 01 02 0:00:00", "P",
            "ccccc", "1970 01 03 0:00:00", "S",
        };

      expectFormat(EN_DATA, ARRAY_SIZE(EN_DATA), Locale("en", "", ""));
      expectFormat(CS_DATA, ARRAY_SIZE(CS_DATA), Locale("cs", "", ""));
}

void DateFormatTest::TestEras()
{
    const char *EN_DATA[] = {
        "yyyy MM dd",

        "MMMM dd yyyy G",    "fp", "1951 07 17", "July 17 1951 AD",          "1951 07 17",
        "MMMM dd yyyy GG",   "fp", "1951 07 17", "July 17 1951 AD",          "1951 07 17",
        "MMMM dd yyyy GGG",  "fp", "1951 07 17", "July 17 1951 AD",          "1951 07 17",
        "MMMM dd yyyy GGGG", "fp", "1951 07 17", "July 17 1951 Anno Domini", "1951 07 17",

        "MMMM dd yyyy G",    "fp", "-438 07 17", "July 17 0439 BC",            "-438 07 17",
        "MMMM dd yyyy GG",   "fp", "-438 07 17", "July 17 0439 BC",            "-438 07 17",
        "MMMM dd yyyy GGG",  "fp", "-438 07 17", "July 17 0439 BC",            "-438 07 17",
        "MMMM dd yyyy GGGG", "fp", "-438 07 17", "July 17 0439 Before Christ", "-438 07 17",
    };

    expect(EN_DATA, ARRAY_SIZE(EN_DATA), Locale("en", "", ""));
}

void DateFormatTest::TestQuarters()
{
    const char *EN_DATA[] = {
        "yyyy MM dd",

        "Q",    "fp", "1970 01 01", "1",           "1970 01 01",
        "QQ",   "fp", "1970 04 01", "02",          "1970 04 01",
        "QQQ",  "fp", "1970 07 01", "Q3",          "1970 07 01",
        "QQQQ", "fp", "1970 10 01", "4th quarter", "1970 10 01",

        "q",    "fp", "1970 01 01", "1",           "1970 01 01",
        "qq",   "fp", "1970 04 01", "02",          "1970 04 01",
        "qqq",  "fp", "1970 07 01", "Q3",          "1970 07 01",
        "qqqq", "fp", "1970 10 01", "4th quarter", "1970 10 01",
    };

    expect(EN_DATA, ARRAY_SIZE(EN_DATA), Locale("en", "", ""));
}

/**
 * Test parsing.  Input is an array that starts with the following
 * header:
 *
 * [0]   = pattern string to parse [i+2] with
 *
 * followed by test cases, each of which is 3 array elements:
 *
 * [i]   = pattern, or NULL to reuse prior pattern
 * [i+1] = input string
 * [i+2] = expected parse result (parsed with pattern [0])
 *
 * If expect parse failure, then [i+2] should be NULL.
 */
void DateFormatTest::expectParse(const char** data, int32_t data_length,
                                 const Locale& loc) {
    const UDate FAIL = (UDate) -1;
    const UnicodeString FAIL_STR("parse failure");
    int32_t i = 0;

    UErrorCode ec = U_ZERO_ERROR;
    SimpleDateFormat fmt("", loc, ec);
    SimpleDateFormat ref(data[i++], loc, ec);
    SimpleDateFormat gotfmt("G yyyy MM dd HH:mm:ss z", loc, ec);
    if (U_FAILURE(ec)) {
        dataerrln("FAIL: SimpleDateFormat constructor - %s", u_errorName(ec));
        return;
    }

    const char* currentPat = NULL;
    while (i<data_length) {
        const char* pattern  = data[i++];
        const char* input    = data[i++];
        const char* expected = data[i++];

        ec = U_ZERO_ERROR;
        if (pattern != NULL) {
            fmt.applyPattern(pattern);
            currentPat = pattern;
        }
        UDate got = fmt.parse(input, ec);
        UnicodeString gotstr(FAIL_STR);
        if (U_FAILURE(ec)) {
            got = FAIL;
        } else {
            gotstr.remove();
            gotfmt.format(got, gotstr);
        }

        UErrorCode ec2 = U_ZERO_ERROR;
        UDate exp = FAIL;
        UnicodeString expstr(FAIL_STR);
        if (expected != NULL) {
            expstr = expected;
            exp = ref.parse(expstr, ec2);
            if (U_FAILURE(ec2)) {
                // This only happens if expected is in wrong format --
                // should never happen once test is debugged.
                errln("FAIL: Internal test error");
                return;
            }
        }

        if (got == exp) {
            logln((UnicodeString)"Ok: " + input + " x " +
                  currentPat + " => " + gotstr);
        } else {
            errln((UnicodeString)"FAIL: " + input + " x " +
                  currentPat + " => " + gotstr + ", expected " +
                  expstr);
        }
    }
}

/**
 * Test formatting and parsing.  Input is an array that starts
 * with the following header:
 *
 * [0]   = pattern string to parse [i+2] with
 *
 * followed by test cases, each of which is 3 array elements:
 *
 * [i]   = pattern, or null to reuse prior pattern
 * [i+1] = control string, either "fp", "pf", or "F".
 * [i+2..] = data strings
 *
 * The number of data strings depends on the control string.
 * Examples:
 * 1. "y/M/d H:mm:ss.SS", "fp", "2004 03 10 16:36:31.567", "2004/3/10 16:36:31.56", "2004 03 10 16:36:31.560",
 * 'f': Format date [i+2] (as parsed using pattern [0]) and expect string [i+3].
 * 'p': Parse string [i+3] and expect date [i+4].
 *
 * 2. "y/M/d H:mm:ss.SSS", "F", "2004 03 10 16:36:31.567", "2004/3/10 16:36:31.567"
 * 'F': Format date [i+2] and expect string [i+3],
 *      then parse string [i+3] and expect date [i+2].
 *
 * 3. "y/M/d H:mm:ss.SSSS", "pf", "2004/3/10 16:36:31.5679", "2004 03 10 16:36:31.567", "2004/3/10 16:36:31.5670",
 * 'p': Parse string [i+2] and expect date [i+3].
 * 'f': Format date [i+3] and expect string [i+4].
 */
void DateFormatTest::expect(const char** data, int32_t data_length,
                            const Locale& loc) {
    int32_t i = 0;
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeString str, str2;
    SimpleDateFormat fmt("", loc, ec);
    SimpleDateFormat ref(data[i++], loc, ec);
    SimpleDateFormat univ("EE G yyyy MM dd HH:mm:ss.SSS z", loc, ec);
    if (U_FAILURE(ec)) {
        dataerrln("Fail construct SimpleDateFormat: %s", u_errorName(ec));
        return;
    }

    UnicodeString currentPat;
    while (i<data_length) {
        const char* pattern  = data[i++];
        if (pattern != NULL) {
            fmt.applyPattern(pattern);
            currentPat = pattern;
        }

        const char* control = data[i++];

        if (uprv_strcmp(control, "fp") == 0) {
            // 'f'
            const char* datestr = data[i++];
            const char* string = data[i++];
            UDate date = ref.parse(ctou(datestr), ec);
            if (!assertSuccess("parse", ec)) return;
            assertEquals((UnicodeString)"\"" + currentPat + "\".format(" + datestr + ")",
                         ctou(string),
                         fmt.format(date, str.remove()));
            // 'p'
            datestr = data[i++];
            date = ref.parse(ctou(datestr), ec);
            if (!assertSuccess("parse", ec)) return;
            UDate parsedate = fmt.parse(ctou(string), ec);
            if (assertSuccess((UnicodeString)"\"" + currentPat + "\".parse(" + string + ")", ec)) {
                assertEquals((UnicodeString)"\"" + currentPat + "\".parse(" + string + ")",
                             univ.format(date, str.remove()),
                             univ.format(parsedate, str2.remove()));
            }
        }

        else if (uprv_strcmp(control, "pf") == 0) {
            // 'p'
            const char* string = data[i++];
            const char* datestr = data[i++];
            UDate date = ref.parse(ctou(datestr), ec);
            if (!assertSuccess("parse", ec)) return;
            UDate parsedate = fmt.parse(ctou(string), ec);
            if (assertSuccess((UnicodeString)"\"" + currentPat + "\".parse(" + string + ")", ec)) {
                assertEquals((UnicodeString)"\"" + currentPat + "\".parse(" + string + ")",
                             univ.format(date, str.remove()),
                             univ.format(parsedate, str2.remove()));
            }
            // 'f'
            string = data[i++];
            assertEquals((UnicodeString)"\"" + currentPat + "\".format(" + datestr + ")",
                         ctou(string),
                         fmt.format(date, str.remove()));
        }

        else if (uprv_strcmp(control, "F") == 0) {
            const char* datestr  = data[i++];
            const char* string   = data[i++];
            UDate date = ref.parse(ctou(datestr), ec);
            if (!assertSuccess("parse", ec)) return;
            assertEquals((UnicodeString)"\"" + currentPat + "\".format(" + datestr + ")",
                         ctou(string),
                         fmt.format(date, str.remove()));

            UDate parsedate = fmt.parse(string, ec);
            if (assertSuccess((UnicodeString)"\"" + currentPat + "\".parse(" + string + ")", ec)) {
                assertEquals((UnicodeString)"\"" + currentPat + "\".parse(" + string + ")",
                             univ.format(date, str.remove()),
                             univ.format(parsedate, str2.remove()));
            }
        }

        else {
            errln((UnicodeString)"FAIL: Invalid control string " + control);
            return;
        }
    }
}

/**
 * Test formatting.  Input is an array that starts
 * with the following header:
 *
 * [0]   = pattern string to parse [i+2] with
 *
 * followed by test cases, each of which is 3 array elements:
 *
 * [i]   = pattern, or null to reuse prior pattern
 * [i+1] = data string a
 * [i+2] = data string b
 *
 * Examples:
 * Format date [i+1] and expect string [i+2].
 *
 * "y/M/d H:mm:ss.SSSS", "2004/3/10 16:36:31.5679", "2004 03 10 16:36:31.567"
 */
void DateFormatTest::expectFormat(const char** data, int32_t data_length,
                            const Locale& loc) {
    int32_t i = 0;
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeString str, str2;
    SimpleDateFormat fmt("", loc, ec);
    SimpleDateFormat ref(data[i++], loc, ec);
    SimpleDateFormat univ("EE G yyyy MM dd HH:mm:ss.SSS z", loc, ec);
    if (U_FAILURE(ec)) {
        dataerrln("Fail construct SimpleDateFormat: %s", u_errorName(ec));
        return;
    }

    UnicodeString currentPat;

    while (i<data_length) {
        const char* pattern  = data[i++];
        if (pattern != NULL) {
            fmt.applyPattern(pattern);
            currentPat = pattern;
        }

        const char* datestr = data[i++];
        const char* string = data[i++];
        UDate date = ref.parse(ctou(datestr), ec);
        if (!assertSuccess("parse", ec)) return;
        assertEquals((UnicodeString)"\"" + currentPat + "\".format(" + datestr + ")",
                        ctou(string),
                        fmt.format(date, str.remove()));
    }
}

void DateFormatTest::TestGenericTime() {
  const Locale en("en");
  // Note: We no longer parse strings in different styles.
/*
  const char* ZDATA[] = {
        "yyyy MM dd HH:mm zzz",
        // round trip
        "y/M/d H:mm zzzz", "F", "2004 01 01 01:00 PST", "2004/1/1 1:00 Pacific Standard Time",
        "y/M/d H:mm zzz", "F", "2004 01 01 01:00 PST", "2004/1/1 1:00 PST",
        "y/M/d H:mm vvvv", "F", "2004 01 01 01:00 PST", "2004/1/1 1:00 Pacific Time",
        "y/M/d H:mm v", "F", "2004 01 01 01:00 PST", "2004/1/1 1:00 PT",
        // non-generic timezone string influences dst offset even if wrong for date/time
        "y/M/d H:mm zzz", "pf", "2004/1/1 1:00 PDT", "2004 01 01 01:00 PDT", "2004/1/1 0:00 PST",
        "y/M/d H:mm vvvv", "pf", "2004/1/1 1:00 PDT", "2004 01 01 01:00 PDT", "2004/1/1 0:00 Pacific Time",
        "y/M/d H:mm zzz", "pf", "2004/7/1 1:00 PST", "2004 07 01 02:00 PDT", "2004/7/1 2:00 PDT",
        "y/M/d H:mm vvvv", "pf", "2004/7/1 1:00 PST", "2004 07 01 02:00 PDT", "2004/7/1 2:00 Pacific Time",
        // generic timezone generates dst offset appropriate for local time
        "y/M/d H:mm zzz", "pf", "2004/1/1 1:00 PT", "2004 01 01 01:00 PST", "2004/1/1 1:00 PST",
        "y/M/d H:mm vvvv", "pf", "2004/1/1 1:00 PT", "2004 01 01 01:00 PST", "2004/1/1 1:00 Pacific Time",
        "y/M/d H:mm zzz", "pf", "2004/7/1 1:00 PT", "2004 07 01 01:00 PDT", "2004/7/1 1:00 PDT",
        "y/M/d H:mm vvvv", "pf", "2004/7/1 1:00 PT", "2004 07 01 01:00 PDT", "2004/7/1 1:00 Pacific Time",
        // daylight savings time transition edge cases.
        // time to parse does not really exist, PT interpreted as earlier time
        "y/M/d H:mm zzz", "pf", "2005/4/3 2:30 PT", "2005 04 03 03:30 PDT", "2005/4/3 3:30 PDT",
        "y/M/d H:mm zzz", "pf", "2005/4/3 2:30 PST", "2005 04 03 03:30 PDT", "2005/4/3 3:30 PDT",
        "y/M/d H:mm zzz", "pf", "2005/4/3 2:30 PDT", "2005 04 03 01:30 PST", "2005/4/3 1:30 PST",
        "y/M/d H:mm v", "pf", "2005/4/3 2:30 PT", "2005 04 03 03:30 PDT", "2005/4/3 3:30 PT",
        "y/M/d H:mm v", "pf", "2005/4/3 2:30 PST", "2005 04 03 03:30 PDT", "2005/4/3 3:30 PT",
        "y/M/d H:mm v", "pf", "2005/4/3 2:30 PDT", "2005 04 03 01:30 PST", "2005/4/3 1:30 PT",
        "y/M/d H:mm", "pf", "2005/4/3 2:30", "2005 04 03 03:30 PDT", "2005/4/3 3:30",
        // time to parse is ambiguous, PT interpreted as later time
        "y/M/d H:mm zzz", "pf", "2005/10/30 1:30 PT", "2005 10 30 01:30 PST", "2005/10/30 1:30 PST",
        "y/M/d H:mm v", "pf", "2005/10/30 1:30 PT", "2005 10 30  01:30 PST", "2005/10/30 1:30 PT",
        "y/M/d H:mm", "pf", "2005/10/30 1:30 PT", "2005 10 30 01:30 PST", "2005/10/30 1:30",

        "y/M/d H:mm zzz", "pf", "2004/10/31 1:30 PT", "2004 10 31 01:30 PST", "2004/10/31 1:30 PST",
        "y/M/d H:mm zzz", "pf", "2004/10/31 1:30 PST", "2004 10 31 01:30 PST", "2004/10/31 1:30 PST",
        "y/M/d H:mm zzz", "pf", "2004/10/31 1:30 PDT", "2004 10 31 01:30 PDT", "2004/10/31 1:30 PDT",
        "y/M/d H:mm v", "pf", "2004/10/31 1:30 PT", "2004 10 31 01:30 PST", "2004/10/31 1:30 PT",
        "y/M/d H:mm v", "pf", "2004/10/31 1:30 PST", "2004 10 31 01:30 PST", "2004/10/31 1:30 PT",
        "y/M/d H:mm v", "pf", "2004/10/31 1:30 PDT", "2004 10 31 01:30 PDT", "2004/10/31 1:30 PT",
        "y/M/d H:mm", "pf", "2004/10/31 1:30", "2004 10 31 01:30 PST", "2004/10/31 1:30",
  };
*/
  const char* ZDATA[] = {
        "yyyy MM dd HH:mm zzz",
        // round trip
        "y/M/d H:mm zzzz", "F", "2004 01 01 01:00 PST", "2004/1/1 1:00 Pacific Standard Time",
        "y/M/d H:mm zzz", "F", "2004 01 01 01:00 PST", "2004/1/1 1:00 PST",
        "y/M/d H:mm vvvv", "F", "2004 01 01 01:00 PST", "2004/1/1 1:00 Pacific Time",
        "y/M/d H:mm v", "F", "2004 01 01 01:00 PST", "2004/1/1 1:00 PT",
        // non-generic timezone string influences dst offset even if wrong for date/time
        "y/M/d H:mm zzz", "pf", "2004/1/1 1:00 PDT", "2004 01 01 01:00 PDT", "2004/1/1 0:00 PST",
        "y/M/d H:mm zzz", "pf", "2004/7/1 1:00 PST", "2004 07 01 02:00 PDT", "2004/7/1 2:00 PDT",
        // generic timezone generates dst offset appropriate for local time
        "y/M/d H:mm zzz", "pf", "2004/1/1 1:00 PST", "2004 01 01 01:00 PST", "2004/1/1 1:00 PST",
        "y/M/d H:mm vvvv", "pf", "2004/1/1 1:00 Pacific Time", "2004 01 01 01:00 PST", "2004/1/1 1:00 Pacific Time",
        "y/M/d H:mm zzz", "pf", "2004/7/1 1:00 PDT", "2004 07 01 01:00 PDT", "2004/7/1 1:00 PDT",
        "y/M/d H:mm vvvv", "pf", "2004/7/1 1:00 Pacific Time", "2004 07 01 01:00 PDT", "2004/7/1 1:00 Pacific Time",
        // daylight savings time transition edge cases.
        // time to parse does not really exist, PT interpreted as earlier time
        "y/M/d H:mm zzz", "pf", "2005/4/3 2:30 PST", "2005 04 03 03:30 PDT", "2005/4/3 3:30 PDT",
        "y/M/d H:mm zzz", "pf", "2005/4/3 2:30 PDT", "2005 04 03 01:30 PST", "2005/4/3 1:30 PST",
        "y/M/d H:mm v", "pf", "2005/4/3 2:30 PT", "2005 04 03 03:30 PDT", "2005/4/3 3:30 PT",
        "y/M/d H:mm", "pf", "2005/4/3 2:30", "2005 04 03 03:30 PDT", "2005/4/3 3:30",
        // time to parse is ambiguous, PT interpreted as later time
        "y/M/d H:mm v", "pf", "2005/10/30 1:30 PT", "2005 10 30  01:30 PST", "2005/10/30 1:30 PT",
        "y/M/d H:mm", "pf", "2005/10/30 1:30 PT", "2005 10 30 01:30 PST", "2005/10/30 1:30",

        "y/M/d H:mm zzz", "pf", "2004/10/31 1:30 PST", "2004 10 31 01:30 PST", "2004/10/31 1:30 PST",
        "y/M/d H:mm zzz", "pf", "2004/10/31 1:30 PDT", "2004 10 31 01:30 PDT", "2004/10/31 1:30 PDT",
        "y/M/d H:mm v", "pf", "2004/10/31 1:30 PT", "2004 10 31 01:30 PST", "2004/10/31 1:30 PT",
        "y/M/d H:mm", "pf", "2004/10/31 1:30", "2004 10 31 01:30 PST", "2004/10/31 1:30",
  };

  const int32_t ZDATA_length = sizeof(ZDATA)/ sizeof(ZDATA[0]);
  expect(ZDATA, ZDATA_length, en);

  UErrorCode status = U_ZERO_ERROR;

  logln("cross format/parse tests");    // Note: We no longer support cross format/parse
  UnicodeString basepat("yy/MM/dd H:mm ");
  SimpleDateFormat formats[] = {
    SimpleDateFormat(basepat + "vvv", en, status),
    SimpleDateFormat(basepat + "vvvv", en, status),
    SimpleDateFormat(basepat + "zzz", en, status),
    SimpleDateFormat(basepat + "zzzz", en, status)
  };
  if (U_FAILURE(status)) {
    dataerrln("Fail construct SimpleDateFormat: %s", u_errorName(status));
    return;
  }
  const int32_t formats_length = sizeof(formats)/sizeof(formats[0]);

  UnicodeString test;
  SimpleDateFormat univ("yyyy MM dd HH:mm zzz", en, status);
  ASSERT_OK(status);
  const UnicodeString times[] = {
    "2004 01 02 03:04 PST",
    "2004 07 08 09:10 PDT"
  };
  int32_t times_length = sizeof(times)/sizeof(times[0]);
  for (int i = 0; i < times_length; ++i) {
    UDate d = univ.parse(times[i], status);
    logln(UnicodeString("\ntime: ") + d);
    for (int j = 0; j < formats_length; ++j) {
      test.remove();
      formats[j].format(d, test);
      logln("\ntest: '" + test + "'");
      for (int k = 0; k < formats_length; ++k) {
        UDate t = formats[k].parse(test, status);
        if (U_SUCCESS(status)) {
          if (d != t) {
            errln((UnicodeString)"FAIL: format " + k +
                  " incorrectly parsed output of format " + j +
                  " (" + test + "), returned " +
                  dateToString(t) + " instead of " + dateToString(d));
          } else {
            logln((UnicodeString)"OK: format " + k + " parsed ok");
          }
        } else if (status == U_PARSE_ERROR) {
          errln((UnicodeString)"FAIL: format " + k +
                " could not parse output of format " + j +
                " (" + test + ")");
        }
      }
    }
  }
}

void DateFormatTest::TestGenericTimeZoneOrder() {
  // generic times should parse the same no matter what the placement of the time zone string

  // Note: We no longer support cross style format/parse

  //const char* XDATA[] = {
  //  "yyyy MM dd HH:mm zzz",
  //  // standard time, explicit daylight/standard
  //  "y/M/d H:mm zzz", "pf", "2004/1/1 1:00 PT", "2004 01 01 01:00 PST", "2004/1/1 1:00 PST",
  //  "y/M/d zzz H:mm", "pf", "2004/1/1 PT 1:00", "2004 01 01 01:00 PST", "2004/1/1 PST 1:00",
  //  "zzz y/M/d H:mm", "pf", "PT 2004/1/1 1:00", "2004 01 01 01:00 PST", "PST 2004/1/1 1:00",

  //  // standard time, generic
  //  "y/M/d H:mm vvvv", "pf", "2004/1/1 1:00 PT", "2004 01 01 01:00 PST", "2004/1/1 1:00 Pacific Time",
  //  "y/M/d vvvv H:mm", "pf", "2004/1/1 PT 1:00", "2004 01 01 01:00 PST", "2004/1/1 Pacific Time 1:00",
  //  "vvvv y/M/d H:mm", "pf", "PT 2004/1/1 1:00", "2004 01 01 01:00 PST", "Pacific Time 2004/1/1 1:00",

  //  // dahylight time, explicit daylight/standard
  //  "y/M/d H:mm zzz", "pf", "2004/7/1 1:00 PT", "2004 07 01 01:00 PDT", "2004/7/1 1:00 PDT",
  //  "y/M/d zzz H:mm", "pf", "2004/7/1 PT 1:00", "2004 07 01 01:00 PDT", "2004/7/1 PDT 1:00",
  //  "zzz y/M/d H:mm", "pf", "PT 2004/7/1 1:00", "2004 07 01 01:00 PDT", "PDT 2004/7/1 1:00",

  //  // daylight time, generic
  //  "y/M/d H:mm vvvv", "pf", "2004/7/1 1:00 PT", "2004 07 01 01:00 PDT", "2004/7/1 1:00 Pacific Time",
  //  "y/M/d vvvv H:mm", "pf", "2004/7/1 PT 1:00", "2004 07 01 01:00 PDT", "2004/7/1 Pacific Time 1:00",
  //  "vvvv y/M/d H:mm", "pf", "PT 2004/7/1 1:00", "2004 07 01 01:00 PDT", "Pacific Time 2004/7/1 1:00",
  //};
  const char* XDATA[] = {
    "yyyy MM dd HH:mm zzz",
    // standard time, explicit daylight/standard
    "y/M/d H:mm zzz", "pf", "2004/1/1 1:00 PST", "2004 01 01 01:00 PST", "2004/1/1 1:00 PST",
    "y/M/d zzz H:mm", "pf", "2004/1/1 PST 1:00", "2004 01 01 01:00 PST", "2004/1/1 PST 1:00",
    "zzz y/M/d H:mm", "pf", "PST 2004/1/1 1:00", "2004 01 01 01:00 PST", "PST 2004/1/1 1:00",

    // standard time, generic
    "y/M/d H:mm vvvv", "pf", "2004/1/1 1:00 Pacific Time", "2004 01 01 01:00 PST", "2004/1/1 1:00 Pacific Time",
    "y/M/d vvvv H:mm", "pf", "2004/1/1 Pacific Time 1:00", "2004 01 01 01:00 PST", "2004/1/1 Pacific Time 1:00",
    "vvvv y/M/d H:mm", "pf", "Pacific Time 2004/1/1 1:00", "2004 01 01 01:00 PST", "Pacific Time 2004/1/1 1:00",

    // dahylight time, explicit daylight/standard
    "y/M/d H:mm zzz", "pf", "2004/7/1 1:00 PDT", "2004 07 01 01:00 PDT", "2004/7/1 1:00 PDT",
    "y/M/d zzz H:mm", "pf", "2004/7/1 PDT 1:00", "2004 07 01 01:00 PDT", "2004/7/1 PDT 1:00",
    "zzz y/M/d H:mm", "pf", "PDT 2004/7/1 1:00", "2004 07 01 01:00 PDT", "PDT 2004/7/1 1:00",

    // daylight time, generic
    "y/M/d H:mm v", "pf", "2004/7/1 1:00 PT", "2004 07 01 01:00 PDT", "2004/7/1 1:00 PT",
    "y/M/d v H:mm", "pf", "2004/7/1 PT 1:00", "2004 07 01 01:00 PDT", "2004/7/1 PT 1:00",
    "v y/M/d H:mm", "pf", "PT 2004/7/1 1:00", "2004 07 01 01:00 PDT", "PT 2004/7/1 1:00",
  };
  const int32_t XDATA_length = sizeof(XDATA)/sizeof(XDATA[0]);
  Locale en("en");
  expect(XDATA, XDATA_length, en);
}

void DateFormatTest::TestZTimeZoneParsing(void) {
    UErrorCode status = U_ZERO_ERROR;
    const Locale en("en");
    UnicodeString test;
    //SimpleDateFormat univ("yyyy-MM-dd'T'HH:mm Z", en, status);
    SimpleDateFormat univ("HH:mm Z", en, status);
    if (failure(status, "construct SimpleDateFormat", TRUE)) return;
    const TimeZone *t = TimeZone::getGMT();
    univ.setTimeZone(*t);

    univ.setLenient(false);
    ParsePosition pp(0);
    struct {
        UnicodeString input;
        UnicodeString expected_result;
    } tests[] = {
        { "11:00 -0200", "13:00 +0000" },
        { "11:00 +0200", "09:00 +0000" },
        { "11:00 +0400", "07:00 +0000" },
        { "11:00 +0530", "05:30 +0000" }
    };

    UnicodeString result;
    int32_t tests_length = sizeof(tests)/sizeof(tests[0]);
    for (int i = 0; i < tests_length; ++i) {
        pp.setIndex(0);
        UDate d = univ.parse(tests[i].input, pp);
        if(pp.getIndex() != tests[i].input.length()){
            errln("Test %i: setZoneString() did not succeed. Consumed: %i instead of %i",
                  i, pp.getIndex(), tests[i].input.length()); 
            return;
        }
        result.remove();
        univ.format(d, result);
        if(result != tests[i].expected_result) {
            errln("Expected " + tests[i].expected_result
                  + " got " + result);
            return;
        }
        logln("SUCCESS: Parsed " + tests[i].input
              + " got " + result
              + " expected " + tests[i].expected_result);
    }
}

void DateFormatTest::TestHost(void)
{
#if U_PLATFORM_HAS_WIN32_API
    Win32DateTimeTest::testLocales(this);
#endif
}

// Relative Date Tests

void DateFormatTest::TestRelative(int daysdelta,
                                  const Locale& loc,
                                  const char *expectChars) {
    char banner[25];
    sprintf(banner, "%d", daysdelta);
    UnicodeString bannerStr(banner, "");

    UErrorCode status = U_ZERO_ERROR;

    FieldPosition pos(0);
    UnicodeString test;
    Locale en("en");
    DateFormat *fullrelative = DateFormat::createDateInstance(DateFormat::kFullRelative, loc);

    if (fullrelative == NULL) {
        dataerrln("DateFormat::createDateInstance(DateFormat::kFullRelative, %s) returned NULL", loc.getName());
        return;
    }

    DateFormat *full         = DateFormat::createDateInstance(DateFormat::kFull        , loc);

    if (full == NULL) {
        errln("DateFormat::createDateInstance(DateFormat::kFull, %s) returned NULL", loc.getName());
        return;
    }

    DateFormat *en_full =         DateFormat::createDateInstance(DateFormat::kFull,         en);

    if (en_full == NULL) {
        errln("DateFormat::createDateInstance(DateFormat::kFull, en) returned NULL");
        return;
    }

    DateFormat *en_fulltime =         DateFormat::createDateTimeInstance(DateFormat::kFull,DateFormat::kFull,en);

    if (en_fulltime == NULL) {
        errln("DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull, en) returned NULL");
        return;
    }

    UnicodeString result;
    UnicodeString normalResult;
    UnicodeString expect;
    UnicodeString parseResult;

    Calendar *c = Calendar::createInstance(status);

    // Today = Today
    c->setTime(Calendar::getNow(), status);
    if(daysdelta != 0) {
        c->add(Calendar::DATE,daysdelta,status);
    }
    ASSERT_OK(status);

    // calculate the expected string
    if(expectChars != NULL) {
        expect = expectChars;
    } else {
        full->format(*c, expect, pos); // expected = normal full
    }

    fullrelative   ->format(*c, result, pos);
    en_full        ->format(*c, normalResult, pos);

    if(result != expect) {
        errln("FAIL: Relative Format ["+bannerStr+"] of "+normalResult+" failed, expected "+expect+" but got " + result);
    } else {
        logln("PASS: Relative Format ["+bannerStr+"] of "+normalResult+" got " + result);
    }


    //verify
    UDate d = fullrelative->parse(result, status);
    ASSERT_OK(status);

    UnicodeString parseFormat; // parse rel->format full
    en_full->format(d, parseFormat, status);

    UnicodeString origFormat;
    en_full->format(*c, origFormat, pos);

    if(parseFormat!=origFormat) {
        errln("FAIL: Relative Parse ["+bannerStr+"] of "+result+" failed, expected "+parseFormat+" but got "+origFormat);
    } else {
        logln("PASS: Relative Parse ["+bannerStr+"] of "+result+" passed, got "+parseFormat);
    }

    delete full;
    delete fullrelative;
    delete en_fulltime;
    delete en_full;
    delete c;
}


void DateFormatTest::TestRelative(void)
{
    Locale en("en");
    TestRelative( 0, en, "today");
    TestRelative(-1, en, "yesterday");
    TestRelative( 1, en, "tomorrow");
    TestRelative( 2, en, NULL);
    TestRelative( -2, en, NULL);
    TestRelative( 3, en, NULL);
    TestRelative( -3, en, NULL);
    TestRelative( 300, en, NULL);
    TestRelative( -300, en, NULL);
}

void DateFormatTest::TestRelativeClone(void)
{
    /*
    Verify that a cloned formatter gives the same results
    and is useable after the original has been deleted.
    */
    UErrorCode status = U_ZERO_ERROR;
    Locale loc("en");
    UDate now = Calendar::getNow();
    DateFormat *full = DateFormat::createDateInstance(DateFormat::kFullRelative, loc);
    if (full == NULL) {
        dataerrln("FAIL: Can't create Relative date instance");
        return;
    }
    UnicodeString result1;
    full->format(now, result1, status);
    Format *fullClone = full->clone();
    delete full;
    full = NULL;

    UnicodeString result2;
    fullClone->format(now, result2, status);
    ASSERT_OK(status);
    if (result1 != result2) {
        errln("FAIL: Clone returned different result from non-clone.");
    }
    delete fullClone;
}

void DateFormatTest::TestHostClone(void)
{
    /*
    Verify that a cloned formatter gives the same results
    and is useable after the original has been deleted.
    */
    // This is mainly important on Windows.
    UErrorCode status = U_ZERO_ERROR;
    Locale loc("en_US@compat=host");
    UDate now = Calendar::getNow();
    DateFormat *full = DateFormat::createDateInstance(DateFormat::kFull, loc);
    if (full == NULL) {
        dataerrln("FAIL: Can't create Relative date instance");
        return;
    }
    UnicodeString result1;
    full->format(now, result1, status);
    Format *fullClone = full->clone();
    delete full;
    full = NULL;

    UnicodeString result2;
    fullClone->format(now, result2, status);
    ASSERT_OK(status);
    if (result1 != result2) {
        errln("FAIL: Clone returned different result from non-clone.");
    }
    delete fullClone;
}

void DateFormatTest::TestTimeZoneDisplayName()
{
    // This test data was ported from ICU4J.  Don't know why the 6th column in there because it's not being
    // used currently.
    const char *fallbackTests[][6]  = {
        { "en", "America/Los_Angeles", "2004-01-15T00:00:00Z", "Z", "-0800", "-8:00" },
        { "en", "America/Los_Angeles", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-08:00", "-8:00" },
        { "en", "America/Los_Angeles", "2004-01-15T00:00:00Z", "ZZZZZ", "-08:00", "-8:00" },
        { "en", "America/Los_Angeles", "2004-01-15T00:00:00Z", "z", "PST", "America/Los_Angeles" },
        { "en", "America/Los_Angeles", "2004-01-15T00:00:00Z", "zzzz", "Pacific Standard Time", "America/Los_Angeles" },
        { "en", "America/Los_Angeles", "2004-07-15T00:00:00Z", "Z", "-0700", "-7:00" },
        { "en", "America/Los_Angeles", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-07:00", "-7:00" },
        { "en", "America/Los_Angeles", "2004-07-15T00:00:00Z", "z", "PDT", "America/Los_Angeles" },
        { "en", "America/Los_Angeles", "2004-07-15T00:00:00Z", "zzzz", "Pacific Daylight Time", "America/Los_Angeles" },
        { "en", "America/Los_Angeles", "2004-07-15T00:00:00Z", "v", "PT", "America/Los_Angeles" },
        { "en", "America/Los_Angeles", "2004-07-15T00:00:00Z", "vvvv", "Pacific Time", "America/Los_Angeles" },
        { "en", "America/Los_Angeles", "2004-07-15T00:00:00Z", "VVVV", "Los Angeles Time", "America/Los_Angeles" },
        { "en_GB", "America/Los_Angeles", "2004-01-15T12:00:00Z", "z", "GMT-8", "America/Los_Angeles" },
        { "en", "America/Phoenix", "2004-01-15T00:00:00Z", "Z", "-0700", "-7:00" },
        { "en", "America/Phoenix", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-07:00", "-7:00" },
        { "en", "America/Phoenix", "2004-01-15T00:00:00Z", "z", "MST", "America/Phoenix" },
        { "en", "America/Phoenix", "2004-01-15T00:00:00Z", "zzzz", "Mountain Standard Time", "America/Phoenix" },
        { "en", "America/Phoenix", "2004-07-15T00:00:00Z", "Z", "-0700", "-7:00" },
        { "en", "America/Phoenix", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-07:00", "-7:00" },
        { "en", "America/Phoenix", "2004-07-15T00:00:00Z", "z", "MST", "America/Phoenix" },
        { "en", "America/Phoenix", "2004-07-15T00:00:00Z", "zzzz", "Mountain Standard Time", "America/Phoenix" },
        { "en", "America/Phoenix", "2004-07-15T00:00:00Z", "v", "MST", "America/Phoenix" },
        { "en", "America/Phoenix", "2004-07-15T00:00:00Z", "vvvv", "Mountain Standard Time", "America/Phoenix" },
        { "en", "America/Phoenix", "2004-07-15T00:00:00Z", "VVVV", "Phoenix Time", "America/Phoenix" },

        { "en", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "en", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "en", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "en", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "Argentina Standard Time", "-3:00" },
        { "en", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "en", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "en", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "en", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "Argentina Standard Time", "-3:00" },
        { "en", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "Buenos Aires Time", "America/Buenos_Aires" },
        { "en", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "Argentina Standard Time", "America/Buenos_Aires" },
        { "en", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "VVVV", "Buenos Aires Time", "America/Buenos_Aires" },

        { "en", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "en", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "en", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "en", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "Argentina Standard Time", "-3:00" },
        { "en", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "en", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "en", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "en", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "Argentina Standard Time", "-3:00" },
        { "en", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "Buenos Aires Time", "America/Buenos_Aires" },
        { "en", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "Argentina Standard Time", "America/Buenos_Aires" },
        { "en", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "VVVV", "Buenos Aires Time", "America/Buenos_Aires" },

        { "en", "America/Havana", "2004-01-15T00:00:00Z", "Z", "-0500", "-5:00" },
        { "en", "America/Havana", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-05:00", "-5:00" },
        { "en", "America/Havana", "2004-01-15T00:00:00Z", "z", "GMT-5", "-5:00" },
        { "en", "America/Havana", "2004-01-15T00:00:00Z", "zzzz", "Cuba Standard Time", "-5:00" },
        { "en", "America/Havana", "2004-07-15T00:00:00Z", "Z", "-0400", "-4:00" },
        { "en", "America/Havana", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-04:00", "-4:00" },
        { "en", "America/Havana", "2004-07-15T00:00:00Z", "z", "GMT-4", "-4:00" },
        { "en", "America/Havana", "2004-07-15T00:00:00Z", "zzzz", "Cuba Daylight Time", "-4:00" },
        { "en", "America/Havana", "2004-07-15T00:00:00Z", "v", "Cuba Time", "America/Havana" },
        { "en", "America/Havana", "2004-07-15T00:00:00Z", "vvvv", "Cuba Time", "America/Havana" },
        { "en", "America/Havana", "2004-07-15T00:00:00Z", "VVVV", "Cuba Time", "America/Havana" },

        { "en", "Australia/ACT", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "en", "Australia/ACT", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "en", "Australia/ACT", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "en", "Australia/ACT", "2004-01-15T00:00:00Z", "zzzz", "Australian Eastern Daylight Time", "+11:00" },
        { "en", "Australia/ACT", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "en", "Australia/ACT", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "en", "Australia/ACT", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "en", "Australia/ACT", "2004-07-15T00:00:00Z", "zzzz", "Australian Eastern Standard Time", "+10:00" },
        { "en", "Australia/ACT", "2004-07-15T00:00:00Z", "v", "Sydney Time", "Australia/Sydney" },
        { "en", "Australia/ACT", "2004-07-15T00:00:00Z", "vvvv", "Eastern Australia Time", "Australia/Sydney" },
        { "en", "Australia/ACT", "2004-07-15T00:00:00Z", "VVVV", "Sydney Time", "Australia/Sydney" },

        { "en", "Australia/Sydney", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "en", "Australia/Sydney", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "en", "Australia/Sydney", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "en", "Australia/Sydney", "2004-01-15T00:00:00Z", "zzzz", "Australian Eastern Daylight Time", "+11:00" },
        { "en", "Australia/Sydney", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "en", "Australia/Sydney", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "en", "Australia/Sydney", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "en", "Australia/Sydney", "2004-07-15T00:00:00Z", "zzzz", "Australian Eastern Standard Time", "+10:00" },
        { "en", "Australia/Sydney", "2004-07-15T00:00:00Z", "v", "Sydney Time", "Australia/Sydney" },
        { "en", "Australia/Sydney", "2004-07-15T00:00:00Z", "vvvv", "Eastern Australia Time", "Australia/Sydney" },
        { "en", "Australia/Sydney", "2004-07-15T00:00:00Z", "VVVV", "Sydney Time", "Australia/Sydney" },

        { "en", "Europe/London", "2004-01-15T00:00:00Z", "Z", "+0000", "+0:00" },
        { "en", "Europe/London", "2004-01-15T00:00:00Z", "ZZZZ", "GMT", "+0:00" },
        { "en", "Europe/London", "2004-01-15T00:00:00Z", "z", "GMT", "+0:00" },
        { "en", "Europe/London", "2004-01-15T00:00:00Z", "zzzz", "Greenwich Mean Time", "+0:00" },
        { "en", "Europe/London", "2004-07-15T00:00:00Z", "Z", "+0100", "+1:00" },
        { "en", "Europe/London", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+01:00", "+1:00" },
        { "en", "Europe/London", "2004-07-15T00:00:00Z", "z", "GMT+1", "Europe/London" },
        { "en", "Europe/London", "2004-07-15T00:00:00Z", "zzzz", "British Summer Time", "Europe/London" },
    // icu en.txt has exemplar city for this time zone
        { "en", "Europe/London", "2004-07-15T00:00:00Z", "v", "United Kingdom Time", "Europe/London" },
        { "en", "Europe/London", "2004-07-15T00:00:00Z", "vvvv", "United Kingdom Time", "Europe/London" },
        { "en", "Europe/London", "2004-07-15T00:00:00Z", "VVVV", "United Kingdom Time", "Europe/London" },

        { "en", "Etc/GMT+3", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "en", "Etc/GMT+3", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "en", "Etc/GMT+3", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "en", "Etc/GMT+3", "2004-01-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "en", "Etc/GMT+3", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "en", "Etc/GMT+3", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "en", "Etc/GMT+3", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "en", "Etc/GMT+3", "2004-07-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "en", "Etc/GMT+3", "2004-07-15T00:00:00Z", "v", "GMT-3", "-3:00" },
        { "en", "Etc/GMT+3", "2004-07-15T00:00:00Z", "vvvv", "GMT-03:00", "-3:00" },

        // JB#5150
        { "en", "Asia/Calcutta", "2004-01-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "en", "Asia/Calcutta", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "en", "Asia/Calcutta", "2004-01-15T00:00:00Z", "z", "GMT+5:30", "+5:30" },
        { "en", "Asia/Calcutta", "2004-01-15T00:00:00Z", "zzzz", "India Standard Time", "+5:30" },
        { "en", "Asia/Calcutta", "2004-07-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "en", "Asia/Calcutta", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "en", "Asia/Calcutta", "2004-07-15T00:00:00Z", "z", "GMT+5:30", "+05:30" },
        { "en", "Asia/Calcutta", "2004-07-15T00:00:00Z", "zzzz", "India Standard Time", "+5:30" },
        { "en", "Asia/Calcutta", "2004-07-15T00:00:00Z", "v", "India Time", "Asia/Calcutta" },
        { "en", "Asia/Calcutta", "2004-07-15T00:00:00Z", "vvvv", "India Standard Time", "Asia/Calcutta" },

        // Proper CLDR primary zone support #9733
        { "en", "Asia/Shanghai", "2013-01-01T00:00:00Z", "VVVV", "China Time", "Asia/Shanghai" },
        { "en", "Asia/Harbin", "2013-01-01T00:00:00Z", "VVVV", "Harbin Time", "Asia/Harbin" },

        // ==========

        { "de", "America/Los_Angeles", "2004-01-15T00:00:00Z", "Z", "-0800", "-8:00" },
        { "de", "America/Los_Angeles", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-08:00", "-8:00" },
        { "de", "America/Los_Angeles", "2004-01-15T00:00:00Z", "z", "GMT-8", "-8:00" },
        { "de", "America/Los_Angeles", "2004-01-15T00:00:00Z", "zzzz", "Nordamerikanische Westk\\u00fcsten-Normalzeit", "-8:00" },
        { "de", "America/Los_Angeles", "2004-07-15T00:00:00Z", "Z", "-0700", "-7:00" },
        { "de", "America/Los_Angeles", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-07:00", "-7:00" },
        { "de", "America/Los_Angeles", "2004-07-15T00:00:00Z", "z", "GMT-7", "-7:00" },
        { "de", "America/Los_Angeles", "2004-07-15T00:00:00Z", "zzzz", "Nordamerikanische Westk\\u00fcsten-Sommerzeit", "-7:00" },
        { "de", "America/Los_Angeles", "2004-07-15T00:00:00Z", "v", "Los Angeles Zeit", "America/Los_Angeles" },
        { "de", "America/Los_Angeles", "2004-07-15T00:00:00Z", "vvvv", "Nordamerikanische Westk\\u00fcstenzeit", "America/Los_Angeles" },

        { "de", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "de", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "de", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "de", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "Argentinische Normalzeit", "-3:00" },
        { "de", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "de", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "de", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "de", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "Argentinische Normalzeit", "-3:00" },
        { "de", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "Buenos Aires Zeit", "America/Buenos_Aires" },
        { "de", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "Argentinische Normalzeit", "America/Buenos_Aires" },

        { "de", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "de", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "de", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "de", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "Argentinische Normalzeit", "-3:00" },
        { "de", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "de", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "de", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "de", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "Argentinische Normalzeit", "-3:00" },
        { "de", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "Buenos Aires Zeit", "America/Buenos_Aires" },
        { "de", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "Argentinische Normalzeit", "America/Buenos_Aires" },

        { "de", "America/Havana", "2004-01-15T00:00:00Z", "Z", "-0500", "-5:00" },
        { "de", "America/Havana", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-05:00", "-5:00" },
        { "de", "America/Havana", "2004-01-15T00:00:00Z", "z", "GMT-5", "-5:00" },
        { "de", "America/Havana", "2004-01-15T00:00:00Z", "zzzz", "Kubanische Normalzeit", "-5:00" },
        { "de", "America/Havana", "2004-07-15T00:00:00Z", "Z", "-0400", "-4:00" },
        { "de", "America/Havana", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-04:00", "-4:00" },
        { "de", "America/Havana", "2004-07-15T00:00:00Z", "z", "GMT-4", "-4:00" },
        { "de", "America/Havana", "2004-07-15T00:00:00Z", "zzzz", "Kubanische Sommerzeit", "-4:00" },
        { "de", "America/Havana", "2004-07-15T00:00:00Z", "v", "Kuba Zeit", "America/Havana" },
        { "de", "America/Havana", "2004-07-15T00:00:00Z", "vvvv", "Kubanische Zeit", "America/Havana" },
        // added to test proper fallback of country name
        { "de_CH", "America/Havana", "2004-07-15T00:00:00Z", "v", "Kuba Zeit", "America/Havana" },
        { "de_CH", "America/Havana", "2004-07-15T00:00:00Z", "vvvv", "Kubanische Zeit", "America/Havana" },

        { "de", "Australia/ACT", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "de", "Australia/ACT", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "de", "Australia/ACT", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "de", "Australia/ACT", "2004-01-15T00:00:00Z", "zzzz", "Ostaustralische Sommerzeit", "+11:00" },
        { "de", "Australia/ACT", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "de", "Australia/ACT", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "de", "Australia/ACT", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "de", "Australia/ACT", "2004-07-15T00:00:00Z", "zzzz", "Ostaustralische Normalzeit", "+10:00" },
        { "de", "Australia/ACT", "2004-07-15T00:00:00Z", "v", "Sydney Zeit", "Australia/Sydney" },
        { "de", "Australia/ACT", "2004-07-15T00:00:00Z", "vvvv", "Ostaustralische Zeit", "Australia/Sydney" },

        { "de", "Australia/Sydney", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "de", "Australia/Sydney", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "de", "Australia/Sydney", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "de", "Australia/Sydney", "2004-01-15T00:00:00Z", "zzzz", "Ostaustralische Sommerzeit", "+11:00" },
        { "de", "Australia/Sydney", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "de", "Australia/Sydney", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "de", "Australia/Sydney", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "de", "Australia/Sydney", "2004-07-15T00:00:00Z", "zzzz", "Ostaustralische Normalzeit", "+10:00" },
        { "de", "Australia/Sydney", "2004-07-15T00:00:00Z", "v", "Sydney Zeit", "Australia/Sydney" },
        { "de", "Australia/Sydney", "2004-07-15T00:00:00Z", "vvvv", "Ostaustralische Zeit", "Australia/Sydney" },

        { "de", "Europe/London", "2004-01-15T00:00:00Z", "Z", "+0000", "+0:00" },
        { "de", "Europe/London", "2004-01-15T00:00:00Z", "ZZZZ", "GMT", "+0:00" },
        { "de", "Europe/London", "2004-01-15T00:00:00Z", "z", "GMT", "+0:00" },
        { "de", "Europe/London", "2004-01-15T00:00:00Z", "zzzz", "Mittlere Greenwich-Zeit", "+0:00" },
        { "de", "Europe/London", "2004-07-15T00:00:00Z", "Z", "+0100", "+1:00" },
        { "de", "Europe/London", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+01:00", "+1:00" },
        { "de", "Europe/London", "2004-07-15T00:00:00Z", "z", "GMT+1", "+1:00" },
        { "de", "Europe/London", "2004-07-15T00:00:00Z", "zzzz", "Britische Sommerzeit", "+1:00" },
        { "de", "Europe/London", "2004-07-15T00:00:00Z", "v", "Vereinigtes K\\u00f6nigreich Zeit", "Europe/London" },
        { "de", "Europe/London", "2004-07-15T00:00:00Z", "vvvv", "Vereinigtes K\\u00f6nigreich Zeit", "Europe/London" },

        { "de", "Etc/GMT+3", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "de", "Etc/GMT+3", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "de", "Etc/GMT+3", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "de", "Etc/GMT+3", "2004-01-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "de", "Etc/GMT+3", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "de", "Etc/GMT+3", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "de", "Etc/GMT+3", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "de", "Etc/GMT+3", "2004-07-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "de", "Etc/GMT+3", "2004-07-15T00:00:00Z", "v", "GMT-3", "-3:00" },
        { "de", "Etc/GMT+3", "2004-07-15T00:00:00Z", "vvvv", "GMT-03:00", "-3:00" },

        // JB#5150
        { "de", "Asia/Calcutta", "2004-01-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "de", "Asia/Calcutta", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "de", "Asia/Calcutta", "2004-01-15T00:00:00Z", "z", "GMT+5:30", "+5:30" },
        { "de", "Asia/Calcutta", "2004-01-15T00:00:00Z", "zzzz", "Indische Zeit", "+5:30" },
        { "de", "Asia/Calcutta", "2004-07-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "de", "Asia/Calcutta", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "de", "Asia/Calcutta", "2004-07-15T00:00:00Z", "z", "GMT+5:30", "+05:30" },
        { "de", "Asia/Calcutta", "2004-07-15T00:00:00Z", "zzzz", "Indische Zeit", "+5:30" },
        { "de", "Asia/Calcutta", "2004-07-15T00:00:00Z", "v", "Indien Zeit", "Asia/Calcutta" },
        { "de", "Asia/Calcutta", "2004-07-15T00:00:00Z", "vvvv", "Indische Zeit", "Asia/Calcutta" },

        // ==========

        { "zh", "America/Los_Angeles", "2004-01-15T00:00:00Z", "Z", "-0800", "-8:00" },
        { "zh", "America/Los_Angeles", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-08:00", "-8:00" },
        { "zh", "America/Los_Angeles", "2004-01-15T00:00:00Z", "z", "GMT-8", "America/Los_Angeles" },
        { "zh", "America/Los_Angeles", "2004-01-15T00:00:00Z", "zzzz", "\\u5317\\u7f8e\\u592a\\u5e73\\u6d0b\\u6807\\u51c6\\u65f6\\u95f4", "America/Los_Angeles" },
        { "zh", "America/Los_Angeles", "2004-07-15T00:00:00Z", "Z", "-0700", "-7:00" },
        { "zh", "America/Los_Angeles", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-07:00", "-7:00" },
        { "zh", "America/Los_Angeles", "2004-07-15T00:00:00Z", "z", "GMT-7", "America/Los_Angeles" },
        { "zh", "America/Los_Angeles", "2004-07-15T00:00:00Z", "zzzz", "\\u5317\\u7f8e\\u592a\\u5e73\\u6d0b\\u590f\\u4ee4\\u65f6\\u95f4", "America/Los_Angeles" },
    // icu zh.txt has exemplar city for this time zone
        { "zh", "America/Los_Angeles", "2004-07-15T00:00:00Z", "v", "\\u6D1B\\u6749\\u77F6\\u65F6\\u95F4", "America/Los_Angeles" },
        { "zh", "America/Los_Angeles", "2004-07-15T00:00:00Z", "vvvv", "\\u5317\\u7f8e\\u592a\\u5e73\\u6d0b\\u65f6\\u95f4", "America/Los_Angeles" },

        { "zh", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "\\u963f\\u6839\\u5ef7\\u6807\\u51c6\\u65f6\\u95f4", "-3:00" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "\\u963f\\u6839\\u5ef7\\u6807\\u51c6\\u65f6\\u95f4", "-3:00" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "\\u5E03\\u5B9C\\u8BFA\\u65AF\\u827E\\u5229\\u65AF\\u65F6\\u95F4", "America/Buenos_Aires" },
        { "zh", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "\\u963f\\u6839\\u5ef7\\u6807\\u51c6\\u65f6\\u95f4", "America/Buenos_Aires" },

        { "zh", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "zh", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "zh", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "zh", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "\\u963f\\u6839\\u5ef7\\u6807\\u51c6\\u65f6\\u95f4", "-3:00" },
        { "zh", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "zh", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "zh", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "zh", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "\\u963f\\u6839\\u5ef7\\u6807\\u51c6\\u65f6\\u95f4", "-3:00" },
        { "zh", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "\\u5E03\\u5B9C\\u8BFA\\u65AF\\u827E\\u5229\\u65AF\\u65F6\\u95F4", "America/Buenos_Aires" },
        { "zh", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "\\u963f\\u6839\\u5ef7\\u6807\\u51c6\\u65f6\\u95f4", "America/Buenos_Aires" },

        { "zh", "America/Havana", "2004-01-15T00:00:00Z", "Z", "-0500", "-5:00" },
        { "zh", "America/Havana", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-05:00", "-5:00" },
        { "zh", "America/Havana", "2004-01-15T00:00:00Z", "z", "GMT-5", "-5:00" },
        { "zh", "America/Havana", "2004-01-15T00:00:00Z", "zzzz", "\\u53e4\\u5df4\\u6807\\u51c6\\u65f6\\u95f4", "-5:00" },
        { "zh", "America/Havana", "2004-07-15T00:00:00Z", "Z", "-0400", "-4:00" },
        { "zh", "America/Havana", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-04:00", "-4:00" },
        { "zh", "America/Havana", "2004-07-15T00:00:00Z", "z", "GMT-4", "-4:00" },
        { "zh", "America/Havana", "2004-07-15T00:00:00Z", "zzzz", "\\u53e4\\u5df4\\u590f\\u4ee4\\u65f6\\u95f4", "-4:00" },
        { "zh", "America/Havana", "2004-07-15T00:00:00Z", "v", "\\u53e4\\u5df4\\u65f6\\u95f4", "America/Havana" },
        { "zh", "America/Havana", "2004-07-15T00:00:00Z", "vvvv", "\\u53e4\\u5df4\\u65f6\\u95f4", "America/Havana" },

        { "zh", "Australia/ACT", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "zh", "Australia/ACT", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "zh", "Australia/ACT", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "zh", "Australia/ACT", "2004-01-15T00:00:00Z", "zzzz", "\\u6fb3\\u5927\\u5229\\u4e9a\\u4e1c\\u90e8\\u590f\\u4ee4\\u65f6\\u95f4", "+11:00" },
        { "zh", "Australia/ACT", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "zh", "Australia/ACT", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "zh", "Australia/ACT", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "zh", "Australia/ACT", "2004-07-15T00:00:00Z", "zzzz", "\\u6fb3\\u5927\\u5229\\u4e9a\\u4e1c\\u90e8\\u6807\\u51c6\\u65f6\\u95f4", "+10:00" },
    // icu zh.txt does not have info for this time zone
        { "zh", "Australia/ACT", "2004-07-15T00:00:00Z", "v", "\\u6089\\u5C3C\\u65F6\\u95F4", "Australia/Sydney" },
        { "zh", "Australia/ACT", "2004-07-15T00:00:00Z", "vvvv", "\\u6fb3\\u5927\\u5229\\u4e9a\\u4e1c\\u90e8\\u65f6\\u95f4", "Australia/Sydney" },

        { "zh", "Australia/Sydney", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "zh", "Australia/Sydney", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "zh", "Australia/Sydney", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "zh", "Australia/Sydney", "2004-01-15T00:00:00Z", "zzzz", "\\u6fb3\\u5927\\u5229\\u4e9a\\u4e1c\\u90e8\\u590f\\u4ee4\\u65f6\\u95f4", "+11:00" },
        { "zh", "Australia/Sydney", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "zh", "Australia/Sydney", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "zh", "Australia/Sydney", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "zh", "Australia/Sydney", "2004-07-15T00:00:00Z", "zzzz", "\\u6fb3\\u5927\\u5229\\u4e9a\\u4e1c\\u90e8\\u6807\\u51c6\\u65f6\\u95f4", "+10:00" },
        { "zh", "Australia/Sydney", "2004-07-15T00:00:00Z", "v", "\\u6089\\u5C3C\\u65F6\\u95F4", "Australia/Sydney" },
        { "zh", "Australia/Sydney", "2004-07-15T00:00:00Z", "vvvv", "\\u6fb3\\u5927\\u5229\\u4e9a\\u4e1c\\u90e8\\u65f6\\u95f4", "Australia/Sydney" },

        { "zh", "Europe/London", "2004-01-15T00:00:00Z", "Z", "+0000", "+0:00" },
        { "zh", "Europe/London", "2004-01-15T00:00:00Z", "ZZZZ", "GMT", "+0:00" },
        { "zh", "Europe/London", "2004-01-15T00:00:00Z", "z", "GMT", "+0:00" },
        { "zh", "Europe/London", "2004-01-15T00:00:00Z", "ZZZZ", "GMT", "+0:00" },
        { "zh", "Europe/London", "2004-01-15T00:00:00Z", "z", "GMT", "+0:00" },
        { "zh", "Europe/London", "2004-01-15T00:00:00Z", "zzzz", "\\u683C\\u6797\\u5C3C\\u6CBB\\u6807\\u51C6\\u65F6\\u95F4", "+0:00" },
        { "zh", "Europe/London", "2004-07-15T00:00:00Z", "Z", "+0100", "+1:00" },
        { "zh", "Europe/London", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+01:00", "+1:00" },
        { "zh", "Europe/London", "2004-07-15T00:00:00Z", "z", "GMT+1", "+1:00" },
        { "zh", "Europe/London", "2004-07-15T00:00:00Z", "zzzz", "\\u82f1\\u56fd\\u590f\\u4ee4\\u65f6\\u95f4", "+1:00" },
        { "zh", "Europe/London", "2004-07-15T00:00:00Z", "v", "\\u82f1\\u56fd\\u65f6\\u95f4", "Europe/London" },
        { "zh", "Europe/London", "2004-07-15T00:00:00Z", "vvvv", "\\u82f1\\u56fd\\u65f6\\u95f4", "Europe/London" },
        { "zh", "Europe/London", "2004-07-15T00:00:00Z", "VVVV", "\\u82f1\\u56fd\\u65f6\\u95f4", "Europe/London" },

        { "zh", "Etc/GMT+3", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-01-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-07-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-07-15T00:00:00Z", "v", "GMT-3", "-3:00" },
        { "zh", "Etc/GMT+3", "2004-07-15T00:00:00Z", "vvvv", "GMT-03:00", "-3:00" },

        // JB#5150
        { "zh", "Asia/Calcutta", "2004-01-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "zh", "Asia/Calcutta", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "zh", "Asia/Calcutta", "2004-01-15T00:00:00Z", "z", "GMT+5:30", "+5:30" },
        { "zh", "Asia/Calcutta", "2004-01-15T00:00:00Z", "zzzz", "\\u5370\\u5ea6\\u65f6\\u95f4", "+5:30" },
        { "zh", "Asia/Calcutta", "2004-07-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "zh", "Asia/Calcutta", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "zh", "Asia/Calcutta", "2004-07-15T00:00:00Z", "z", "GMT+5:30", "+05:30" },
        { "zh", "Asia/Calcutta", "2004-07-15T00:00:00Z", "zzzz", "\\u5370\\u5ea6\\u65f6\\u95f4", "+5:30" },
        { "zh", "Asia/Calcutta", "2004-07-15T00:00:00Z", "v", "\\u5370\\u5ea6\\u65f6\\u95f4", "Asia/Calcutta" },
        { "zh", "Asia/Calcutta", "2004-07-15T00:00:00Z", "vvvv", "\\u5370\\u5ea6\\u65f6\\u95f4", "Asia/Calcutta" },

        // Proper CLDR primary zone support #9733
        { "zh", "Asia/Shanghai", "2013-01-01T00:00:00Z", "VVVV", "\\u4e2d\\u56fd\\u65f6\\u95f4", "Asia/Shanghai" },
        { "zh", "Asia/Harbin", "2013-01-01T00:00:00Z", "VVVV", "\\u54c8\\u5c14\\u6ee8\\u65f6\\u95f4", "Asia/Harbin" },

        // ==========

        { "hi", "America/Los_Angeles", "2004-01-15T00:00:00Z", "Z", "-0800", "-8:00" },
        { "hi", "America/Los_Angeles", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-08:00", "-8:00" },
        { "hi", "America/Los_Angeles", "2004-01-15T00:00:00Z", "z", "GMT-8", "-8:00" },
        { "hi", "America/Los_Angeles", "2004-01-15T00:00:00Z", "zzzz", "\\u0909\\u0924\\u094d\\u0924\\u0930\\u0940 \\u0905\\u092e\\u0947\\u0930\\u093f\\u0915\\u0940 \\u092a\\u094d\\u0930\\u0936\\u093e\\u0902\\u0924 \\u092e\\u093e\\u0928\\u0915 \\u0938\\u092e\\u092f", "-8:00" },
        { "hi", "America/Los_Angeles", "2004-07-15T00:00:00Z", "Z", "-0700", "-7:00" },
        { "hi", "America/Los_Angeles", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-07:00", "-7:00" },
        { "hi", "America/Los_Angeles", "2004-07-15T00:00:00Z", "z", "GMT-7", "-7:00" },
        { "hi", "America/Los_Angeles", "2004-07-15T00:00:00Z", "zzzz", "\\u0909\\u0924\\u094d\\u0924\\u0930\\u0940 \\u0905\\u092e\\u0947\\u0930\\u093f\\u0915\\u0940 \\u092a\\u094d\\u0930\\u0936\\u093e\\u0902\\u0924 \\u0921\\u0947\\u0932\\u093e\\u0907\\u091f \\u0938\\u092e\\u092f", "-7:00" },
        { "hi", "America/Los_Angeles", "2004-07-15T00:00:00Z", "v",  "\\u0932\\u0949\\u0938 \\u090f\\u0902\\u091c\\u093f\\u0932\\u094d\\u0938 \\u0938\\u092e\\u092f", "America/Los_Angeles" },
        { "hi", "America/Los_Angeles", "2004-07-15T00:00:00Z", "vvvv", "\\u0909\\u0924\\u094d\\u0924\\u0930\\u0940 \\u0905\\u092e\\u0947\\u0930\\u093f\\u0915\\u0940 \\u092a\\u094d\\u0930\\u0936\\u093e\\u0902\\u0924 \\u0938\\u092e\\u092f", "America/Los_Angeles" },

        { "hi", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "\\u0905\\u0930\\u094D\\u091C\\u0947\\u0902\\u091F\\u0940\\u0928\\u093E \\u092E\\u093E\\u0928\\u0915 \\u0938\\u092E\\u092F", "-3:00" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "\\u0905\\u0930\\u094D\\u091C\\u0947\\u0902\\u091F\\u0940\\u0928\\u093E \\u092E\\u093E\\u0928\\u0915 \\u0938\\u092E\\u092F", "-3:00" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "\\u092C\\u094D\\u092F\\u0942\\u0928\\u0938 \\u0906\\u092F\\u0930\\u0938 \\u0938\\u092E\\u092F", "America/Buenos_Aires" },
        { "hi", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "\\u0905\\u0930\\u094D\\u091C\\u0947\\u0902\\u091F\\u0940\\u0928\\u093E \\u092E\\u093E\\u0928\\u0915 \\u0938\\u092E\\u092F", "America/Buenos_Aires" },

        { "hi", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "hi", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "hi", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "hi", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "\\u0905\\u0930\\u094D\\u091C\\u0947\\u0902\\u091F\\u0940\\u0928\\u093E \\u092E\\u093E\\u0928\\u0915 \\u0938\\u092E\\u092F", "-3:00" },
        { "hi", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "hi", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "hi", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "hi", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "\\u0905\\u0930\\u094D\\u091C\\u0947\\u0902\\u091F\\u0940\\u0928\\u093E \\u092E\\u093E\\u0928\\u0915 \\u0938\\u092E\\u092F", "-3:00" },
        { "hi", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "\\u092C\\u094D\\u092F\\u0942\\u0928\\u0938 \\u0906\\u092F\\u0930\\u0938 \\u0938\\u092E\\u092F", "America/Buenos_Aires" },
        { "hi", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "\\u0905\\u0930\\u094D\\u091C\\u0947\\u0902\\u091F\\u0940\\u0928\\u093E \\u092E\\u093E\\u0928\\u0915 \\u0938\\u092E\\u092F", "America/Buenos_Aires" },

        { "hi", "America/Havana", "2004-01-15T00:00:00Z", "Z", "-0500", "-5:00" },
        { "hi", "America/Havana", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-05:00", "-5:00" },
        { "hi", "America/Havana", "2004-01-15T00:00:00Z", "z", "GMT-5", "-5:00" },
        { "hi", "America/Havana", "2004-01-15T00:00:00Z", "zzzz", "\\u0915\\u094d\\u092f\\u0942\\u092c\\u093e \\u092e\\u093e\\u0928\\u0915 \\u0938\\u092e\\u092f", "-5:00" },
        { "hi", "America/Havana", "2004-07-15T00:00:00Z", "Z", "-0400", "-4:00" },
        { "hi", "America/Havana", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-04:00", "-4:00" },
        { "hi", "America/Havana", "2004-07-15T00:00:00Z", "z", "GMT-4", "-4:00" },
        { "hi", "America/Havana", "2004-07-15T00:00:00Z", "zzzz", "\\u0915\\u094d\\u092f\\u0942\\u092c\\u093e \\u0921\\u0947\\u0932\\u093e\\u0907\\u091f \\u0938\\u092e\\u092f", "-4:00" },
        { "hi", "America/Havana", "2004-07-15T00:00:00Z", "v", "\\u0915\\u094d\\u092f\\u0942\\u092c\\u093e \\u0938\\u092E\\u092F", "America/Havana" },
        { "hi", "America/Havana", "2004-07-15T00:00:00Z", "vvvv", "\\u0915\\u094d\\u092f\\u0942\\u092c\\u093e \\u0938\\u092e\\u092f", "America/Havana" },

        { "hi", "Australia/ACT", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "hi", "Australia/ACT", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "hi", "Australia/ACT", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "hi", "Australia/ACT", "2004-01-15T00:00:00Z", "zzzz", "\\u0911\\u0938\\u094d\\u200d\\u091f\\u094d\\u0930\\u0947\\u0932\\u093f\\u092f\\u093e\\u0908 \\u092a\\u0942\\u0930\\u094d\\u0935\\u0940 \\u0921\\u0947\\u0932\\u093e\\u0907\\u091f \\u0938\\u092e\\u092f", "+11:00" },
        { "hi", "Australia/ACT", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "hi", "Australia/ACT", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "hi", "Australia/ACT", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "hi", "Australia/ACT", "2004-07-15T00:00:00Z", "zzzz", "\\u0911\\u0938\\u094d\\u200d\\u091f\\u094d\\u0930\\u0947\\u0932\\u093f\\u092f\\u093e\\u0908 \\u092a\\u0942\\u0930\\u094d\\u0935\\u0940 \\u092e\\u093e\\u0928\\u0915 \\u0938\\u092e\\u092f", "+10:00" },
        { "hi", "Australia/ACT", "2004-07-15T00:00:00Z", "v", "\\u0938\\u093F\\u0921\\u0928\\u0940 \\u0938\\u092E\\u092F", "Australia/Sydney" },
        { "hi", "Australia/ACT", "2004-07-15T00:00:00Z", "vvvv", "\\u092a\\u0942\\u0930\\u094d\\u0935\\u0940 \\u0911\\u0938\\u094d\\u091f\\u094d\\u0930\\u0947\\u0932\\u093f\\u092f\\u093e \\u0938\\u092e\\u092f", "Australia/Sydney" },

        { "hi", "Australia/Sydney", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "hi", "Australia/Sydney", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "hi", "Australia/Sydney", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "hi", "Australia/Sydney", "2004-01-15T00:00:00Z", "zzzz", "\\u0911\\u0938\\u094d\\u200d\\u091f\\u094d\\u0930\\u0947\\u0932\\u093f\\u092f\\u093e\\u0908 \\u092a\\u0942\\u0930\\u094d\\u0935\\u0940 \\u0921\\u0947\\u0932\\u093e\\u0907\\u091f \\u0938\\u092e\\u092f", "+11:00" },
        { "hi", "Australia/Sydney", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "hi", "Australia/Sydney", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "hi", "Australia/Sydney", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "hi", "Australia/Sydney", "2004-07-15T00:00:00Z", "zzzz", "\\u0911\\u0938\\u094d\\u200d\\u091f\\u094d\\u0930\\u0947\\u0932\\u093f\\u092f\\u093e\\u0908 \\u092a\\u0942\\u0930\\u094d\\u0935\\u0940 \\u092e\\u093e\\u0928\\u0915 \\u0938\\u092e\\u092f", "+10:00" },
        { "hi", "Australia/Sydney", "2004-07-15T00:00:00Z", "v", "\\u0938\\u093F\\u0921\\u0928\\u0940 \\u0938\\u092E\\u092F", "Australia/Sydney" },
        { "hi", "Australia/Sydney", "2004-07-15T00:00:00Z", "vvvv", "\\u092a\\u0942\\u0930\\u094d\\u0935\\u0940 \\u0911\\u0938\\u094d\\u091f\\u094d\\u0930\\u0947\\u0932\\u093f\\u092f\\u093e \\u0938\\u092e\\u092f", "Australia/Sydney" },

        { "hi", "Europe/London", "2004-01-15T00:00:00Z", "Z", "+0000", "+0:00" },
        { "hi", "Europe/London", "2004-01-15T00:00:00Z", "ZZZZ", "GMT", "+0:00" },
        { "hi", "Europe/London", "2004-01-15T00:00:00Z", "z", "GMT", "+0:00" },
        { "hi", "Europe/London", "2004-01-15T00:00:00Z", "zzzz", "\\u0917\\u094d\\u0930\\u0940\\u0928\\u0935\\u093f\\u091a \\u092e\\u0940\\u0928 \\u091f\\u093e\\u0907\\u092e", "+0:00" },
        { "hi", "Europe/London", "2004-07-15T00:00:00Z", "Z", "+0100", "+1:00" },
        { "hi", "Europe/London", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+01:00", "+1:00" },
        { "hi", "Europe/London", "2004-07-15T00:00:00Z", "z", "GMT+1", "+1:00" },
        { "hi", "Europe/London", "2004-07-15T00:00:00Z", "zzzz", "\\u092c\\u094d\\u0930\\u093f\\u091f\\u093f\\u0936 \\u0917\\u094d\\u0930\\u0940\\u0937\\u094d\\u092e\\u0915\\u093e\\u0932\\u0940\\u0928 \\u0938\\u092e\\u092f", "+1:00" },
        { "hi", "Europe/London", "2004-07-15T00:00:00Z", "v", "\\u092f\\u0942\\u0928\\u093e\\u0907\\u091f\\u0947\\u0921 \\u0915\\u093f\\u0902\\u0917\\u0921\\u092e \\u0938\\u092e\\u092f", "Europe/London" },
        { "hi", "Europe/London", "2004-07-15T00:00:00Z", "vvvv", "\\u092f\\u0942\\u0928\\u093e\\u0907\\u091f\\u0947\\u0921 \\u0915\\u093f\\u0902\\u0917\\u0921\\u092e \\u0938\\u092e\\u092f", "Europe/London" },

        { "hi", "Etc/GMT+3", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-01-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-07-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-07-15T00:00:00Z", "v", "GMT-3", "-3:00" },
        { "hi", "Etc/GMT+3", "2004-07-15T00:00:00Z", "vvvv", "GMT-03:00", "-3:00" },

        { "hi", "Asia/Calcutta", "2004-01-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "hi", "Asia/Calcutta", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "hi", "Asia/Calcutta", "2004-01-15T00:00:00Z", "z", "IST", "+5:30" },
        { "hi", "Asia/Calcutta", "2004-01-15T00:00:00Z", "zzzz", "\\u092D\\u093E\\u0930\\u0924\\u0940\\u092F \\u0938\\u092E\\u092F", "+5:30" },
        { "hi", "Asia/Calcutta", "2004-07-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "hi", "Asia/Calcutta", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "hi", "Asia/Calcutta", "2004-07-15T00:00:00Z", "z", "IST", "+05:30" },
        { "hi", "Asia/Calcutta", "2004-07-15T00:00:00Z", "zzzz", "\\u092D\\u093E\\u0930\\u0924\\u0940\\u092F \\u0938\\u092E\\u092F", "+5:30" },
        { "hi", "Asia/Calcutta", "2004-07-15T00:00:00Z", "v", "IST", "Asia/Calcutta" },
        { "hi", "Asia/Calcutta", "2004-07-15T00:00:00Z", "vvvv", "\\u092D\\u093E\\u0930\\u0924\\u0940\\u092F \\u0938\\u092E\\u092F", "Asia/Calcutta" },

        // ==========

        { "bg", "America/Los_Angeles", "2004-01-15T00:00:00Z", "Z", "-0800", "-8:00" },
        { "bg", "America/Los_Angeles", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-08:00", "-8:00" },
        { "bg", "America/Los_Angeles", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-8", "America/Los_Angeles" },
        { "bg", "America/Los_Angeles", "2004-01-15T00:00:00Z", "zzzz", "\\u0421\\u0435\\u0432\\u0435\\u0440\\u043d\\u043e\\u0430\\u043c\\u0435\\u0440\\u0438\\u043a\\u0430\\u043d\\u0441\\u043a\\u043e \\u0442\\u0438\\u0445\\u043e\\u043e\\u043a\\u0435\\u0430\\u043d\\u0441\\u043a\\u043e \\u0441\\u0442\\u0430\\u043d\\u0434\\u0430\\u0440\\u0442\\u043d\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "America/Los_Angeles" },
        { "bg", "America/Los_Angeles", "2004-07-15T00:00:00Z", "Z", "-0700", "-7:00" },
        { "bg", "America/Los_Angeles", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-07:00", "-7:00" },
        { "bg", "America/Los_Angeles", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-7", "America/Los_Angeles" },
        { "bg", "America/Los_Angeles", "2004-07-15T00:00:00Z", "zzzz", "\\u0421\\u0435\\u0432\\u0435\\u0440\\u043d\\u043e\\u0430\\u043c\\u0435\\u0440\\u0438\\u043a\\u0430\\u043d\\u0441\\u043a\\u043e \\u0442\\u0438\\u0445\\u043e\\u043e\\u043a\\u0435\\u0430\\u043d\\u0441\\u043a\\u043e \\u043b\\u044f\\u0442\\u043d\\u043e \\u0447\\u0430\\u0441\\u043e\\u0432\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "America/Los_Angeles" },
    // icu bg.txt has exemplar city for this time zone
        { "bg", "America/Los_Angeles", "2004-07-15T00:00:00Z", "v", "\\u041B\\u043E\\u0441 \\u0410\\u043D\\u0434\\u0436\\u0435\\u043B\\u0438\\u0441", "America/Los_Angeles" },
        { "bg", "America/Los_Angeles", "2004-07-15T00:00:00Z", "vvvv", "\\u0421\\u0435\\u0432\\u0435\\u0440\\u043d\\u043e\\u0430\\u043c\\u0435\\u0440\\u0438\\u043a\\u0430\\u043d\\u0441\\u043a\\u043e \\u0442\\u0438\\u0445\\u043e\\u043e\\u043a\\u0435\\u0430\\u043d\\u0441\\u043a\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "America/Los_Angeles" },
        { "bg", "America/Los_Angeles", "2004-07-15T00:00:00Z", "VVVV", "\\u041B\\u043E\\u0441 \\u0410\\u043D\\u0434\\u0436\\u0435\\u043B\\u0438\\u0441", "America/Los_Angeles" },

        { "bg", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-3", "-3:00" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "\\u0410\\u0440\\u0436\\u0435\\u043D\\u0442\\u0438\\u043D\\u0441\\u043a\\u043e \\u0441\\u0442\\u0430\\u043d\\u0434\\u0430\\u0440\\u0442\\u043d\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "-3:00" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-3", "-3:00" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "\\u0410\\u0440\\u0436\\u0435\\u043D\\u0442\\u0438\\u043D\\u0441\\u043a\\u043e \\u0441\\u0442\\u0430\\u043d\\u0434\\u0430\\u0440\\u0442\\u043d\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "-3:00" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "\\u0411\\u0443\\u0435\\u043D\\u043E\\u0441 \\u0410\\u0439\\u0440\\u0435\\u0441", "America/Buenos_Aires" },
        { "bg", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "\\u0410\\u0440\\u0436\\u0435\\u043D\\u0442\\u0438\\u043D\\u0441\\u043a\\u043e \\u0441\\u0442\\u0430\\u043d\\u0434\\u0430\\u0440\\u0442\\u043d\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "America/Buenos_Aires" },

        { "bg", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "bg", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },
        { "bg", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-3", "-3:00" },
        { "bg", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "\\u0410\\u0440\\u0436\\u0435\\u043D\\u0442\\u0438\\u043D\\u0441\\u043a\\u043e \\u0441\\u0442\\u0430\\u043d\\u0434\\u0430\\u0440\\u0442\\u043d\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "-3:00" },
        { "bg", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "bg", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },
        { "bg", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-3", "-3:00" },
        { "bg", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "\\u0410\\u0440\\u0436\\u0435\\u043D\\u0442\\u0438\\u043D\\u0441\\u043a\\u043e \\u0441\\u0442\\u0430\\u043d\\u0434\\u0430\\u0440\\u0442\\u043d\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "-3:00" },
    // icu bg.txt does not have info for this time zone
        { "bg", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "\\u0411\\u0443\\u0435\\u043D\\u043E\\u0441 \\u0410\\u0439\\u0440\\u0435\\u0441", "America/Buenos_Aires" },
        { "bg", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "\\u0410\\u0440\\u0436\\u0435\\u043D\\u0442\\u0438\\u043D\\u0441\\u043a\\u043e \\u0441\\u0442\\u0430\\u043d\\u0434\\u0430\\u0440\\u0442\\u043d\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "America/Buenos_Aires" },

        { "bg", "America/Havana", "2004-01-15T00:00:00Z", "Z", "-0500", "-5:00" },
        { "bg", "America/Havana", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-05:00", "-5:00" },
        { "bg", "America/Havana", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-5", "-5:00" },
        { "bg", "America/Havana", "2004-01-15T00:00:00Z", "zzzz", "\\u041a\\u0443\\u0431\\u0438\\u043d\\u0441\\u043a\\u043e \\u0441\\u0442\\u0430\\u043d\\u0434\\u0430\\u0440\\u0442\\u043d\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "-5:00" },
        { "bg", "America/Havana", "2004-07-15T00:00:00Z", "Z", "-0400", "-4:00" },
        { "bg", "America/Havana", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-04:00", "-4:00" },
        { "bg", "America/Havana", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-4", "-4:00" },
        { "bg", "America/Havana", "2004-07-15T00:00:00Z", "zzzz", "\\u041a\\u0443\\u0431\\u0438\\u043d\\u0441\\u043a\\u043e \\u043b\\u044f\\u0442\\u043d\\u043e \\u0447\\u0430\\u0441\\u043e\\u0432\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "-4:00" },
        { "bg", "America/Havana", "2004-07-15T00:00:00Z", "v", "\\u041a\\u0443\\u0431\\u0430", "America/Havana" },
        { "bg", "America/Havana", "2004-07-15T00:00:00Z", "vvvv", "\\u041a\\u0443\\u0431\\u0438\\u043d\\u0441\\u043a\\u043e \\u0432\\u0440\\u0435\\u043C\\u0435", "America/Havana" },

        { "bg", "Australia/ACT", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "bg", "Australia/ACT", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+11:00", "+11:00" },
        { "bg", "Australia/ACT", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+11", "+11:00" },
        { "bg", "Australia/ACT", "2004-01-15T00:00:00Z", "zzzz", "\\u0410\\u0432\\u0441\\u0442\\u0440\\u0430\\u043B\\u0438\\u044F \\u2013 \\u0438\\u0437\\u0442\\u043E\\u0447\\u043D\\u043E \\u043B\\u044F\\u0442\\u043D\\u043E \\u0447\\u0430\\u0441\\u043E\\u0432\\u043E \\u0432\\u0440\\u0435\\u043C\\u0435", "+11:00" },
        { "bg", "Australia/ACT", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "bg", "Australia/ACT", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+10:00", "+10:00" },
        { "bg", "Australia/ACT", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+10", "+10:00" },
        { "bg", "Australia/ACT", "2004-07-15T00:00:00Z", "zzzz", "\\u0410\\u0432\\u0441\\u0442\\u0440\\u0430\\u043B\\u0438\\u044F \\u2013 \\u0438\\u0437\\u0442\\u043E\\u0447\\u043D\\u043E \\u0441\\u0442\\u0430\\u043D\\u0434\\u0430\\u0440\\u0442\\u043D\\u043E \\u0432\\u0440\\u0435\\u043C\\u0435", "+10:00" },
        { "bg", "Australia/ACT", "2004-07-15T00:00:00Z", "v", "\\u0421\\u0438\\u0434\\u043D\\u0438", "Australia/Sydney" },
        { "bg", "Australia/ACT", "2004-07-15T00:00:00Z", "vvvv", "\\u0410\\u0432\\u0441\\u0442\\u0440\\u0430\\u043B\\u0438\\u044F \\u2013 \\u0438\\u0437\\u0442\\u043E\\u0447\\u043D\\u043E \\u0432\\u0440\\u0435\\u043C\\u0435", "Australia/Sydney" },

        { "bg", "Australia/Sydney", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "bg", "Australia/Sydney", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+11:00", "+11:00" },
        { "bg", "Australia/Sydney", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+11", "+11:00" },
        { "bg", "Australia/Sydney", "2004-01-15T00:00:00Z", "zzzz", "\\u0410\\u0432\\u0441\\u0442\\u0440\\u0430\\u043B\\u0438\\u044F \\u2013 \\u0438\\u0437\\u0442\\u043E\\u0447\\u043D\\u043E \\u043B\\u044F\\u0442\\u043D\\u043E \\u0447\\u0430\\u0441\\u043E\\u0432\\u043E \\u0432\\u0440\\u0435\\u043C\\u0435", "+11:00" },
        { "bg", "Australia/Sydney", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "bg", "Australia/Sydney", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+10:00", "+10:00" },
        { "bg", "Australia/Sydney", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+10", "+10:00" },
        { "bg", "Australia/Sydney", "2004-07-15T00:00:00Z", "zzzz", "\\u0410\\u0432\\u0441\\u0442\\u0440\\u0430\\u043B\\u0438\\u044F \\u2013 \\u0438\\u0437\\u0442\\u043E\\u0447\\u043D\\u043E \\u0441\\u0442\\u0430\\u043D\\u0434\\u0430\\u0440\\u0442\\u043D\\u043E \\u0432\\u0440\\u0435\\u043C\\u0435", "+10:00" },
        { "bg", "Australia/Sydney", "2004-07-15T00:00:00Z", "v", "\\u0421\\u0438\\u0434\\u043D\\u0438", "Australia/Sydney" },
        { "bg", "Australia/Sydney", "2004-07-15T00:00:00Z", "vvvv", "\\u0410\\u0432\\u0441\\u0442\\u0440\\u0430\\u043B\\u0438\\u044F \\u2013 \\u0438\\u0437\\u0442\\u043E\\u0447\\u043D\\u043E \\u0432\\u0440\\u0435\\u043C\\u0435", "Australia/Sydney" },

        { "bg", "Europe/London", "2004-01-15T00:00:00Z", "Z", "+0000", "+0:00" },
        { "bg", "Europe/London", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447", "+0:00" },
        { "bg", "Europe/London", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447", "+0:00" },
        { "bg", "Europe/London", "2004-01-15T00:00:00Z", "zzzz", "\\u0421\\u0440\\u0435\\u0434\\u043d\\u043e \\u0433\\u0440\\u0438\\u043d\\u0443\\u0438\\u0447\\u043a\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "+0:00" },
        { "bg", "Europe/London", "2004-07-15T00:00:00Z", "Z", "+0100", "+1:00" },
        { "bg", "Europe/London", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+01:00", "+1:00" },
        { "bg", "Europe/London", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+1", "+1:00" },
        { "bg", "Europe/London", "2004-07-15T00:00:00Z", "zzzz", "\\u0411\\u0440\\u0438\\u0442\\u0430\\u043d\\u0441\\u043a\\u043e \\u043b\\u044f\\u0442\\u043d\\u043e \\u0447\\u0430\\u0441\\u043e\\u0432\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "+1:00" },
        { "bg", "Europe/London", "2004-07-15T00:00:00Z", "v", "\\u0412\\u0435\\u043b\\u0438\\u043a\\u043e\\u0431\\u0440\\u0438\\u0442\\u0430\\u043d\\u0438\\u044f", "Europe/London" },
        { "bg", "Europe/London", "2004-07-15T00:00:00Z", "vvvv", "\\u0412\\u0435\\u043b\\u0438\\u043a\\u043e\\u0431\\u0440\\u0438\\u0442\\u0430\\u043d\\u0438\\u044f", "Europe/London" },

        { "bg", "Etc/GMT+3", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-3", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-01-15T00:00:00Z", "zzzz", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-3", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-07-15T00:00:00Z", "zzzz", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-07-15T00:00:00Z", "v", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-3", "-3:00" },
        { "bg", "Etc/GMT+3", "2004-07-15T00:00:00Z", "vvvv", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447-03:00", "-3:00" },

        // JB#5150
        { "bg", "Asia/Calcutta", "2004-01-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "bg", "Asia/Calcutta", "2004-01-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+05:30", "+5:30" },
        { "bg", "Asia/Calcutta", "2004-01-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+5:30", "+5:30" },
        { "bg", "Asia/Calcutta", "2004-01-15T00:00:00Z", "zzzz", "\\u0418\\u043d\\u0434\\u0438\\u0439\\u0441\\u043a\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "+5:30" },
        { "bg", "Asia/Calcutta", "2004-07-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "bg", "Asia/Calcutta", "2004-07-15T00:00:00Z", "ZZZZ", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+05:30", "+5:30" },
        { "bg", "Asia/Calcutta", "2004-07-15T00:00:00Z", "z", "\\u0413\\u0440\\u0438\\u0438\\u043D\\u0443\\u0438\\u0447+5:30", "+05:30" },
        { "bg", "Asia/Calcutta", "2004-07-15T00:00:00Z", "zzzz", "\\u0418\\u043d\\u0434\\u0438\\u0439\\u0441\\u043a\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "+5:30" },
        { "bg", "Asia/Calcutta", "2004-07-15T00:00:00Z", "v", "\\u0418\\u043D\\u0434\\u0438\\u044F", "Asia/Calcutta" },
        { "bg", "Asia/Calcutta", "2004-07-15T00:00:00Z", "vvvv", "\\u0418\\u043d\\u0434\\u0438\\u0439\\u0441\\u043a\\u043e \\u0432\\u0440\\u0435\\u043c\\u0435", "Asia/Calcutta" },
    // ==========

        { "ja", "America/Los_Angeles", "2004-01-15T00:00:00Z", "Z", "-0800", "-8:00" },
        { "ja", "America/Los_Angeles", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-08:00", "-8:00" },
        { "ja", "America/Los_Angeles", "2004-01-15T00:00:00Z", "z", "GMT-8", "America/Los_Angeles" },
        { "ja", "America/Los_Angeles", "2004-01-15T00:00:00Z", "zzzz", "\\u30a2\\u30e1\\u30ea\\u30ab\\u592a\\u5e73\\u6d0b\\u6a19\\u6e96\\u6642", "America/Los_Angeles" },
        { "ja", "America/Los_Angeles", "2004-07-15T00:00:00Z", "Z", "-0700", "-700" },
        { "ja", "America/Los_Angeles", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-07:00", "-7:00" },
        { "ja", "America/Los_Angeles", "2004-07-15T00:00:00Z", "z", "GMT-7", "America/Los_Angeles" },
        { "ja", "America/Los_Angeles", "2004-07-15T00:00:00Z", "zzzz", "\\u30a2\\u30e1\\u30ea\\u30ab\\u592a\\u5e73\\u6d0b\\u590f\\u6642\\u9593", "America/Los_Angeles" },
    // icu ja.txt has exemplar city for this time zone
        { "ja", "America/Los_Angeles", "2004-07-15T00:00:00Z", "v", "\\u30ED\\u30B5\\u30F3\\u30BC\\u30EB\\u30B9\\u6642\\u9593", "America/Los_Angeles" },
        { "ja", "America/Los_Angeles", "2004-07-15T00:00:00Z", "vvvv", "\\u30A2\\u30E1\\u30EA\\u30AB\\u592A\\u5e73\\u6D0B\\u6642\\u9593", "America/Los_Angeles" },
        { "ja", "America/Los_Angeles", "2004-07-15T00:00:00Z", "VVVV", "\\u30ED\\u30B5\\u30F3\\u30BC\\u30EB\\u30B9\\u6642\\u9593", "America/Los_Angeles" },

        { "ja", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ja", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ja", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ja", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "\\u30A2\\u30EB\\u30BC\\u30F3\\u30C1\\u30F3\\u6A19\\u6E96\\u6642", "-3:00" },
        { "ja", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ja", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ja", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ja", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "\\u30A2\\u30EB\\u30BC\\u30F3\\u30C1\\u30F3\\u6A19\\u6E96\\u6642", "-3:00" },
    // icu ja.txt does not have info for this time zone
        { "ja", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "\\u30D6\\u30A8\\u30CE\\u30B9\\u30A2\\u30A4\\u30EC\\u30B9\\u6642\\u9593", "America/Buenos_Aires" },
        { "ja", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "\\u30A2\\u30EB\\u30BC\\u30F3\\u30C1\\u30F3\\u6A19\\u6E96\\u6642", "America/Buenos_Aires" },

        { "ja", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ja", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ja", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ja", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "\\u30A2\\u30EB\\u30BC\\u30F3\\u30C1\\u30F3\\u6A19\\u6E96\\u6642", "-3:00" },
        { "ja", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ja", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ja", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ja", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "\\u30A2\\u30EB\\u30BC\\u30F3\\u30C1\\u30F3\\u6A19\\u6E96\\u6642", "-3:00" },
        { "ja", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "\\u30D6\\u30A8\\u30CE\\u30B9\\u30A2\\u30A4\\u30EC\\u30B9\\u6642\\u9593", "America/Buenos_Aires" },
        { "ja", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "\\u30A2\\u30EB\\u30BC\\u30F3\\u30C1\\u30F3\\u6A19\\u6E96\\u6642", "America/Buenos_Aires" },

        { "ja", "America/Havana", "2004-01-15T00:00:00Z", "Z", "-0500", "-5:00" },
        { "ja", "America/Havana", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-05:00", "-5:00" },
        { "ja", "America/Havana", "2004-01-15T00:00:00Z", "z", "GMT-5", "-5:00" },
        { "ja", "America/Havana", "2004-01-15T00:00:00Z", "zzzz", "\\u30AD\\u30E5\\u30FC\\u30D0\\u6A19\\u6E96\\u6642", "-5:00" },
        { "ja", "America/Havana", "2004-07-15T00:00:00Z", "Z", "-0400", "-4:00" },
        { "ja", "America/Havana", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-04:00", "-4:00" },
        { "ja", "America/Havana", "2004-07-15T00:00:00Z", "z", "GMT-4", "-4:00" },
        { "ja", "America/Havana", "2004-07-15T00:00:00Z", "zzzz", "\\u30AD\\u30E5\\u30FC\\u30D0\\u590F\\u6642\\u9593", "-4:00" },
        { "ja", "America/Havana", "2004-07-15T00:00:00Z", "v", "\\u30ad\\u30e5\\u30fc\\u30d0\\u6642\\u9593", "America/Havana" },
        { "ja", "America/Havana", "2004-07-15T00:00:00Z", "vvvv", "\\u30ad\\u30e5\\u30fc\\u30d0\\u6642\\u9593", "America/Havana" },

        { "ja", "Australia/ACT", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "ja", "Australia/ACT", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "ja", "Australia/ACT", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "ja", "Australia/ACT", "2004-01-15T00:00:00Z", "zzzz", "\\u30AA\\u30FC\\u30B9\\u30C8\\u30E9\\u30EA\\u30A2\\u6771\\u90E8\\u590F\\u6642\\u9593", "+11:00" },
        { "ja", "Australia/ACT", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "ja", "Australia/ACT", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "ja", "Australia/ACT", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "ja", "Australia/ACT", "2004-07-15T00:00:00Z", "zzzz", "\\u30AA\\u30FC\\u30B9\\u30C8\\u30E9\\u30EA\\u30A2\\u6771\\u90E8\\u6A19\\u6E96\\u6642", "+10:00" },
    // icu ja.txt does not have info for this time zone
        { "ja", "Australia/ACT", "2004-07-15T00:00:00Z", "v", "\\u30B7\\u30C9\\u30CB\\u30FC\\u6642\\u9593", "Australia/Sydney" },
        { "ja", "Australia/ACT", "2004-07-15T00:00:00Z", "vvvv", "\\u30AA\\u30FC\\u30B9\\u30C8\\u30E9\\u30EA\\u30A2\\u6771\\u90E8\\u6642\\u9593", "Australia/Sydney" },

        { "ja", "Australia/Sydney", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "ja", "Australia/Sydney", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "ja", "Australia/Sydney", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "ja", "Australia/Sydney", "2004-01-15T00:00:00Z", "zzzz", "\\u30AA\\u30FC\\u30B9\\u30C8\\u30E9\\u30EA\\u30A2\\u6771\\u90E8\\u590F\\u6642\\u9593", "+11:00" },
        { "ja", "Australia/Sydney", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "ja", "Australia/Sydney", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "ja", "Australia/Sydney", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "ja", "Australia/Sydney", "2004-07-15T00:00:00Z", "zzzz", "\\u30AA\\u30FC\\u30B9\\u30C8\\u30E9\\u30EA\\u30A2\\u6771\\u90E8\\u6A19\\u6E96\\u6642", "+10:00" },
        { "ja", "Australia/Sydney", "2004-07-15T00:00:00Z", "v", "\\u30B7\\u30C9\\u30CB\\u30FC\\u6642\\u9593", "Australia/Sydney" },
        { "ja", "Australia/Sydney", "2004-07-15T00:00:00Z", "vvvv", "\\u30AA\\u30FC\\u30B9\\u30C8\\u30E9\\u30EA\\u30A2\\u6771\\u90E8\\u6642\\u9593", "Australia/Sydney" },

        { "ja", "Europe/London", "2004-01-15T00:00:00Z", "Z", "+0000", "+0:00" },
        { "ja", "Europe/London", "2004-01-15T00:00:00Z", "ZZZZ", "GMT", "+0:00" },
        { "ja", "Europe/London", "2004-01-15T00:00:00Z", "z", "GMT", "+0:00" },
        { "ja", "Europe/London", "2004-01-15T00:00:00Z", "zzzz", "\\u30B0\\u30EA\\u30CB\\u30C3\\u30B8\\u6A19\\u6E96\\u6642", "+0:00" },
        { "ja", "Europe/London", "2004-07-15T00:00:00Z", "Z", "+0100", "+1:00" },
        { "ja", "Europe/London", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+01:00", "+1:00" },
        { "ja", "Europe/London", "2004-07-15T00:00:00Z", "z", "GMT+1", "+1:00" },
        { "ja", "Europe/London", "2004-07-15T00:00:00Z", "zzzz", "\\u82f1\\u56fd\\u590f\\u6642\\u9593", "+1:00" },
        { "ja", "Europe/London", "2004-07-15T00:00:00Z", "v", "\\u30a4\\u30ae\\u30ea\\u30b9\\u6642\\u9593", "Europe/London" },
        { "ja", "Europe/London", "2004-07-15T00:00:00Z", "vvvv", "\\u30a4\\u30ae\\u30ea\\u30b9\\u6642\\u9593", "Europe/London" },
        { "ja", "Europe/London", "2004-07-15T00:00:00Z", "VVVV", "\\u30a4\\u30ae\\u30ea\\u30b9\\u6642\\u9593", "Europe/London" },

        { "ja", "Etc/GMT+3", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-01-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-07-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-07-15T00:00:00Z", "v", "GMT-3", "-3:00" },
        { "ja", "Etc/GMT+3", "2004-07-15T00:00:00Z", "vvvv", "GMT-03:00", "-3:00" },

        // JB#5150
        { "ja", "Asia/Calcutta", "2004-01-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "ja", "Asia/Calcutta", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "ja", "Asia/Calcutta", "2004-01-15T00:00:00Z", "z", "GMT+5:30", "+5:30" },
        { "ja", "Asia/Calcutta", "2004-01-15T00:00:00Z", "zzzz", "\\u30A4\\u30F3\\u30C9\\u6642\\u9593", "+5:30" },
        { "ja", "Asia/Calcutta", "2004-07-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "ja", "Asia/Calcutta", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "ja", "Asia/Calcutta", "2004-07-15T00:00:00Z", "z", "GMT+5:30", "+05:30" },
        { "ja", "Asia/Calcutta", "2004-07-15T00:00:00Z", "zzzz", "\\u30A4\\u30F3\\u30C9\\u6642\\u9593", "+5:30" },
        { "ja", "Asia/Calcutta", "2004-07-15T00:00:00Z", "v", "\\u30A4\\u30F3\\u30C9\\u6642\\u9593", "Asia/Calcutta" },
        { "ja", "Asia/Calcutta", "2004-07-15T00:00:00Z", "vvvv", "\\u30A4\\u30F3\\u30C9\\u6642\\u9593", "Asia/Calcutta" },

    // ==========

        { "ti", "America/Los_Angeles", "2004-01-15T00:00:00Z", "Z", "-0800", "-8:00" },
        { "ti", "America/Los_Angeles", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-08:00", "-8:00" },
        { "ti", "America/Los_Angeles", "2004-01-15T00:00:00Z", "z", "GMT-8", "-8:00" },
        { "ti", "America/Los_Angeles", "2004-01-15T00:00:00Z", "zzzz", "GMT-08:00", "-8:00" },
        { "ti", "America/Los_Angeles", "2004-07-15T00:00:00Z", "Z", "-0700", "-7:00" },
        { "ti", "America/Los_Angeles", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-07:00", "-7:00" },
        { "ti", "America/Los_Angeles", "2004-07-15T00:00:00Z", "z", "GMT-7", "-7:00" },
        { "ti", "America/Los_Angeles", "2004-07-15T00:00:00Z", "zzzz", "GMT-07:00", "-7:00" },
        { "ti", "America/Los_Angeles", "2004-07-15T00:00:00Z", "v", "Los Angeles", "America/Los_Angeles" },
        { "ti", "America/Los_Angeles", "2004-07-15T00:00:00Z", "vvvv", "Los Angeles", "America/Los_Angeles" },

        { "ti", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "Buenos Aires", "America/Buenos_Aires" },
        { "ti", "America/Argentina/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "Buenos Aires", "America/Buenos_Aires" },

        { "ti", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ti", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ti", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ti", "America/Buenos_Aires", "2004-01-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "ti", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ti", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ti", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ti", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "ti", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "v", "Buenos Aires", "America/Buenos_Aires" },
        { "ti", "America/Buenos_Aires", "2004-07-15T00:00:00Z", "vvvv", "Buenos Aires", "America/Buenos_Aires" },

        { "ti", "America/Havana", "2004-01-15T00:00:00Z", "Z", "-0500", "-5:00" },
        { "ti", "America/Havana", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-05:00", "-5:00" },
        { "ti", "America/Havana", "2004-01-15T00:00:00Z", "z", "GMT-5", "-5:00" },
        { "ti", "America/Havana", "2004-01-15T00:00:00Z", "zzzz", "GMT-05:00", "-5:00" },
        { "ti", "America/Havana", "2004-07-15T00:00:00Z", "Z", "-0400", "-4:00" },
        { "ti", "America/Havana", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-04:00", "-4:00" },
        { "ti", "America/Havana", "2004-07-15T00:00:00Z", "z", "GMT-4", "-4:00" },
        { "ti", "America/Havana", "2004-07-15T00:00:00Z", "zzzz", "GMT-04:00", "-4:00" },
        { "ti", "America/Havana", "2004-07-15T00:00:00Z", "v", "CU", "America/Havana" },
        { "ti", "America/Havana", "2004-07-15T00:00:00Z", "vvvv", "CU", "America/Havana" },

        { "ti", "Australia/ACT", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "ti", "Australia/ACT", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "ti", "Australia/ACT", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "ti", "Australia/ACT", "2004-01-15T00:00:00Z", "zzzz", "GMT+11:00", "+11:00" },
        { "ti", "Australia/ACT", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "ti", "Australia/ACT", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "ti", "Australia/ACT", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "ti", "Australia/ACT", "2004-07-15T00:00:00Z", "zzzz", "GMT+10:00", "+10:00" },
        { "ti", "Australia/ACT", "2004-07-15T00:00:00Z", "v", "Sydney", "Australia/Sydney" },
        { "ti", "Australia/ACT", "2004-07-15T00:00:00Z", "vvvv", "Sydney", "Australia/Sydney" },

        { "ti", "Australia/Sydney", "2004-01-15T00:00:00Z", "Z", "+1100", "+11:00" },
        { "ti", "Australia/Sydney", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+11:00", "+11:00" },
        { "ti", "Australia/Sydney", "2004-01-15T00:00:00Z", "z", "GMT+11", "+11:00" },
        { "ti", "Australia/Sydney", "2004-01-15T00:00:00Z", "zzzz", "GMT+11:00", "+11:00" },
        { "ti", "Australia/Sydney", "2004-07-15T00:00:00Z", "Z", "+1000", "+10:00" },
        { "ti", "Australia/Sydney", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+10:00", "+10:00" },
        { "ti", "Australia/Sydney", "2004-07-15T00:00:00Z", "z", "GMT+10", "+10:00" },
        { "ti", "Australia/Sydney", "2004-07-15T00:00:00Z", "zzzz", "GMT+10:00", "+10:00" },
        { "ti", "Australia/Sydney", "2004-07-15T00:00:00Z", "v", "Sydney", "Australia/Sydney" },
        { "ti", "Australia/Sydney", "2004-07-15T00:00:00Z", "vvvv", "Sydney", "Australia/Sydney" },

        { "ti", "Europe/London", "2004-01-15T00:00:00Z", "Z", "+0000", "+0:00" },
        { "ti", "Europe/London", "2004-01-15T00:00:00Z", "ZZZZ", "GMT", "+0:00" },
        { "ti", "Europe/London", "2004-01-15T00:00:00Z", "z", "GMT", "+0:00" },
        { "ti", "Europe/London", "2004-01-15T00:00:00Z", "zzzz", "GMT", "+0:00" },
        { "ti", "Europe/London", "2004-07-15T00:00:00Z", "Z", "+0100", "+1:00" },
        { "ti", "Europe/London", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+01:00", "+1:00" },
        { "ti", "Europe/London", "2004-07-15T00:00:00Z", "z", "GMT+1", "+1:00" },
        { "ti", "Europe/London", "2004-07-15T00:00:00Z", "zzzz", "GMT+01:00", "+1:00" },
        { "ti", "Europe/London", "2004-07-15T00:00:00Z", "v", "GB", "Europe/London" },
        { "ti", "Europe/London", "2004-07-15T00:00:00Z", "vvvv", "GB", "Europe/London" },

        { "ti", "Etc/GMT+3", "2004-01-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-01-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-01-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-01-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-07-15T00:00:00Z", "Z", "-0300", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-07-15T00:00:00Z", "ZZZZ", "GMT-03:00", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-07-15T00:00:00Z", "z", "GMT-3", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-07-15T00:00:00Z", "zzzz", "GMT-03:00", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-07-15T00:00:00Z", "v", "GMT-3", "-3:00" },
        { "ti", "Etc/GMT+3", "2004-07-15T00:00:00Z", "vvvv", "GMT-03:00", "-3:00" },

        // JB#5150
        { "ti", "Asia/Calcutta", "2004-01-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "ti", "Asia/Calcutta", "2004-01-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "ti", "Asia/Calcutta", "2004-01-15T00:00:00Z", "z", "GMT+5:30", "+5:30" },
        { "ti", "Asia/Calcutta", "2004-01-15T00:00:00Z", "zzzz", "GMT+05:30", "+5:30" },
        { "ti", "Asia/Calcutta", "2004-07-15T00:00:00Z", "Z", "+0530", "+5:30" },
        { "ti", "Asia/Calcutta", "2004-07-15T00:00:00Z", "ZZZZ", "GMT+05:30", "+5:30" },
        { "ti", "Asia/Calcutta", "2004-07-15T00:00:00Z", "z", "GMT+5:30", "+05:30" },
        { "ti", "Asia/Calcutta", "2004-07-15T00:00:00Z", "zzzz", "GMT+05:30", "+5:30" },
        { "ti", "Asia/Calcutta", "2004-07-15T00:00:00Z", "v", "IN", "Alna/Calcutta" },
        { "ti", "Asia/Calcutta", "2004-07-15T00:00:00Z", "vvvv", "IN", "Asia/Calcutta" },

        // Ticket#8589 Partial location name to use country name if the zone is the golden
        // zone for the time zone's country.
        { "en_MX", "America/Chicago", "1995-07-15T00:00:00Z", "vvvv", "Central Time (United States)", "America/Chicago"},

        // Tests proper handling of time zones that should have empty sets when inherited from the parent.
        // For example, en_GB understands CET as Central European Time, but en_HK, which inherits from en_GB
        // does not
        { "en_GB", "Europe/Paris", "2004-01-15T00:00:00Z", "zzzz", "Central European Standard Time", "+1:00"},
        { "en_GB", "Europe/Paris", "2004-07-15T00:00:00Z", "zzzz", "Central European Summer Time", "+2:00"},
        { "en_GB", "Europe/Paris", "2004-01-15T00:00:00Z", "z", "CET", "+1:00"},
        { "en_GB", "Europe/Paris", "2004-07-15T00:00:00Z", "z", "CEST", "+2:00"},
        { "en_HK", "Europe/Paris", "2004-01-15T00:00:00Z", "zzzz", "Central European Standard Time", "+1:00"},
        { "en_HK", "Europe/Paris", "2004-07-15T00:00:00Z", "zzzz", "Central European Summer Time", "+2:00"},
        { "en_HK", "Europe/Paris", "2004-01-15T00:00:00Z", "z", "GMT+1", "+1:00"},
        { "en_HK", "Europe/Paris", "2004-07-15T00:00:00Z", "z", "GMT+2", "+2:00"},

        { NULL, NULL, NULL, NULL, NULL, NULL },
    };

    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = GregorianCalendar::createInstance(status);
    if (failure(status, "GregorianCalendar::createInstance", TRUE)) return;
    SimpleDateFormat testfmt(UnicodeString("yyyy-MM-dd'T'HH:mm:ss'Z'"), status);
    if (failure(status, "SimpleDateFormat constructor", TRUE)) return;
    testfmt.setTimeZone(*TimeZone::getGMT());

    for (int i = 0; fallbackTests[i][0]; i++) {
        const char **testLine = fallbackTests[i];
        UnicodeString info[5];
        for ( int j = 0 ; j < 5 ; j++ ) {
            info[j] = UnicodeString(testLine[j], -1, US_INV);
        }
        info[4] = info[4].unescape();
        logln("%s;%s;%s;%s", testLine[0], testLine[1], testLine[2], testLine[3]);

        TimeZone *tz = TimeZone::createTimeZone(info[1]);

        UDate d = testfmt.parse(testLine[2], status);
        cal->setTime(d, status);
        if (U_FAILURE(status)) {
            errln(UnicodeString("Failed to set date: ") + testLine[2]);
        }

        SimpleDateFormat fmt(info[3], Locale(testLine[0]),status);
        ASSERT_OK(status);
        cal->adoptTimeZone(tz);
        UnicodeString result;
        FieldPosition pos(0);
        fmt.format(*cal,result,pos);
        if (result != info[4]) {
            errln(info[0] + ";" + info[1] + ";" + info[2] + ";" + info[3] + " expected: '" +
                  info[4] + "' but got: '" + result + "'");
        }
    }
    delete cal;
}

void DateFormatTest::TestRoundtripWithCalendar(void) {
    UErrorCode status = U_ZERO_ERROR;

    TimeZone *tz = TimeZone::createTimeZone("Europe/Paris");
    TimeZone *gmt = TimeZone::createTimeZone("Etc/GMT");

    Calendar *calendars[] = {
        Calendar::createInstance(*tz, Locale("und@calendar=gregorian"), status),
        Calendar::createInstance(*tz, Locale("und@calendar=buddhist"), status),
//        Calendar::createInstance(*tz, Locale("und@calendar=hebrew"), status),
        Calendar::createInstance(*tz, Locale("und@calendar=islamic"), status),
        Calendar::createInstance(*tz, Locale("und@calendar=japanese"), status),
        NULL
    };
    if (U_FAILURE(status)) {
        dataerrln("Failed to initialize calendars: %s", u_errorName(status));
        for (int i = 0; calendars[i] != NULL; i++) {
            delete calendars[i];
        }
        return;
    }

    //FIXME The formatters commented out below are currently failing because of
    // the calendar calculation problem reported by #6691

    // The order of test formatters must match the order of calendars above.
    DateFormat *formatters[] = {
        DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull, Locale("en_US")), //calendar=gregorian
        DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull, Locale("th_TH")), //calendar=buddhist
//        DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull, Locale("he_IL@calendar=hebrew")),
        DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull, Locale("ar_EG@calendar=islamic")),
//        DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull, Locale("ja_JP@calendar=japanese")),
        NULL
    };

    UDate d = Calendar::getNow();
    UnicodeString buf;
    FieldPosition fpos;
    ParsePosition ppos;

    for (int i = 0; formatters[i] != NULL; i++) {
        buf.remove();
        fpos.setBeginIndex(0);
        fpos.setEndIndex(0);
        calendars[i]->setTime(d, status);

        // Normal case output - the given calendar matches the calendar
        // used by the formatter
        formatters[i]->format(*calendars[i], buf, fpos);
        UnicodeString refStr(buf);

        for (int j = 0; calendars[j] != NULL; j++) {
            if (j == i) {
                continue;
            }
            buf.remove();
            fpos.setBeginIndex(0);
            fpos.setEndIndex(0);
            calendars[j]->setTime(d, status);

            // Even the different calendar type is specified,
            // we should get the same result.
            formatters[i]->format(*calendars[j], buf, fpos);
            if (refStr != buf) {
                errln((UnicodeString)"FAIL: Different format result with a different calendar for the same time -"
                        + "\n Reference calendar type=" + calendars[i]->getType()
                        + "\n Another calendar type=" + calendars[j]->getType()
                        + "\n Expected result=" + refStr
                        + "\n Actual result=" + buf);
            }
        }

        calendars[i]->setTimeZone(*gmt);
        calendars[i]->clear();
        ppos.setErrorIndex(-1);
        ppos.setIndex(0);

        // Normal case parse result - the given calendar matches the calendar
        // used by the formatter
        formatters[i]->parse(refStr, *calendars[i], ppos);

        for (int j = 0; calendars[j] != NULL; j++) {
            if (j == i) {
                continue;
            }
            calendars[j]->setTimeZone(*gmt);
            calendars[j]->clear();
            ppos.setErrorIndex(-1);
            ppos.setIndex(0);

            // Even the different calendar type is specified,
            // we should get the same time and time zone.
            formatters[i]->parse(refStr, *calendars[j], ppos);
            if (calendars[i]->getTime(status) != calendars[j]->getTime(status)
                || calendars[i]->getTimeZone() != calendars[j]->getTimeZone()) {
                UnicodeString tzid;
                errln((UnicodeString)"FAIL: Different parse result with a different calendar for the same string -"
                        + "\n Reference calendar type=" + calendars[i]->getType()
                        + "\n Another calendar type=" + calendars[j]->getType()
                        + "\n Date string=" + refStr
                        + "\n Expected time=" + calendars[i]->getTime(status)
                        + "\n Expected time zone=" + calendars[i]->getTimeZone().getID(tzid)
                        + "\n Actual time=" + calendars[j]->getTime(status)
                        + "\n Actual time zone=" + calendars[j]->getTimeZone().getID(tzid));
            }
        }
        if (U_FAILURE(status)) {
            errln((UnicodeString)"FAIL: " + u_errorName(status));
            break;
        }
    }

    delete tz;
    delete gmt;
    for (int i = 0; calendars[i] != NULL; i++) {
        delete calendars[i];
    }
    for (int i = 0; formatters[i] != NULL; i++) {
        delete formatters[i];
    }
}

/*
void DateFormatTest::TestRelativeError(void)
{
    UErrorCode status;
    Locale en("en");

    DateFormat *en_reltime_reldate =         DateFormat::createDateTimeInstance(DateFormat::kFullRelative,DateFormat::kFullRelative,en);
    if(en_reltime_reldate == NULL) {
        logln("PASS: rel date/rel time failed");
    } else {
        errln("FAIL: rel date/rel time created, should have failed.");
        delete en_reltime_reldate;
    }
}

void DateFormatTest::TestRelativeOther(void)
{
    logln("Nothing in this test. When we get more data from CLDR, put in some tests of -2, +2, etc. ");
}
*/

void DateFormatTest::Test6338(void)
{
    UErrorCode status = U_ZERO_ERROR;

    SimpleDateFormat *fmt1 = new SimpleDateFormat(UnicodeString("y-M-d"), Locale("ar"), status);
    if (failure(status, "new SimpleDateFormat", TRUE)) return;

    UDate dt1 = date(2008-1900, UCAL_JUNE, 10, 12, 00);
    UnicodeString str1;
    str1 = fmt1->format(dt1, str1);
    logln(str1);

    UDate dt11 = fmt1->parse(str1, status);
    failure(status, "fmt->parse");

    UnicodeString str11;
    str11 = fmt1->format(dt11, str11);
    logln(str11);

    if (str1 != str11) {
        errln((UnicodeString)"FAIL: Different dates str1:" + str1
            + " str2:" + str11);
    }
    delete fmt1;

    /////////////////

    status = U_ZERO_ERROR;
    SimpleDateFormat *fmt2 = new SimpleDateFormat(UnicodeString("y M d"), Locale("ar"), status);
    failure(status, "new SimpleDateFormat");

    UDate dt2 = date(2008-1900, UCAL_JUNE, 10, 12, 00);
    UnicodeString str2;
    str2 = fmt2->format(dt2, str2);
    logln(str2);

    UDate dt22 = fmt2->parse(str2, status);
    failure(status, "fmt->parse");

    UnicodeString str22;
    str22 = fmt2->format(dt22, str22);
    logln(str22);

    if (str2 != str22) {
        errln((UnicodeString)"FAIL: Different dates str1:" + str2
            + " str2:" + str22);
    }
    delete fmt2;

    /////////////////

    status = U_ZERO_ERROR;
    SimpleDateFormat *fmt3 = new SimpleDateFormat(UnicodeString("y-M-d"), Locale("en-us"), status);
    failure(status, "new SimpleDateFormat");

    UDate dt3 = date(2008-1900, UCAL_JUNE, 10, 12, 00);
    UnicodeString str3;
    str3 = fmt3->format(dt3, str3);
    logln(str3);

    UDate dt33 = fmt3->parse(str3, status);
    failure(status, "fmt->parse");

    UnicodeString str33;
    str33 = fmt3->format(dt33, str33);
    logln(str33);

    if (str3 != str33) {
        errln((UnicodeString)"FAIL: Different dates str1:" + str3
            + " str2:" + str33);
    }
    delete fmt3;

    /////////////////

    status = U_ZERO_ERROR;
    SimpleDateFormat *fmt4 = new SimpleDateFormat(UnicodeString("y M  d"), Locale("en-us"), status);
    failure(status, "new SimpleDateFormat");

    UDate dt4 = date(2008-1900, UCAL_JUNE, 10, 12, 00);
    UnicodeString str4;
    str4 = fmt4->format(dt4, str4);
    logln(str4);

    UDate dt44 = fmt4->parse(str4, status);
    failure(status, "fmt->parse");

    UnicodeString str44;
    str44 = fmt4->format(dt44, str44);
    logln(str44);

    if (str4 != str44) {
        errln((UnicodeString)"FAIL: Different dates str1:" + str4
            + " str2:" + str44);
    }
    delete fmt4;

}

void DateFormatTest::Test6726(void)
{
    // status
//    UErrorCode status = U_ZERO_ERROR;

    // fmtf, fmtl, fmtm, fmts;
    UnicodeString strf, strl, strm, strs;
    UDate dt = date(2008-1900, UCAL_JUNE, 10, 12, 00);

    Locale loc("ja");
    DateFormat* fmtf = DateFormat::createDateTimeInstance(DateFormat::FULL, DateFormat::FULL, loc);
    DateFormat* fmtl = DateFormat::createDateTimeInstance(DateFormat::LONG, DateFormat::FULL, loc);
    DateFormat* fmtm = DateFormat::createDateTimeInstance(DateFormat::MEDIUM, DateFormat::FULL, loc);
    DateFormat* fmts = DateFormat::createDateTimeInstance(DateFormat::SHORT, DateFormat::FULL, loc);
    if (fmtf == NULL || fmtl == NULL || fmtm == NULL || fmts == NULL) {
        dataerrln("Unable to create DateFormat. got NULL.");
        /* It may not be true that if one is NULL all is NULL.  Just to be safe. */
        delete fmtf;
        delete fmtl;
        delete fmtm;
        delete fmts;

        return;
    }
    strf = fmtf->format(dt, strf);
    strl = fmtl->format(dt, strl);
    strm = fmtm->format(dt, strm);
    strs = fmts->format(dt, strs);


    logln("strm.charAt(10)=%04X wanted 0x20\n", strm.charAt(10));
    if (strm.charAt(10) != UChar(0x0020)) {
      errln((UnicodeString)"FAIL: Improper formatted date: " + strm );
    }
    logln("strs.charAt(10)=%04X wanted 0x20\n", strs.charAt(8));
    if (strs.charAt(10)  != UChar(0x0020)) {
        errln((UnicodeString)"FAIL: Improper formatted date: " + strs);
    }

    delete fmtf;
    delete fmtl;
    delete fmtm;
    delete fmts;

    return;
}

/**
 * Test DateFormat's parsing of default GMT variants.  See ticket#6135
 */
void DateFormatTest::TestGMTParsing() {
    const char* DATA[] = {
        "HH:mm:ss Z",

        // pattern, input, expected output (in quotes)
        "HH:mm:ss Z",       "10:20:30 GMT+03:00",   "10:20:30 +0300",
        "HH:mm:ss Z",       "10:20:30 UT-02:00",    "10:20:30 -0200",
        "HH:mm:ss Z",       "10:20:30 GMT",         "10:20:30 +0000",
        "HH:mm:ss vvvv",    "10:20:30 UT+10:00",    "10:20:30 +1000",
        "HH:mm:ss zzzz",    "10:20:30 UTC",         "10:20:30 +0000",   // standalone "UTC"
        "ZZZZ HH:mm:ss",    "UT 10:20:30",          "10:20:30 +0000",
        "z HH:mm:ss",       "UT+0130 10:20:30",     "10:20:30 +0130",
        "z HH:mm:ss",       "UTC+0130 10:20:30",    "10:20:30 +0130",
        // Note: GMT-1100 no longer works because of the introduction of the short
        // localized GMT support. Previous implementation support this level of
        // leniency (no separator char in localized GMT format), but the new
        // implementation handles GMT-11 as the legitimate short localized GMT format
        // and stop at there. Otherwise, roundtrip would be broken.
        //"HH mm Z ss",       "10 20 GMT-1100 30",    "10:20:30 -1100",
        "HH mm Z ss",       "10 20 GMT-11 30",    "10:20:30 -1100",
        "HH:mm:ssZZZZZ",    "14:25:45Z",            "14:25:45 +0000",
        "HH:mm:ssZZZZZ",    "15:00:00-08:00",       "15:00:00 -0800",
    };
    const int32_t DATA_len = sizeof(DATA)/sizeof(DATA[0]);
    expectParse(DATA, DATA_len, Locale("en"));
}

// Test case for localized GMT format parsing
// with no delimitters in offset format (Chinese locale)
void DateFormatTest::Test6880() {
    UErrorCode status = U_ZERO_ERROR;
    UDate d1, d2, dp1, dp2, dexp1, dexp2;
    UnicodeString s1, s2;

    TimeZone *tz = TimeZone::createTimeZone("Asia/Shanghai");
    GregorianCalendar gcal(*tz, status);
    if (failure(status, "construct GregorianCalendar", TRUE)) return;
    
    gcal.clear();
    gcal.set(1910, UCAL_JULY, 1, 12, 00);   // offset 8:05:57
    d1 = gcal.getTime(status);

    gcal.clear();
    gcal.set(1950, UCAL_JULY, 1, 12, 00);   // offset 8:00
    d2 = gcal.getTime(status);

    gcal.clear();
    gcal.set(1970, UCAL_JANUARY, 1, 12, 00);
    dexp2 = gcal.getTime(status);
    dexp1 = dexp2 - (5*60 + 57)*1000;   // subtract 5m57s

    if (U_FAILURE(status)) {
        errln("FAIL: Gregorian calendar error");
    }

    DateFormat *fmt = DateFormat::createTimeInstance(DateFormat::kFull, Locale("zh"));
    if (fmt == NULL) {
        dataerrln("Unable to create DateFormat. Got NULL.");
        return;
    }
    fmt->adoptTimeZone(tz);

    fmt->format(d1, s1);
    fmt->format(d2, s2);

    dp1 = fmt->parse(s1, status);
    dp2 = fmt->parse(s2, status);

    if (U_FAILURE(status)) {
        errln("FAIL: Parse failure");
    }

    if (dp1 != dexp1) {
        errln("FAIL: Failed to parse " + s1 + " parsed: " + dp1 + " expected: " + dexp1);
    }
    if (dp2 != dexp2) {
        errln("FAIL: Failed to parse " + s2 + " parsed: " + dp2 + " expected: " + dexp2);
    }

    delete fmt;
}

typedef struct {
    const char * localeStr;
    UBool        lenient;
    UBool        expectFail;
    UnicodeString datePattern;
    UnicodeString dateString;
} NumAsStringItem;

void DateFormatTest::TestNumberAsStringParsing()
{
    const NumAsStringItem items[] = {
        // loc lenient fail?  datePattern                                         dateString
        { "",   FALSE, TRUE,  UnicodeString("y MMMM d HH:mm:ss"),                 UnicodeString("2009 7 14 08:43:57") },
        { "",   TRUE,  FALSE, UnicodeString("y MMMM d HH:mm:ss"),                 UnicodeString("2009 7 14 08:43:57") },
        { "en", FALSE, FALSE, UnicodeString("MMM d, y"),                          UnicodeString("Jul 14, 2009") },
        { "en", TRUE,  FALSE, UnicodeString("MMM d, y"),                          UnicodeString("Jul 14, 2009") },
        { "en", FALSE, TRUE,  UnicodeString("MMM d, y"),                          UnicodeString("7 14, 2009") },
        { "en", TRUE,  FALSE, UnicodeString("MMM d, y"),                          UnicodeString("7 14, 2009") },
        { "ja", FALSE, FALSE, UnicodeString("yyyy/MM/dd"),                        UnicodeString("2009/07/14")         },
        { "ja", TRUE,  FALSE, UnicodeString("yyyy/MM/dd"),                        UnicodeString("2009/07/14")         },
      //{ "ja", FALSE, FALSE, UnicodeString("yyyy/MMMMM/d"),                      UnicodeString("2009/7/14")          }, // #8860 covers test failure
        { "ja", TRUE,  FALSE, UnicodeString("yyyy/MMMMM/d"),                      UnicodeString("2009/7/14")          },
        { "ja", FALSE, FALSE, CharsToUnicodeString("y\\u5E74M\\u6708d\\u65E5"),   CharsToUnicodeString("2009\\u5E747\\u670814\\u65E5")   },
        { "ja", TRUE,  FALSE, CharsToUnicodeString("y\\u5E74M\\u6708d\\u65E5"),   CharsToUnicodeString("2009\\u5E747\\u670814\\u65E5")   },
        { "ja", FALSE, FALSE, CharsToUnicodeString("y\\u5E74MMMd\\u65E5"),        CharsToUnicodeString("2009\\u5E747\\u670814\\u65E5")   },
        { "ja", TRUE,  FALSE, CharsToUnicodeString("y\\u5E74MMMd\\u65E5"),        CharsToUnicodeString("2009\\u5E747\\u670814\\u65E5")   }, // #8820 fixes test failure
        { "ko", FALSE, FALSE, UnicodeString("yyyy. M. d."),                       UnicodeString("2009. 7. 14.")       },
        { "ko", TRUE,  FALSE, UnicodeString("yyyy. M. d."),                       UnicodeString("2009. 7. 14.")       },
        { "ko", FALSE, FALSE, UnicodeString("yyyy. MMMMM d."),                    CharsToUnicodeString("2009. 7\\uC6D4 14.")             },
        { "ko", TRUE,  FALSE, UnicodeString("yyyy. MMMMM d."),                    CharsToUnicodeString("2009. 7\\uC6D4 14.")             }, // #8820 fixes test failure
        { "ko", FALSE, FALSE, CharsToUnicodeString("y\\uB144 M\\uC6D4 d\\uC77C"), CharsToUnicodeString("2009\\uB144 7\\uC6D4 14\\uC77C") },
        { "ko", TRUE,  FALSE, CharsToUnicodeString("y\\uB144 M\\uC6D4 d\\uC77C"), CharsToUnicodeString("2009\\uB144 7\\uC6D4 14\\uC77C") },
        { "ko", FALSE, FALSE, CharsToUnicodeString("y\\uB144 MMM d\\uC77C"),      CharsToUnicodeString("2009\\uB144 7\\uC6D4 14\\uC77C") },
        { "ko", TRUE,  FALSE, CharsToUnicodeString("y\\uB144 MMM d\\uC77C"),      CharsToUnicodeString("2009\\uB144 7\\uC6D4 14\\uC77C") }, // #8820 fixes test failure
        { NULL, FALSE, FALSE, UnicodeString(""),                                  UnicodeString("")                   }
    };
    const NumAsStringItem * itemPtr;
    for (itemPtr = items; itemPtr->localeStr != NULL; itemPtr++ ) {
        Locale locale = Locale::createFromName(itemPtr->localeStr);
        UErrorCode status = U_ZERO_ERROR;
        SimpleDateFormat *formatter = new SimpleDateFormat(itemPtr->datePattern, locale, status);
        if (formatter == NULL || U_FAILURE(status)) {
            dataerrln("Unable to create SimpleDateFormat - %s", u_errorName(status));
            return;
        }

        formatter->setLenient(itemPtr->lenient);
        formatter->setBooleanAttribute(UDAT_PARSE_ALLOW_WHITESPACE, itemPtr->lenient, status).setBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, itemPtr->lenient, status);
        UDate date1 = formatter->parse(itemPtr->dateString, status);
        if (U_FAILURE(status)) {
            if (!itemPtr->expectFail) {
                errln("FAIL, err when expected success: Locale \"" + UnicodeString(itemPtr->localeStr) + "\", lenient " + itemPtr->lenient +
                        ": using pattern \"" + itemPtr->datePattern + "\", could not parse \"" + itemPtr->dateString + "\"; err: " + u_errorName(status) );
            }
        } else if (itemPtr->expectFail) {
                errln("FAIL, expected err but got none: Locale \"" + UnicodeString(itemPtr->localeStr) + "\", lenient " + itemPtr->lenient +
                        ": using pattern \"" + itemPtr->datePattern + "\", did parse \"" + itemPtr->dateString + "\"." );
        } else if (!itemPtr->lenient) {
            UnicodeString formatted;
            formatter->format(date1, formatted);
            if (formatted != itemPtr->dateString) {
                errln("FAIL, mismatch formatting parsed date: Locale \"" + UnicodeString(itemPtr->localeStr) + "\", lenient " + itemPtr->lenient +
                        ": using pattern \"" + itemPtr->datePattern + "\", did parse \"" + itemPtr->dateString + "\", formatted result \"" + formatted + "\".");
            }
        }

        delete formatter;
    }
}

void DateFormatTest::TestISOEra() { 
   
    const char* data[] = { 
    // input, output 
    "BC 4004-10-23T07:00:00Z", "BC 4004-10-23T07:00:00Z", 
    "AD 4004-10-23T07:00:00Z", "AD 4004-10-23T07:00:00Z", 
    "-4004-10-23T07:00:00Z"  , "BC 4005-10-23T07:00:00Z", 
    "4004-10-23T07:00:00Z"   , "AD 4004-10-23T07:00:00Z", 
    }; 
 
    int32_t numData = 8; 
 
    UErrorCode status = U_ZERO_ERROR; 
 
    // create formatter 
    SimpleDateFormat *fmt1 = new SimpleDateFormat(UnicodeString("GGG yyyy-MM-dd'T'HH:mm:ss'Z"), status); 
    failure(status, "new SimpleDateFormat", TRUE); 
    if (status == U_MISSING_RESOURCE_ERROR) {
        if (fmt1 != NULL) {
            delete fmt1;
        }
        return;
    }
    for(int i=0; i < numData; i+=2) { 
        // create input string 
        UnicodeString in = data[i]; 
 
        // parse string to date 
        UDate dt1 = fmt1->parse(in, status); 
        failure(status, "fmt->parse", TRUE); 
 
        // format date back to string 
        UnicodeString out; 
        out = fmt1->format(dt1, out); 
        logln(out); 
 
        // check that roundtrip worked as expected 
        UnicodeString expected = data[i+1]; 
        if (out != expected) { 
            dataerrln((UnicodeString)"FAIL: " + in + " -> " + out + " expected -> " + expected); 
        } 
    } 
 
    delete fmt1; 
} 
void DateFormatTest::TestFormalChineseDate() { 
   
    UErrorCode status = U_ZERO_ERROR; 
    UnicodeString pattern ("y\\u5e74M\\u6708d\\u65e5", -1, US_INV );
    pattern = pattern.unescape();
    UnicodeString override ("y=hanidec;M=hans;d=hans", -1, US_INV );
    
    // create formatter 
    SimpleDateFormat *sdf = new SimpleDateFormat(pattern,override,Locale::getChina(),status);
    failure(status, "new SimpleDateFormat with override", TRUE); 

    UDate thedate = date(2009-1900, UCAL_JULY, 28);
    FieldPosition pos(0);
    UnicodeString result;
    sdf->format(thedate,result,pos);
 
    UnicodeString expected = "\\u4e8c\\u3007\\u3007\\u4e5d\\u5e74\\u4e03\\u6708\\u4e8c\\u5341\\u516b\\u65e5"; 
    expected = expected.unescape();
    if (result != expected) { 
        dataerrln((UnicodeString)"FAIL: -> " + result + " expected -> " + expected); 
    } 
 
    UDate parsedate = sdf->parse(expected,status);
    if ( parsedate != thedate ) {
        UnicodeString pat1 ("yyyy-MM-dd'T'HH:mm:ss'Z'", -1, US_INV );
        SimpleDateFormat *usf = new SimpleDateFormat(pat1,Locale::getEnglish(),status);
        UnicodeString parsedres,expres;
        usf->format(parsedate,parsedres,pos);
        usf->format(thedate,expres,pos);
        dataerrln((UnicodeString)"FAIL: parsed -> " + parsedres + " expected -> " + expres); 
        delete usf;
    }
    delete sdf;
}

// Test case for #8675
// Incorrect parse offset with stand alone GMT string on 2nd or later iteration.
void DateFormatTest::TestStandAloneGMTParse() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleDateFormat *sdf = new SimpleDateFormat("ZZZZ", Locale(""), status);
    
    if (U_SUCCESS(status)) {

        UnicodeString inText("GMT$$$");
        for (int32_t i = 0; i < 10; i++) {
            ParsePosition pos(0);
            sdf->parse(inText, pos);
            if (pos.getIndex() != 3) {
                errln((UnicodeString)"FAIL: Incorrect output parse position: actual=" + pos.getIndex() + " expected=3");
            }
        }

        delete sdf;
    } else {
        dataerrln("Unable to create SimpleDateFormat - %s", u_errorName(status));
    }
}

void DateFormatTest::TestParsePosition() {
    const char* TestData[][4] = {
        // {<pattern>, <lead>, <date string>, <trail>}
        {"yyyy-MM-dd HH:mm:ssZ", "", "2010-01-10 12:30:00+0500", ""},
        {"yyyy-MM-dd HH:mm:ss ZZZZ", "", "2010-01-10 12:30:00 GMT+05:00", ""},
        {"Z HH:mm:ss", "", "-0100 13:20:30", ""},
        {"y-M-d Z", "", "2011-8-25 -0400", " Foo"},
        {"y/M/d H:mm:ss z", "", "2011/7/1 12:34:00 PDT", ""},
        {"y/M/d H:mm:ss z", "+123", "2011/7/1 12:34:00 PDT", " PST"},
        {"vvvv a h:mm:ss", "", "Pacific Time AM 10:21:45", ""},
        {"HH:mm v M/d", "111", "14:15 PT 8/10", " 12345"},
        {"'time zone:' VVVV 'date:' yyyy-MM-dd", "xxxx", "time zone: Los Angeles Time date: 2010-02-25", "xxxx"},
        {"yG", "", "2012AD", ""},
        {"yG", "", "2012", "x"},
        {0, 0, 0, 0},
    };

    for (int32_t i = 0; TestData[i][0]; i++) {
        UErrorCode status = U_ZERO_ERROR;
        SimpleDateFormat *sdf = new SimpleDateFormat(UnicodeString(TestData[i][0]), status);
        if (failure(status, "new SimpleDateFormat", TRUE)) return;

        int32_t startPos, resPos;

        // lead text
        UnicodeString input(TestData[i][1]);
        startPos = input.length();

        // date string
        input += TestData[i][2];
        resPos = input.length();

        // trail text
        input += TestData[i][3];

        ParsePosition pos(startPos);
        //UDate d = sdf->parse(input, pos);
        (void)sdf->parse(input, pos);

        if (pos.getIndex() != resPos) {
            errln(UnicodeString("FAIL: Parsing [") + input + "] with pattern [" + TestData[i][0] + "] returns position - "
                + pos.getIndex() + ", expected - " + resPos);
        }

        delete sdf;
    }
}


typedef struct {
    int32_t era;
    int32_t year;
    int32_t month; // 1-based
    int32_t isLeapMonth;
    int32_t day;
} ChineseCalTestDate;

#define NUM_TEST_DATES 3

typedef struct {
    const char *   locale;
    int32_t        style; // <0 => custom
    UnicodeString  dateString[NUM_TEST_DATES];
} MonthPatternItem;

void DateFormatTest::TestMonthPatterns()
{
    const ChineseCalTestDate dates[NUM_TEST_DATES] = {
        // era yr mo lp da
        {  78, 29, 4, 0, 2 }, // (in chinese era 78) gregorian 2012-4-22
        {  78, 29, 4, 1, 2 }, // (in chinese era 78) gregorian 2012-5-22
        {  78, 29, 5, 0, 2 }, // (in chinese era 78) gregorian 2012-6-20
    };

    const MonthPatternItem items[] = {
        // locale                     date style;           expected formats for the 3 dates above
        { "root@calendar=chinese",    DateFormat::kLong,  { UnicodeString("ren-chen M04 2"),  UnicodeString("ren-chen M04bis 2"),  UnicodeString("ren-chen M05 2") } },
        { "root@calendar=chinese",    DateFormat::kShort, { UnicodeString("29-04-02"),      UnicodeString("29-04bis-02"),           UnicodeString("29-05-02") } },
        { "root@calendar=chinese",    -1,                 { UnicodeString("29-4-2"),        UnicodeString("29-4bis-2"),             UnicodeString("29-5-2") } },
        { "root@calendar=chinese",    -2,                 { UnicodeString("78x29-4-2"),     UnicodeString("78x29-4bis-2"),          UnicodeString("78x29-5-2") } },
        { "root@calendar=chinese",    -3,                 { UnicodeString("ren-chen-4-2"),  UnicodeString("ren-chen-4bis-2"),       UnicodeString("ren-chen-5-2") } },
        { "root@calendar=chinese",    -4,                 { UnicodeString("ren-chen M04 2"),  UnicodeString("ren-chen M04bis 2"),   UnicodeString("ren-chen M05 2") } },
        { "en@calendar=gregorian",    -3,                 { UnicodeString("2012-4-22"),     UnicodeString("2012-5-22"),             UnicodeString("2012-6-20") } },
        { "en@calendar=chinese",      DateFormat::kLong,  { UnicodeString("Month4 2, ren-chen"), UnicodeString("Month4bis 2, ren-chen"), UnicodeString("Month5 2, ren-chen") } },
        { "en@calendar=chinese",      DateFormat::kShort, { UnicodeString("4/2/29"),        UnicodeString("4bis/2/29"),             UnicodeString("5/2/29") } },
        { "zh@calendar=chinese",      DateFormat::kLong,  { CharsToUnicodeString("\\u58EC\\u8FB0\\u5E74\\u56DB\\u6708\\u4E8C\\u65E5"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0\\u5E74\\u95F0\\u56DB\\u6708\\u4E8C\\u65E5"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0\\u5E74\\u4E94\\u6708\\u4E8C\\u65E5") } },
        { "zh@calendar=chinese",      DateFormat::kShort, { CharsToUnicodeString("\\u58EC\\u8FB0-4-2"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0-\\u95F04-2"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0-5-2") } },
        { "zh@calendar=chinese",      -3,                 { CharsToUnicodeString("\\u58EC\\u8FB0-4-2"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0-\\u95F04-2"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0-5-2") } },
        { "zh@calendar=chinese",      -4,                 { CharsToUnicodeString("\\u58EC\\u8FB0 \\u56DB\\u6708 2"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0 \\u95F0\\u56DB\\u6708 2"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0 \\u4E94\\u6708 2") } },
        { "zh_Hant@calendar=chinese", DateFormat::kLong,  { CharsToUnicodeString("\\u58EC\\u8FB0\\u5E74\\u56DB\\u6708\\u4E8C\\u65E5"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0\\u5E74\\u958F\\u56DB\\u6708\\u4E8C\\u65E5"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0\\u5E74\\u4E94\\u6708\\u4E8C\\u65E5") } },
        { "zh_Hant@calendar=chinese", DateFormat::kShort, { CharsToUnicodeString("\\u58EC\\u8FB0/4/2"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0/\\u958F4/2"),
                                                            CharsToUnicodeString("\\u58EC\\u8FB0/5/2") } },
        { "fr@calendar=chinese",      DateFormat::kLong,  { CharsToUnicodeString("2 s\\u00ECyu\\u00E8 ren-chen"),
                                                            CharsToUnicodeString("2 s\\u00ECyu\\u00E8bis ren-chen"),
                                                            CharsToUnicodeString("2 w\\u01D4yu\\u00E8 ren-chen") } },
        { "fr@calendar=chinese",      DateFormat::kShort, { UnicodeString("2/4/29"),        UnicodeString("2/4bis/29"),             UnicodeString("2/5/29") } },
        { "en@calendar=dangi",        DateFormat::kLong,  { UnicodeString("Month3bis 2, 29"),  UnicodeString("Month4 2, 29"),       UnicodeString("Month5 1, 29") } },
        { "en@calendar=dangi",        DateFormat::kShort, { UnicodeString("3bis/2/29"),     UnicodeString("4/2/29"),                UnicodeString("5/1/29") } },
        { "en@calendar=dangi",        -2,                 { UnicodeString("78x29-3bis-2"),  UnicodeString("78x29-4-2"),             UnicodeString("78x29-5-1") } },
        { "ko@calendar=dangi",        DateFormat::kLong,  { CharsToUnicodeString("\\uC784\\uC9C4\\uB144 3bis\\uC6D4 2\\uC77C"),
                                                            CharsToUnicodeString("\\uC784\\uC9C4\\uB144 4\\uC6D4 2\\uC77C"),
                                                            CharsToUnicodeString("\\uC784\\uC9C4\\uB144 5\\uC6D4 1\\uC77C") } },
        { "ko@calendar=dangi",        DateFormat::kShort, { CharsToUnicodeString("29. 3bis. 2."),
                                                            CharsToUnicodeString("29. 4. 2."),
                                                            CharsToUnicodeString("29. 5. 1.") } },
        // terminator
        { NULL,                       0,                  { UnicodeString(""), UnicodeString(""), UnicodeString("") } }
    };
    
    //.                               style: -1        -2            -3       -4
    const UnicodeString customPatterns[] = { "y-Ml-d", "G'x'y-Ml-d", "U-M-d", "U MMM d" }; // like old root pattern, using 'l'

    UErrorCode status = U_ZERO_ERROR;
    Locale rootChineseCalLocale = Locale::createFromName("root@calendar=chinese");
    Calendar * rootChineseCalendar = Calendar::createInstance(rootChineseCalLocale, status);
    if (U_SUCCESS(status)) {
        const MonthPatternItem * itemPtr;
        for (itemPtr = items; itemPtr->locale != NULL; itemPtr++ ) {
            Locale locale = Locale::createFromName(itemPtr->locale);
            DateFormat * dmft = (itemPtr->style >= 0)?
                    DateFormat::createDateInstance((DateFormat::EStyle)itemPtr->style, locale):
                    new SimpleDateFormat(customPatterns[-itemPtr->style - 1], locale, status);
            if ( dmft != NULL ) {
                if (U_SUCCESS(status)) {
                    const ChineseCalTestDate * datePtr = dates;
                    int32_t idate;
                    for (idate = 0; idate < NUM_TEST_DATES; idate++, datePtr++) {
                        rootChineseCalendar->clear();
                        rootChineseCalendar->set(UCAL_ERA, datePtr->era);
                        rootChineseCalendar->set(datePtr->year, datePtr->month-1, datePtr->day);
                        rootChineseCalendar->set(UCAL_IS_LEAP_MONTH, datePtr->isLeapMonth);
                        UnicodeString result;
                        FieldPosition fpos(0);
                        dmft->format(*rootChineseCalendar, result, fpos);
                        if ( result.compare(itemPtr->dateString[idate]) != 0 ) {
                            errln( UnicodeString("FAIL: Chinese calendar format for locale ") + UnicodeString(itemPtr->locale) + ", style " + itemPtr->style +
                                    ", expected \"" + itemPtr->dateString[idate] + "\", got \"" + result + "\"");
                        } else {
                            // formatted OK, try parse
                            ParsePosition ppos(0);
                            // ensure we are really parsing the fields we should be
                            rootChineseCalendar->set(UCAL_YEAR, 1);
                            rootChineseCalendar->set(UCAL_MONTH, 0);
                            rootChineseCalendar->set(UCAL_IS_LEAP_MONTH, 0);
                            rootChineseCalendar->set(UCAL_DATE, 1);
                            //
                            dmft->parse(result, *rootChineseCalendar, ppos);
                            int32_t year = rootChineseCalendar->get(UCAL_YEAR, status);
                            int32_t month = rootChineseCalendar->get(UCAL_MONTH, status) + 1;
                            int32_t isLeapMonth = rootChineseCalendar->get(UCAL_IS_LEAP_MONTH, status);
                            int32_t day = rootChineseCalendar->get(UCAL_DATE, status);
                            if ( ppos.getIndex() < result.length() || year != datePtr->year || month != datePtr->month || isLeapMonth != datePtr->isLeapMonth || day != datePtr->day ) {
                                errln( UnicodeString("FAIL: Chinese calendar parse for locale ") + UnicodeString(itemPtr->locale) + ", style " + itemPtr->style +
                                    ", string \"" + result + "\", expected " + datePtr->year +"-"+datePtr->month+"("+datePtr->isLeapMonth+")-"+datePtr->day + ", got pos " +
                                    ppos.getIndex() + " " + year +"-"+month+"("+isLeapMonth+")-"+day);
                            }
                        }
                    }
                } else {
                    dataerrln("Error creating SimpleDateFormat for Chinese calendar- %s", u_errorName(status));
                }
                delete dmft;
            } else {
                dataerrln("FAIL: Unable to create DateFormat for Chinese calendar- %s", u_errorName(status));
            }
        }
        delete rootChineseCalendar;
    } else {
        errln(UnicodeString("FAIL: Unable to create Calendar for root@calendar=chinese"));
    }
}

typedef struct {
    const char * locale;
    UnicodeString pattern;
    UDisplayContext capitalizationContext;
    UnicodeString expectedFormat;
} TestContextItem;

void DateFormatTest::TestContext()
{
    const UDate july022008 = 1215000001979.0;
    const TestContextItem items[] = {
        //locale              pattern    capitalizationContext                              expected formatted date
        { "fr", UnicodeString("MMMM y"), UDISPCTX_CAPITALIZATION_NONE,                      UnicodeString("juillet 2008") },
#if !UCONFIG_NO_BREAK_ITERATION
        { "fr", UnicodeString("MMMM y"), UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UnicodeString("juillet 2008") },
        { "fr", UnicodeString("MMMM y"), UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UnicodeString("Juillet 2008") },
        { "fr", UnicodeString("MMMM y"), UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UnicodeString("juillet 2008") },
        { "fr", UnicodeString("MMMM y"), UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UnicodeString("Juillet 2008") },
#endif
        { "cs", UnicodeString("LLLL y"), UDISPCTX_CAPITALIZATION_NONE,                      CharsToUnicodeString("\\u010Dervenec 2008") },
#if !UCONFIG_NO_BREAK_ITERATION
        { "cs", UnicodeString("LLLL y"), UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    CharsToUnicodeString("\\u010Dervenec 2008") },
        { "cs", UnicodeString("LLLL y"), UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, CharsToUnicodeString("\\u010Cervenec 2008") },
        { "cs", UnicodeString("LLLL y"), UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       CharsToUnicodeString("\\u010Cervenec 2008") },
        { "cs", UnicodeString("LLLL y"), UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            CharsToUnicodeString("\\u010Dervenec 2008") },
#endif
        // terminator
        { NULL, UnicodeString(""),       (UDisplayContext)0, UnicodeString("") }
    };
    UErrorCode status = U_ZERO_ERROR;
    Calendar* cal = Calendar::createInstance(status);
    if (U_FAILURE(status)) {
        dataerrln(UnicodeString("FAIL: Unable to create Calendar for default timezone and locale."));
    } else {
        cal->setTime(july022008, status);
        const TestContextItem * itemPtr;
        for (itemPtr = items; itemPtr->locale != NULL; itemPtr++ ) {
           Locale locale = Locale::createFromName(itemPtr->locale);
           status = U_ZERO_ERROR;
           SimpleDateFormat * sdmft = new SimpleDateFormat(itemPtr->pattern, locale, status);
           if (U_FAILURE(status)) {
                dataerrln(UnicodeString("FAIL: Unable to create SimpleDateFormat for specified pattern with locale ") + UnicodeString(itemPtr->locale));
           } else {
               sdmft->setContext(itemPtr->capitalizationContext, status);
               UnicodeString result;
               FieldPosition pos(0);
               sdmft->format(*cal, result, pos);
               if (result.compare(itemPtr->expectedFormat) != 0) {
                   errln(UnicodeString("FAIL: format for locale ") + UnicodeString(itemPtr->locale) +
                           ", status " + (int)status +
                           ", capitalizationContext " + (int)itemPtr->capitalizationContext +
                           ", expected " + itemPtr->expectedFormat + ", got " + result);
               }
           }
           if (sdmft) {
               delete sdmft;
           }
        }
    }
    if (cal) {
        delete cal;
    }
}

// test item for a particular locale + calendar and date format
typedef struct {
    int32_t year;
    int32_t month;
    int32_t day;
    int32_t hour;
    int32_t minute;
    UnicodeString formattedDate;
} CalAndFmtTestItem;

// test item giving locale + calendar, date format, and CalAndFmtTestItems
typedef struct {
    const char * locale; // with calendar
    DateFormat::EStyle style;
    const CalAndFmtTestItem *caftItems;
} TestNonGregoItem;

void DateFormatTest::TestNonGregoFmtParse()
{
    // test items for he@calendar=hebrew, long date format
    const CalAndFmtTestItem cafti_he_hebrew_long[] = {
        { 4999, 12, 29, 12, 0, CharsToUnicodeString("\\u05DB\\u05F4\\u05D8 \\u05D1\\u05D0\\u05DC\\u05D5\\u05DC \\u05D3\\u05F3\\u05EA\\u05EA\\u05E7\\u05E6\\u05F4\\u05D8") },
        { 5100,  0,  1, 12, 0, CharsToUnicodeString("\\u05D0\\u05F3 \\u05D1\\u05EA\\u05E9\\u05E8\\u05D9 \\u05E7\\u05F3") },
        { 5774,  5,  1, 12, 0, CharsToUnicodeString("\\u05D0\\u05F3 \\u05D1\\u05D0\\u05D3\\u05E8 \\u05D0\\u05F3 \\u05EA\\u05E9\\u05E2\\u05F4\\u05D3") },
        { 5999, 12, 29, 12, 0, CharsToUnicodeString("\\u05DB\\u05F4\\u05D8 \\u05D1\\u05D0\\u05DC\\u05D5\\u05DC \\u05EA\\u05EA\\u05E7\\u05E6\\u05F4\\u05D8") },
        { 6100,  0,  1, 12, 0, CharsToUnicodeString("\\u05D0\\u05F3 \\u05D1\\u05EA\\u05E9\\u05E8\\u05D9 \\u05D5\\u05F3\\u05E7\\u05F3") },
        {    0,  0,  0,  0, 0, UnicodeString("") } // terminator
    };
    // overal test items
    const TestNonGregoItem items[] = {
        { "he@calendar=hebrew", DateFormat::kLong, cafti_he_hebrew_long },
        { NULL, DateFormat::kNone, NULL } // terminator
    };
    const TestNonGregoItem * itemPtr;
    for (itemPtr = items; itemPtr->locale != NULL; itemPtr++) {
        Locale locale = Locale::createFromName(itemPtr->locale);
        DateFormat * dfmt = DateFormat::createDateInstance(itemPtr->style, locale);
        if (dfmt == NULL) {
            dataerrln("DateFormat::createDateInstance fails for locale %s", itemPtr->locale);
        } else {
            Calendar * cal = (dfmt->getCalendar())->clone();
            if (cal == NULL) {
                dataerrln("(DateFormat::getCalendar)->clone() fails for locale %s", itemPtr->locale);
            } else {
                const CalAndFmtTestItem * caftItemPtr;
                for (caftItemPtr = itemPtr->caftItems; caftItemPtr->year != 0; caftItemPtr++) {
                    cal->clear();
                    cal->set(UCAL_YEAR,   caftItemPtr->year);
                    cal->set(UCAL_MONTH,  caftItemPtr->month);
                    cal->set(UCAL_DATE,   caftItemPtr->day);
                    cal->set(UCAL_HOUR_OF_DAY, caftItemPtr->hour);
                    cal->set(UCAL_MINUTE, caftItemPtr->minute);
                    UnicodeString result;
                    FieldPosition fpos(0);
                    dfmt->format(*cal, result, fpos);
                    if ( result.compare(caftItemPtr->formattedDate) != 0 ) {
                        errln( UnicodeString("FAIL: date format for locale ") + UnicodeString(itemPtr->locale) + ", style " + itemPtr->style +
                                ", expected \"" + caftItemPtr->formattedDate + "\", got \"" + result + "\"");
                    } else {
                        // formatted OK, try parse
                        ParsePosition ppos(0);
                        dfmt->parse(result, *cal, ppos);
                        UErrorCode status = U_ZERO_ERROR;
                        int32_t year = cal->get(UCAL_YEAR, status);
                        int32_t month = cal->get(UCAL_MONTH, status);
                        int32_t day = cal->get(UCAL_DATE, status);
                        if ( U_FAILURE(status) || ppos.getIndex() < result.length() || year != caftItemPtr->year || month != caftItemPtr->month || day != caftItemPtr->day ) {
                            errln( UnicodeString("FAIL: date parse for locale ") + UnicodeString(itemPtr->locale) + ", style " + itemPtr->style +
                                ", string \"" + result + "\", expected " + caftItemPtr->year +"-"+caftItemPtr->month+"-"+caftItemPtr->day + ", got pos " +
                                ppos.getIndex() + " " + year +"-"+month+"-"+day + " status " + UnicodeString(u_errorName(status)) );
                        }
                    }
                }
                delete cal;
            }
            delete dfmt;
        }
    }
}

static const UDate TEST_DATE = 1326585600000.;  // 2012-jan-15

void DateFormatTest::TestDotAndAtLeniency() {
    // Test for date/time parsing regression with CLDR 22.1/ICU 50 pattern strings.
    // For details see http://bugs.icu-project.org/trac/ticket/9789
    static const char *locales[] = { "en", "fr" };
    for (int32_t i = 0; i < LENGTHOF(locales); ++i) {
        Locale locale(locales[i]);

        for (DateFormat::EStyle dateStyle = DateFormat::FULL; dateStyle <= DateFormat::SHORT;
                  dateStyle = static_cast<DateFormat::EStyle>(dateStyle + 1)) {
            LocalPointer<DateFormat> dateFormat(DateFormat::createDateInstance(dateStyle, locale));

            for (DateFormat::EStyle timeStyle = DateFormat::FULL; timeStyle <= DateFormat::SHORT;
                      timeStyle = static_cast<DateFormat::EStyle>(timeStyle + 1)) {
                LocalPointer<DateFormat> format(DateFormat::createDateTimeInstance(dateStyle, timeStyle, locale));
                LocalPointer<DateFormat> timeFormat(DateFormat::createTimeInstance(timeStyle, locale));
                UnicodeString formattedString;
                if (format.isNull()) {
                    dataerrln("Unable to create DateFormat");
                    continue;
                }
                format->format(TEST_DATE, formattedString);

                if (!showParse(*format, formattedString)) {
                    errln(UnicodeString("    with date-time: dateStyle=") + dateStyle + " timeStyle=" + timeStyle);
                }

                UnicodeString ds, ts;
                formattedString = dateFormat->format(TEST_DATE, ds) + "  " + timeFormat->format(TEST_DATE, ts);
                if (!showParse(*format, formattedString)) {
                    errln(UnicodeString("    with date sp sp time: dateStyle=") + dateStyle + " timeStyle=" + timeStyle);
                }
                if (formattedString.indexOf("n ") >= 0) { // will add "." after the end of text ending in 'n', like Jan.
                    UnicodeString plusDot(formattedString);
                    plusDot.findAndReplace("n ", "n. ").append(".");
                    if (!showParse(*format, plusDot)) {
                        errln(UnicodeString("    with date plus-dot time: dateStyle=") + dateStyle + " timeStyle=" + timeStyle);
                    }
                }
                if (formattedString.indexOf(". ") >= 0) { // will subtract "." at the end of strings.
                    UnicodeString minusDot(formattedString);
                    minusDot.findAndReplace(". ", " ");
                    if (!showParse(*format, minusDot)) {
                        errln(UnicodeString("    with date minus-dot time: dateStyle=") + dateStyle + " timeStyle=" + timeStyle);
                    }
                }
            }
        }
    }
}

UBool DateFormatTest::showParse(DateFormat &format, const UnicodeString &formattedString) {
    ParsePosition parsePosition;
    UDate parsed = format.parse(formattedString, parsePosition);
    UBool ok = TEST_DATE == parsed && parsePosition.getIndex() == formattedString.length();
    UnicodeString pattern;
    static_cast<SimpleDateFormat &>(format).toPattern(pattern);
    if (ok) {
        logln(pattern + "  parsed: " + formattedString);
    } else {
        errln(pattern + "  fails to parse: " + formattedString);
    }
    return ok;
}


typedef struct {
    const char * locale;
    UBool leniency;
    UnicodeString parseString;
    UnicodeString pattern;
    UnicodeString expectedResult;       // null indicates expected error
} TestDateFormatLeniencyItem;

void DateFormatTest::TestDateFormatLeniency() {
    // For details see http://bugs.icu-project.org/trac/ticket/10261
    
    const UDate july022008 = 1215000001979.0;
    const TestDateFormatLeniencyItem items[] = {
        //locale    leniency    parse String                    pattern                             expected result
        { "en",     true,       UnicodeString("2008-07 02"),    UnicodeString("yyyy-LLLL dd"),      UnicodeString("2008-July 02") },
        { "en",     false,      UnicodeString("2008-07 02"),    UnicodeString("yyyy-LLLL dd"),      UnicodeString("") },
        { "en",     true,       UnicodeString("2008-Jan 02"),   UnicodeString("yyyy-LLL. dd"),      UnicodeString("2008-Jan 02") },
        { "en",     false,      UnicodeString("2008-Jan 02"),   UnicodeString("yyyy-LLL. dd"),      UnicodeString("") },
        { "en",     true,       UnicodeString("2008-Jan--02"),  UnicodeString("yyyy-MMM' -- 'dd"),  UnicodeString("2008-Jan 02") },
        { "en",     false,      UnicodeString("2008-Jan--02"),  UnicodeString("yyyy-MMM' -- 'dd"),  UnicodeString("") },
        // terminator
        { NULL,     true,       UnicodeString(""),              UnicodeString(""),                  UnicodeString("") }                
    };
    UErrorCode status = U_ZERO_ERROR;
    Calendar* cal = Calendar::createInstance(status);
    if (U_FAILURE(status)) {
        dataerrln(UnicodeString("FAIL: Unable to create Calendar for default timezone and locale."));
    } else {
        cal->setTime(july022008, status);
        const TestDateFormatLeniencyItem * itemPtr;
        for (itemPtr = items; itemPtr->locale != NULL; itemPtr++ ) {
                                            
           Locale locale = Locale::createFromName(itemPtr->locale);
           status = U_ZERO_ERROR;
           ParsePosition pos(0);
           SimpleDateFormat * sdmft = new SimpleDateFormat(itemPtr->pattern, locale, status);
           if (U_FAILURE(status)) {
               dataerrln("Unable to create SimpleDateFormat - %s", u_errorName(status));
               continue;
           }
           sdmft->setLenient(itemPtr->leniency);
           sdmft->setBooleanAttribute(UDAT_PARSE_ALLOW_WHITESPACE, itemPtr->leniency, status).setBooleanAttribute(UDAT_PARSE_ALLOW_NUMERIC, itemPtr->leniency, status);
           /*UDate d = */sdmft->parse(itemPtr->parseString, pos);

           delete sdmft;
           if(pos.getErrorIndex() > -1)
               if(itemPtr->expectedResult.length() != 0) {
                 errln("error: unexpected error - " + itemPtr->parseString + " - error index " + pos.getErrorIndex() + " - leniency " + itemPtr->leniency);
                 continue;
               } else {
                 continue;
               }
        }
    }
    delete cal;

}

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
