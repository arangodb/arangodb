// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2007-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  udatpg_test.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007aug01
*   created by: Markus W. Scherer
*
*   Test of the C wrapper for the DateTimePatternGenerator.
*   Calls each C API function and exercises code paths in the wrapper,
*   but the full functionality is tested in the C++ intltest.
*
*   One item to note: C API functions which return a const UChar *
*   should return a NUL-terminated string.
*   (The C++ implementation needs to use getTerminatedBuffer()
*   on UnicodeString objects which end up being returned this way.)
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <stdbool.h>

#include "unicode/udat.h"
#include "unicode/udatpg.h"
#include "unicode/ustring.h"
#include "cintltst.h"
#include "cmemory.h"

void addDateTimePatternGeneratorTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/udatpg_test/" #x)

static void TestOpenClose(void);
static void TestUsage(void);
static void TestBuilder(void);
static void TestOptions(void);
static void TestGetFieldDisplayNames(void);
static void TestGetDefaultHourCycle(void);
static void TestGetDefaultHourCycleOnEmptyInstance(void);
static void TestEras(void);
static void TestDateTimePatterns(void);
static void TestRegionOverride(void);

void addDateTimePatternGeneratorTest(TestNode** root) {
    TESTCASE(TestOpenClose);
    TESTCASE(TestUsage);
    TESTCASE(TestBuilder);
    TESTCASE(TestOptions);
    TESTCASE(TestGetFieldDisplayNames);
    TESTCASE(TestGetDefaultHourCycle);
    TESTCASE(TestGetDefaultHourCycleOnEmptyInstance);
    TESTCASE(TestEras);
    TESTCASE(TestDateTimePatterns);
    TESTCASE(TestRegionOverride);
}

/*
 * Pipe symbol '|'. We pass only the first UChar without NUL-termination.
 * The second UChar is just to verify that the API does not pick that up.
 */
static const UChar pipeString[]={ 0x7c, 0x0a };

static const UChar testSkeleton1[]={ 0x48, 0x48, 0x6d, 0x6d, 0 }; /* HHmm */
static const UChar expectingBestPattern[]={ 0x48, 0x2e, 0x6d, 0x6d, 0 }; /* H.mm */
static const UChar testPattern[]={ 0x48, 0x48, 0x3a, 0x6d, 0x6d, 0 }; /* HH:mm */
static const UChar expectingSkeleton[]= { 0x48, 0x48, 0x6d, 0x6d, 0 }; /* HHmm */
static const UChar expectingBaseSkeleton[]= { 0x48, 0x6d, 0 }; /* HHmm */
static const UChar redundantPattern[]={ 0x79, 0x79, 0x4d, 0x4d, 0x4d, 0 }; /* yyMMM */
static const UChar testFormat[]= {0x7B, 0x31, 0x7D, 0x20, 0x7B, 0x30, 0x7D, 0};  /* {1} {0} */
static const UChar appendItemName[]= {0x68, 0x72, 0};  /* hr */
static const UChar testPattern2[]={ 0x48, 0x48, 0x3a, 0x6d, 0x6d, 0x20, 0x76, 0 }; /* HH:mm v */
static const UChar replacedStr[]={ 0x76, 0x76, 0x76, 0x76, 0 }; /* vvvv */
/* results for getBaseSkeletons() - {Hmv}, {yMMM} */
static const UChar resultBaseSkeletons[2][10] = {{0x48,0x6d, 0x76, 0}, {0x79, 0x4d, 0x4d, 0x4d, 0 } };
static const UChar sampleFormatted[] = {0x31, 0x30, 0x20, 0x6A, 0x75, 0x69, 0x6C, 0x2E, 0}; /* 10 juil. */
static const UChar skeleton[]= {0x4d, 0x4d, 0x4d, 0x64, 0};  /* MMMd */
static const UChar timeZoneGMT[] = { 0x0047, 0x004d, 0x0054, 0x0000 };  /* "GMT" */

static void TestOpenClose(void) {
    UErrorCode errorCode=U_ZERO_ERROR;
    UDateTimePatternGenerator *dtpg, *dtpg2;
    const UChar *s;
    int32_t length;

    /* Open a DateTimePatternGenerator for the default locale. */
    dtpg=udatpg_open(NULL, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err_status(errorCode, "udatpg_open(NULL) failed - %s\n", u_errorName(errorCode));
        return;
    }
    udatpg_close(dtpg);

    /* Now one for German. */
    dtpg=udatpg_open("de", &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("udatpg_open(de) failed - %s\n", u_errorName(errorCode));
        return;
    }

    /* Make some modification which we verify gets passed on to the clone. */
    udatpg_setDecimal(dtpg, pipeString, 1);

    /* Clone the generator. */
    dtpg2=udatpg_clone(dtpg, &errorCode);
    if(U_FAILURE(errorCode) || dtpg2==NULL) {
        log_err("udatpg_clone() failed - %s\n", u_errorName(errorCode));
        return;
    }

    /* Verify that the clone has the custom decimal symbol. */
    s=udatpg_getDecimal(dtpg2, &length);
    if(s==pipeString || length!=1 || 0!=u_memcmp(s, pipeString, length) || s[length]!=0) { 
        log_err("udatpg_getDecimal(cloned object) did not return the expected string\n");
        return;
    }

    udatpg_close(dtpg);
    udatpg_close(dtpg2);
}

