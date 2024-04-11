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
#include "unicode/udisplaycontext.h"
#include "cintltst.h"
#include "cmemory.h"
#include "cformtst.h"

static void TestDateIntervalFormat(void);
static void TestFPos_SkelWithSeconds(void);
static void TestFormatToResult(void);
static void TestFormatCalendarToResult(void);

void addDateIntervalFormatTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/cdateintervalformattest/" #x)

void addDateIntervalFormatTest(TestNode** root)
{
    TESTCASE(TestDateIntervalFormat);
    TESTCASE(TestFPos_SkelWithSeconds);
    TESTCASE(TestFormatToResult);
    TESTCASE(TestFormatCalendarToResult);
}

static const char tzUSPacific[] = "US/Pacific";
static const char tzAsiaTokyo[] = "Asia/Tokyo";
#define Date201103021030 1299090600000.0 /* 2011-Mar-02 1030 in US/Pacific, 2011-Mar-03 0330 in Asia/Tokyo */
#define Date201009270800 1285599629000.0 /* 2010-Sep-27 0800 in US/Pacific */
#define Date158210140000 -12219379142000.0
#define Date158210160000 -12219206342000.0
#define _MINUTE (60.0*1000.0)
#define _HOUR   (60.0*60.0*1000.0)
#define _DAY    (24.0*60.0*60.0*1000.0)

typedef struct {
    const char * locale;
    const char * skeleton;
    UDisplayContext context;
    const char * tzid;
    const UDate  from;
    const UDate  to;
    const UChar * resultExpected;
} DateIntervalFormatTestItem;

#define CAP_NONE  UDISPCTX_CAPITALIZATION_NONE
#define CAP_BEGIN UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE
#define CAP_LIST  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU
#define CAP_ALONE UDISPCTX_CAPITALIZATION_FOR_STANDALONE

/* Just a small set of tests for now, the real functionality is tested in the C++ tests */
static const DateIntervalFormatTestItem testItems[] = {
    { "en", "MMMdHHmm", CAP_NONE,  tzUSPacific, Date201103021030, Date201103021030 + 7.0*_HOUR,  u"Mar 2, 10:30\u2009\u2013\u200917:30" },
    { "en", "MMMdHHmm", CAP_NONE,  tzAsiaTokyo, Date201103021030, Date201103021030 + 7.0*_HOUR,  u"Mar 3, 03:30\u2009\u2013\u200910:30" },
    { "en", "yMMMEd",   CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 12.0*_HOUR, u"Mon, Sep 27, 2010" },
    { "en", "yMMMEd",   CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 31.0*_DAY,  u"Mon, Sep 27\u2009\u2013\u2009Thu, Oct 28, 2010" },
    { "en", "yMMMEd",   CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 410.0*_DAY, u"Mon, Sep 27, 2010\u2009\u2013\u2009Fri, Nov 11, 2011" },
    { "de", "Hm",       CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 12.0*_HOUR, u"08:00\u201320:00 Uhr" },
    { "de", "Hm",       CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 31.0*_DAY,  u"27.9.2010, 08:00\u2009\u2013\u200928.10.2010, 08:00" },
    { "ja", "MMMd",     CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 1.0*_DAY,   u"9月27日～28日" },
    { "cs", "MMMEd",    CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 60.0*_DAY,  u"po 27. 9.\u2009\u2013\u2009pá 26. 11." },
    { "cs", "yMMMM",    CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 60.0*_DAY,  u"září\u2013listopad 2010" },
    { "cs", "yMMMM",    CAP_NONE,  tzUSPacific, Date201009270800, Date201009270800 + 1.0*_DAY,   u"září 2010" },
#if !UCONFIG_NO_BREAK_ITERATION
    { "cs", "MMMEd",    CAP_BEGIN, tzUSPacific, Date201009270800, Date201009270800 + 60.0*_DAY,  u"Po 27. 9.\u2009\u2013\u2009pá 26. 11." },
    { "cs", "yMMMM",    CAP_BEGIN, tzUSPacific, Date201009270800, Date201009270800 + 60.0*_DAY,  u"Září\u2013listopad 2010" },
    { "cs", "yMMMM",    CAP_BEGIN, tzUSPacific, Date201009270800, Date201009270800 + 1.0*_DAY,   u"Září 2010" },
    { "cs", "MMMEd",    CAP_LIST,  tzUSPacific, Date201009270800, Date201009270800 + 60.0*_DAY,  u"Po 27. 9.\u2009\u2013\u2009pá 26. 11." },
    { "cs", "yMMMM",    CAP_LIST,  tzUSPacific, Date201009270800, Date201009270800 + 60.0*_DAY,  u"Září\u2013listopad 2010" },
    { "cs", "yMMMM",    CAP_LIST,  tzUSPacific, Date201009270800, Date201009270800 + 1.0*_DAY,   u"Září 2010" },
    { "cs", "MMMEd",    CAP_ALONE, tzUSPacific, Date201009270800, Date201009270800 + 60.0*_DAY,  u"po 27. 9.\u2009\u2013\u2009pá 26. 11." },
#endif
    { "cs", "yMMMM",    CAP_ALONE, tzUSPacific, Date201009270800, Date201009270800 + 60.0*_DAY,  u"září\u2013listopad 2010" },
    { "cs", "yMMMM",    CAP_ALONE, tzUSPacific, Date201009270800, Date201009270800 + 1.0*_DAY,   u"září 2010" },
    { NULL, NULL,       CAP_NONE,  NULL,        0,                0,                             NULL }
};

