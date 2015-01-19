/*
*******************************************************************************
* Copyright (C) 2007-2013, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "tzfmttst.h"

#include "simplethread.h"
#include "unicode/timezone.h"
#include "unicode/simpletz.h"
#include "unicode/calendar.h"
#include "unicode/strenum.h"
#include "unicode/smpdtfmt.h"
#include "unicode/uchar.h"
#include "unicode/basictz.h"
#include "unicode/tzfmt.h"
#include "unicode/localpointer.h"
#include "cstring.h"
#include "zonemeta.h"

static const char* PATTERNS[] = {
    "z",
    "zzzz",
    "Z",    // equivalent to "xxxx"
    "ZZZZ", // equivalent to "OOOO"
    "v",
    "vvvv",
    "O",
    "OOOO",
    "X",
    "XX",
    "XXX",
    "XXXX",
    "XXXXX",
    "x",
    "xx",
    "xxx",
    "xxxx",
    "xxxxx",
    "V",
    "VV",
    "VVV",
    "VVVV"
};
static const int NUM_PATTERNS = sizeof(PATTERNS)/sizeof(const char*);

static const UChar ETC_UNKNOWN[] = {0x45, 0x74, 0x63, 0x2F, 0x55, 0x6E, 0x6B, 0x6E, 0x6F, 0x77, 0x6E, 0};

static const UChar ETC_SLASH[] = { 0x45, 0x74, 0x63, 0x2F, 0 }; // "Etc/"
static const UChar SYSTEMV_SLASH[] = { 0x53, 0x79, 0x73, 0x74, 0x65, 0x6D, 0x56, 0x2F, 0 }; // "SystemV/
static const UChar RIYADH8[] = { 0x52, 0x69, 0x79, 0x61, 0x64, 0x68, 0x38, 0 }; // "Riyadh8"

static UBool contains(const char** list, const char* str) {
    for (int32_t i = 0; list[i]; i++) {
        if (uprv_strcmp(list[i], str) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

void
TimeZoneFormatTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) {
        logln("TestSuite TimeZoneFormatTest");
    }
    switch (index) {
        TESTCASE(0, TestTimeZoneRoundTrip);
        TESTCASE(1, TestTimeRoundTrip);
        TESTCASE(2, TestParse);
        TESTCASE(3, TestISOFormat);
        TESTCASE(4, TestFormat);
        default: name = ""; break;
    }
}

void
TimeZoneFormatTest::TestTimeZoneRoundTrip(void) {
    UErrorCode status = U_ZERO_ERROR;

    SimpleTimeZone unknownZone(-31415, ETC_UNKNOWN);
    int32_t badDstOffset = -1234;
    int32_t badZoneOffset = -2345;

    int32_t testDateData[][3] = {
        {2007, 1, 15},
        {2007, 6, 15},
        {1990, 1, 15},
        {1990, 6, 15},
        {1960, 1, 15},
        {1960, 6, 15},
    };

    Calendar *cal = Calendar::createInstance(TimeZone::createTimeZone((UnicodeString)"UTC"), status);
    if (U_FAILURE(status)) {
        dataerrln("Calendar::createInstance failed: %s", u_errorName(status));
        return;
    }

    // Set up rule equivalency test range
    UDate low, high;
    cal->set(1900, UCAL_JANUARY, 1);
    low = cal->getTime(status);
    cal->set(2040, UCAL_JANUARY, 1);
    high = cal->getTime(status);
    if (U_FAILURE(status)) {
        errln("getTime failed");
        return;
    }

    // Set up test dates
    UDate DATES[(sizeof(testDateData)/sizeof(int32_t))/3];
    const int32_t nDates = (sizeof(testDateData)/sizeof(int32_t))/3;
    cal->clear();
    for (int32_t i = 0; i < nDates; i++) {
        cal->set(testDateData[i][0], testDateData[i][1], testDateData[i][2]);
        DATES[i] = cal->getTime(status);
        if (U_FAILURE(status)) {
            errln("getTime failed");
            return;
        }
    }

    // Set up test locales
    const Locale testLocales[] = {
        Locale("en"),
        Locale("en_CA"),
        Locale("fr"),
        Locale("zh_Hant")
    };

    const Locale *LOCALES;
    int32_t nLocales;

    if (quick) {
        LOCALES = testLocales;
        nLocales = sizeof(testLocales)/sizeof(Locale);
    } else {
        LOCALES = Locale::getAvailableLocales(nLocales);
    }

    StringEnumeration *tzids = TimeZone::createEnumeration();
    int32_t inRaw, inDst;
    int32_t outRaw, outDst;

    // Run the roundtrip test
    for (int32_t locidx = 0; locidx < nLocales; locidx++) {
        UnicodeString localGMTString;
        SimpleDateFormat gmtFmt(UnicodeString("ZZZZ"), LOCALES[locidx], status);
        if (U_FAILURE(status)) {
            dataerrln("Error creating SimpleDateFormat - %s", u_errorName(status));
            continue;
        }
        gmtFmt.setTimeZone(*TimeZone::getGMT());
        gmtFmt.format(0.0, localGMTString);

        for (int32_t patidx = 0; patidx < NUM_PATTERNS; patidx++) {

            SimpleDateFormat *sdf = new SimpleDateFormat((UnicodeString)PATTERNS[patidx], LOCALES[locidx], status);
            if (U_FAILURE(status)) {
                dataerrln((UnicodeString)"new SimpleDateFormat failed for pattern " +
                    PATTERNS[patidx] + " for locale " + LOCALES[locidx].getName() + " - " + u_errorName(status));
                status = U_ZERO_ERROR;
                continue;
            }

            tzids->reset(status);
            const UnicodeString *tzid;
            while ((tzid = tzids->snext(status))) {
                TimeZone *tz = TimeZone::createTimeZone(*tzid);

                for (int32_t datidx = 0; datidx < nDates; datidx++) {
                    UnicodeString tzstr;
                    FieldPosition fpos(0);
                    // Format
                    sdf->setTimeZone(*tz);
                    sdf->format(DATES[datidx], tzstr, fpos);

                    // Before parse, set unknown zone to SimpleDateFormat instance
                    // just for making sure that it does not depends on the time zone
                    // originally set.
                    sdf->setTimeZone(unknownZone);

                    // Parse
                    ParsePosition pos(0);
                    Calendar *outcal = Calendar::createInstance(unknownZone, status);
                    if (U_FAILURE(status)) {
                        errln("Failed to create an instance of calendar for receiving parse result.");
                        status = U_ZERO_ERROR;
                        continue;
                    }
                    outcal->set(UCAL_DST_OFFSET, badDstOffset);
                    outcal->set(UCAL_ZONE_OFFSET, badZoneOffset);

                    sdf->parse(tzstr, *outcal, pos);

                    // Check the result
                    const TimeZone &outtz = outcal->getTimeZone();
                    UnicodeString outtzid;
                    outtz.getID(outtzid);

                    tz->getOffset(DATES[datidx], false, inRaw, inDst, status);
                    if (U_FAILURE(status)) {
                        errln((UnicodeString)"Failed to get offsets from time zone" + *tzid);
                        status = U_ZERO_ERROR;
                    }
                    outtz.getOffset(DATES[datidx], false, outRaw, outDst, status);
                    if (U_FAILURE(status)) {
                        errln((UnicodeString)"Failed to get offsets from time zone" + outtzid);
                        status = U_ZERO_ERROR;
                    }

                    if (uprv_strcmp(PATTERNS[patidx], "V") == 0) {
                        // Short zone ID - should support roundtrip for canonical CLDR IDs
                        UnicodeString canonicalID;
                        TimeZone::getCanonicalID(*tzid, canonicalID, status);
                        if (U_FAILURE(status)) {
                            // Uknown ID - we should not get here
                            errln((UnicodeString)"Unknown ID " + *tzid);
                            status = U_ZERO_ERROR;
                        } else if (outtzid != canonicalID) {
                            if (outtzid.compare(ETC_UNKNOWN, -1) == 0) {
                                // Note that some zones like Asia/Riyadh87 does not have
                                // short zone ID and "unk" is used as fallback
                                logln((UnicodeString)"Canonical round trip failed (probably as expected); tz=" + *tzid
                                        + ", locale=" + LOCALES[locidx].getName() + ", pattern=" + PATTERNS[patidx]
                                        + ", time=" + DATES[datidx] + ", str=" + tzstr
                                        + ", outtz=" + outtzid);
                            } else {
                                errln((UnicodeString)"Canonical round trip failed; tz=" + *tzid
                                    + ", locale=" + LOCALES[locidx].getName() + ", pattern=" + PATTERNS[patidx]
                                    + ", time=" + DATES[datidx] + ", str=" + tzstr
                                    + ", outtz=" + outtzid);
                            }
                        }
                    } else if (uprv_strcmp(PATTERNS[patidx], "VV") == 0) {
                        // Zone ID - full roundtrip support
                        if (outtzid != *tzid) {
                            errln((UnicodeString)"Zone ID round trip failued; tz="  + *tzid
                                + ", locale=" + LOCALES[locidx].getName() + ", pattern=" + PATTERNS[patidx]
                                + ", time=" + DATES[datidx] + ", str=" + tzstr
                                + ", outtz=" + outtzid);
                        }
                    } else if (uprv_strcmp(PATTERNS[patidx], "VVV") == 0 || uprv_strcmp(PATTERNS[patidx], "VVVV") == 0) {
                        // Location: time zone rule must be preserved except
                        // zones not actually associated with a specific location.
                        // Time zones in this category do not have "/" in its ID.
                        UnicodeString canonical;
                        TimeZone::getCanonicalID(*tzid, canonical, status);
                        if (U_FAILURE(status)) {
                            // Uknown ID - we should not get here
                            errln((UnicodeString)"Unknown ID " + *tzid);
                            status = U_ZERO_ERROR;
                        } else if (outtzid != canonical) {
                            // Canonical ID did not match - check the rules
                            if (!((BasicTimeZone*)&outtz)->hasEquivalentTransitions((BasicTimeZone&)*tz, low, high, TRUE, status)) {
                                if (canonical.indexOf((UChar)0x27 /*'/'*/) == -1) {
                                    // Exceptional cases, such as CET, EET, MET and WET
                                    logln((UnicodeString)"Canonical round trip failed (as expected); tz=" + *tzid
                                            + ", locale=" + LOCALES[locidx].getName() + ", pattern=" + PATTERNS[patidx]
                                            + ", time=" + DATES[datidx] + ", str=" + tzstr
                                            + ", outtz=" + outtzid);
                                } else {
                                    errln((UnicodeString)"Canonical round trip failed; tz=" + *tzid
                                        + ", locale=" + LOCALES[locidx].getName() + ", pattern=" + PATTERNS[patidx]
                                        + ", time=" + DATES[datidx] + ", str=" + tzstr
                                        + ", outtz=" + outtzid);
                                }
                                if (U_FAILURE(status)) {
                                    errln("hasEquivalentTransitions failed");
                                    status = U_ZERO_ERROR;
                                }
                            }
                        }

                    } else {
                        UBool isOffsetFormat = (*PATTERNS[patidx] == 'Z'
                                                || *PATTERNS[patidx] == 'O'
                                                || *PATTERNS[patidx] == 'X'
                                                || *PATTERNS[patidx] == 'x');
                        UBool minutesOffset = FALSE;
                        if (*PATTERNS[patidx] == 'X' || *PATTERNS[patidx] == 'x') {
                            minutesOffset = (uprv_strlen(PATTERNS[patidx]) <= 3);
                        }

                        if (!isOffsetFormat) {
                            // Check if localized GMT format is used as a fallback of name styles
                            int32_t numDigits = 0;
                            for (int n = 0; n < tzstr.length(); n++) {
                                if (u_isdigit(tzstr.charAt(n))) {
                                    numDigits++;
                                }
                            }
                            isOffsetFormat = (numDigits > 0);
                        }
                        if (isOffsetFormat || tzstr == localGMTString) {
                            // Localized GMT or ISO: total offset (raw + dst) must be preserved.
                            int32_t inOffset = inRaw + inDst;
                            int32_t outOffset = outRaw + outDst;
                            int32_t diff = outOffset - inOffset;
                            if (minutesOffset) {
                                diff = (diff / 60000) * 60000;
                            }
                            if (diff != 0) {
                                errln((UnicodeString)"Offset round trip failed; tz=" + *tzid
                                    + ", locale=" + LOCALES[locidx].getName() + ", pattern=" + PATTERNS[patidx]
                                    + ", time=" + DATES[datidx] + ", str=" + tzstr
                                    + ", inOffset=" + inOffset + ", outOffset=" + outOffset);
                            }
                        } else {
                            // Specific or generic: raw offset must be preserved.
                            if (inRaw != outRaw) {
                                errln((UnicodeString)"Raw offset round trip failed; tz=" + *tzid
                                    + ", locale=" + LOCALES[locidx].getName() + ", pattern=" + PATTERNS[patidx]
                                    + ", time=" + DATES[datidx] + ", str=" + tzstr
                                    + ", inRawOffset=" + inRaw + ", outRawOffset=" + outRaw);
                            }
                        }
                    }
                    delete outcal;
                }
                delete tz;
            }
            delete sdf;
        }
    }
    delete cal;
    delete tzids;
}