typedef struct {
    UDateTimePatternField field;
    UChar name[12];
} AppendItemNameData;

static const AppendItemNameData appendItemNameData[] = { /* for Finnish */
    { UDATPG_YEAR_FIELD,    {0x0076,0x0075,0x006F,0x0073,0x0069,0} }, /* "vuosi" */
    { UDATPG_MONTH_FIELD,   {0x006B,0x0075,0x0075,0x006B,0x0061,0x0075,0x0073,0x0069,0} }, /* "kuukausi" */
    { UDATPG_WEEKDAY_FIELD, {0x0076,0x0069,0x0069,0x006B,0x006F,0x006E,0x0070,0x00E4,0x0069,0x0076,0x00E4,0} },
    { UDATPG_DAY_FIELD,     {0x0070,0x00E4,0x0069,0x0076,0x00E4,0} },
    { UDATPG_HOUR_FIELD,    {0x0074,0x0075,0x006E,0x0074,0x0069,0} }, /* "tunti" */
    { UDATPG_FIELD_COUNT,   {0}        }  /* terminator */
};

static void TestUsage(void) {
    UErrorCode errorCode=U_ZERO_ERROR;
    UDateTimePatternGenerator *dtpg;
    const AppendItemNameData * appItemNameDataPtr;
    UChar bestPattern[20];
    UChar result[20];
    int32_t length;    
    UChar *s;
    const UChar *r;
    
    dtpg=udatpg_open("fi", &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err_status(errorCode, "udatpg_open(fi) failed - %s\n", u_errorName(errorCode));
        return;
    }
    length = udatpg_getBestPattern(dtpg, testSkeleton1, 4,
                                   bestPattern, 20, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("udatpg_getBestPattern failed - %s\n", u_errorName(errorCode));
        return;
    }
    if((u_memcmp(bestPattern, expectingBestPattern, length)!=0) || bestPattern[length]!=0) { 
        log_err("udatpg_getBestPattern did not return the expected string\n");
        return;
    }
    
    
    /* Test skeleton == NULL */
    s=NULL;
    length = udatpg_getBestPattern(dtpg, s, 0, bestPattern, 20, &errorCode);
    if(!U_FAILURE(errorCode)&&(length!=0) ) {
        log_err("udatpg_getBestPattern failed in illegal argument - skeleton is NULL.\n");
        return;
    }
    
    /* Test udatpg_getSkeleton */
    length = udatpg_getSkeleton(dtpg, testPattern, 5, result, 20,  &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("udatpg_getSkeleton failed - %s\n", u_errorName(errorCode));
        return;
    }
    if((u_memcmp(result, expectingSkeleton, length)!=0) || result[length]!=0) { 
        log_err("udatpg_getSkeleton did not return the expected string\n");
        return;
    }
    
    /* Test pattern == NULL */
    s=NULL;
    length = udatpg_getSkeleton(dtpg, s, 0, result, 20, &errorCode);
    if(!U_FAILURE(errorCode)&&(length!=0) ) {
        log_err("udatpg_getSkeleton failed in illegal argument - pattern is NULL.\n");
        return;
    }    
    
    /* Test udatpg_getBaseSkeleton */
    length = udatpg_getBaseSkeleton(dtpg, testPattern, 5, result, 20,  &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("udatpg_getBaseSkeleton failed - %s\n", u_errorName(errorCode));
        return;
    }
    if((u_memcmp(result, expectingBaseSkeleton, length)!=0) || result[length]!=0) { 
        log_err("udatpg_getBaseSkeleton did not return the expected string\n");
        return;
    }
    
    /* Test pattern == NULL */
    s=NULL;
    length = udatpg_getBaseSkeleton(dtpg, s, 0, result, 20, &errorCode);
    if(!U_FAILURE(errorCode)&&(length!=0) ) {
        log_err("udatpg_getBaseSkeleton failed in illegal argument - pattern is NULL.\n");
        return;
    }
    
    /* set append format to {1}{0} */
    udatpg_setAppendItemFormat( dtpg, UDATPG_MONTH_FIELD, testFormat, 7 );
    r = udatpg_getAppendItemFormat(dtpg, UDATPG_MONTH_FIELD, &length);
    
    
    if(length!=7 || 0!=u_memcmp(r, testFormat, length) || r[length]!=0) { 
        log_err("udatpg_setAppendItemFormat did not return the expected string\n");
        return;
    }
    
    for (appItemNameDataPtr = appendItemNameData; appItemNameDataPtr->field <  UDATPG_FIELD_COUNT; appItemNameDataPtr++) {
        int32_t nameLength;
        const UChar * namePtr = udatpg_getAppendItemName(dtpg, appItemNameDataPtr->field, &nameLength);
        if ( namePtr == NULL || u_strncmp(appItemNameDataPtr->name, namePtr, nameLength) != 0 ) {
            log_err("udatpg_getAppendItemName returns invalid name for field %d\n", (int)appItemNameDataPtr->field);
        }
    }
    
    /* set append name to hr */
    udatpg_setAppendItemName(dtpg, UDATPG_HOUR_FIELD, appendItemName, 2);
    r = udatpg_getAppendItemName(dtpg, UDATPG_HOUR_FIELD, &length);
    
    if(length!=2 || 0!=u_memcmp(r, appendItemName, length) || r[length]!=0) { 
        log_err("udatpg_setAppendItemName did not return the expected string\n");
        return;
    }
    
    /* set date time format to {1}{0} */
    udatpg_setDateTimeFormat( dtpg, testFormat, 7 );
    r = udatpg_getDateTimeFormat(dtpg, &length);
    
    if(length!=7 || 0!=u_memcmp(r, testFormat, length) || r[length]!=0) { 
        log_err("udatpg_setDateTimeFormat did not return the expected string\n");
        return;
    }
    udatpg_close(dtpg);
}