enum {
    kSkelBufLen = 32,
    kTZIDBufLen = 96,
    kFormatBufLen = 128
};

static void TestDateIntervalFormat(void)
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

            udtitvfmt_setContext(udtitvfmt, testItemPtr->context, &status);
            if ( U_FAILURE(status) ) {
                log_err("FAIL: udtitvfmt_setContext for locale %s, skeleton %s, context %04X -  %s\n",
                        testItemPtr->locale, testItemPtr->skeleton, (unsigned)testItemPtr->context, myErrorName(status) );
            } else {
                UDisplayContext getContext = udtitvfmt_getContext(udtitvfmt, UDISPCTX_TYPE_CAPITALIZATION, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: udtitvfmt_getContext for locale %s, skeleton %s, context %04X -  %s\n",
                            testItemPtr->locale, testItemPtr->skeleton, (unsigned)testItemPtr->context, myErrorName(status) );
                } else if (getContext != testItemPtr->context) {
                    log_err("FAIL: udtitvfmt_getContext for locale %s, skeleton %s, context %04X -  got context %04X\n",
                            testItemPtr->locale, testItemPtr->skeleton, (unsigned)testItemPtr->context, (unsigned)getContext );
                }
            }
            status = U_ZERO_ERROR;
            int32_t fmtLen = udtitvfmt_format(udtitvfmt, testItemPtr->from, testItemPtr->to, result, kFormatBufLen, NULL, &status);
            if (fmtLen >= kFormatBufLen) {
                result[kFormatBufLen-1] = 0;
            }
            if ( U_SUCCESS(status) ) {
                if ( u_strcmp(result, testItemPtr->resultExpected) != 0 ) {
                    char bcharBufExp[kFormatBufLen];
                    char bcharBufGet[kFormatBufLen];
                    log_err("ERROR: udtitvfmt_format for locale %s, skeleton %s, tzid %s, from %.1f, to %.1f: expect %s, get %s\n",
                             testItemPtr->locale, testItemPtr->skeleton, tzidForLog, testItemPtr->from, testItemPtr->to,
                             u_austrcpy(bcharBufExp,testItemPtr->resultExpected), u_austrcpy(bcharBufGet,result) );
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
    {  3,  5, "09:00\\u2009\\u2013\\u200909:20" },
    {  3,  5, "09:00\\u2009\\u2013\\u200911:00" },
    {  3,  5, "09:00\\u2009\\u2013\\u200921:00" },
    { 15, 17, "11/20/2014, 09:00\\u2009\\u2013\\u200911/28/2014, 09:00" },
    { 15, 17, "11/20/2014, 09:00\\u2009\\u2013\\u200912/6/2014, 09:00" },
    { 15, 17, "11/20/2014, 09:00\\u2009\\u2013\\u20092/28/2015, 09:00" }
};

static const ExpectPosAndFormat exp_en_HHmmss[kNumDeltas] = {
    {  3,  5, "09:00:00" },
    {  3,  5, "09:00:00" },
    {  3,  5, "09:00:00\\u2009\\u2013\\u200909:00:20" },
    {  3,  5, "09:00:00\\u2009\\u2013\\u200909:20:00" },
    {  3,  5, "09:00:00\\u2009\\u2013\\u200911:00:00" },
    {  3,  5, "09:00:00\\u2009\\u2013\\u200921:00:00" },
    { 15, 17, "11/20/2014, 09:00:00\\u2009\\u2013\\u200911/28/2014, 09:00:00" },
    { 15, 17, "11/20/2014, 09:00:00\\u2009\\u2013\\u200912/6/2014, 09:00:00" },
    { 15, 17, "11/20/2014, 09:00:00\\u2009\\u2013\\u20092/28/2015, 09:00:00" }
};

static const ExpectPosAndFormat exp_en_yyMMdd[kNumDeltas] = {
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14\\u2009\\u2013\\u200911/28/14" },
    {  0,  0, "11/20/14\\u2009\\u2013\\u200912/6/14" },
    {  0,  0, "11/20/14\\u2009\\u2013\\u20092/28/15" }
};

static const ExpectPosAndFormat exp_en_yyMMddHHmm[kNumDeltas] = {
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200909:20" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200911:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200921:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200911/28/14, 09:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200912/06/14, 09:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200902/28/15, 09:00" }
};