struct LocaleData {
    int32_t index;
    int32_t testCounts;
    UDate *times;
    const Locale* locales; // Static
    int32_t nLocales; // Static
    UBool quick; // Static
    UDate START_TIME; // Static
    UDate END_TIME; // Static
    int32_t numDone;
};

class TestTimeRoundTripThread: public SimpleThread {
public:
    TestTimeRoundTripThread(IntlTest& tlog, LocaleData &ld, int32_t i)
        : log(tlog), data(ld), index(i) {}
    virtual void run() {
        UErrorCode status = U_ZERO_ERROR;
        UBool REALLY_VERBOSE = FALSE;

        // These patterns are ambiguous at DST->STD local time overlap
        const char* AMBIGUOUS_DST_DECESSION[] = { "v", "vvvv", "V", "VV", "VVV", "VVVV", 0 };

        // These patterns are ambiguous at STD->STD/DST->DST local time overlap
        const char* AMBIGUOUS_NEGATIVE_SHIFT[] = { "z", "zzzz", "v", "vvvv", "V", "VV", "VVV", "VVVV", 0 };

        // These patterns only support integer minutes offset
        const char* MINUTES_OFFSET[] = { "X", "XX", "XXX", "x", "xx", "xxx", 0 };

        // Workaround for #6338
        //UnicodeString BASEPATTERN("yyyy-MM-dd'T'HH:mm:ss.SSS");
        UnicodeString BASEPATTERN("yyyy.MM.dd HH:mm:ss.SSS");

        // timer for performance analysis
        UDate timer;
        UDate testTimes[4];
        UBool expectedRoundTrip[4];
        int32_t testLen = 0;

        StringEnumeration *tzids = TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, NULL, NULL, status);
        if (U_FAILURE(status)) {
            if (status == U_MISSING_RESOURCE_ERROR) {
                /* This error is generally caused by data not being present. However, an infinite loop will occur
                 * because the thread thinks that the test data is never done so we should treat the data as done.
                 */
                log.dataerrln("TimeZone::createTimeZoneIDEnumeration failed - %s", u_errorName(status));
                data.numDone = data.nLocales;
            } else {
                log.errln("TimeZone::createTimeZoneIDEnumeration failed: %s", u_errorName(status));
            }
            return;
        }