static void TestBuilder(void) {
    UErrorCode errorCode=U_ZERO_ERROR;
    UDateTimePatternGenerator *dtpg;
    UDateTimePatternConflict conflict;
    UEnumeration *en;
    UChar result[20];
    int32_t length, pLength;  
    const UChar *s, *p;
    const UChar* ptrResult[2]; 
    int32_t count=0;
    UDateTimePatternGenerator *generator;
    int32_t formattedCapacity, resultLen,patternCapacity ;
    UChar   pattern[40], formatted[40];
    UDateFormat *formatter;
    UDate sampleDate = 837039928046.0;
    static const char locale[]= "fr";
    UErrorCode status=U_ZERO_ERROR;
    
    /* test create an empty DateTimePatternGenerator */
    dtpg=udatpg_openEmpty(&errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("udatpg_openEmpty() failed - %s\n", u_errorName(errorCode));
        return;
    }
    
    /* Add a pattern */
    conflict = udatpg_addPattern(dtpg, redundantPattern, 5, false, result, 20, 
                                 &length, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("udatpg_addPattern() failed - %s\n", u_errorName(errorCode));
        return;
    }
    /* Add a redundant pattern */
    conflict = udatpg_addPattern(dtpg, redundantPattern, 5, false, result, 20,
                                 &length, &errorCode);
    if(conflict == UDATPG_NO_CONFLICT) {
        log_err("udatpg_addPattern() failed to find the duplicate pattern.\n");
        return;
    }
    /* Test pattern == NULL */
    s=NULL;
    length = udatpg_addPattern(dtpg, s, 0, false, result, 20,
                               &length, &errorCode);
    if(!U_FAILURE(errorCode)&&(length!=0) ) {
        log_err("udatpg_addPattern failed in illegal argument - pattern is NULL.\n");
        return;
    }

    /* replace field type */
    errorCode=U_ZERO_ERROR;
    conflict = udatpg_addPattern(dtpg, testPattern2, 7, false, result, 20,
                                 &length, &errorCode);
    if((conflict != UDATPG_NO_CONFLICT)||U_FAILURE(errorCode)) {
        log_err("udatpg_addPattern() failed to add HH:mm v. - %s\n", u_errorName(errorCode));
        return;
    }
    length = udatpg_replaceFieldTypes(dtpg, testPattern2, 7, replacedStr, 4,
                                      result, 20, &errorCode);
    if (U_FAILURE(errorCode) || (length==0) ) {
        log_err("udatpg_replaceFieldTypes failed!\n");
        return;
    }
    
    /* Get all skeletons and the crroespong pattern for each skeleton. */
    ptrResult[0] = testPattern2;
    ptrResult[1] = redundantPattern; 
    count=0;
    en = udatpg_openSkeletons(dtpg, &errorCode);  
    if (U_FAILURE(errorCode) || (length==0) ) {
        log_err("udatpg_openSkeletons failed!\n");
        return;
    }
    while ( (s=uenum_unext(en, &length, &errorCode))!= NULL) {
        p = udatpg_getPatternForSkeleton(dtpg, s, length, &pLength);
        if (U_FAILURE(errorCode) || p==NULL || u_memcmp(p, ptrResult[count], pLength)!=0 ) {
            log_err("udatpg_getPatternForSkeleton failed!\n");
            return;
        }
        count++;
    }
    uenum_close(en);
    
    /* Get all baseSkeletons */
    en = udatpg_openBaseSkeletons(dtpg, &errorCode);
    count=0;
    while ( (s=uenum_unext(en, &length, &errorCode))!= NULL) {
        p = udatpg_getPatternForSkeleton(dtpg, s, length, &pLength);
        if (U_FAILURE(errorCode) || p==NULL || u_memcmp(p, resultBaseSkeletons[count], pLength)!=0 ) {
            log_err("udatpg_getPatternForSkeleton failed!\n");
            return;
        }
        count++;
    }
    if (U_FAILURE(errorCode) || (length==0) ) {
        log_err("udatpg_openSkeletons failed!\n");
        return;
    }
    uenum_close(en);
    
    udatpg_close(dtpg);
    
    /* sample code in Userguide */
    patternCapacity = UPRV_LENGTHOF(pattern);
    status=U_ZERO_ERROR;
    generator=udatpg_open(locale, &status);
    if(U_FAILURE(status)) {
        return;
    }

    /* get a pattern for an abbreviated month and day */
    length = udatpg_getBestPattern(generator, skeleton, 4,
                                   pattern, patternCapacity, &status);
    formatter = udat_open(UDAT_PATTERN, UDAT_PATTERN, locale, timeZoneGMT, -1,
                          pattern, length, &status);
    if (formatter==NULL) {
        log_err("Failed to initialize the UDateFormat of the sample code in Userguide.\n");
        udatpg_close(generator);
        return;
    }

    /* use it to format (or parse) */
    formattedCapacity = UPRV_LENGTHOF(formatted);
    resultLen=udat_format(formatter, ucal_getNow(), formatted, formattedCapacity,
                          NULL, &status);
    /* for French, the result is "13 sept." */

    /* cannot use the result from ucal_getNow() because the value change evreyday. */
    resultLen=udat_format(formatter, sampleDate, formatted, formattedCapacity,
                          NULL, &status);
    if ( u_memcmp(sampleFormatted, formatted, resultLen) != 0 ) {
        log_err("Failed udat_format() of sample code in Userguide.\n");
    }
    udatpg_close(generator);
    udat_close(formatter);
}

