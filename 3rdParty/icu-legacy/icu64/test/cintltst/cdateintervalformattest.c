// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 2011-2016, International Business Machines Corporation
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
#include "cformtst.h"

static void TestDateIntervalFormat(void);
static void TestFPos_SkelWithSeconds(void);
static void TestFormatToResult(void);

void addDateIntervalFormatTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/cdateintervalformattest/" #x)

void addDateIntervalFormatTest(TestNode** root)
{
    TESTCASE(TestDateIntervalFormat);
    TESTCASE(TestFPos_SkelWithSeconds);
    TESTCASE(TestFormatToResult);
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
    { "de", "Hm",       tzUSPacific, Date201009270800, Date201009270800 + 12.0*_HOUR, "08:00\\u201320:00 Uhr" },
    { "de", "Hm",       tzUSPacific, Date201009270800, Date201009270800 + 31.0*_DAY,  "27.9.2010, 08:00 \\u2013 28.10.2010, 08:00" },
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

/********************************************************************
 * TestFPos_SkelWithSeconds and related data
 ********************************************************************
 */

static UChar zoneGMT[] = { 0x47,0x4D,0x54,0 }; // GMT
static const UDate startTime = 1416474000000.0; // 2014 Nov 20 09:00 GMT

static const double deltas[] = {
	0.0, // none
	200.0, // 200 millisec
	20000.0, // 20 sec
	1200000.0, // 20 min
	7200000.0, // 2 hrs
	43200000.0, // 12 hrs
	691200000.0, // 8 days
	1382400000.0, // 16 days,
	8640000000.0, // 100 days
	-1.0
};
enum { kNumDeltas = UPRV_LENGTHOF(deltas) - 1 };

typedef struct {
    int32_t posBegin;
    int32_t posEnd;
    const char * format;
} ExpectPosAndFormat;

static const ExpectPosAndFormat exp_en_HHmm[kNumDeltas] = {
    {  3,  5, "09:00" },
    {  3,  5, "09:00" },
    {  3,  5, "09:00" },
    {  3,  5, "09:00 \\u2013 09:20" },
    {  3,  5, "09:00 \\u2013 11:00" },
    {  3,  5, "09:00 \\u2013 21:00" },
    { 15, 17, "11/20/2014, 09:00 \\u2013 11/28/2014, 09:00" },
    { 15, 17, "11/20/2014, 09:00 \\u2013 12/6/2014, 09:00" },
    { 15, 17, "11/20/2014, 09:00 \\u2013 2/28/2015, 09:00" }
};

static const ExpectPosAndFormat exp_en_HHmmss[kNumDeltas] = {
    {  3,  5, "09:00:00" },
    {  3,  5, "09:00:00" },
    {  3,  5, "09:00:00 \\u2013 09:00:20" },
    {  3,  5, "09:00:00 \\u2013 09:20:00" },
    {  3,  5, "09:00:00 \\u2013 11:00:00" },
    {  3,  5, "09:00:00 \\u2013 21:00:00" },
    { 15, 17, "11/20/2014, 09:00:00 \\u2013 11/28/2014, 09:00:00" },
    { 15, 17, "11/20/2014, 09:00:00 \\u2013 12/6/2014, 09:00:00" },
    { 15, 17, "11/20/2014, 09:00:00 \\u2013 2/28/2015, 09:00:00" }
};

static const ExpectPosAndFormat exp_en_yyMMdd[kNumDeltas] = {
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14 \\u2013 11/28/14" },
    {  0,  0, "11/20/14 \\u2013 12/6/14" },
    {  0,  0, "11/20/14 \\u2013 2/28/15" }
};

static const ExpectPosAndFormat exp_en_yyMMddHHmm[kNumDeltas] = {
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00 \\u2013 09:20" },
    { 13, 15, "11/20/14, 09:00 \\u2013 11:00" },
    { 13, 15, "11/20/14, 09:00 \\u2013 21:00" },
    { 13, 15, "11/20/14, 09:00 \\u2013 11/28/14, 09:00" },
    { 13, 15, "11/20/14, 09:00 \\u2013 12/06/14, 09:00" },
    { 13, 15, "11/20/14, 09:00 \\u2013 02/28/15, 09:00" }
};

static const ExpectPosAndFormat exp_en_yyMMddHHmmss[kNumDeltas] = {
    { 13, 15, "11/20/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00 \\u2013 09:00:20" },
    { 13, 15, "11/20/14, 09:00:00 \\u2013 09:20:00" },
    { 13, 15, "11/20/14, 09:00:00 \\u2013 11:00:00" },
    { 13, 15, "11/20/14, 09:00:00 \\u2013 21:00:00" },
    { 13, 15, "11/20/14, 09:00:00 \\u2013 11/28/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00 \\u2013 12/06/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00 \\u2013 02/28/15, 09:00:00" }
};

static const ExpectPosAndFormat exp_en_yMMMdhmmssz[kNumDeltas] = {
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT \\u2013 9:00:20 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT \\u2013 9:20:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT \\u2013 11:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT \\u2013 9:00:00 PM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT \\u2013 Nov 28, 2014, 9:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT \\u2013 Dec 6, 2014, 9:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT \\u2013 Feb 28, 2015, 9:00:00 AM GMT" }
};

static const ExpectPosAndFormat exp_ja_yyMMddHHmm[kNumDeltas] = {
    { 11, 13, "14/11/20 9:00" },
    { 11, 13, "14/11/20 9:00" },
    { 11, 13, "14/11/20 9:00" },
    { 11, 13, "14/11/20 9\\u664200\\u5206\\uFF5E9\\u664220\\u5206" },
    { 11, 13, "14/11/20 9\\u664200\\u5206\\uFF5E11\\u664200\\u5206" },
    { 11, 13, "14/11/20 9\\u664200\\u5206\\uFF5E21\\u664200\\u5206" },
    { 11, 13, "14/11/20 9:00\\uFF5E14/11/28 9:00" },
    { 11, 13, "14/11/20 9:00\\uFF5E14/12/06 9:00" },
    { 11, 13, "14/11/20 9:00\\uFF5E15/02/28 9:00" }
};

static const ExpectPosAndFormat exp_ja_yyMMddHHmmss[kNumDeltas] = {
    { 11, 13, "14/11/20 9:00:00" },
    { 11, 13, "14/11/20 9:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E9:00:20" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E9:20:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E11:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E21:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E14/11/28 9:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E14/12/06 9:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E15/02/28 9:00:00" }
};

static const ExpectPosAndFormat exp_ja_yMMMdHHmmss[kNumDeltas] = {
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E9:00:20" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E9:20:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E11:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E21:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E2014\\u5E7411\\u670828\\u65E5 9:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E2014\\u5E7412\\u67086\\u65E5 9:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E2015\\u5E742\\u670828\\u65E5 9:00:00" }
};

typedef struct {
	const char * locale;
	const char * skeleton;
	UDateFormatField fieldToCheck;
	const ExpectPosAndFormat * expected;
} LocaleAndSkeletonItem;

static const LocaleAndSkeletonItem locSkelItems[] = {
	{ "en",		"HHmm",         UDAT_MINUTE_FIELD, exp_en_HHmm },
	{ "en",		"HHmmss",       UDAT_MINUTE_FIELD, exp_en_HHmmss },
	{ "en",		"yyMMdd",       UDAT_MINUTE_FIELD, exp_en_yyMMdd },
	{ "en",		"yyMMddHHmm",   UDAT_MINUTE_FIELD, exp_en_yyMMddHHmm },
	{ "en",		"yyMMddHHmmss", UDAT_MINUTE_FIELD, exp_en_yyMMddHHmmss },
	{ "en",		"yMMMdhmmssz",  UDAT_MINUTE_FIELD, exp_en_yMMMdhmmssz },
	{ "ja",		"yyMMddHHmm",   UDAT_MINUTE_FIELD, exp_ja_yyMMddHHmm },
	{ "ja",		"yyMMddHHmmss", UDAT_MINUTE_FIELD, exp_ja_yyMMddHHmmss },
	{ "ja",		"yMMMdHHmmss",  UDAT_MINUTE_FIELD, exp_ja_yMMMdHHmmss },
	{ NULL, NULL, (UDateFormatField)0, NULL }
};

enum { kSizeUBuf = 96, kSizeBBuf = 192 };

static void TestFPos_SkelWithSeconds()
{
	const LocaleAndSkeletonItem * locSkelItemPtr;
	for (locSkelItemPtr = locSkelItems; locSkelItemPtr->locale != NULL; locSkelItemPtr++) {
	    UDateIntervalFormat* udifmt;
	    UChar   ubuf[kSizeUBuf];
	    int32_t ulen, uelen;
	    UErrorCode status = U_ZERO_ERROR;
            ulen = u_unescape(locSkelItemPtr->skeleton, ubuf, kSizeUBuf);
	    udifmt = udtitvfmt_open(locSkelItemPtr->locale, ubuf, ulen, zoneGMT, -1, &status);
	    if ( U_FAILURE(status) ) {
           log_data_err("FAIL: udtitvfmt_open for locale %s, skeleton %s: %s\n",
                    locSkelItemPtr->locale, locSkelItemPtr->skeleton, u_errorName(status));
	    } else {
			const double * deltasPtr = deltas;
			const ExpectPosAndFormat * expectedPtr = locSkelItemPtr->expected;
			for (; *deltasPtr >= 0.0; deltasPtr++, expectedPtr++) {
			    UFieldPosition fpos = { locSkelItemPtr->fieldToCheck, 0, 0 };
			    UChar uebuf[kSizeUBuf];
			    char bbuf[kSizeBBuf];
			    char bebuf[kSizeBBuf];
			    status = U_ZERO_ERROR;
			    uelen = u_unescape(expectedPtr->format, uebuf, kSizeUBuf);
			    ulen = udtitvfmt_format(udifmt, startTime, startTime + *deltasPtr, ubuf, kSizeUBuf, &fpos, &status);
			    if ( U_FAILURE(status) ) {
			        log_err("FAIL: udtitvfmt_format for locale %s, skeleton %s, delta %.1f: %s\n",
			             locSkelItemPtr->locale, locSkelItemPtr->skeleton, *deltasPtr, u_errorName(status));
			    } else if ( ulen != uelen || u_strncmp(ubuf,uebuf,uelen) != 0 ||
			                fpos.beginIndex != expectedPtr->posBegin || fpos.endIndex != expectedPtr->posEnd ) {
			        u_strToUTF8(bbuf, kSizeBBuf, NULL, ubuf, ulen, &status);
			        u_strToUTF8(bebuf, kSizeBBuf, NULL, uebuf, uelen, &status); // convert back to get unescaped string
			        log_err("FAIL: udtitvfmt_format for locale %s, skeleton %s, delta %12.1f, expect %d-%d \"%s\", get %d-%d \"%s\"\n",
			             locSkelItemPtr->locale, locSkelItemPtr->skeleton, *deltasPtr,
			             expectedPtr->posBegin, expectedPtr->posEnd, bebuf,
			             fpos.beginIndex, fpos.endIndex, bbuf);
			    }
			}
	        udtitvfmt_close(udifmt);
	    }
    }
}

static void TestFormatToResult() {
    UErrorCode ec = U_ZERO_ERROR;
    UDateIntervalFormat* fmt = udtitvfmt_open("de", u"dMMMMyHHmm", -1, zoneGMT, -1, &ec);
    UFormattedDateInterval* fdi = udtitvfmt_openResult(&ec);
    assertSuccess("Opening", &ec);

    {
        const char* message = "Field position test 1";
        const UChar* expectedString = u"27. September 2010, 15:00 – 2. März 2011, 18:30";
        udtitvfmt_formatToResult(fmt, fdi, Date201009270800, Date201103021030, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 25},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 13},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 20, 22},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 23, 25},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 28, 47},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 28, 29},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 31, 35},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 36, 40},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 42, 44},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 45, 47}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char* message = "Field position test 1";
        const UChar* expectedString = u"27. September 2010, 15:00–22:00 Uhr";
        udtitvfmt_formatToResult(fmt, fdi, Date201009270800, Date201009270800 + 7*_HOUR, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 13},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 20, 25},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 20, 22},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 23, 25},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 26, 31},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 26, 28},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 29, 31},
            {UFIELD_CATEGORY_DATE, UDAT_AM_PM_FIELD, 32, 35}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    udtitvfmt_close(fmt);
    udtitvfmt_closeResult(fdi);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