        int32_t locidx = -1;
        UDate times[NUM_PATTERNS];
        for (int32_t i = 0; i < NUM_PATTERNS; i++) {
            times[i] = 0;
        }

        int32_t testCounts = 0;

        while (true) {
            umtx_lock(NULL); // Lock to increment the index
            for (int32_t i = 0; i < NUM_PATTERNS; i++) {
                data.times[i] += times[i];
                data.testCounts += testCounts;
            }
            if (data.index < data.nLocales) {
                locidx = data.index;
                data.index++;
            } else {
                locidx = -1;
            }
            umtx_unlock(NULL); // Unlock for other threads to use

            if (locidx == -1) {
                log.logln((UnicodeString) "Thread " + index + " is done.");
                break;
            }

            log.logln((UnicodeString) "\nThread " + index + ": Locale: " + UnicodeString(data.locales[locidx].getName()));

            for (int32_t patidx = 0; patidx < NUM_PATTERNS; patidx++) {
                log.logln((UnicodeString) "    Pattern: " + PATTERNS[patidx]);
                times[patidx] = 0;

                UnicodeString pattern(BASEPATTERN);
                pattern.append(" ").append(PATTERNS[patidx]);

                SimpleDateFormat *sdf = new SimpleDateFormat(pattern, data.locales[locidx], status);
                if (U_FAILURE(status)) {
                    log.errcheckln(status, (UnicodeString) "new SimpleDateFormat failed for pattern " + 
                        pattern + " for locale " + data.locales[locidx].getName() + " - " + u_errorName(status));
                    status = U_ZERO_ERROR;
                    continue;
                }

                UBool minutesOffset = contains(MINUTES_OFFSET, PATTERNS[patidx]);

                tzids->reset(status);
                const UnicodeString *tzid;

                timer = Calendar::getNow();

                while ((tzid = tzids->snext(status))) {
                    if (uprv_strcmp(PATTERNS[patidx], "V") == 0) {
                        // Some zones do not have short ID assigned, such as Asia/Riyadh87.
                        // The time roundtrip will fail for such zones with pattern "V" (short zone ID).
                        // This is expected behavior.
                        const UChar* shortZoneID = ZoneMeta::getShortID(*tzid);
                        if (shortZoneID == NULL) {
                            continue;
                        }
                    } else if (uprv_strcmp(PATTERNS[patidx], "VVV") == 0) {
                        // Some zones are not associated with any region, such as Etc/GMT+8.
                        // The time roundtrip will fail for such zone with pattern "VVV" (exemplar location).
                        // This is expected behavior.
                        if (tzid->indexOf((UChar)0x2F) < 0 || tzid->indexOf(ETC_SLASH, -1, 0) >= 0
                            || tzid->indexOf(SYSTEMV_SLASH, -1, 0) >= 0 || tzid->indexOf(RIYADH8, -1, 0) >= 0) {
                            continue;
                        }
                    }

                    BasicTimeZone *tz = (BasicTimeZone*) TimeZone::createTimeZone(*tzid);
                    sdf->setTimeZone(*tz);

                    UDate t = data.START_TIME;
                    TimeZoneTransition tzt;
                    UBool tztAvail = FALSE;
                    UBool middle = TRUE;

                    while (t < data.END_TIME) {
                        if (!tztAvail) {
                            testTimes[0] = t;
                            expectedRoundTrip[0] = TRUE;
                            testLen = 1;
                        } else {
                            int32_t fromOffset = tzt.getFrom()->getRawOffset() + tzt.getFrom()->getDSTSavings();
                            int32_t toOffset = tzt.getTo()->getRawOffset() + tzt.getTo()->getDSTSavings();
                            int32_t delta = toOffset - fromOffset;
                            if (delta < 0) {
                                UBool isDstDecession = tzt.getFrom()->getDSTSavings() > 0 && tzt.getTo()->getDSTSavings() == 0;
                                testTimes[0] = t + delta - 1;
                                expectedRoundTrip[0] = TRUE;
                                testTimes[1] = t + delta;
                                expectedRoundTrip[1] = isDstDecession ?
                                    !contains(AMBIGUOUS_DST_DECESSION, PATTERNS[patidx]) :
                                    !contains(AMBIGUOUS_NEGATIVE_SHIFT, PATTERNS[patidx]);
                                testTimes[2] = t - 1;
                                expectedRoundTrip[2] = isDstDecession ?
                                    !contains(AMBIGUOUS_DST_DECESSION, PATTERNS[patidx]) :
                                    !contains(AMBIGUOUS_NEGATIVE_SHIFT, PATTERNS[patidx]);
                                testTimes[3] = t;
                                expectedRoundTrip[3] = TRUE;
                                testLen = 4;
                            } else {
                                testTimes[0] = t - 1;
                                expectedRoundTrip[0] = TRUE;
                                testTimes[1] = t;
                                expectedRoundTrip[1] = TRUE;
                                testLen = 2;
                            }
                        }
                        for (int32_t testidx = 0; testidx < testLen; testidx++) {
                            if (data.quick) {
                                // reduce regular test time
                                if (!expectedRoundTrip[testidx]) {
                                    continue;
                                }
                            }

                            testCounts++;

                            UnicodeString text;
                            FieldPosition fpos(0);
                            sdf->format(testTimes[testidx], text, fpos);

                            UDate parsedDate = sdf->parse(text, status);
                            if (U_FAILURE(status)) {
                                log.errln((UnicodeString) "Parse failure for text=" + text + ", tzid=" + *tzid + ", locale=" + data.locales[locidx].getName()
                                        + ", pattern=" + PATTERNS[patidx] + ", time=" + testTimes[testidx]);
                                status = U_ZERO_ERROR;
                                continue;
                            }

                            int32_t timeDiff = (int32_t)(parsedDate - testTimes[testidx]);
                            UBool bTimeMatch = minutesOffset ?
                                (timeDiff/60000)*60000 == 0 : timeDiff == 0;
                            if (!bTimeMatch) {
                                UnicodeString msg = (UnicodeString) "Time round trip failed for " + "tzid=" + *tzid + ", locale=" + data.locales[locidx].getName() + ", pattern=" + PATTERNS[patidx]
                                        + ", text=" + text + ", time=" + testTimes[testidx] + ", restime=" + parsedDate + ", diff=" + (parsedDate - testTimes[testidx]);
                                // Timebomb for TZData update
                                if (expectedRoundTrip[testidx]) {
                                    log.errln((UnicodeString) "FAIL: " + msg);
                                } else if (REALLY_VERBOSE) {
                                    log.logln(msg);
                                }
                            }
                        }
                        tztAvail = tz->getNextTransition(t, FALSE, tzt);
                        if (!tztAvail) {
                            break;
                        }
                        if (middle) {
                            // Test the date in the middle of two transitions.
                            t += (int64_t) ((tzt.getTime() - t) / 2);
                            middle = FALSE;
                            tztAvail = FALSE;
                        } else {
                            t = tzt.getTime();
                        }
                    }
                    delete tz;
                }
                times[patidx] += (Calendar::getNow() - timer);
                delete sdf;
            }
            umtx_lock(NULL);
            data.numDone++;
            umtx_unlock(NULL);
        }
        delete tzids;
    }