typedef struct DTPtnGenOptionsData {
    const char *                    locale;
    const UChar *                   skel;
    UDateTimePatternMatchOptions    options;
    const UChar *                   expectedPattern;
} DTPtnGenOptionsData;
enum { kTestOptionsPatLenMax = 32 };

static const UChar skel_Hmm[]     = u"Hmm";
static const UChar skel_HHmm[]    = u"HHmm";
static const UChar skel_hhmm[]    = u"hhmm";
static const UChar patn_hcmm_a[]  = u"h:mm\u202Fa";
static const UChar patn_HHcmm[]   = u"HH:mm";
static const UChar patn_hhcmm_a[] = u"hh:mm\u202Fa";
static const UChar patn_HHpmm[]   = u"HH.mm";
static const UChar patn_hpmm_a[]  = u"h.mm\u202Fa";
static const UChar patn_Hpmm[]    = u"H.mm";
static const UChar patn_hhpmm_a[] = u"hh.mm\u202Fa";

static void TestOptions(void) {
    const DTPtnGenOptionsData testData[] = {
        /*loc   skel       options                       expectedPattern */
        { "en", skel_Hmm,  UDATPG_MATCH_NO_OPTIONS,        patn_HHcmm   },
        { "en", skel_HHmm, UDATPG_MATCH_NO_OPTIONS,        patn_HHcmm   },
        { "en", skel_hhmm, UDATPG_MATCH_NO_OPTIONS,        patn_hcmm_a  },
        { "en", skel_Hmm,  UDATPG_MATCH_HOUR_FIELD_LENGTH, patn_HHcmm   },
        { "en", skel_HHmm, UDATPG_MATCH_HOUR_FIELD_LENGTH, patn_HHcmm   },
        { "en", skel_hhmm, UDATPG_MATCH_HOUR_FIELD_LENGTH, patn_hhcmm_a },
        { "da", skel_Hmm,  UDATPG_MATCH_NO_OPTIONS,        patn_HHpmm   },
        { "da", skel_HHmm, UDATPG_MATCH_NO_OPTIONS,        patn_HHpmm   },
        { "da", skel_hhmm, UDATPG_MATCH_NO_OPTIONS,        patn_hpmm_a  },
        { "da", skel_Hmm,  UDATPG_MATCH_HOUR_FIELD_LENGTH, patn_Hpmm    },
        { "da", skel_HHmm, UDATPG_MATCH_HOUR_FIELD_LENGTH, patn_HHpmm   },
        { "da", skel_hhmm, UDATPG_MATCH_HOUR_FIELD_LENGTH, patn_hhpmm_a },
    };

    int count = UPRV_LENGTHOF(testData);
    const DTPtnGenOptionsData * testDataPtr = testData;

    for (; count-- > 0; ++testDataPtr) {
        UErrorCode status = U_ZERO_ERROR;
        UDateTimePatternGenerator * dtpgen = udatpg_open(testDataPtr->locale, &status);
        if ( U_SUCCESS(status) ) {
            UChar pattern[kTestOptionsPatLenMax];
            int32_t patLen = udatpg_getBestPatternWithOptions(dtpgen, testDataPtr->skel, -1,
                                                              testDataPtr->options, pattern,
                                                              kTestOptionsPatLenMax, &status);
            if ( U_FAILURE(status) || u_strncmp(pattern, testDataPtr->expectedPattern, patLen+1) != 0 ) {
                char skelBytes[kTestOptionsPatLenMax];
                char expectedPatternBytes[kTestOptionsPatLenMax];
                char patternBytes[kTestOptionsPatLenMax];
                log_err("ERROR udatpg_getBestPatternWithOptions, locale %s, skeleton %s, options 0x%04X, expected pattern %s, got %s, status %d\n",
                        testDataPtr->locale, u_austrncpy(skelBytes,testDataPtr->skel,kTestOptionsPatLenMax), testDataPtr->options,
                        u_austrncpy(expectedPatternBytes,testDataPtr->expectedPattern,kTestOptionsPatLenMax),
                        u_austrncpy(patternBytes,pattern,kTestOptionsPatLenMax), status );
            }
            udatpg_close(dtpgen);
        } else {
            log_data_err("ERROR udatpg_open failed for locale %s : %s - (Are you missing data?)\n", testDataPtr->locale, myErrorName(status));
        }
    }
}

