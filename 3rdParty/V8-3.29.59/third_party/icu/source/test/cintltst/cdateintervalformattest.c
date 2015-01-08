/********************************************************************
 * Copyright (c) 2011-2013, International Business Machines Corporation
 * and others. All Rights Reserved.
 ********************************************************************/
/* C API TEST FOR DATE INTERVAL FORMAT */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udateintervalformat.h"
#include "unicode/udat.h"
#include "unicode/ucal.h"
#include "unicode/ustring.h"
#include "cintltst.h"
#include "cmemory.h"

static void TestDateIntervalFormat(void);

#define LEN(a) (sizeof(a)/sizeof(a[0]))

void addDateIntervalFormatTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/cdateintervalformattest/" #x)

void addDateIntervalFormatTest(TestNode** root)
{
    TESTCASE(TestDateIntervalFormat);
}

static const char tzUSPacific[] = "US/Pacific";
static const char tzAsiaTokyo[] = "Asia/Tokyo";
#define Date201103021030 1299090600000.0 /* 2011-Mar-02 1030 in US/Pacific, 2011-Mar-03 0330 in Asia/Tokyo */
#define Date201009270800 1285599629000.0 /* 2010-Sep-27 0800 in US/Pacific */
#define _MINUTE (60.0*1000.0)
#define _HOUR   (60.0*60.0*1000.0)
#define _DAY    (24.0*60.0*60.0*1000.0)

typedef struct {
    const char * locale;
    const char * skeleton;
    const char * tzid;
    const UDate  from;
    const UDate  to;
    const char * resultExpected;
} DateIntervalFormatTestItem;

/* Just a small set of tests for now, the real functionality is tested in the C++ tests */
static const DateIntervalFormatTestItem testItems[] = {
    { "en", "MMMdHHmm", tzUSPacific, Date201103021030, Date201103021030 + 7.0*_HOUR,  "Mar 2, 10:30 \\u2013 17:30" },
    { "en", "MMMdHHmm", tzAsiaTokyo, Date201103021030, Date201103021030 + 7.0*_HOUR,  "Mar 3, 03:30 \\u2013 10:30" },
    { "en", "yMMMEd",   tzUSPacific, Date201009270800, Date201009270800 + 12.0*_HOUR, "Mon, Sep 27, 2010" },
    { "en", "yMMMEd",   tzUSPacific, Date201009270800, Date201009270800 + 31.0*_DAY,  "Mon, Sep 27 \\u2013 Thu, Oct 28, 2010" },
    { "en", "yMMMEd",   tzUSPacific, Date201009270800, Date201009270800 + 410.0*_DAY, "Mon, Sep 27, 2010 \\u2013 Fri, Nov 11, 2011" },
    { "de", "Hm",       tzUSPacific, Date201009270800, Date201009270800 + 12.0*_HOUR, "08:00-20:00" },
    { "de", "Hm",       tzUSPacific, Date201009270800, Date201009270800 + 31.0*_DAY,  "27.9.2010 08:00 - 28.10.2010 08:00" },
    { "ja", "MMMd",     tzUSPacific, Date201009270800, Date201009270800 + 1.0*_DAY,   "9\\u670827\\u65E5\\uFF5E28\\u65E5" },
    { NULL, NULL,       NULL,        0,                0,                             NULL }
};

enum {
    kSkelBufLen = 32,
    kTZIDBufLen = 96,
    kFormatBufLen = 128
};

static void TestDateIntervalFormat()
{
    const DateIntervalFormatTestItem * testItemPtr;
    UErrorCode status = U_ZERO_ERROR;
    ctest_setTimeZone(NULL, &status);
    log_verbose("\nTesting udtitvfmt_open() and udtitvfmt_format() with various parameters\n");
    for ( testItemPtr = testItems; testItemPtr->locale != NULL; ++testItemPtr ) {
        UDateIntervalFormat* udtitvfmt;
        int32_t tzidLen;
        UChar skelBuf[kSkelBufLen];
        UChar tzidBuf[kTZIDBufLen];
        const char * tzidForLog = (testItemPtr->tzid)? testItemPtr->tzid: "NULL";

        status = U_ZERO_ERROR;
        u_unescape(testItemPtr->skeleton, skelBuf, kSkelBufLen);
        if ( testItemPtr->tzid ) {
            u_unescape(testItemPtr->tzid, tzidBuf, kTZIDBufLen);
            tzidLen = -1;
        } else {
            tzidLen = 0;
        }
        udtitvfmt = udtitvfmt_open(testItemPtr->locale, skelBuf, -1, tzidBuf, tzidLen, &status);
        if ( U_SUCCESS(status) ) {
            UChar result[kFormatBufLen];
            UChar resultExpected[kFormatBufLen];
            int32_t fmtLen = udtitvfmt_format(udtitvfmt, testItemPtr->from, testItemPtr->to, result, kFormatBufLen, NULL, &status);
            if (fmtLen >= kFormatBufLen) {
                result[kFormatBufLen-1] = 0;
            }
            if ( U_SUCCESS(status) ) {
                u_unescape(testItemPtr->resultExpected, resultExpected, kFormatBufLen);
                if ( u_strcmp(result, resultExpected) != 0 ) {
                    char bcharBuf[kFormatBufLen];
                    log_err("ERROR: udtitvfmt_format for locale %s, skeleton %s, tzid %s, from %.1f, to %.1f: expect %s, get %s\n",
                             testItemPtr->locale, testItemPtr->skeleton, tzidForLog, testItemPtr->from, testItemPtr->to,
                             testItemPtr->resultExpected, u_austrcpy(bcharBuf,result) );
                }
            } else {
                log_err("FAIL: udtitvfmt_format for locale %s, skeleton %s, tzid %s, from %.1f, to %.1f: %s\n",
                        testItemPtr->locale, testItemPtr->skeleton, tzidForLog, testItemPtr->from, testItemPtr->to, myErrorName(status) );
            }
            udtitvfmt_close(udtitvfmt);
        } else {
            log_data_err("FAIL: udtitvfmt_open for locale %s, skeleton %s, tzid %s - %s\n",
                    testItemPtr->locale, testItemPtr->skeleton, tzidForLog, myErrorName(status) );
        }
    }
    ctest_resetTimeZone();
}

#endif /* #if !UCONFIG_NO_FORMATTING */