private:
    IntlTest& log;
    LocaleData& data;
    int32_t index;
};

void
TimeZoneFormatTest::TestTimeRoundTrip(void) {
    int32_t nThreads = threadCount;
    const Locale *LOCALES;
    int32_t nLocales;
    int32_t testCounts = 0;

    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = Calendar::createInstance(TimeZone::createTimeZone((UnicodeString) "UTC"), status);
    if (U_FAILURE(status)) {
        dataerrln("Calendar::createInstance failed: %s", u_errorName(status));
        return;
    }

    const char* testAllProp = getProperty("TimeZoneRoundTripAll");
    UBool bTestAll = (testAllProp && uprv_strcmp(testAllProp, "true") == 0);

    UDate START_TIME, END_TIME;
    if (bTestAll || !quick) {
        cal->set(1900, UCAL_JANUARY, 1);
    } else {
        cal->set(1990, UCAL_JANUARY, 1);
    }
    START_TIME = cal->getTime(status);

    cal->set(2015, UCAL_JANUARY, 1);
    END_TIME = cal->getTime(status);

    if (U_FAILURE(status)) {
        errln("getTime failed");
        return;
    }

    UDate times[NUM_PATTERNS];
    for (int32_t i = 0; i < NUM_PATTERNS; i++) {
        times[i] = 0;
    }

    // Set up test locales
    const Locale locales1[] = {Locale("en")};
    const Locale locales2[] = {
        Locale("ar_EG"), Locale("bg_BG"), Locale("ca_ES"), Locale("da_DK"), Locale("de"),
        Locale("de_DE"), Locale("el_GR"), Locale("en"), Locale("en_AU"), Locale("en_CA"),
        Locale("en_US"), Locale("es"), Locale("es_ES"), Locale("es_MX"), Locale("fi_FI"),
        Locale("fr"), Locale("fr_CA"), Locale("fr_FR"), Locale("he_IL"), Locale("hu_HU"),
        Locale("it"), Locale("it_IT"), Locale("ja"), Locale("ja_JP"), Locale("ko"),
        Locale("ko_KR"), Locale("nb_NO"), Locale("nl_NL"), Locale("nn_NO"), Locale("pl_PL"),
        Locale("pt"), Locale("pt_BR"), Locale("pt_PT"), Locale("ru_RU"), Locale("sv_SE"),
        Locale("th_TH"), Locale("tr_TR"), Locale("zh"), Locale("zh_Hans"), Locale("zh_Hans_CN"),
        Locale("zh_Hant"), Locale("zh_Hant_TW")
    };

    if (bTestAll) {
        LOCALES = Locale::getAvailableLocales(nLocales);
    } else if (quick) {
        LOCALES = locales1;
        nLocales = sizeof(locales1)/sizeof(Locale);
    } else {
        LOCALES = locales2;
        nLocales = sizeof(locales2)/sizeof(Locale);
    }

    LocaleData data;
    data.index = 0;
    data.testCounts = testCounts;
    data.times = times;
    data.locales = LOCALES;
    data.nLocales = nLocales;
    data.quick = quick;
    data.START_TIME = START_TIME;
    data.END_TIME = END_TIME;
    data.numDone = 0;

#if (ICU_USE_THREADS==0)
    TestTimeRoundTripThread fakeThread(*this, data, 0);
    fakeThread.run();
#else
    TestTimeRoundTripThread **threads = new TestTimeRoundTripThread*[threadCount];
    int32_t i;
    for (i = 0; i < nThreads; i++) {
        threads[i] = new TestTimeRoundTripThread(*this, data, i);
        if (threads[i]->start() != 0) {
            errln("Error starting thread %d", i);
        }
    }

    UBool done = false;
    while (true) {
        umtx_lock(NULL);
        if (data.numDone == nLocales) {
            done = true;
        }
        umtx_unlock(NULL);
        if (done)
            break;
        SimpleThread::sleep(1000);
    }

    for (i = 0; i < nThreads; i++) {
        delete threads[i];
    }
    delete [] threads;

#endif
    UDate total = 0;
    logln("### Elapsed time by patterns ###");
    for (int32_t i = 0; i < NUM_PATTERNS; i++) {
        logln(UnicodeString("") + data.times[i] + "ms (" + PATTERNS[i] + ")");
        total += data.times[i];
    }
    logln((UnicodeString) "Total: " + total + "ms");
    logln((UnicodeString) "Iteration: " + data.testCounts);

    delete cal;
}