typedef struct FieldDisplayNameData {
    const char *            locale;
    UDateTimePatternField   field;
    UDateTimePGDisplayWidth width;
    const char *            expected;
} FieldDisplayNameData;
enum { kFieldDisplayNameMax = 32, kFieldDisplayNameBytesMax  = 64};

static void TestGetFieldDisplayNames(void) {
    const FieldDisplayNameData testData[] = {
        /*loc      field                              width               expectedName */
        { "de",    UDATPG_QUARTER_FIELD,              UDATPG_WIDE,        "Quartal" },
        { "de",    UDATPG_QUARTER_FIELD,              UDATPG_ABBREVIATED, "Quart." },
        { "de",    UDATPG_QUARTER_FIELD,              UDATPG_NARROW,      "Q" },
        { "en",    UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD, UDATPG_WIDE,        "weekday of the month" },
        { "en",    UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD, UDATPG_ABBREVIATED, "wkday. of mo." },
        { "en",    UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD, UDATPG_NARROW,      "wkday. of mo." }, // fallback
        { "en_GB", UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD, UDATPG_WIDE,        "weekday of the month" },
        { "en_GB", UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD, UDATPG_ABBREVIATED, "wkday of mo" }, // override
        { "en_GB", UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD, UDATPG_NARROW,      "wkday of mo" },
        { "it",    UDATPG_SECOND_FIELD,               UDATPG_WIDE,        "secondo" },
        { "it",    UDATPG_SECOND_FIELD,               UDATPG_ABBREVIATED, "s" },
        { "it",    UDATPG_SECOND_FIELD,               UDATPG_NARROW,      "s" },
    };

    int count = UPRV_LENGTHOF(testData);
    const FieldDisplayNameData * testDataPtr = testData;
    for (; count-- > 0; ++testDataPtr) {
        UErrorCode status = U_ZERO_ERROR;
        UDateTimePatternGenerator * dtpgen = udatpg_open(testDataPtr->locale, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("ERROR udatpg_open failed for locale %s : %s - (Are you missing data?)\n", testDataPtr->locale, myErrorName(status));
        } else {
            UChar expName[kFieldDisplayNameMax];
            UChar getName[kFieldDisplayNameMax];
            u_unescape(testDataPtr->expected, expName, kFieldDisplayNameMax);
            
            int32_t getLen = udatpg_getFieldDisplayName(dtpgen, testDataPtr->field, testDataPtr->width,
                                                        getName, kFieldDisplayNameMax, &status);
            if ( U_FAILURE(status) ) {
                log_err("ERROR udatpg_getFieldDisplayName locale %s field %d width %d, got status %s, len %d\n",
                        testDataPtr->locale, testDataPtr->field, testDataPtr->width, u_errorName(status), getLen);
            } else if ( u_strncmp(expName, getName, kFieldDisplayNameMax) != 0 ) {
                char expNameB[kFieldDisplayNameBytesMax];
                char getNameB[kFieldDisplayNameBytesMax];
                log_err("ERROR udatpg_getFieldDisplayName locale %s field %d width %d, expected %s, got %s, status %s\n",
                        testDataPtr->locale, testDataPtr->field, testDataPtr->width,
                        u_austrncpy(expNameB,expName,kFieldDisplayNameBytesMax),
                        u_austrncpy(getNameB,getName,kFieldDisplayNameBytesMax), u_errorName(status) );
            } else if (testDataPtr->width == UDATPG_WIDE && getLen > 1) {
                // test preflight & inadequate buffer
                int32_t getNewLen;
                status = U_ZERO_ERROR;
                getNewLen = udatpg_getFieldDisplayName(dtpgen, testDataPtr->field, UDATPG_WIDE, NULL, 0, &status);
                if (U_FAILURE(status) || getNewLen != getLen) {
                    log_err("ERROR udatpg_getFieldDisplayName locale %s field %d width %d, preflight expected len %d, got %d, status %s\n",
                        testDataPtr->locale, testDataPtr->field, testDataPtr->width, getLen, getNewLen, u_errorName(status) );
                }
                status = U_ZERO_ERROR;
                getNewLen = udatpg_getFieldDisplayName(dtpgen, testDataPtr->field, UDATPG_WIDE, getName, getLen-1, &status);
                if (status!=U_BUFFER_OVERFLOW_ERROR || getNewLen != getLen) {
                    log_err("ERROR udatpg_getFieldDisplayName locale %s field %d width %d, overflow expected len %d & BUFFER_OVERFLOW_ERROR, got %d & status %s\n",
                        testDataPtr->locale, testDataPtr->field, testDataPtr->width, getLen, getNewLen, u_errorName(status) );
                }
            }
            udatpg_close(dtpgen);
        }
    }
}

typedef struct HourCycleData {
    const char *         locale;
    UDateFormatHourCycle   expected;
} HourCycleData;