static const ExpectPosAndFormat exp_en_yyMMddHHmmss[kNumDeltas] = {
    { 13, 15, "11/20/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200909:00:20" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200909:20:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200911:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200921:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200911/28/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200912/06/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200902/28/15, 09:00:00" }
};

static const ExpectPosAndFormat exp_en_yMMMdhmmssz[kNumDeltas] = {
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT\\u2009\\u2013\\u20099:00:20\\u202FAM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT\\u2009\\u2013\\u20099:20:00\\u202FAM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT\\u2009\\u2013\\u200911:00:00\\u202FAM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT\\u2009\\u2013\\u20099:00:00\\u202FPM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT\\u2009\\u2013\\u2009Nov 28, 2014, 9:00:00\\u202FAM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT\\u2009\\u2013\\u2009Dec 6, 2014, 9:00:00\\u202FAM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00\\u202FAM GMT\\u2009\\u2013\\u2009Feb 28, 2015, 9:00:00\\u202FAM GMT" }
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

static void TestFPos_SkelWithSeconds(void)
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

static void TestFormatToResult(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UDateIntervalFormat* fmt = udtitvfmt_open("de", u"dMMMMyHHmm", -1, zoneGMT, -1, &ec);
    UFormattedDateInterval* fdi = udtitvfmt_openResult(&ec);
    assertSuccess("Opening", &ec);

    {
        const char* message = "Field position test 1";
        const UChar* expectedString = u"27. September 2010 um 15:00\u2009\u2013\u20092. März 2011 um 18:30";
        udtitvfmt_formatToResult(fmt, Date201009270800, Date201103021030, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 27},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 13},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 22, 24},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 25, 27},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 30, 51},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 30, 31},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 33, 37},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 38, 42},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 46, 48},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 49, 51}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char* message = "Field position test 2";
        const UChar* expectedString = u"27. September 2010, 15:00–22:00 Uhr";
        udtitvfmt_formatToResult(fmt, Date201009270800, Date201009270800 + 7*_HOUR, fdi, &ec);
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

static void TestFormatCalendarToResult(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UCalendar* ucal1 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);
    ucal_setMillis(ucal1, Date201009270800, &ec);
    UCalendar* ucal2 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);
    ucal_setMillis(ucal2, Date201103021030, &ec);
    UCalendar* ucal3 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);
    ucal_setMillis(ucal3, Date201009270800 + 7*_HOUR, &ec);
    UCalendar* ucal4 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);
    UCalendar* ucal5 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);

    UDateIntervalFormat* fmt = udtitvfmt_open("de", u"dMMMMyHHmm", -1, zoneGMT, -1, &ec);
    UFormattedDateInterval* fdi = udtitvfmt_openResult(&ec);
    assertSuccess("Opening", &ec);

    {
        const char* message = "Field position test 1";
        const UChar* expectedString = u"27. September 2010 um 15:00\u2009\u2013\u20092. März 2011 um 18:30";
        udtitvfmt_formatCalendarToResult(fmt, ucal1, ucal2, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 27},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 13},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 22, 24},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 25, 27},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 30, 51},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 30, 31},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 33, 37},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 38, 42},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 46, 48},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 49, 51}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char* message = "Field position test 2";
        const UChar* expectedString = u"27. September 2010, 15:00–22:00 Uhr";
        udtitvfmt_formatCalendarToResult(fmt, ucal1, ucal3, fdi, &ec);
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
    {
        const char* message = "Field position test 3";
        // Date across Julian Gregorian change date.
        ucal_setMillis(ucal4, Date158210140000, &ec);
        ucal_setMillis(ucal5, Date158210160000, &ec);
        //                                        1         2                        3         4
        //                              0123456789012345678901234     5     6     789012345678901234567890
        const UChar* expectedString = u"4. Oktober 1582 um 00:00\u2009\u2013\u200916. Oktober 1582 um 00:00";
        udtitvfmt_formatCalendarToResult(fmt, ucal4, ucal5, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 24},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 1},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 3, 10},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 11, 15},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 19, 21},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 22, 24},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 27, 52},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 27, 29},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 31, 38},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 39, 43},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 47, 49},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 50, 52}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        // Date across Julian Gregorian change date.
        // We set the Gregorian Change way back.
        ucal_setGregorianChange(ucal5, (UDate)(-8.64e15), &ec);
        ucal_setGregorianChange(ucal4, (UDate)(-8.64e15), &ec);
        ucal_setMillis(ucal4, Date158210140000, &ec);
        ucal_setMillis(ucal5, Date158210160000, &ec);
        const char* message = "Field position test 4";
        //                                        1         2                        3         4
        //                              01234567890123456789012345     6     7     89012345678901234567890
        const UChar* expectedString = u"14. Oktober 1582 um 00:00\u2009\u2013\u200916. Oktober 1582 um 00:00";
        udtitvfmt_formatCalendarToResult(fmt, ucal4, ucal5, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 25},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 11},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 12, 16},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 20, 22},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 23, 25},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 28, 53},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 28, 30},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 32, 39},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 40, 44},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 48, 50},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 51, 53}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    ucal_close(ucal1);
    ucal_close(ucal2);
    ucal_close(ucal3);
    ucal_close(ucal4);
    ucal_close(ucal5);
    udtitvfmt_close(fmt);
    udtitvfmt_closeResult(fdi);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