typedef struct {
    const char*     text;
    int32_t         inPos;
    const char*     locale;
    UTimeZoneFormatStyle    style;
    UBool           parseAll;
    const char*     expected;
    int32_t         outPos;
    UTimeZoneFormatTimeType timeType;
} ParseTestData;

void
TimeZoneFormatTest::TestParse(void) {
    const ParseTestData DATA[] = {
        //   text               inPos   locale      style                               parseAll    expected            outPos  timeType
            {"Z",               0,      "en_US",    UTZFMT_STYLE_ISO_EXTENDED_FULL,     false,      "Etc/GMT",          1,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"Z",               0,      "en_US",    UTZFMT_STYLE_SPECIFIC_LONG,         false,      "Etc/GMT",          1,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"Zambia time",     0,      "en_US",    UTZFMT_STYLE_ISO_EXTENDED_FULL,     true,       "Etc/GMT",          1,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"Zambia time",     0,      "en_US",    UTZFMT_STYLE_GENERIC_LOCATION,      false,      "Africa/Lusaka",    11,     UTZFMT_TIME_TYPE_UNKNOWN},
            {"Zambia time",     0,      "en_US",    UTZFMT_STYLE_ISO_BASIC_LOCAL_FULL,  true,       "Africa/Lusaka",    11,     UTZFMT_TIME_TYPE_UNKNOWN},
            {"+00:00",          0,      "en_US",    UTZFMT_STYLE_ISO_EXTENDED_FULL,     false,      "Etc/GMT",          6,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"-01:30:45",       0,      "en_US",    UTZFMT_STYLE_ISO_EXTENDED_FULL,     false,      "GMT-01:30:45",     9,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"-7",              0,      "en_US",    UTZFMT_STYLE_ISO_BASIC_LOCAL_FULL,  false,      "GMT-07:00",        2,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"-2222",           0,      "en_US",    UTZFMT_STYLE_ISO_BASIC_LOCAL_FULL,  false,      "GMT-22:22",        5,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"-3333",           0,      "en_US",    UTZFMT_STYLE_ISO_BASIC_LOCAL_FULL,  false,      "GMT-03:33",        4,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"XXX+01:30YYY",    3,      "en_US",    UTZFMT_STYLE_LOCALIZED_GMT,         false,      "GMT+01:30",        9,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"GMT0",            0,      "en_US",    UTZFMT_STYLE_SPECIFIC_SHORT,        false,      "Etc/GMT",          3,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"EST",             0,      "en_US",    UTZFMT_STYLE_SPECIFIC_SHORT,        false,      "America/New_York", 3,      UTZFMT_TIME_TYPE_STANDARD},
            {"ESTx",            0,      "en_US",    UTZFMT_STYLE_SPECIFIC_SHORT,        false,      "America/New_York", 3,      UTZFMT_TIME_TYPE_STANDARD},
            {"EDTx",            0,      "en_US",    UTZFMT_STYLE_SPECIFIC_SHORT,        false,      "America/New_York", 3,      UTZFMT_TIME_TYPE_DAYLIGHT},
            {"EST",             0,      "en_US",    UTZFMT_STYLE_SPECIFIC_LONG,         false,      NULL,               0,      UTZFMT_TIME_TYPE_UNKNOWN},
            {"EST",             0,      "en_US",    UTZFMT_STYLE_SPECIFIC_LONG,         true,       "America/New_York", 3,      UTZFMT_TIME_TYPE_STANDARD},
            {"EST",             0,      "en_CA",    UTZFMT_STYLE_SPECIFIC_SHORT,        false,      "America/Toronto",  3,      UTZFMT_TIME_TYPE_STANDARD},
            {NULL,              0,      NULL,       UTZFMT_STYLE_GENERIC_LOCATION,      false,      NULL,               0,      UTZFMT_TIME_TYPE_UNKNOWN}
    };

    for (int32_t i = 0; DATA[i].text; i++) {
        UErrorCode status = U_ZERO_ERROR;
        LocalPointer<TimeZoneFormat> tzfmt(TimeZoneFormat::createInstance(Locale(DATA[i].locale), status));
        if (U_FAILURE(status)) {
            dataerrln("Fail TimeZoneFormat::createInstance: %s", u_errorName(status));
            continue;
        }
        UTimeZoneFormatTimeType ttype = UTZFMT_TIME_TYPE_UNKNOWN;
        ParsePosition pos(DATA[i].inPos);
        int32_t parseOptions = DATA[i].parseAll ? UTZFMT_PARSE_OPTION_ALL_STYLES : UTZFMT_PARSE_OPTION_NONE;
        TimeZone* tz = tzfmt->parse(DATA[i].style, DATA[i].text, pos, parseOptions, &ttype);

        UnicodeString errMsg;
        if (tz) {
            UnicodeString outID;
            tz->getID(outID);
            if (outID != UnicodeString(DATA[i].expected)) {
                errMsg = (UnicodeString)"Time zone ID: " + outID + " - expected: " + DATA[i].expected;
            } else if (pos.getIndex() != DATA[i].outPos) {
                errMsg = (UnicodeString)"Parsed pos: " + pos.getIndex() + " - expected: " + DATA[i].outPos;
            } else if (ttype != DATA[i].timeType) {
                errMsg = (UnicodeString)"Time type: " + ttype + " - expected: " + DATA[i].timeType;
            }
            delete tz;
        } else {
            if (DATA[i].expected) {
                errln((UnicodeString)"Fail: Parse failure - expected: " + DATA[i].expected);
            }
        }
        if (errMsg.length() > 0) {
            errln((UnicodeString)"Fail: " + errMsg + " [text=" + DATA[i].text + ", pos=" + DATA[i].inPos + ", style=" + DATA[i].style + "]");
        }
    }
}