static void TestGetDefaultHourCycle(void) {
    const HourCycleData testData[] = {
        /*loc      expected */
        { "ar_EG",    UDAT_HOUR_CYCLE_12 },
        { "de_DE",    UDAT_HOUR_CYCLE_23 },
        { "en_AU",    UDAT_HOUR_CYCLE_12 },
        { "en_CA",    UDAT_HOUR_CYCLE_12 },
        { "en_US",    UDAT_HOUR_CYCLE_12 },
        { "es_ES",    UDAT_HOUR_CYCLE_23 },
        { "fi",       UDAT_HOUR_CYCLE_23 },
        { "fr",       UDAT_HOUR_CYCLE_23 },
        { "ja_JP",    UDAT_HOUR_CYCLE_23 },
        { "zh_CN",    UDAT_HOUR_CYCLE_23 },
        { "zh_HK",    UDAT_HOUR_CYCLE_12 },
        { "zh_TW",    UDAT_HOUR_CYCLE_12 },
        { "ko_KR",    UDAT_HOUR_CYCLE_12 },
    };
    int count = UPRV_LENGTHOF(testData);
    const HourCycleData * testDataPtr = testData;
    for (; count-- > 0; ++testDataPtr) {
        UErrorCode status = U_ZERO_ERROR;
        UDateTimePatternGenerator * dtpgen =
            udatpg_open(testDataPtr->locale, &status);
        if ( U_FAILURE(status) ) {
            log_data_err( "ERROR udatpg_open failed for locale %s : %s - (Are you missing data?)\n",
                         testDataPtr->locale, myErrorName(status));
        } else {
            UDateFormatHourCycle actual = udatpg_getDefaultHourCycle(dtpgen, &status);
            if (U_FAILURE(status) || testDataPtr->expected != actual) {
                log_err("ERROR dtpgen locale %s udatpg_getDefaultHourCycle expected to get %d but get %d\n",
                        testDataPtr->locale, testDataPtr->expected, actual);
            }
            udatpg_close(dtpgen);
        }
    }
}

// Ensure that calling udatpg_getDefaultHourCycle on an empty instance doesn't call UPRV_UNREACHABLE_EXIT/abort.
static void TestGetDefaultHourCycleOnEmptyInstance(void) {
    UErrorCode status = U_ZERO_ERROR;
    UDateTimePatternGenerator * dtpgen = udatpg_openEmpty(&status);

    if (U_FAILURE(status)) {
        log_data_err("ERROR udatpg_openEmpty failed, status: %s \n", myErrorName(status));
        return;
    }

    (void)udatpg_getDefaultHourCycle(dtpgen, &status);
    if (!U_FAILURE(status)) {
        log_data_err("ERROR expected udatpg_getDefaultHourCycle on an empty instance to fail, status: %s", myErrorName(status));
    }

    status = U_USELESS_COLLATOR_ERROR;
    (void)udatpg_getDefaultHourCycle(dtpgen, &status);
    if (status != U_USELESS_COLLATOR_ERROR) {
        log_data_err("ERROR udatpg_getDefaultHourCycle shouldn't modify status if it is already failed, status: %s", myErrorName(status));
    }

    udatpg_close(dtpgen);
}

// Test for ICU-21202: Make sure DateTimePatternGenerator supplies an era field for year formats using the
// Buddhist and Japanese calendars for all English-speaking locales.
static void TestEras(void) {
    const char* localeIDs[] = {
        "en_US@calendar=japanese",
        "en_GB@calendar=japanese",
        "en_150@calendar=japanese",
        "en_001@calendar=japanese",
        "en@calendar=japanese",
        "en_US@calendar=buddhist",
        "en_GB@calendar=buddhist",
        "en_150@calendar=buddhist",
        "en_001@calendar=buddhist",
        "en@calendar=buddhist",
    };
    
    UErrorCode err = U_ZERO_ERROR;
    for (int32_t i = 0; i < UPRV_LENGTHOF(localeIDs); i++) {
        const char* locale = localeIDs[i];
        UDateTimePatternGenerator* dtpg = udatpg_open(locale, &err);
        if (U_SUCCESS(err)) {
            UChar pattern[200];
            udatpg_getBestPattern(dtpg, u"y", 1, pattern, 200, &err);
            
            if (u_strchr(pattern, u'G') == NULL) {
                log_err("missing era field for locale %s\n", locale);
            }
        }
        udatpg_close(dtpg);
    }
}

enum { kNumDateTimePatterns = 4 };

typedef struct {
    const char* localeID;
    const UChar* expectPat[kNumDateTimePatterns];
} DTPLocaleAndResults;

static void doDTPatternTest(UDateTimePatternGenerator* udtpg,
                            const UChar** skeletons,
                            DTPLocaleAndResults* localeAndResultsPtr);

static void TestDateTimePatterns(void) {
    const UChar* skeletons[kNumDateTimePatterns] = {
        u"yMMMMEEEEdjmm", // full date, short time
        u"yMMMMdjmm",     // long date, short time
        u"yMMMdjmm",      // medium date, short time
        u"yMdjmm"         // short date, short time
    };
    // The following tests some locales in which there are differences between the
    // DateTimePatterns of various length styles.
    DTPLocaleAndResults localeAndResults[] = {
        { "en", { u"EEEE, MMMM d, y 'at' h:mm\u202Fa", // long != medium
                  u"MMMM d, y 'at' h:mm\u202Fa",
                  u"MMM d, y, h:mm\u202Fa",
                  u"M/d/y, h:mm\u202Fa" } },
        { "fr", { u"EEEE d MMMM y 'à' HH:mm", // medium != short
                  u"d MMMM y 'à' HH:mm",
                  u"d MMM y, HH:mm",
                  u"dd/MM/y HH:mm" } },
        { "ha", { u"EEEE d MMMM, y 'da' HH:mm",
                  u"d MMMM, y 'da' HH:mm",
                  u"d MMM, y, HH:mm",
                  u"y-MM-dd, HH:mm" } },
        { NULL, { NULL, NULL, NULL, NULL } } // terminator
    };

    const UChar* enDTPatterns[kNumDateTimePatterns] = {
        u"{1} 'at' {0}",
        u"{1} 'at' {0}",
        u"{1}, {0}",
        u"{1}, {0}"
    };
    const UChar* modDTPatterns[kNumDateTimePatterns] = {
        u"{1} _0_ {0}",
        u"{1} _1_ {0}",
        u"{1} _2_ {0}",
        u"{1} _3_ {0}"
    };
    DTPLocaleAndResults enModResults = { "en", { u"EEEE, MMMM d, y _0_ h:mm\u202Fa",
                                                 u"MMMM d, y _1_ h:mm\u202Fa",
                                                 u"MMM d, y _2_ h:mm\u202Fa",
                                                 u"M/d/y _3_ h:mm\u202Fa" }
    };

    // Test various locales with standard data
    UErrorCode status;
    UDateTimePatternGenerator* udtpg;
    DTPLocaleAndResults* localeAndResultsPtr = localeAndResults;
    for (; localeAndResultsPtr->localeID != NULL; localeAndResultsPtr++) {
        status = U_ZERO_ERROR;
        udtpg = udatpg_open(localeAndResultsPtr->localeID, &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: udatpg_open for locale %s: %s", localeAndResultsPtr->localeID, myErrorName(status));
        } else {
            doDTPatternTest(udtpg, skeletons, localeAndResultsPtr);
            udatpg_close(udtpg);
        }
    }
    // Test getting and modifying date-time combining patterns
    status = U_ZERO_ERROR;
    udtpg = udatpg_open("en", &status);
    if (U_FAILURE(status)) {
        log_data_err("FAIL: udatpg_open #2 for locale en: %s", myErrorName(status));
    } else {
        char bExpect[64];
        char bGet[64];
        const UChar* uGet;
        int32_t uGetLen, uExpectLen;

        // Test error: style out of range
        status = U_ZERO_ERROR;
        uGet = udatpg_getDateTimeFormatForStyle(udtpg, UDAT_NONE, &uGetLen, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR || uGetLen != 0 || uGet==NULL || *uGet!= 0) {
            if (uGet==NULL) {
                log_err("FAIL: udatpg_getDateTimeFormatForStyle with invalid style, expected U_ILLEGAL_ARGUMENT_ERROR "
                        "and ptr to empty string but got %s, len %d, ptr = NULL\n", myErrorName(status), uGetLen);
            } else {
                log_err("FAIL: udatpg_getDateTimeFormatForStyle with invalid style, expected U_ILLEGAL_ARGUMENT_ERROR "
                        "and ptr to empty string but got %s, len %d, *ptr = %04X\n", myErrorName(status), uGetLen, *uGet);
            }
        }

        // Test normal getting and setting
        for (int32_t patStyle = 0; patStyle < kNumDateTimePatterns; patStyle++) {
            status = U_ZERO_ERROR;
            uExpectLen = u_strlen(enDTPatterns[patStyle]);
            uGet = udatpg_getDateTimeFormatForStyle(udtpg, patStyle, &uGetLen, &status);
            if (U_FAILURE(status)) {
                log_err("FAIL udatpg_getDateTimeFormatForStyle %d (en before mod), get %s\n", patStyle, myErrorName(status));
            } else if (uGetLen != uExpectLen || u_strncmp(uGet, enDTPatterns[patStyle], uExpectLen) != 0) {
                u_austrcpy(bExpect, enDTPatterns[patStyle]);
                u_austrcpy(bGet, uGet);
                log_err("ERROR udatpg_getDateTimeFormatForStyle %d (en before mod), expect %d:\"%s\", get %d:\"%s\"\n",
                        patStyle, uExpectLen, bExpect, uGetLen, bGet);
            }
            status = U_ZERO_ERROR;
            udatpg_setDateTimeFormatForStyle(udtpg, patStyle, modDTPatterns[patStyle], -1, &status);
            if (U_FAILURE(status)) {
                log_err("FAIL udatpg_setDateTimeFormatForStyle %d (en), get %s\n", patStyle, myErrorName(status));
            } else {
                uExpectLen = u_strlen(modDTPatterns[patStyle]);
                uGet = udatpg_getDateTimeFormatForStyle(udtpg, patStyle, &uGetLen, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL udatpg_getDateTimeFormatForStyle %d (en after  mod), get %s\n", patStyle, myErrorName(status));
                } else if (uGetLen != uExpectLen || u_strncmp(uGet, modDTPatterns[patStyle], uExpectLen) != 0) {
                    u_austrcpy(bExpect, modDTPatterns[patStyle]);
                    u_austrcpy(bGet, uGet);
                    log_err("ERROR udatpg_getDateTimeFormatForStyle %d (en after  mod), expect %d:\"%s\", get %d:\"%s\"\n",
                            patStyle, uExpectLen, bExpect, uGetLen, bGet);
                }
            }
        }
        // Test result of setting
        doDTPatternTest(udtpg, skeletons, &enModResults);
        // Test old get/set functions
        uExpectLen = u_strlen(modDTPatterns[UDAT_MEDIUM]);
        uGet = udatpg_getDateTimeFormat(udtpg, &uGetLen);
        if (uGetLen != uExpectLen || u_strncmp(uGet, modDTPatterns[UDAT_MEDIUM], uExpectLen) != 0) {
            u_austrcpy(bExpect, modDTPatterns[UDAT_MEDIUM]);
            u_austrcpy(bGet, uGet);
            log_err("ERROR udatpg_getDateTimeFormat (en after  mod), expect %d:\"%s\", get %d:\"%s\"\n",
                    uExpectLen, bExpect, uGetLen, bGet);
        }
        udatpg_setDateTimeFormat(udtpg, modDTPatterns[UDAT_SHORT], -1); // set all dateTimePatterns to the short format
        uExpectLen = u_strlen(modDTPatterns[UDAT_SHORT]);
        u_austrcpy(bExpect, modDTPatterns[UDAT_SHORT]);
        for (int32_t patStyle = 0; patStyle < kNumDateTimePatterns; patStyle++) {
            status = U_ZERO_ERROR;
            uGet = udatpg_getDateTimeFormatForStyle(udtpg, patStyle, &uGetLen, &status);
            if (U_FAILURE(status)) {
                log_err("FAIL udatpg_getDateTimeFormatForStyle %d (en after second mod), get %s\n", patStyle, myErrorName(status));
            } else if (uGetLen != uExpectLen || u_strncmp(uGet, modDTPatterns[UDAT_SHORT], uExpectLen) != 0) {
                u_austrcpy(bGet, uGet);
                log_err("ERROR udatpg_getDateTimeFormatForStyle %d (en after second mod), expect %d:\"%s\", get %d:\"%s\"\n",
                        patStyle, uExpectLen, bExpect, uGetLen, bGet);
            }
        }

        udatpg_close(udtpg);
    }
}