void
TimeZoneFormatTest::TestISOFormat(void) {
    const int32_t OFFSET[] = {
        0,          // 0
        999,        // 0.999s
        -59999,     // -59.999s
        60000,      // 1m
        -77777,     // -1m 17.777s
        1800000,    // 30m
        -3600000,   // -1h
        36000000,   // 10h
        -37800000,  // -10h 30m
        -37845000,  // -10h 30m 45s
        108000000,  // 30h
    };

    const char* ISO_STR[][11] = {
        // 0
        {
            "Z", "Z", "Z", "Z", "Z",
            "+00", "+0000", "+00:00", "+0000", "+00:00",
            "+0000"
        },
        // 999
        {
            "Z", "Z", "Z", "Z", "Z",
            "+00", "+0000", "+00:00", "+0000", "+00:00",
            "+0000"
        },
        // -59999
        {
            "Z", "Z", "Z", "-000059", "-00:00:59",
            "+00", "+0000", "+00:00", "-000059", "-00:00:59",
            "-000059"
        },
        // 60000
        {
            "+0001", "+0001", "+00:01", "+0001", "+00:01",
            "+0001", "+0001", "+00:01", "+0001", "+00:01",
            "+0001"
        },
        // -77777
        {
            "-0001", "-0001", "-00:01", "-000117", "-00:01:17",
            "-0001", "-0001", "-00:01", "-000117", "-00:01:17",
            "-000117"
        },
        // 1800000
        {
            "+0030", "+0030", "+00:30", "+0030", "+00:30",
            "+0030", "+0030", "+00:30", "+0030", "+00:30",
            "+0030"
        },
        // -3600000
        {
            "-01", "-0100", "-01:00", "-0100", "-01:00",
            "-01", "-0100", "-01:00", "-0100", "-01:00",
            "-0100"
        },
        // 36000000
        {
            "+10", "+1000", "+10:00", "+1000", "+10:00",
            "+10", "+1000", "+10:00", "+1000", "+10:00",
            "+1000"
        },
        // -37800000
        {
            "-1030", "-1030", "-10:30", "-1030", "-10:30",
            "-1030", "-1030", "-10:30", "-1030", "-10:30",
            "-1030"
        },
        // -37845000
        {
            "-1030", "-1030", "-10:30", "-103045", "-10:30:45",
            "-1030", "-1030", "-10:30", "-103045", "-10:30:45",
            "-103045"
        },
        // 108000000
        {
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            0
        }
    };

    const char* PATTERN[] = {
        "X", "XX", "XXX", "XXXX", "XXXXX",
        "x", "xx", "xxx", "xxxx", "xxxxx",
        "Z", // equivalent to "xxxx"
        0
    };

    const int32_t MIN_OFFSET_UNIT[] = {
        60000, 60000, 60000, 1000, 1000,
        60000, 60000, 60000, 1000, 1000,
        1000,
    };

    // Formatting
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<SimpleDateFormat> sdf(new SimpleDateFormat(status));
    if (U_FAILURE(status)) {
        dataerrln("Fail new SimpleDateFormat: %s", u_errorName(status));
        return;
    }
    UDate d = Calendar::getNow();

    for (uint32_t i = 0; i < sizeof(OFFSET)/sizeof(OFFSET[0]); i++) {
        SimpleTimeZone* tz = new SimpleTimeZone(OFFSET[i], UnicodeString("Zone Offset:") + OFFSET[i] + "ms");
        sdf->adoptTimeZone(tz);
        for (int32_t j = 0; PATTERN[j] != 0; j++) {
            sdf->applyPattern(UnicodeString(PATTERN[j]));
            UnicodeString result;
            sdf->format(d, result);

            if (ISO_STR[i][j]) {
                if (result != UnicodeString(ISO_STR[i][j])) {
                    errln((UnicodeString)"FAIL: pattern=" + PATTERN[j] + ", offset=" + OFFSET[i] + " -> "
                        + result + " (expected: " + ISO_STR[i][j] + ")");
                }
            } else {
                // Offset out of range
                // Note: for now, there is no way to propagate the error status through
                // the SimpleDateFormat::format above.
                if (result.length() > 0) {
                    errln((UnicodeString)"FAIL: Non-Empty result for pattern=" + PATTERN[j] + ", offset=" + OFFSET[i]
                        + " (expected: empty result)");
                }
            }
        }
    }

    // Parsing
    LocalPointer<Calendar> outcal(Calendar::createInstance(status));
    if (U_FAILURE(status)) {
        dataerrln("Fail new Calendar: %s", u_errorName(status));
        return;
    }
    for (int32_t i = 0; ISO_STR[i][0] != NULL; i++) {
        for (int32_t j = 0; PATTERN[j] != 0; j++) {
            if (ISO_STR[i][j] == 0) {
                continue;
            }
            ParsePosition pos(0);
            SimpleTimeZone* bogusTZ = new SimpleTimeZone(-1, UnicodeString("Zone Offset: -1ms"));
            outcal->adoptTimeZone(bogusTZ);
            sdf->applyPattern(PATTERN[j]);

            sdf->parse(UnicodeString(ISO_STR[i][j]), *(outcal.getAlias()), pos);

            if (pos.getIndex() != (int32_t)uprv_strlen(ISO_STR[i][j])) {
                errln((UnicodeString)"FAIL: Failed to parse the entire input string: " + ISO_STR[i][j]);
            }

            const TimeZone& outtz = outcal->getTimeZone();
            int32_t outOffset = outtz.getRawOffset();
            int32_t adjustedOffset = OFFSET[i] / MIN_OFFSET_UNIT[j] * MIN_OFFSET_UNIT[j];
            if (outOffset != adjustedOffset) {
                errln((UnicodeString)"FAIL: Incorrect offset:" + outOffset + "ms for input string: " + ISO_STR[i][j]
                    + " (expected:" + adjustedOffset + "ms)");
            }
        }
    }
}