static void doDTPatternTest(UDateTimePatternGenerator* udtpg,
                            const UChar** skeletons,
                            DTPLocaleAndResults* localeAndResultsPtr) {
    for (int32_t patStyle = 0; patStyle < kNumDateTimePatterns; patStyle++) {
        UChar uGet[64];
        int32_t uGetLen, uExpectLen;
        UErrorCode status = U_ZERO_ERROR;
        uExpectLen = u_strlen(localeAndResultsPtr->expectPat[patStyle]);
        uGetLen = udatpg_getBestPattern(udtpg, skeletons[patStyle], -1, uGet, 64, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL udatpg_getBestPattern locale %s style %d: %s\n", localeAndResultsPtr->localeID, patStyle, myErrorName(status));
        } else if (uGetLen != uExpectLen || u_strncmp(uGet, localeAndResultsPtr->expectPat[patStyle], uExpectLen) != 0) {
            char bExpect[64];
            char bGet[64];
            u_austrcpy(bExpect, localeAndResultsPtr->expectPat[patStyle]);
            u_austrcpy(bGet, uGet);
            log_err("ERROR udatpg_getBestPattern locale %s style %d, expect %d:\"%s\", get %d:\"%s\"\n",
                    localeAndResultsPtr->localeID, patStyle, uExpectLen, bExpect, uGetLen, bGet);
        }
    }
}

static void TestRegionOverride(void) {
    typedef struct RegionOverrideTest {
        const char* locale;
        const UChar* expectedPattern;
        UDateFormatHourCycle expectedHourCycle;
    } RegionOverrideTest;

    const RegionOverrideTest testCases[] = {
        { "en_US",           u"h:mm\u202fa", UDAT_HOUR_CYCLE_12 },
        { "en_GB",           u"HH:mm",  UDAT_HOUR_CYCLE_23 },
        { "en_US@rg=GBZZZZ", u"HH:mm",  UDAT_HOUR_CYCLE_23 },
        { "en_US@hours=h23", u"HH:mm",  UDAT_HOUR_CYCLE_23 },
    };

    for (int32_t i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        UErrorCode err = U_ZERO_ERROR;
        UChar actualPattern[200];
        UDateTimePatternGenerator* dtpg = udatpg_open(testCases[i].locale, &err);

        if (assertSuccess("Error creating dtpg", &err)) {
            UDateFormatHourCycle actualHourCycle = udatpg_getDefaultHourCycle(dtpg, &err);
            udatpg_getBestPattern(dtpg, u"jmm", -1, actualPattern, 200, &err);

            if (assertSuccess("Error using dtpg", &err)) {
                assertIntEquals("Wrong hour cycle", testCases[i].expectedHourCycle, actualHourCycle);
                assertUEquals("Wrong pattern", testCases[i].expectedPattern, actualPattern);
            }
        }
        udatpg_close(dtpg);
    }
}
#endif