typedef struct {
    const char*     locale;
    const char*     tzid;
    UDate           date;
    UTimeZoneFormatStyle    style;
    const char*     expected;
    UTimeZoneFormatTimeType timeType;
} FormatTestData;

void
TimeZoneFormatTest::TestFormat(void) {
    UDate dateJan = 1358208000000.0;    // 2013-01-15T00:00:00Z
    UDate dateJul = 1373846400000.0;    // 2013-07-15T00:00:00Z

    const FormatTestData DATA[] = {
        {
            "en",
            "America/Los_Angeles", 
            dateJan,
            UTZFMT_STYLE_GENERIC_LOCATION,
            "Los Angeles Time",
            UTZFMT_TIME_TYPE_UNKNOWN
        },
        {
            "en",
            "America/Los_Angeles",
            dateJan,
            UTZFMT_STYLE_GENERIC_LONG,
            "Pacific Time",
            UTZFMT_TIME_TYPE_UNKNOWN
        },
        {
            "en",
            "America/Los_Angeles",
            dateJan,
            UTZFMT_STYLE_SPECIFIC_LONG,
            "Pacific Standard Time",
            UTZFMT_TIME_TYPE_STANDARD
        },
        {
            "en",
            "America/Los_Angeles",
            dateJul,
            UTZFMT_STYLE_SPECIFIC_LONG,
            "Pacific Daylight Time",
            UTZFMT_TIME_TYPE_DAYLIGHT
        },
        {
            "ja",
            "America/Los_Angeles",
            dateJan,
            UTZFMT_STYLE_ZONE_ID,
            "America/Los_Angeles",
            UTZFMT_TIME_TYPE_UNKNOWN
        },
        {
            "fr",
            "America/Los_Angeles",
            dateJul,
            UTZFMT_STYLE_ZONE_ID_SHORT,
            "uslax",
            UTZFMT_TIME_TYPE_UNKNOWN
        },
        {
            "en",
            "America/Los_Angeles",
            dateJan,
            UTZFMT_STYLE_EXEMPLAR_LOCATION,
            "Los Angeles",
            UTZFMT_TIME_TYPE_UNKNOWN
        },

        {
            "ja",
            "Asia/Tokyo",
            dateJan,
            UTZFMT_STYLE_GENERIC_LONG,
            "\\u65E5\\u672C\\u6A19\\u6E96\\u6642",
            UTZFMT_TIME_TYPE_UNKNOWN
        },

        {0, 0, 0.0, UTZFMT_STYLE_GENERIC_LOCATION, 0, UTZFMT_TIME_TYPE_UNKNOWN}
    };

    for (int32_t i = 0; DATA[i].locale; i++) {
        UErrorCode status = U_ZERO_ERROR;
        LocalPointer<TimeZoneFormat> tzfmt(TimeZoneFormat::createInstance(Locale(DATA[i].locale), status));
        if (U_FAILURE(status)) {
            dataerrln("Fail TimeZoneFormat::createInstance: %s", u_errorName(status));
            continue;
        }

        LocalPointer<TimeZone> tz(TimeZone::createTimeZone(DATA[i].tzid));
        UnicodeString out;
        UTimeZoneFormatTimeType timeType;

        tzfmt->format(DATA[i].style, *(tz.getAlias()), DATA[i].date, out, &timeType);
        UnicodeString expected(DATA[i].expected, -1, US_INV);
        expected = expected.unescape();

        assertEquals(UnicodeString("Format result for ") + DATA[i].tzid + " (Test Case " + i + ")", expected, out);
        if (DATA[i].timeType != timeType) {
            dataerrln(UnicodeString("Formatted time zone type (Test Case ") + i + "), returned="
                + timeType + ", expected=" + DATA[i].timeType);
        }
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
