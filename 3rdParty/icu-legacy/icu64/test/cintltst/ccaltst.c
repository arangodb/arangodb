// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 1997-2016, International Business Machines
 * Corporation and others. All Rights Reserved.
 ********************************************************************
 *
 * File CCALTST.C
 *
 * Modification History:
 *        Name                     Description            
 *     Madhu Katragadda               Creation
 ********************************************************************
 */

/* C API AND FUNCTIONALITY TEST FOR CALENDAR (ucol.h)*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "unicode/uloc.h"
#include "unicode/ucal.h"
#include "unicode/udat.h"
#include "unicode/ustring.h"
#include "cintltst.h"
#include "ccaltst.h"
#include "cformtst.h"
#include "cmemory.h"
#include "cstring.h"
#include "ulist.h"

void TestGregorianChange(void);
void TestFieldDifference(void);
void TestAddRollEra0AndEraBounds(void);
void TestGetTZTransition(void);
void TestGetWindowsTimeZoneID(void);
void TestGetTimeZoneIDByWindowsID(void);
void TestJpnCalAddSetNextEra(void);
void TestUcalOpenBufferRead(void);
void TestGetTimeZoneOffsetFromLocal(void);

void TestFWWithISO8601(void);

void addCalTest(TestNode** root);

void addCalTest(TestNode** root)
{

    addTest(root, &TestCalendar, "tsformat/ccaltst/TestCalendar");
    addTest(root, &TestGetSetDateAPI, "tsformat/ccaltst/TestGetSetDateAPI");
    addTest(root, &TestFieldGetSet, "tsformat/ccaltst/TestFieldGetSet");
    addTest(root, &TestAddRollExtensive, "tsformat/ccaltst/TestAddRollExtensive");
    addTest(root, &TestGetLimits, "tsformat/ccaltst/TestGetLimits");
    addTest(root, &TestDOWProgression, "tsformat/ccaltst/TestDOWProgression");
    addTest(root, &TestGMTvsLocal, "tsformat/ccaltst/TestGMTvsLocal");
    addTest(root, &TestGregorianChange, "tsformat/ccaltst/TestGregorianChange");
    addTest(root, &TestGetKeywordValuesForLocale, "tsformat/ccaltst/TestGetKeywordValuesForLocale");
    addTest(root, &TestWeekend, "tsformat/ccaltst/TestWeekend");
    addTest(root, &TestFieldDifference, "tsformat/ccaltst/TestFieldDifference");
    addTest(root, &TestAmbiguousWallTime, "tsformat/ccaltst/TestAmbiguousWallTime");
    addTest(root, &TestAddRollEra0AndEraBounds, "tsformat/ccaltst/TestAddRollEra0AndEraBounds");
    addTest(root, &TestGetTZTransition, "tsformat/ccaltst/TestGetTZTransition");
    addTest(root, &TestGetWindowsTimeZoneID, "tsformat/ccaltst/TestGetWindowsTimeZoneID");
    addTest(root, &TestGetTimeZoneIDByWindowsID, "tsformat/ccaltst/TestGetTimeZoneIDByWindowsID");
    addTest(root, &TestJpnCalAddSetNextEra, "tsformat/ccaltst/TestJpnCalAddSetNextEra");
    addTest(root, &TestUcalOpenBufferRead, "tsformat/ccaltst/TestUcalOpenBufferRead");
    addTest(root, &TestGetTimeZoneOffsetFromLocal, "tsformat/ccaltst/TestGetTimeZoneOffsetFromLocal");
    addTest(root, &TestFWWithISO8601, "tsformat/ccaltst/TestFWWithISO8601");
    addTest(root, &TestGetIanaTimeZoneID, "tstformat/ccaltst/TestGetIanaTimeZoneID");
}

/* "GMT" */
static const UChar fgGMTID [] = { 0x0047, 0x004d, 0x0054, 0x0000 };

/* "PST" */
static const UChar PST[] = {0x50, 0x53, 0x54, 0x00}; /* "PST" */

static const UChar EUROPE_PARIS[] = {0x45, 0x75, 0x72, 0x6F, 0x70, 0x65, 0x2F, 0x50, 0x61, 0x72, 0x69, 0x73, 0x00}; /* "Europe/Paris" */

static const UChar AMERICA_LOS_ANGELES[] = {0x41, 0x6D, 0x65, 0x72, 0x69, 0x63, 0x61, 0x2F,
    0x4C, 0x6F, 0x73, 0x5F, 0x41, 0x6E, 0x67, 0x65, 0x6C, 0x65, 0x73, 0x00}; /* America/Los_Angeles */

typedef struct {
    const char *    locale;
    UCalendarType   calType;
    const char *    expectedResult;
} UCalGetTypeTest;

static const UCalGetTypeTest ucalGetTypeTests[] = {
    { "en_US",                   UCAL_GREGORIAN, "gregorian" },
    { "ja_JP@calendar=japanese", UCAL_DEFAULT,   "japanese"  },
    { "th_TH",                   UCAL_GREGORIAN, "gregorian" },
    { "th_TH",                   UCAL_DEFAULT,   "buddhist"  },
    { "th-TH-u-ca-gregory",      UCAL_DEFAULT,   "gregorian" },
    { "ja_JP@calendar=japanese", UCAL_GREGORIAN, "gregorian" },
    { "fr_CH",                   UCAL_DEFAULT,   "gregorian" },
    { "fr_SA",                   UCAL_DEFAULT,   "islamic-umalqura" },
    { "fr_CH@rg=sazzzz",         UCAL_DEFAULT,   "islamic-umalqura" },
    { "fr_CH@rg=sa14",           UCAL_DEFAULT,   "islamic-umalqura" },
    { "fr_CH@calendar=japanese;rg=sazzzz", UCAL_DEFAULT, "japanese" },
    { "fr_CH@rg=twcyi",          UCAL_DEFAULT,   "gregorian" }, // test for ICU-22364
    { "fr_CH@rg=ugw",            UCAL_DEFAULT,   "gregorian" }, // test for ICU-22364
    { "fr_TH@rg=SA",             UCAL_DEFAULT,   "buddhist"  }, /* ignore malformed rg tag */
    { "th@rg=SA",                UCAL_DEFAULT,   "buddhist"  }, /* ignore malformed rg tag */
    { "",                        UCAL_GREGORIAN, "gregorian" },
    { NULL,                      UCAL_GREGORIAN, "gregorian" },
    { NULL, 0, NULL } /* terminator */
};    
    
static void TestCalendar(void)
{
    UCalendar *caldef = 0, *caldef2 = 0, *calfr = 0, *calit = 0, *calfrclone = 0;
    UEnumeration* uenum = NULL;
    int32_t count, count2, i,j;
    UChar tzID[4];
    UChar *tzdname = 0;
    UErrorCode status = U_ZERO_ERROR;
    UDate now;
    UDateFormat *datdef = 0;
    UChar *result = 0;
    int32_t resultlength, resultlengthneeded;
    char tempMsgBuf[1024];  // u_austrcpy() of some formatted dates & times.
    char tempMsgBuf2[256];  // u_austrcpy() of some formatted dates & times.
    UChar zone1[64], zone2[64];
    const char *tzver = 0;
    int32_t tzverLen = 0;
    UChar canonicalID[64];
    UBool isSystemID = false;
    const UCalGetTypeTest * ucalGetTypeTestPtr;

#ifdef U_USE_UCAL_OBSOLETE_2_8
    /*Testing countAvailableTimeZones*/
    int32_t offset=0;
    log_verbose("\nTesting ucal_countAvailableTZIDs\n");
    count=ucal_countAvailableTZIDs(offset);
    log_verbose("The number of timezone id's present with offset 0 are %d:\n", count);
    if(count < 5) /* Don't hard code an exact == test here! */
        log_err("FAIL: error in the ucal_countAvailableTZIDs - got %d expected at least 5 total\n", count);

    /*Testing getAvailableTZIDs*/
    log_verbose("\nTesting ucal_getAvailableTZIDs");
    for(i=0;i<count;i++){
        ucal_getAvailableTZIDs(offset, i, &status);
        if(U_FAILURE(status)){
            log_err("FAIL: ucal_getAvailableTZIDs returned %s\n", u_errorName(status));
        }
        log_verbose("%s\n", u_austrcpy(tempMsgBuf, ucal_getAvailableTZIDs(offset, i, &status)));
    }
    /*get Illegal TZID where index >= count*/
    ucal_getAvailableTZIDs(offset, i, &status);
    if(status != U_INDEX_OUTOFBOUNDS_ERROR){
        log_err("FAIL:for TZID index >= count Expected INDEX_OUTOFBOUNDS_ERROR Got %s\n", u_errorName(status));
    }
    status=U_ZERO_ERROR;
#endif
    
    /*Test ucal_openTimeZones, ucal_openCountryTimeZones and ucal_openTimeZoneIDEnumeration */
    for (j=0; j<6; ++j) {
        const char *api = "?";
        const int32_t offsetMinus5 = -5*60*60*1000;
        switch (j) {
        case 0:
            api = "ucal_openTimeZones()";
            uenum = ucal_openTimeZones(&status);
            break;
        case 1:
            api = "ucal_openCountryTimeZones(US)";
            uenum = ucal_openCountryTimeZones("US", &status);
            break;
        case 2:
            api = "ucal_openTimeZoneIDEnumerarion(UCAL_ZONE_TYPE_CANONICAL, NULL, NULL)";
            uenum = ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, NULL, NULL, &status);
            break;
        case 3:
            api = "ucal_openTimeZoneIDEnumerarion(UCAL_ZONE_TYPE_CANONICAL_LOCATION, CA, NULL)";
            uenum = ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL_LOCATION, "CA", NULL, &status);
            break;
        case 4:
            api = "ucal_openTimeZoneIDEnumerarion(UCAL_ZONE_TYPE_ANY, NULL, -5 hour)";
            uenum = ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, NULL, &offsetMinus5, &status);
            break;
        case 5:
            api = "ucal_openTimeZoneIDEnumerarion(UCAL_ZONE_TYPE_ANY, US, -5 hour)";
            uenum = ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, "US", &offsetMinus5, &status);
            break;
        }
        if (U_FAILURE(status)) {
            log_err_status(status, "FAIL: %s failed with %s\n", api,
                    u_errorName(status));
        } else {
            const char* id;
            int32_t len;
            count = uenum_count(uenum, &status);
            log_verbose("%s returned %d timezone id's:\n", api, count);
            if (count < 5) { /* Don't hard code an exact == test here! */
                log_data_err("FAIL: in %s, got %d, expected at least 5 -> %s (Are you missing data?)\n", api, count, u_errorName(status));
            }
            uenum_reset(uenum, &status);    
            if (U_FAILURE(status)){
                log_err("FAIL: uenum_reset for %s returned %s\n",
                        api, u_errorName(status));
            }
            for (i=0; i<count; i++) {
                id = uenum_next(uenum, &len, &status);
                if (U_FAILURE(status)){
                    log_err("FAIL: uenum_next for %s returned %s\n",
                            api, u_errorName(status));
                } else {
                    log_verbose("%s\n", id);
                }
            }
            /* Next one should be NULL */
            id = uenum_next(uenum, &len, &status);
            if (id != NULL) {
                log_err("FAIL: uenum_next for %s returned %s, expected NULL\n",
                        api, id);
            }
        }
        uenum_close(uenum);
    }

    /*Test ucal_getDSTSavings*/
    status = U_ZERO_ERROR;
    i = ucal_getDSTSavings(fgGMTID, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: ucal_getDSTSavings(GMT) => %s\n",
                u_errorName(status));
    } else if (i != 0) {
        log_data_err("FAIL: ucal_getDSTSavings(GMT) => %d, expect 0 (Are you missing data?)\n", i);
    }
    i = ucal_getDSTSavings(PST, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: ucal_getDSTSavings(PST) => %s\n",
                u_errorName(status));
    } else if (i != 1*60*60*1000) {
        log_err("FAIL: ucal_getDSTSavings(PST) => %d, expect %d\n", i, 1*60*60*1000);
    }

    /*Test ucal_set/getDefaultTimeZone and ucal_getHostTimeZone */
    status = U_ZERO_ERROR;
    i = ucal_getDefaultTimeZone(zone1, UPRV_LENGTHOF(zone1), &status);
    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) {
        log_err("FAIL: ucal_getDefaultTimeZone() => %s\n",
                u_errorName(status));
    } else {
        ucal_setDefaultTimeZone(EUROPE_PARIS, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL: ucal_setDefaultTimeZone(Europe/Paris) => %s\n",
                    u_errorName(status));
        } else {
            i = ucal_getDefaultTimeZone(zone2, UPRV_LENGTHOF(zone2), &status);
            if (U_FAILURE(status)) {
                log_err("FAIL: ucal_getDefaultTimeZone() => %s\n",
                        u_errorName(status));
            } else {
                if (u_strcmp(zone2, EUROPE_PARIS) != 0) {
                    log_data_err("FAIL: ucal_getDefaultTimeZone() did not return Europe/Paris (Are you missing data?)\n");
                } else {
                    // Redetect the host timezone, it should be the same as zone1 even though ICU's default timezone has been changed.
                    i = ucal_getHostTimeZone(zone2, UPRV_LENGTHOF(zone2), &status);
                    if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) {
                        log_err("FAIL: ucal_getHostTimeZone() => %s\n", u_errorName(status));
                    } else {
                        if (u_strcmp(zone1, zone2) != 0) {
                            log_err("FAIL: ucal_getHostTimeZone() should give the same host timezone even if the default changed. (Got '%s', Expected '%s').\n",
                                u_austrcpy(tempMsgBuf, zone2), u_austrcpy(tempMsgBuf2, zone1));
                        }
                    }
                }
            }
        }
        status = U_ZERO_ERROR;
        ucal_setDefaultTimeZone(zone1, &status);
    }
    
    /*Test ucal_getTZDataVersion*/
    status = U_ZERO_ERROR;
    tzver = ucal_getTZDataVersion(&status);
    if (U_FAILURE(status)) {
        log_err_status(status, "FAIL: ucal_getTZDataVersion() => %s\n", u_errorName(status));
    } else {
        tzverLen = uprv_strlen(tzver);
        if (tzverLen == 5 || tzverLen == 6 /* 4 digits + 1 or 2 letters */) {
            log_verbose("PASS: ucal_getTZDataVersion returned %s\n", tzver);
        } else {
            log_err("FAIL: Bad version string was returned by ucal_getTZDataVersion\n");
        }
    }
    
    /*Testing ucal_getCanonicalTimeZoneID*/
    status = U_ZERO_ERROR;
    resultlength = ucal_getCanonicalTimeZoneID(PST, -1,
        canonicalID, UPRV_LENGTHOF(canonicalID), &isSystemID, &status);
    if (U_FAILURE(status)) {
        log_data_err("FAIL: error in ucal_getCanonicalTimeZoneID : %s\n", u_errorName(status));
    } else {
        if (u_strcmp(AMERICA_LOS_ANGELES, canonicalID) != 0) {
            log_data_err("FAIL: ucal_getCanonicalTimeZoneID(%s) returned %s : expected - %s (Are you missing data?)\n",
                PST, canonicalID, AMERICA_LOS_ANGELES);
        }
        if (!isSystemID) {
            log_data_err("FAIL: ucal_getCanonicalTimeZoneID(%s) set %d to isSystemID (Are you missing data?)\n",
                PST, isSystemID);
        }
    }

    /*Testing the  ucal_open() function*/
    status = U_ZERO_ERROR;
    log_verbose("\nTesting the ucal_open()\n");
    u_uastrcpy(tzID, "PST");
    caldef=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_TRADITIONAL, &status);
    if(U_FAILURE(status)){
        log_data_err("FAIL: error in ucal_open caldef : %s\n - (Are you missing data?)", u_errorName(status));
    }
    
    caldef2=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_TRADITIONAL, &status);
    if(U_FAILURE(status)){
        log_data_err("FAIL: error in ucal_open caldef : %s - (Are you missing data?)\n", u_errorName(status));
    }
    u_strcpy(tzID, fgGMTID);
    calfr=ucal_open(tzID, u_strlen(tzID), "fr_FR", UCAL_TRADITIONAL, &status);
    if(U_FAILURE(status)){
        log_data_err("FAIL: error in ucal_open calfr : %s - (Are you missing data?)\n", u_errorName(status));
    }
    calit=ucal_open(tzID, u_strlen(tzID), "it_IT", UCAL_TRADITIONAL, &status);
    if(U_FAILURE(status))    {
        log_data_err("FAIL: error in ucal_open calit : %s - (Are you missing data?)\n", u_errorName(status));
    }

    /*Testing the  clone() function*/
    calfrclone = ucal_clone(calfr, &status);
    if(U_FAILURE(status)){
        log_data_err("FAIL: error in ucal_clone calfr : %s - (Are you missing data?)\n", u_errorName(status));
    }
    
    /*Testing udat_getAvailable() and udat_countAvailable()*/ 
    log_verbose("\nTesting getAvailableLocales and countAvailable()\n");
    count=ucal_countAvailable();
    /* use something sensible w/o hardcoding the count */
    if(count > 0) {
        log_verbose("PASS: ucal_countAvailable() works fine\n");
        log_verbose("The no: of locales for which calendars are available are %d\n", count);
    } else {
        log_data_err("FAIL: Error in countAvailable()\n");
    }

    for(i=0;i<count;i++) {
       log_verbose("%s\n", ucal_getAvailable(i)); 
    }
    

    /*Testing the equality between calendar's*/
    log_verbose("\nTesting ucal_equivalentTo()\n");
    if(caldef && caldef2 && calfr && calit) { 
      if(ucal_equivalentTo(caldef, caldef2) == false || ucal_equivalentTo(caldef, calfr)== true || 
        ucal_equivalentTo(caldef, calit)== true || ucal_equivalentTo(calfr, calfrclone) == false) {
          log_data_err("FAIL: Error. equivalentTo test failed (Are you missing data?)\n");
      } else {
          log_verbose("PASS: equivalentTo test passed\n");
      }
    }
    

    /*Testing the current time and date using ucal_getnow()*/
    log_verbose("\nTesting the ucal_getNow function to check if it is fetching tbe current time\n");
    now=ucal_getNow();
    /* open the date format and format the date to check the output */
    datdef=udat_open(UDAT_FULL,UDAT_FULL ,NULL, NULL, 0,NULL,0,&status);
    if(U_FAILURE(status)){
        log_data_err("FAIL: error in creating the dateformat : %s (Are you missing data?)\n", u_errorName(status));
        ucal_close(caldef2);
        ucal_close(calfr);
        ucal_close(calit);
        ucal_close(calfrclone);
        ucal_close(caldef);
        return;
    }
    log_verbose("PASS: The current date and time fetched is %s\n", u_austrcpy(tempMsgBuf, myDateFormat(datdef, now)) );
    
    
    /*Testing the TimeZoneDisplayName */
    log_verbose("\nTesting the fetching of time zone display name\n");
    /*for the US locale */
    resultlength=0;
    resultlengthneeded=ucal_getTimeZoneDisplayName(caldef, UCAL_DST, "en_US", NULL, resultlength, &status);
    
    if(status==U_BUFFER_OVERFLOW_ERROR)
    {
        status=U_ZERO_ERROR;
        resultlength=resultlengthneeded+1;
        result=(UChar*)malloc(sizeof(UChar) * resultlength);
        ucal_getTimeZoneDisplayName(caldef, UCAL_DST, "en_US", result, resultlength, &status);
    }
    if(U_FAILURE(status))    {
        log_err("FAIL: Error in getting the timezone display name : %s\n", u_errorName(status));
    }
    else{    
        log_verbose("PASS: getting the time zone display name successful : %s, %d needed \n",
            u_errorName(status), resultlengthneeded);
    }
    

#define expectPDT "Pacific Daylight Time"

    tzdname=(UChar*)malloc(sizeof(UChar) * (sizeof(expectPDT)+1));
    u_uastrcpy(tzdname, expectPDT);
    if(u_strcmp(tzdname, result)==0){
        log_verbose("PASS: got the correct time zone display name %s\n", u_austrcpy(tempMsgBuf, result) );
    }
    else{
        log_err("FAIL: got the wrong time zone(DST) display name %s, wanted %s\n", austrdup(result) , expectPDT);
    }
    
    ucal_getTimeZoneDisplayName(caldef, UCAL_SHORT_DST, "en_US", result, resultlength, &status);
    u_uastrcpy(tzdname, "PDT");
    if(u_strcmp(tzdname, result) != 0){
        log_err("FAIL: got the wrong time zone(SHORT_DST) display name %s, wanted %s\n", austrdup(result), austrdup(tzdname));
    }

    ucal_getTimeZoneDisplayName(caldef, UCAL_STANDARD, "en_US", result, resultlength, &status);
    u_uastrcpy(tzdname, "Pacific Standard Time");
    if(u_strcmp(tzdname, result) != 0){
        log_err("FAIL: got the wrong time zone(STANDARD) display name %s, wanted %s\n", austrdup(result), austrdup(tzdname));
    }

    ucal_getTimeZoneDisplayName(caldef, UCAL_SHORT_STANDARD, "en_US", result, resultlength, &status);
    u_uastrcpy(tzdname, "PST");
    if(u_strcmp(tzdname, result) != 0){
        log_err("FAIL: got the wrong time zone(SHORT_STANDARD) display name %s, wanted %s\n", austrdup(result), austrdup(tzdname));
    }
    
    
    /*testing the setAttributes and getAttributes of a UCalendar*/
    log_verbose("\nTesting the getAttributes and set Attributes\n");
    count=ucal_getAttribute(calit, UCAL_LENIENT);
    count2=ucal_getAttribute(calfr, UCAL_LENIENT);
    ucal_setAttribute(calit, UCAL_LENIENT, 0);
    ucal_setAttribute(caldef, UCAL_LENIENT, count2);
    if( ucal_getAttribute(calit, UCAL_LENIENT) !=0 || 
        ucal_getAttribute(calfr, UCAL_LENIENT)!=ucal_getAttribute(caldef, UCAL_LENIENT) )
        log_err("FAIL: there is an error in getAttributes or setAttributes\n");
    else
        log_verbose("PASS: attribute set and got successfully\n");
        /*set it back to original value */
    log_verbose("Setting it back to normal\n");
    ucal_setAttribute(calit, UCAL_LENIENT, count);
    if(ucal_getAttribute(calit, UCAL_LENIENT)!=count)
        log_err("FAIL: Error in setting the attribute back to normal\n");
    
    /*setting the first day of the week to other values  */
    count=ucal_getAttribute(calit, UCAL_FIRST_DAY_OF_WEEK);
    for (i=1; i<=7; ++i) {
        ucal_setAttribute(calit, UCAL_FIRST_DAY_OF_WEEK,i);
        if (ucal_getAttribute(calit, UCAL_FIRST_DAY_OF_WEEK) != i) 
            log_err("FAIL: set/getFirstDayOfWeek failed\n");
    }
    /*get bogus Attribute*/
    count=ucal_getAttribute(calit, (UCalendarAttribute)99); /* BOGUS_ATTRIBUTE */
    if(count != -1){
        log_err("FAIL: get/bogus attribute should return -1\n");
    }

    /*set it back to normal */
    ucal_setAttribute(calit, UCAL_FIRST_DAY_OF_WEEK,count);
    /*setting minimal days of the week to other values */
    count=ucal_getAttribute(calit, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);
    for (i=1; i<=7; ++i) {
        ucal_setAttribute(calit, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK,i);
        if (ucal_getAttribute(calit, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK) != i) 
            log_err("FAIL: set/getMinimalDaysInFirstWeek failed\n");
    }
    /*set it back to normal */
    ucal_setAttribute(calit, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK,count);
    
    
    /*testing if the UCalendar's timezone is currently in day light saving's time*/
    log_verbose("\nTesting if the UCalendar is currently in daylight saving's time\n");
    ucal_setDateTime(caldef, 1999, UCAL_MARCH, 3, 10, 45, 20, &status); 
    ucal_inDaylightTime(caldef, &status );
    if(U_FAILURE(status))    {
        log_err("Error in ucal_inDaylightTime: %s\n", u_errorName(status));
    }
    if(!ucal_inDaylightTime(caldef, &status))
        log_verbose("PASS: It is not in daylight saving's time\n");
    else
        log_err("FAIL: It is not in daylight saving's time\n");

    /*closing the UCalendar*/
    ucal_close(caldef);
    ucal_close(caldef2);
    ucal_close(calfr);
    ucal_close(calit);
    ucal_close(calfrclone);
    
    /*testing ucal_getType, and ucal_open with UCAL_GREGORIAN*/
    for (ucalGetTypeTestPtr = ucalGetTypeTests; ucalGetTypeTestPtr->expectedResult != NULL; ++ucalGetTypeTestPtr) {
        const char * localeToDisplay = (ucalGetTypeTestPtr->locale != NULL)? ucalGetTypeTestPtr->locale: "<NULL>";
        status = U_ZERO_ERROR;
        caldef = ucal_open(NULL, 0, ucalGetTypeTestPtr->locale, ucalGetTypeTestPtr->calType, &status);
        if ( U_SUCCESS(status) ) {
            const char * calType = ucal_getType(caldef, &status);
            if ( U_SUCCESS(status) && calType != NULL ) {
                if ( uprv_strcmp( calType, ucalGetTypeTestPtr->expectedResult ) != 0 ) {
                    log_err("FAIL: ucal_open %s type %d does not return %s calendar\n", localeToDisplay,
                                                ucalGetTypeTestPtr->calType, ucalGetTypeTestPtr->expectedResult);
                }
            } else {
                log_err("FAIL: ucal_open %s type %d, then ucal_getType fails\n", localeToDisplay, ucalGetTypeTestPtr->calType);
            }
            ucal_close(caldef);
        } else {
            log_err("FAIL: ucal_open %s type %d fails\n", localeToDisplay, ucalGetTypeTestPtr->calType);
        }
    }

    /*closing the UDateFormat used */
    udat_close(datdef);
    free(result);
    free(tzdname);
}

/*------------------------------------------------------*/
/*Testing the getMillis, setMillis, setDate and setDateTime functions extensively*/

static void TestGetSetDateAPI(void)
{
    UCalendar *caldef = 0, *caldef2 = 0, *caldef3 = 0;
    UChar tzID[4];
    UDate d1;
    int32_t hour;
    int32_t zoneOffset;
    UDateFormat *datdef = 0;
    UErrorCode status=U_ZERO_ERROR;
    UDate d2= 837039928046.0;
    UChar temp[30];
	double testMillis;
	int32_t dateBit;
    UChar id[4];
    int32_t idLen;

    log_verbose("\nOpening the calendars()\n");
    u_strcpy(tzID, fgGMTID);
    /*open the calendars used */
    caldef=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_TRADITIONAL, &status);
    caldef2=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_TRADITIONAL, &status);
    caldef3=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_TRADITIONAL, &status);
    /*open the dateformat */
    /* this is supposed to open default date format, but later on it treats it like it is "en_US" 
       - very bad if you try to run the tests on machine where default locale is NOT "en_US" */
    /*datdef=udat_open(UDAT_DEFAULT,UDAT_DEFAULT ,NULL,fgGMTID,-1, &status);*/
    datdef=udat_open(UDAT_DEFAULT,UDAT_DEFAULT ,"en_US",fgGMTID,-1,NULL,0, &status);
    if(U_FAILURE(status))
    {
        log_data_err("error in creating the dateformat : %s (Are you missing data?)\n", u_errorName(status));
        ucal_close(caldef);
        ucal_close(caldef2);
        ucal_close(caldef3);
        udat_close(datdef);
        return;
    }

    /*Testing getMillis and setMillis */
    log_verbose("\nTesting the date and time fetched in millis for a calendar using getMillis\n");
    d1=ucal_getMillis(caldef, &status);
    if(U_FAILURE(status)){
        log_err("Error in getMillis : %s\n", u_errorName(status));
    }
    
    /*testing setMillis */
    log_verbose("\nTesting the set date and time function using setMillis\n");
    ucal_setMillis(caldef, d2, &status);
    if(U_FAILURE(status)){
        log_err("Error in setMillis : %s\n", u_errorName(status));
    }

    /*testing if the calendar date is set properly or not  */
    d1=ucal_getMillis(caldef, &status);
    if(u_strcmp(myDateFormat(datdef, d1), myDateFormat(datdef, d2))!=0)
        log_err("error in setMillis or getMillis\n");
    /*-------------------*/
    
    /*testing large negative millis*/
	/*test a previously failed millis and beyond the lower bounds - ICU trac #9403 */
	// -184303902611600000.0         - just beyond lower bounds (#9403 sets U_ILLEGAL_ARGUMENT_ERROR in strict mode)
	// -46447814188001000.0	         - fixed by #9403

    log_verbose("\nTesting very large valid millis & invalid setMillis values (in both strict & lienent modes) detected\n");

	testMillis = -46447814188001000.0;	// point where floorDivide in handleComputeFields failed as per #9403
	log_verbose("using value[%lf]\n", testMillis);
    ucal_setAttribute(caldef3, UCAL_LENIENT, 0);
    ucal_setMillis(caldef3, testMillis, &status);
   	if(U_FAILURE(status)){
   		log_err("Fail: setMillis incorrectly detected invalid value : for millis : %e : returned  : %s\n", testMillis, u_errorName(status));
		status = U_ZERO_ERROR;
	}

    log_verbose("\nTesting invalid setMillis values detected\n");
	testMillis = -184303902611600000.0;
	log_verbose("using value[%lf]\n", testMillis);
    ucal_setAttribute(caldef3, UCAL_LENIENT, 1);
   	ucal_setMillis(caldef3, testMillis, &status);
   	if(U_FAILURE(status)){
   		log_err("Fail: setMillis incorrectly detected invalid value : for millis : %e : returned  : %s\n", testMillis, u_errorName(status));
		status = U_ZERO_ERROR;
   	} else {
        dateBit = ucal_get(caldef2, UCAL_MILLISECOND, &status);
        if(testMillis == dateBit)
        {
		    log_err("Fail: error in setMillis, allowed invalid value %e : returns millisecond : %d", testMillis, dateBit);
        } else {
            log_verbose("Pass: setMillis correctly pinned min, returned : %d", dateBit);
        }
	}

    log_verbose("\nTesting invalid setMillis values detected\n");
	testMillis = -184303902611600000.0;
	log_verbose("using value[%lf]\n", testMillis);
    ucal_setAttribute(caldef3, UCAL_LENIENT, 0);
   	ucal_setMillis(caldef3, testMillis, &status);
   	if(U_FAILURE(status)){
   		log_verbose("Pass: Illegal argument error as expected : for millis : %e : returned  : %s\n", testMillis, u_errorName(status));
		status = U_ZERO_ERROR;
   	} else {
		dateBit = ucal_get(caldef3, UCAL_DAY_OF_MONTH, &status);
		log_err("Fail: error in setMillis, allowed invalid value %e : returns DayOfMonth : %d", testMillis, dateBit);
	}
	/*-------------------*/
    
    
    ctest_setTimeZone(NULL, &status);

    /*testing ucal_setTimeZone() and ucal_getTimeZoneID function*/
    log_verbose("\nTesting if the function ucal_setTimeZone() and ucal_getTimeZoneID work fine\n");
    idLen = ucal_getTimeZoneID(caldef2, id, UPRV_LENGTHOF(id), &status);
    (void)idLen;    /* Suppress set but not used warning. */
    if (U_FAILURE(status)) {
        log_err("Error in getTimeZoneID : %s\n", u_errorName(status));
    } else if (u_strcmp(id, fgGMTID) != 0) {
        log_err("FAIL: getTimeZoneID returns a wrong ID: actual=%d, expected=%s\n", austrdup(id), austrdup(fgGMTID));
    } else {
        log_verbose("PASS: getTimeZoneID works fine\n");
    }

    ucal_setMillis(caldef2, d2, &status); 
    if(U_FAILURE(status)){
        log_err("Error in getMillis : %s\n", u_errorName(status));
    }
    hour=ucal_get(caldef2, UCAL_HOUR_OF_DAY, &status);
        
    u_uastrcpy(tzID, "PST");
    ucal_setTimeZone(caldef2,tzID, 3, &status);
    if(U_FAILURE(status)){
        log_err("Error in setting the time zone using ucal_setTimeZone(): %s\n", u_errorName(status));
    }
    else
        log_verbose("ucal_setTimeZone worked fine\n");

    idLen = ucal_getTimeZoneID(caldef2, id, UPRV_LENGTHOF(id), &status);
    if (U_FAILURE(status)) {
        log_err("Error in getTimeZoneID : %s\n", u_errorName(status));
    } else if (u_strcmp(id, tzID) != 0) {
        log_err("FAIL: getTimeZoneID returns a wrong ID: actual=%d, expected=%s\n", austrdup(id), austrdup(tzID));
    } else {
        log_verbose("PASS: getTimeZoneID works fine\n");
    }

    if(hour == ucal_get(caldef2, UCAL_HOUR_OF_DAY, &status))
        log_err("FAIL: Error setting the time zone doesn't change the represented time\n");
    else if((hour-8 + 1) != ucal_get(caldef2, UCAL_HOUR_OF_DAY, &status)) /*because it is not in daylight savings time */
        log_err("FAIL: Error setTimeZone doesn't change the represented time correctly with 8 hour offset\n");
    else
        log_verbose("PASS: setTimeZone works fine\n");
        
    /*testing setTimeZone roundtrip */
    log_verbose("\nTesting setTimeZone() roundtrip\n");
    u_strcpy(tzID, fgGMTID);
    ucal_setTimeZone(caldef2, tzID, 3, &status);
    if(U_FAILURE(status)){
        log_err("Error in setting the time zone using ucal_setTimeZone(): %s\n", u_errorName(status));
    }
    if(d2==ucal_getMillis(caldef2, &status))
        log_verbose("PASS: setTimeZone roundtrip test passed\n");
    else
        log_err("FAIL: setTimeZone roundtrip test failed\n");

    zoneOffset = ucal_get(caldef2, UCAL_ZONE_OFFSET, &status);
    if(U_FAILURE(status)){
        log_err("Error in getting the time zone using ucal_get() after using ucal_setTimeZone(): %s\n", u_errorName(status));
    }
    else if (zoneOffset != 0) {
        log_err("Error in getting the time zone using ucal_get() after using ucal_setTimeZone() offset=%d\n", zoneOffset);
    }

    ucal_setTimeZone(caldef2, NULL, -1, &status);
    if(U_FAILURE(status)){
        log_err("Error in setting the time zone using ucal_setTimeZone(): %s\n", u_errorName(status));
    }
    if(ucal_getMillis(caldef2, &status))
        log_verbose("PASS: setTimeZone roundtrip test passed\n");
    else
        log_err("FAIL: setTimeZone roundtrip test failed\n");

    zoneOffset = ucal_get(caldef2, UCAL_ZONE_OFFSET, &status);
    if(U_FAILURE(status)){
        log_err("Error in getting the time zone using ucal_get() after using ucal_setTimeZone(): %s\n", u_errorName(status));
    }
    else if (zoneOffset != -28800000) {
        log_err("Error in getting the time zone using ucal_get() after using ucal_setTimeZone() offset=%d\n", zoneOffset);
    }

    ctest_resetTimeZone();

/*----------------------------*     */
    
    

    /*Testing  if setDate works fine  */
    log_verbose("\nTesting the ucal_setDate() function \n");
    u_strcpy(temp, u"Dec 17, 1971, 11:05:28\u202FPM");
    ucal_setDate(caldef,1971, UCAL_DECEMBER, 17, &status);
    if(U_FAILURE(status)){
        log_err("error in setting the calendar date : %s\n", u_errorName(status));
    }
    /*checking if the calendar date is set properly or not  */
    d1=ucal_getMillis(caldef, &status);
    if(u_strcmp(myDateFormat(datdef, d1), temp)==0)
        log_verbose("PASS:setDate works fine\n");
    else
        log_err("FAIL:Error in setDate()\n");

    
    /* Testing setDate Extensively with various input values */
    log_verbose("\nTesting ucal_setDate() extensively\n");
    ucal_setDate(caldef, 1999, UCAL_JANUARY, 10, &status);
    verify1("1999  10th day of January  is :", caldef, datdef, 1999, UCAL_JANUARY, 10);
    ucal_setDate(caldef, 1999, UCAL_DECEMBER, 3, &status);
    verify1("1999 3rd day of December  is :", caldef, datdef, 1999, UCAL_DECEMBER, 3);
    ucal_setDate(caldef, 2000, UCAL_MAY, 3, &status);
    verify1("2000 3rd day of May is :", caldef, datdef, 2000, UCAL_MAY, 3);
    ucal_setDate(caldef, 1999, UCAL_AUGUST, 32, &status);
    verify1("1999 32th day of August is :", caldef, datdef, 1999, UCAL_SEPTEMBER, 1);
    ucal_setDate(caldef, 1999, UCAL_MARCH, 0, &status); 
    verify1("1999 0th day of March is :", caldef, datdef, 1999, UCAL_FEBRUARY, 28);
    ucal_setDate(caldef, 0, UCAL_MARCH, 12, &status);
    
    /*--------------------*/

    /*Testing if setDateTime works fine */
    log_verbose("\nTesting the ucal_setDateTime() function \n");
    u_strcpy(temp, u"May 3, 1972, 4:30:42\u202FPM");
    ucal_setDateTime(caldef,1972, UCAL_MAY, 3, 16, 30, 42, &status);
    if(U_FAILURE(status)){
        log_err("error in setting the calendar date : %s\n", u_errorName(status));
    }
    /*checking  if the calendar date is set properly or not  */
    d1=ucal_getMillis(caldef, &status);
    if(u_strcmp(myDateFormat(datdef, d1), temp)==0)
        log_verbose("PASS: setDateTime works fine\n");
    else
        log_err("FAIL: Error in setDateTime\n");

    
    
    /*Testing setDateTime extensively with various input values*/
    log_verbose("\nTesting ucal_setDateTime() function extensively\n");
    ucal_setDateTime(caldef, 1999, UCAL_OCTOBER, 10, 6, 45, 30, &status);
    verify2("1999  10th day of October  at 6:45:30 is :", caldef, datdef, 1999, UCAL_OCTOBER, 10, 6, 45, 30, 0 );
    ucal_setDateTime(caldef, 1999, UCAL_MARCH, 3, 15, 10, 55, &status);
    verify2("1999 3rd day of March   at 15:10:55 is :", caldef, datdef, 1999, UCAL_MARCH, 3, 3, 10, 55, 1);
    ucal_setDateTime(caldef, 1999, UCAL_MAY, 3, 25, 30, 45, &status);
    verify2("1999 3rd day of May at 25:30:45 is :", caldef, datdef, 1999, UCAL_MAY, 4, 1, 30, 45, 0);
    ucal_setDateTime(caldef, 1999, UCAL_AUGUST, 32, 22, 65, 40, &status);
    verify2("1999 32th day of August at 22:65:40 is :", caldef, datdef, 1999, UCAL_SEPTEMBER, 1, 11, 5, 40,1);
    ucal_setDateTime(caldef, 1999, UCAL_MARCH, 12, 0, 0, 0,&status);
    verify2("1999 12th day of March at 0:0:0 is :", caldef, datdef, 1999, UCAL_MARCH, 12, 0, 0, 0, 0);
    ucal_setDateTime(caldef, 1999, UCAL_MARCH, 12, -10, -10,0, &status);
    verify2("1999 12th day of March is at -10:-10:0 :", caldef, datdef, 1999, UCAL_MARCH, 11, 1, 50, 0, 1);
    
    
    
    /*close caldef and datdef*/
    ucal_close(caldef);
    ucal_close(caldef2);
    ucal_close(caldef3);
    udat_close(datdef);
}

/*----------------------------------------------------------- */
/**
 * Confirm the functioning of the calendar field related functions.
 */
static void TestFieldGetSet(void)
{
    UCalendar *cal = 0;
    UChar tzID[4];
    UDateFormat *datdef = 0;
    UDate d1 = 0;
    UErrorCode status=U_ZERO_ERROR;
    (void)d1;   /* Suppress set but not used warning. */
    log_verbose("\nFetching pointer to UCalendar using the ucal_open()\n");
    u_strcpy(tzID, fgGMTID);
    /*open the calendar used */
    cal=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_TRADITIONAL, &status);
    if (U_FAILURE(status)) {
        log_data_err("ucal_open failed: %s - (Are you missing data?)\n", u_errorName(status));
        return; 
    }
    datdef=udat_open(UDAT_SHORT,UDAT_SHORT ,NULL,fgGMTID,-1,NULL, 0, &status);
    if(U_FAILURE(status))
    {
        log_data_err("error in creating the dateformat : %s (Are you missing data?)\n", u_errorName(status));
    }
    
    /*Testing ucal_get()*/
    log_verbose("\nTesting the ucal_get() function of Calendar\n");
    ucal_setDateTime(cal, 1999, UCAL_MARCH, 12, 5, 25, 30, &status);
    if(U_FAILURE(status)){
        log_data_err("error in the setDateTime() : %s (Are you missing data?)\n", u_errorName(status));
    }
    if(ucal_get(cal, UCAL_YEAR, &status)!=1999 || ucal_get(cal, UCAL_MONTH, &status)!=2 || 
        ucal_get(cal, UCAL_DATE, &status)!=12 || ucal_get(cal, UCAL_HOUR, &status)!=5)
        log_data_err("error in ucal_get() -> %s (Are you missing data?)\n", u_errorName(status));    
    else if(ucal_get(cal, UCAL_DAY_OF_WEEK_IN_MONTH, &status)!=2 || ucal_get(cal, UCAL_DAY_OF_WEEK, &status)!=6
        || ucal_get(cal, UCAL_WEEK_OF_MONTH, &status)!=2 || ucal_get(cal, UCAL_WEEK_OF_YEAR, &status)!= 11)
        log_err("FAIL: error in ucal_get()\n");
    else
        log_verbose("PASS: ucal_get() works fine\n");

    /*Testing the ucal_set() , ucal_clear() functions of calendar*/
    log_verbose("\nTesting the set, and clear  field functions of calendar\n");
    ucal_setAttribute(cal, UCAL_LENIENT, 0);
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1997);
    ucal_set(cal, UCAL_MONTH, UCAL_JUNE);
    ucal_set(cal, UCAL_DATE, 3);
    verify1("1997 third day of June = ", cal, datdef, 1997, UCAL_JUNE, 3);
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1997);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    ucal_set(cal, UCAL_MONTH, UCAL_JUNE);
    ucal_set(cal, UCAL_DAY_OF_WEEK_IN_MONTH, 1);
    verify1("1997 first Tuesday in June = ", cal, datdef, 1997, UCAL_JUNE, 3);
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1997);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    ucal_set(cal, UCAL_MONTH, UCAL_JUNE);
    ucal_set(cal, UCAL_DAY_OF_WEEK_IN_MONTH, - 1);
    verify1("1997 last Tuesday in June = ", cal, datdef,1997,   UCAL_JUNE, 24);
    /*give undesirable input    */
    status = U_ZERO_ERROR;
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1997);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    ucal_set(cal, UCAL_MONTH, UCAL_JUNE);
    ucal_set(cal, UCAL_DAY_OF_WEEK_IN_MONTH, 0);
    d1 = ucal_getMillis(cal, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) { 
        log_err("FAIL: U_ILLEGAL_ARGUMENT_ERROR was not returned for : 1997 zero-th Tuesday in June\n");
    } else {
        log_verbose("PASS: U_ILLEGAL_ARGUMENT_ERROR as expected\n");
    }
    status = U_ZERO_ERROR;
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1997);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    ucal_set(cal, UCAL_MONTH, UCAL_JUNE);
    ucal_set(cal, UCAL_WEEK_OF_MONTH, 1);
    verify1("1997 Tuesday in week 1 of June = ", cal,datdef, 1997, UCAL_JUNE, 3);
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1997);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    ucal_set(cal, UCAL_MONTH, UCAL_JUNE);
    ucal_set(cal, UCAL_WEEK_OF_MONTH, 5);
    verify1("1997 Tuesday in week 5 of June = ", cal,datdef, 1997, UCAL_JULY, 1);
    status = U_ZERO_ERROR;
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1997);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    ucal_set(cal, UCAL_MONTH, UCAL_JUNE);
    ucal_set(cal, UCAL_WEEK_OF_MONTH, 0);
    ucal_setAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK,1);
    d1 = ucal_getMillis(cal,&status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR){ 
        log_err("FAIL: U_ILLEGAL_ARGUMENT_ERROR was not returned for : 1997 Tuesday zero-th week in June\n");
    } else {
        log_verbose("PASS: U_ILLEGAL_ARGUMENT_ERROR as expected\n");
    }
    status = U_ZERO_ERROR;
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR_WOY, 1997);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    ucal_set(cal, UCAL_WEEK_OF_YEAR, 1);
    verify1("1997 Tuesday in week 1 of year = ", cal, datdef,1996, UCAL_DECEMBER, 31);
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1997);
    ucal_set(cal, UCAL_DAY_OF_WEEK, UCAL_TUESDAY);
    ucal_set(cal, UCAL_WEEK_OF_YEAR, 10);
    verify1("1997 Tuesday in week 10 of year = ", cal,datdef, 1997, UCAL_MARCH, 4);
    ucal_clear(cal);
    ucal_set(cal, UCAL_YEAR, 1999);
    ucal_set(cal, UCAL_DAY_OF_YEAR, 1);
    verify1("1999 1st day of the year =", cal, datdef, 1999, UCAL_JANUARY, 1);
    ucal_set(cal, UCAL_MONTH, -3);
    d1 = ucal_getMillis(cal,&status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR){ 
        log_err("FAIL: U_ILLEGAL_ARGUMENT_ERROR was not returned for : 1999 -3th month\n");
    } else {
        log_verbose("PASS: U_ILLEGAL_ARGUMENT_ERROR as expected\n");
    }

    ucal_setAttribute(cal, UCAL_LENIENT, 1);
    
    ucal_set(cal, UCAL_MONTH, -3);
    verify1("1999 -3th month should be", cal, datdef, 1998, UCAL_OCTOBER, 1);


    /*testing isSet and clearField()*/
    if(!ucal_isSet(cal, UCAL_WEEK_OF_YEAR))
        log_err("FAIL: error in isSet\n");
    else
        log_verbose("PASS: isSet working fine\n");
    ucal_clearField(cal, UCAL_WEEK_OF_YEAR);
    if(ucal_isSet(cal, UCAL_WEEK_OF_YEAR))
        log_err("FAIL: there is an error in clearField or isSet\n");
    else
        log_verbose("PASS :clearField working fine\n");

    /*-------------------------------*/

    ucal_close(cal);
    udat_close(datdef);
}
 
typedef struct {
    const char * zone;
    int32_t      year;
    int32_t      month;
    int32_t      day;
    int32_t      hour;
} TransitionItem;

static const TransitionItem transitionItems[] = {
    { "America/Caracas", 2007, UCAL_DECEMBER,  8, 10 }, /* day before change in UCAL_ZONE_OFFSET */
    { "US/Pacific",      2011,    UCAL_MARCH, 12, 10 }, /* day before change in UCAL_DST_OFFSET */
    { NULL,                 0,             0,  0,  0 }
};

/* ------------------------------------- */
/**
 * Execute adding and rolling in Calendar extensively,
 */
static void TestAddRollExtensive(void)
{
    const TransitionItem * itemPtr;
    UCalendar *cal = 0;
    int32_t i,limit;
    UChar tzID[32];
    UCalendarDateFields e;
    int32_t y,m,d,hr,min,sec,ms;
    int32_t maxlimit = 40;
    UErrorCode status = U_ZERO_ERROR;
    y = 1997; m = UCAL_FEBRUARY; d = 1; hr = 1; min = 1; sec = 0; ms = 0;
   
    log_verbose("Testing add and roll extensively\n");
    
    u_uastrcpy(tzID, "PST");
    /*open the calendar used */
    cal=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_GREGORIAN, &status);
    if (U_FAILURE(status)) {
        log_data_err("ucal_open() failed : %s - (Are you missing data?)\n", u_errorName(status)); 
        return; 
    }

    ucal_set(cal, UCAL_YEAR, y);
    ucal_set(cal, UCAL_MONTH, m);
    ucal_set(cal, UCAL_DATE, d);
    ucal_setAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK,1);

    /* Confirm that adding to various fields works.*/
    log_verbose("\nTesting to confirm that adding to various fields works with ucal_add()\n");
    checkDate(cal, y, m, d);
    ucal_add(cal,UCAL_YEAR, 1, &status);
    if (U_FAILURE(status)) { log_err("ucal_add failed: %s\n", u_errorName(status)); return; }
    y++;
    checkDate(cal, y, m, d);
    ucal_add(cal,UCAL_MONTH, 12, &status);
    if (U_FAILURE(status)) { log_err("ucal_add failed: %s\n", u_errorName(status) ); return; }
    y+=1;
    checkDate(cal, y, m, d);
    ucal_add(cal,UCAL_DATE, 1, &status);
    if (U_FAILURE(status)) { log_err("ucal_add failed: %s\n", u_errorName(status) ); return; }
    d++;
    checkDate(cal, y, m, d);
    ucal_add(cal,UCAL_DATE, 2, &status);
    if (U_FAILURE(status)) { log_err("ucal_add failed: %s\n", u_errorName(status) ); return; }
    d += 2;
    checkDate(cal, y, m, d);
    ucal_add(cal,UCAL_DATE, 28, &status);
    if (U_FAILURE(status)) { log_err("ucal_add failed: %s\n", u_errorName(status) ); return; }
    ++m;
    checkDate(cal, y, m, d);
    ucal_add(cal, (UCalendarDateFields)-1, 10, &status);
    if(status==U_ILLEGAL_ARGUMENT_ERROR)
        log_verbose("Pass: Illegal argument error as expected\n");
    else{
        log_err("Fail: No, illegal argument error as expected. Got....: %s\n", u_errorName(status));
    }
    status=U_ZERO_ERROR;
    

    /*confirm that applying roll to various fields works fine*/
    log_verbose("\nTesting to confirm that ucal_roll() works\n");
    ucal_roll(cal, UCAL_DATE, -1, &status);
    if (U_FAILURE(status)) { log_err("ucal_roll failed: %s\n", u_errorName(status) ); return; }
    d -=1;
    checkDate(cal, y, m, d);
    ucal_roll(cal, UCAL_MONTH, -2, &status);
    if (U_FAILURE(status)) { log_err("ucal_roll failed: %s\n", u_errorName(status) ); return; }
    m -=2;
    checkDate(cal, y, m, d);
    ucal_roll(cal, UCAL_DATE, 1, &status);
    if (U_FAILURE(status)) { log_err("ucal_roll failed: %s\n", u_errorName(status) ); return; }
    d +=1;
    checkDate(cal, y, m, d);
    ucal_roll(cal, UCAL_MONTH, -12, &status);
    if (U_FAILURE(status)) { log_err("ucal_roll failed: %s\n", u_errorName(status) ); return; }
    checkDate(cal, y, m, d);
    ucal_roll(cal, UCAL_YEAR, -1, &status);
    if (U_FAILURE(status)) { log_err("ucal_roll failed: %s\n", u_errorName(status) ); return; }
    y -=1;
    checkDate(cal, y, m, d);
    ucal_roll(cal, UCAL_DATE, 29, &status);
    if (U_FAILURE(status)) { log_err("ucal_roll failed: %s\n", u_errorName(status) ); return; }
    d = 2;
    checkDate(cal, y, m, d);
    ucal_roll(cal, (UCalendarDateFields)-1, 10, &status);
    if(status==U_ILLEGAL_ARGUMENT_ERROR)
        log_verbose("Pass: illegal argument error as expected\n");
    else{
        log_err("Fail: no illegal argument error got..: %s\n", u_errorName(status));
        return;
    }
    status=U_ZERO_ERROR;
    ucal_clear(cal);
    ucal_setDateTime(cal, 1999, UCAL_FEBRUARY, 28, 10, 30, 45,  &status);
    if(U_FAILURE(status)){
        log_err("error is setting the datetime: %s\n", u_errorName(status));
    }
    ucal_add(cal, UCAL_MONTH, 1, &status);
    checkDate(cal, 1999, UCAL_MARCH, 28);
    ucal_add(cal, UCAL_MILLISECOND, 1000, &status);
    checkDateTime(cal, 1999, UCAL_MARCH, 28, 10, 30, 46, 0, UCAL_MILLISECOND);

    ucal_close(cal);
/*--------------- */
    status=U_ZERO_ERROR;
    /* Testing add and roll extensively */
    log_verbose("\nTesting the ucal_add() and ucal_roll() functions extensively\n");
    y = 1997; m = UCAL_FEBRUARY; d = 1; hr = 1; min = 1; sec = 0; ms = 0;
    cal=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_TRADITIONAL, &status);
    if (U_FAILURE(status)) {
        log_err("ucal_open failed: %s\n", u_errorName(status));
        return; 
    }
    ucal_set(cal, UCAL_YEAR, y);
    ucal_set(cal, UCAL_MONTH, m);
    ucal_set(cal, UCAL_DATE, d);
    ucal_set(cal, UCAL_HOUR, hr);
    ucal_set(cal, UCAL_MINUTE, min);
    ucal_set(cal, UCAL_SECOND,sec);
    ucal_set(cal, UCAL_MILLISECOND, ms);
    ucal_setAttribute(cal, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK,1);
    status=U_ZERO_ERROR;

    log_verbose("\nTesting UCalendar add...\n");
    for(e = UCAL_YEAR;e < UCAL_FIELD_COUNT; e=(UCalendarDateFields)((int32_t)e + 1)) {
        limit = maxlimit;
        status = U_ZERO_ERROR;
        for (i = 0; i < limit; i++) {
            ucal_add(cal, e, 1, &status);
            if (U_FAILURE(status)) { limit = i; status = U_ZERO_ERROR; }
        }
        for (i = 0; i < limit; i++) {
            ucal_add(cal, e, -1, &status);
            if (U_FAILURE(status)) { 
                log_err("ucal_add -1 failed: %s\n", u_errorName(status));
                return; 
            }
        }
        checkDateTime(cal, y, m, d, hr, min, sec, ms, e);
    }
    log_verbose("\nTesting calendar ucal_roll()...\n");
    for(e = UCAL_YEAR;e < UCAL_FIELD_COUNT; e=(UCalendarDateFields)((int32_t)e + 1)) {
        limit = maxlimit;
        status = U_ZERO_ERROR;
        for (i = 0; i < limit; i++) {
            ucal_roll(cal, e, 1, &status);
            if (U_FAILURE(status)) {
                limit = i;
                status = U_ZERO_ERROR;
            }
        }
        for (i = 0; i < limit; i++) {
            ucal_roll(cal, e, -1, &status);
            if (U_FAILURE(status)) { 
                log_err("ucal_roll -1 failed: %s\n", u_errorName(status));
                return; 
            }
        }
        checkDateTime(cal, y, m, d, hr, min, sec, ms, e);
    }

    ucal_close(cal);
/*--------------- */
    log_verbose("\nTesting ucal_add() across ZONE_OFFSET and DST_OFFSE transitions.\n");
    for (itemPtr = transitionItems; itemPtr->zone != NULL; itemPtr++) {
        status=U_ZERO_ERROR;
        u_uastrcpy(tzID, itemPtr->zone);
        cal=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_GREGORIAN, &status);
        if (U_FAILURE(status)) {
            log_err("ucal_open failed for zone %s: %s\n", itemPtr->zone, u_errorName(status));
            continue; 
        }
        ucal_setDateTime(cal, itemPtr->year, itemPtr->month, itemPtr->day, itemPtr->hour, 0, 0, &status);
        ucal_add(cal, UCAL_DATE, 1, &status);
        hr = ucal_get(cal, UCAL_HOUR_OF_DAY, &status);
        if ( U_FAILURE(status) ) {
            log_err("ucal_add failed adding day across transition for zone %s: %s\n", itemPtr->zone, u_errorName(status));
        } else if ( hr != itemPtr->hour ) {
            log_err("ucal_add produced wrong hour %d when adding day across transition for zone %s\n", hr, itemPtr->zone);
        } else {
            ucal_add(cal, UCAL_DATE, -1, &status);
            hr = ucal_get(cal, UCAL_HOUR_OF_DAY, &status);
            if ( U_FAILURE(status) ) {
                log_err("ucal_add failed subtracting day across transition for zone %s: %s\n", itemPtr->zone, u_errorName(status));
            } else if ( hr != itemPtr->hour ) {
                log_err("ucal_add produced wrong hour %d when subtracting day across transition for zone %s\n", hr, itemPtr->zone);
            }
        }
        ucal_close(cal);
    }
}

/*------------------------------------------------------ */
/*Testing the Limits for various Fields of Calendar*/
static void TestGetLimits(void)
{
    UCalendar *cal = 0;
    int32_t min, max, gr_min, le_max, ac_min, ac_max, val;
    UChar tzID[4];
    UErrorCode status = U_ZERO_ERROR;
    
    
    u_uastrcpy(tzID, "PST");
    /*open the calendar used */
    cal=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_GREGORIAN, &status);
    if (U_FAILURE(status)) {
        log_data_err("ucal_open() for gregorian calendar failed in TestGetLimits: %s - (Are you missing data?)\n", u_errorName(status));
        return; 
    }
    
    log_verbose("\nTesting the getLimits function for various fields\n");
    
    
    
    ucal_setDate(cal, 1999, UCAL_MARCH, 5, &status); /* Set the date to be March 5, 1999 */
    val = ucal_get(cal, UCAL_DAY_OF_WEEK, &status);
    min = ucal_getLimit(cal, UCAL_DAY_OF_WEEK, UCAL_MINIMUM, &status);
    max = ucal_getLimit(cal, UCAL_DAY_OF_WEEK, UCAL_MAXIMUM, &status);
    if ( (min != UCAL_SUNDAY || max != UCAL_SATURDAY ) && (min > val && val > max)  && (val != UCAL_FRIDAY)){
           log_err("FAIL: Min/max bad\n");
           log_err("FAIL: Day of week %d out of range\n", val);
           log_err("FAIL: FAIL: Day of week should be SUNDAY Got %d\n", val);
    }
    else
        log_verbose("getLimits successful\n");

    val = ucal_get(cal, UCAL_DAY_OF_WEEK_IN_MONTH, &status);
    min = ucal_getLimit(cal, UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_MINIMUM, &status);
    max = ucal_getLimit(cal, UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_MAXIMUM, &status);
    if ( (min != 0 || max != 5 ) && (min > val && val > max)  && (val != 1)){
           log_err("FAIL: Min/max bad\n");
           log_err("FAIL: Day of week in month %d out of range\n", val);
           log_err("FAIL: FAIL: Day of week in month should be SUNDAY Got %d\n", val);
        
    }
    else
        log_verbose("getLimits successful\n");
    
    min=ucal_getLimit(cal, UCAL_MONTH, UCAL_MINIMUM, &status);
    max=ucal_getLimit(cal, UCAL_MONTH, UCAL_MAXIMUM, &status);
    gr_min=ucal_getLimit(cal, UCAL_MONTH, UCAL_GREATEST_MINIMUM, &status);
    le_max=ucal_getLimit(cal, UCAL_MONTH, UCAL_LEAST_MAXIMUM, &status);
    ac_min=ucal_getLimit(cal, UCAL_MONTH, UCAL_ACTUAL_MINIMUM, &status);
    ac_max=ucal_getLimit(cal, UCAL_MONTH, UCAL_ACTUAL_MAXIMUM, &status);
    if(U_FAILURE(status)){
        log_err("Error in getLimits: %s\n", u_errorName(status));
    }
    if(min!=0 || max!=11 || gr_min!=0 || le_max!=11 || ac_min!=0 || ac_max!=11)
        log_err("There is and error in getLimits in fetching the values\n");
    else
        log_verbose("getLimits successful\n");

    ucal_setDateTime(cal, 1999, UCAL_MARCH, 5, 4, 10, 35, &status);
    val=ucal_get(cal, UCAL_HOUR_OF_DAY, &status);
    min=ucal_getLimit(cal, UCAL_HOUR_OF_DAY, UCAL_MINIMUM, &status);
    max=ucal_getLimit(cal, UCAL_HOUR_OF_DAY, UCAL_MAXIMUM, &status);
    gr_min=ucal_getLimit(cal, UCAL_MINUTE, UCAL_GREATEST_MINIMUM, &status);
    le_max=ucal_getLimit(cal, UCAL_MINUTE, UCAL_LEAST_MAXIMUM, &status);
    ac_min=ucal_getLimit(cal, UCAL_MINUTE, UCAL_ACTUAL_MINIMUM, &status);
    ac_max=ucal_getLimit(cal, UCAL_SECOND, UCAL_ACTUAL_MAXIMUM, &status);
    if( (min!=0 || max!= 11 || gr_min!=0 || le_max!=60 || ac_min!=0 || ac_max!=60) &&
        (min>val && val>max) && val!=4){
                
        log_err("FAIL: Min/max bad\n");
        log_err("FAIL: Hour of Day %d out of range\n", val);
        log_err("FAIL: HOUR_OF_DAY should be 4 Got %d\n", val);
    }
    else
        log_verbose("getLimits successful\n");    

    
    /*get BOGUS_LIMIT type*/
    val=ucal_getLimit(cal, UCAL_SECOND, (UCalendarLimitType)99, &status);
    if(val != -1){
        log_err("FAIL: ucal_getLimit() with BOGUS type should return -1\n");
    }
    status=U_ZERO_ERROR;


    ucal_close(cal);
}



/* ------------------------------------- */
 
/**
 * Test that the days of the week progress properly when add is called repeatedly
 * for increments of 24 days.
 */
static void TestDOWProgression(void)
{
    int32_t initialDOW, DOW, newDOW, expectedDOW;
    UCalendar *cal = 0;
    UDateFormat *datfor = 0;
    UDate date1;
    int32_t delta=24;
    UErrorCode status = U_ZERO_ERROR;
    UChar tzID[4];
    char tempMsgBuf[256];
    u_strcpy(tzID, fgGMTID);
    /*open the calendar used */
    cal=ucal_open(tzID, u_strlen(tzID), "en_US", UCAL_TRADITIONAL, &status);
    if (U_FAILURE(status)) {
        log_data_err("ucal_open failed: %s - (Are you missing data?)\n", u_errorName(status));
        return; 
    }

    datfor=udat_open(UDAT_MEDIUM,UDAT_MEDIUM ,NULL, fgGMTID,-1,NULL, 0, &status);
    if(U_FAILURE(status)){
        log_data_err("error in creating the dateformat : %s (Are you missing data?)\n", u_errorName(status));
    }
    

    ucal_setDate(cal, 1999, UCAL_JANUARY, 1, &status);

    log_verbose("\nTesting the DOW progression\n");
    
    initialDOW = ucal_get(cal, UCAL_DAY_OF_WEEK, &status);
    if (U_FAILURE(status)) { 
        log_data_err("ucal_get() failed: %s (Are you missing data?)\n", u_errorName(status) );
    } else {
        newDOW = initialDOW;
        do {
            DOW = newDOW;
            log_verbose("DOW = %d...\n", DOW);
            date1=ucal_getMillis(cal, &status);
            if(U_FAILURE(status)){ log_err("ucal_getMiilis() failed: %s\n", u_errorName(status)); break;}
            log_verbose("%s\n", u_austrcpy(tempMsgBuf, myDateFormat(datfor, date1)));

            ucal_add(cal,UCAL_DAY_OF_WEEK, delta, &status);
            if (U_FAILURE(status)) { log_err("ucal_add() failed: %s\n", u_errorName(status)); break; }

            newDOW = ucal_get(cal, UCAL_DAY_OF_WEEK, &status);
            if (U_FAILURE(status)) { log_err("ucal_get() failed: %s\n", u_errorName(status)); break; }
            expectedDOW = 1 + (DOW + delta - 1) % 7;
            date1=ucal_getMillis(cal, &status);
            if(U_FAILURE(status)){ log_err("ucal_getMiilis() failed: %s\n", u_errorName(status)); break;}
            if (newDOW != expectedDOW) {
                log_err("Day of week should be %d instead of %d on %s", expectedDOW, newDOW, 
                        u_austrcpy(tempMsgBuf, myDateFormat(datfor, date1)) );    
                break; 
            }
        }
        while (newDOW != initialDOW);
    }
    
    ucal_close(cal);
    udat_close(datfor);
}
 
/* ------------------------------------- */
 
/**
 * Confirm that the offset between local time and GMT behaves as expected.
 */
static void TestGMTvsLocal(void)
{
    log_verbose("\nTesting the offset between the GMT and local time\n");
    testZones(1999, 1, 1, 12, 0, 0);
    testZones(1999, 4, 16, 18, 30, 0);
    testZones(1998, 12, 17, 19, 0, 0);
}
 
/* ------------------------------------- */
 
static void testZones(int32_t yr, int32_t mo, int32_t dt, int32_t hr, int32_t mn, int32_t sc)
{
    int32_t offset,utc, expected;
    UCalendar *gmtcal = 0, *cal = 0;
    UDate date1;
    double temp;
    UDateFormat *datfor = 0;
    UErrorCode status = U_ZERO_ERROR;
    UChar tzID[4];
    char tempMsgBuf[256];

    u_strcpy(tzID, fgGMTID);
    gmtcal=ucal_open(tzID, 3, "en_US", UCAL_TRADITIONAL, &status);
    if (U_FAILURE(status)) {
        log_data_err("ucal_open failed: %s - (Are you missing data?)\n", u_errorName(status)); 
        goto cleanup; 
    }
    u_uastrcpy(tzID, "PST");
    cal = ucal_open(tzID, 3, "en_US", UCAL_TRADITIONAL, &status);
    if (U_FAILURE(status)) {
        log_err("ucal_open failed: %s\n", u_errorName(status));
        goto cleanup; 
    }
    
    datfor=udat_open(UDAT_MEDIUM,UDAT_MEDIUM ,NULL, fgGMTID,-1,NULL, 0, &status);
    if(U_FAILURE(status)){
        log_data_err("error in creating the dateformat : %s (Are you missing data?)\n", u_errorName(status));
    }
   
    ucal_setDateTime(gmtcal, yr, mo - 1, dt, hr, mn, sc, &status);
    if (U_FAILURE(status)) {
        log_data_err("ucal_setDateTime failed: %s (Are you missing data?)\n", u_errorName(status));
        goto cleanup; 
    }
    ucal_set(gmtcal, UCAL_MILLISECOND, 0);
    date1 = ucal_getMillis(gmtcal, &status);
    if (U_FAILURE(status)) {
        log_err("ucal_getMillis failed: %s\n", u_errorName(status));
        goto cleanup;
    }
    log_verbose("date = %s\n", u_austrcpy(tempMsgBuf, myDateFormat(datfor, date1)) );

    
    ucal_setMillis(cal, date1, &status);
    if (U_FAILURE(status)) {
        log_err("ucal_setMillis() failed: %s\n", u_errorName(status));
        goto cleanup;
    }

    offset = ucal_get(cal, UCAL_ZONE_OFFSET, &status);
    offset += ucal_get(cal, UCAL_DST_OFFSET, &status);
   
    if (U_FAILURE(status)) {
        log_err("ucal_get() failed: %s\n", u_errorName(status));
        goto cleanup;
    }
    temp=(double)((double)offset / 1000.0 / 60.0 / 60.0);
    /*printf("offset for %s %f hr\n", austrdup(myDateFormat(datfor, date1)), temp);*/
       
    utc = ((ucal_get(cal, UCAL_HOUR_OF_DAY, &status) * 60 +
                    ucal_get(cal, UCAL_MINUTE, &status)) * 60 +
                   ucal_get(cal, UCAL_SECOND, &status)) * 1000 +
                    ucal_get(cal, UCAL_MILLISECOND, &status) - offset;
    if (U_FAILURE(status)) {
        log_err("ucal_get() failed: %s\n", u_errorName(status));
        goto cleanup;
    }
    
    expected = ((hr * 60 + mn) * 60 + sc) * 1000;
    if (utc != expected) {
        temp=(double)(utc - expected)/ 1000 / 60 / 60.0;
        log_err("FAIL: Discrepancy of %d  millis = %fhr\n", utc-expected, temp );
    }
    else
        log_verbose("PASS: the offset between local and GMT is correct\n");

cleanup:
    ucal_close(gmtcal);
    ucal_close(cal);
    udat_close(datfor);
}
 
/* ------------------------------------- */




/* INTERNAL FUNCTIONS USED */
/*------------------------------------------------------------------------------------------- */
 
/* ------------------------------------- */
static void checkDateTime(UCalendar* c, 
                        int32_t y, int32_t m, int32_t d, 
                        int32_t hr, int32_t min, int32_t sec, 
                        int32_t ms, UCalendarDateFields field)

{
    UErrorCode status = U_ZERO_ERROR;
    if (ucal_get(c, UCAL_YEAR, &status) != y ||
        ucal_get(c, UCAL_MONTH, &status) != m ||
        ucal_get(c, UCAL_DATE, &status) != d ||
        ucal_get(c, UCAL_HOUR, &status) != hr ||
        ucal_get(c, UCAL_MINUTE, &status) != min ||
        ucal_get(c, UCAL_SECOND, &status) != sec ||
        ucal_get(c, UCAL_MILLISECOND, &status) != ms) {
        log_err("U_FAILURE for field  %d, Expected y/m/d h:m:s:ms of %d/%d/%d %d:%d:%d:%d  got %d/%d/%d %d:%d:%d:%d\n",
            (int32_t)field, y, m + 1, d, hr, min, sec, ms, 
                    ucal_get(c, UCAL_YEAR, &status),
                    ucal_get(c, UCAL_MONTH, &status) + 1,
                    ucal_get(c, UCAL_DATE, &status),
                    ucal_get(c, UCAL_HOUR, &status),
                    ucal_get(c, UCAL_MINUTE, &status) + 1,
                    ucal_get(c, UCAL_SECOND, &status),
                    ucal_get(c, UCAL_MILLISECOND, &status) );

                    if (U_FAILURE(status)){
                    log_err("ucal_get failed: %s\n", u_errorName(status)); 
                    return; 
                    }
    
     }
    else 
        log_verbose("Confirmed: %d/%d/%d %d:%d:%d:%d\n", y, m + 1, d, hr, min, sec, ms);
        
}
 
/* ------------------------------------- */
static void checkDate(UCalendar* c, int32_t y, int32_t m, int32_t d)
{
    UErrorCode status = U_ZERO_ERROR;
    if (ucal_get(c,UCAL_YEAR, &status) != y ||
        ucal_get(c, UCAL_MONTH, &status) != m ||
        ucal_get(c, UCAL_DATE, &status) != d) {
        
        log_err("FAILURE: Expected y/m/d of %d/%d/%d  got %d/%d/%d\n", y, m + 1, d, 
                    ucal_get(c, UCAL_YEAR, &status),
                    ucal_get(c, UCAL_MONTH, &status) + 1,
                    ucal_get(c, UCAL_DATE, &status) );
        
        if (U_FAILURE(status)) { 
            log_err("ucal_get failed: %s\n", u_errorName(status));
            return; 
        }
    }
    else 
        log_verbose("Confirmed: %d/%d/%d\n", y, m + 1, d);


}

/* ------------------------------------- */
 
/* ------------------------------------- */
 
static void verify1(const char* msg, UCalendar* c, UDateFormat* dat, int32_t year, int32_t month, int32_t day)
{
    UDate d1;
    UErrorCode status = U_ZERO_ERROR;
    if (ucal_get(c, UCAL_YEAR, &status) == year &&
        ucal_get(c, UCAL_MONTH, &status) == month &&
        ucal_get(c, UCAL_DATE, &status) == day) {
        if (U_FAILURE(status)) { 
            log_err("FAIL: Calendar::get failed: %s\n", u_errorName(status));
            return; 
        }
        log_verbose("PASS: %s\n", msg);
        d1=ucal_getMillis(c, &status);
        if (U_FAILURE(status)) { 
            log_err("ucal_getMillis failed: %s\n", u_errorName(status));
            return;
        }
    /*log_verbose(austrdup(myDateFormat(dat, d1)) );*/
    }
    else {
        log_err("FAIL: %s\n", msg);
        d1=ucal_getMillis(c, &status);
        if (U_FAILURE(status)) { 
            log_err("ucal_getMillis failed: %s\n", u_errorName(status) );
            return;
        }
        log_err("Got %s  Expected %d/%d/%d \n", austrdup(myDateFormat(dat, d1)), year, month + 1, day );
        return;
    }

        
}
 
/* ------------------------------------ */
static void verify2(const char* msg, UCalendar* c, UDateFormat* dat, int32_t year, int32_t month, int32_t day, 
                                                                     int32_t hour, int32_t min, int32_t sec, int32_t am_pm)
{
    UDate d1;
    UErrorCode status = U_ZERO_ERROR;
    char tempMsgBuf[256];

    if (ucal_get(c, UCAL_YEAR, &status) == year &&
        ucal_get(c, UCAL_MONTH, &status) == month &&
        ucal_get(c, UCAL_DATE, &status) == day &&
        ucal_get(c, UCAL_HOUR, &status) == hour &&
        ucal_get(c, UCAL_MINUTE, &status) == min &&
        ucal_get(c, UCAL_SECOND, &status) == sec &&
        ucal_get(c, UCAL_AM_PM, &status) == am_pm ){
        if (U_FAILURE(status)) { 
            log_err("FAIL: Calendar::get failed: %s\n", u_errorName(status));
            return; 
        }
        log_verbose("PASS: %s\n", msg); 
        d1=ucal_getMillis(c, &status);
        if (U_FAILURE(status)) { 
            log_err("ucal_getMillis failed: %s\n", u_errorName(status));
            return;
        }
        log_verbose("%s\n" , u_austrcpy(tempMsgBuf, myDateFormat(dat, d1)) );
    }
    else {
        log_err("FAIL: %s\n", msg);
        d1=ucal_getMillis(c, &status);
        if (U_FAILURE(status)) { 
            log_err("ucal_getMillis failed: %s\n", u_errorName(status)); 
            return;
        }
        log_err("Got %s Expected %d/%d/%d/ %d:%d:%d  %s\n", austrdup(myDateFormat(dat, d1)), 
            year, month + 1, day, hour, min, sec, (am_pm==0) ? "AM": "PM");
        
        return;
    }

        
}

void TestGregorianChange(void) {
    static const UChar utc[] = { 0x45, 0x74, 0x63, 0x2f, 0x47, 0x4d, 0x54, 0 }; /* "Etc/GMT" */
    const int32_t dayMillis = 86400 * INT64_C(1000);    /* 1 day = 86400 seconds */
    UCalendar *cal;
    UDate date;
    UErrorCode errorCode = U_ZERO_ERROR;

    /* Test ucal_setGregorianChange() on a Gregorian calendar. */
    errorCode = U_ZERO_ERROR;
    cal = ucal_open(utc, -1, "", UCAL_GREGORIAN, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_data_err("ucal_open(UTC) failed: %s - (Are you missing data?)\n", u_errorName(errorCode));
        return;
    }
    ucal_setGregorianChange(cal, -365 * (dayMillis * (UDate)1), &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("ucal_setGregorianChange(1969) failed: %s\n", u_errorName(errorCode));
    } else {
        date = ucal_getGregorianChange(cal, &errorCode);
        if(U_FAILURE(errorCode) || date != -365 * (dayMillis * (UDate)1)) {
            log_err("ucal_getGregorianChange() failed: %s, date = %f\n", u_errorName(errorCode), date);
        }
    }
    ucal_close(cal);
    /* Test ucal_setGregorianChange() on a iso8601 calendar and it should work
     * as Gregorian. */
    errorCode = U_ZERO_ERROR;
    cal = ucal_open(utc, -1, "en@calendar=iso8601", UCAL_TRADITIONAL, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_data_err("ucal_open(UTC) failed: %s - (Are you missing data?)\n", u_errorName(errorCode));
        return;
    }
    ucal_setGregorianChange(cal, -365 * (dayMillis * (UDate)1), &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("ucal_setGregorianChange(1969) failed: %s\n", u_errorName(errorCode));
    } else {
        date = ucal_getGregorianChange(cal, &errorCode);
        if(U_FAILURE(errorCode) || date != -365 * (dayMillis * (UDate)1)) {
            log_err("ucal_getGregorianChange() failed: %s, date = %f\n", u_errorName(errorCode), date);
        }
    }
    ucal_close(cal);

    /* Test ucal_setGregorianChange() on a non-Gregorian calendar where it should fail. */
    errorCode = U_ZERO_ERROR;
    cal = ucal_open(utc, -1, "th@calendar=buddhist", UCAL_TRADITIONAL, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("ucal_open(UTC, non-Gregorian) failed: %s\n", u_errorName(errorCode));
        return;
    }
    ucal_setGregorianChange(cal, -730 * (dayMillis * (UDate)1), &errorCode);
    if(errorCode != U_UNSUPPORTED_ERROR) {
        log_err("ucal_setGregorianChange(non-Gregorian calendar) did not yield U_UNSUPPORTED_ERROR but %s\n",
                u_errorName(errorCode));
    }
    errorCode = U_ZERO_ERROR;
    date = ucal_getGregorianChange(cal, &errorCode);
    if(errorCode != U_UNSUPPORTED_ERROR) {
        log_err("ucal_getGregorianChange(non-Gregorian calendar) did not yield U_UNSUPPORTED_ERROR but %s\n",
                u_errorName(errorCode));
    }
    ucal_close(cal);
}

static void TestGetKeywordValuesForLocale(void) {
#define PREFERRED_SIZE 26
#define MAX_NUMBER_OF_KEYWORDS 5
    const char *PREFERRED[PREFERRED_SIZE][MAX_NUMBER_OF_KEYWORDS+1] = {
            { "root",        "gregorian", NULL, NULL, NULL, NULL },
            { "und",         "gregorian", NULL, NULL, NULL, NULL },
            { "en_US",       "gregorian", NULL, NULL, NULL, NULL },
            { "en_029",      "gregorian", NULL, NULL, NULL, NULL },
            { "th_TH",       "buddhist", "gregorian", NULL, NULL, NULL },
            { "und_TH",      "buddhist", "gregorian", NULL, NULL, NULL },
            { "en_TH",       "buddhist", "gregorian", NULL, NULL, NULL },
            { "he_IL",       "gregorian", "hebrew", "islamic", "islamic-civil", "islamic-tbla" },
            { "ar_EG",       "gregorian", "coptic", "islamic", "islamic-civil", "islamic-tbla" },
            { "ja",          "gregorian", "japanese", NULL, NULL, NULL },
            { "ps_Guru_IN",  "gregorian", "indian", NULL, NULL, NULL },
            { "th@calendar=gregorian", "buddhist", "gregorian", NULL, NULL, NULL },
            { "en@calendar=islamic",   "gregorian", NULL, NULL, NULL, NULL },
            { "zh_TW",       "gregorian", "roc", "chinese", NULL, NULL },
            { "ar_IR",       "persian", "gregorian", "islamic", "islamic-civil", "islamic-tbla" },
            { "th@rg=SAZZZZ", "islamic-umalqura", "gregorian", "islamic", "islamic-rgsa", NULL },

            // tests for ICU-22364
            { "zh_CN@rg=TW",           "gregorian", "chinese", NULL, NULL, NULL }, // invalid subdivision code
            { "zh_CN@rg=TWzzzz",       "gregorian", "roc", "chinese", NULL, NULL }, // whole region
            { "zh_TW@rg=TWxxxx",       "gregorian", "roc", "chinese", NULL, NULL }, // invalid subdivision code (ignored)
            { "zh_TW@rg=ARa",          "gregorian", NULL, NULL, NULL, NULL }, // single-letter subdivision code
            { "zh_TW@rg=AT1",          "gregorian", NULL, NULL, NULL, NULL }, // single-digit subdivision code
            { "zh_TW@rg=USca",         "gregorian", NULL, NULL, NULL, NULL }, // two-letter subdivision code
            { "zh_TW@rg=IT53",         "gregorian", NULL, NULL, NULL, NULL }, // two-digit subdivision code
            { "zh_TW@rg=AUnsw",        "gregorian", NULL, NULL, NULL, NULL }, // three-letter subdivision code
            { "zh_TW@rg=EE130",        "gregorian", NULL, NULL, NULL, NULL }, // three-digit subdivision code
            { "zh_TW@rg=417zzzz",      "gregorian", NULL, NULL, NULL, NULL }, // three-digit region code
    };
    const int32_t EXPECTED_SIZE[PREFERRED_SIZE] = { 1, 1, 1, 1, 2, 2, 2, 5, 5, 2, 2, 2, 1, 3, 5, 4, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1 };
    UErrorCode status = U_ZERO_ERROR;
    int32_t i, size, j;
    UEnumeration *all, *pref;
    const char *loc = NULL;
    UBool matchPref, matchAll;
    const char *value;
    int32_t valueLength;
    UList *ALLList = NULL;
    
    UEnumeration *ALL = ucal_getKeywordValuesForLocale("calendar", uloc_getDefault(), false, &status);
    if (U_SUCCESS(status)) {
        for (i = 0; i < PREFERRED_SIZE; i++) {
            pref = NULL;
            all = NULL;
            loc = PREFERRED[i][0];
            pref = ucal_getKeywordValuesForLocale("calendar", loc, true, &status);
            matchPref = false;
            matchAll = false;
            
            value = NULL;
            valueLength = 0;
            
            if (U_SUCCESS(status) && uenum_count(pref, &status) == EXPECTED_SIZE[i]) {
                matchPref = true;
                for (j = 0; j < EXPECTED_SIZE[i]; j++) {
                    if ((value = uenum_next(pref, &valueLength, &status)) != NULL && U_SUCCESS(status)) {
                        if (uprv_strcmp(value, PREFERRED[i][j+1]) != 0) {
                            matchPref = false;
                            break;
                        }
                    } else {
                        matchPref = false;
                        log_err("ERROR getting keyword value for locale \"%s\"\n", loc);
                        break;
                    }
                }
            }
            
            if (!matchPref) {
                log_err("FAIL: Preferred values for locale \"%s\" does not match expected.\n", loc);
                break;
            }
            uenum_close(pref);
            
            all = ucal_getKeywordValuesForLocale("calendar", loc, false, &status);
            
            size = uenum_count(all, &status);
            
            if (U_SUCCESS(status) && size == uenum_count(ALL, &status)) {
                matchAll = true;
                ALLList = ulist_getListFromEnum(ALL);
                for (j = 0; j < size; j++) {
                    if ((value = uenum_next(all, &valueLength, &status)) != NULL && U_SUCCESS(status)) {
                        if (!ulist_containsString(ALLList, value, (int32_t)uprv_strlen(value))) {
                            log_err("Locale %s have %s not in ALL\n", loc, value);
                            matchAll = false;
                            break;
                        }
                    } else {
                        matchAll = false;
                        log_err("ERROR getting \"all\" keyword value for locale \"%s\"\n", loc);
                        break;
                    }
                }
            }
            if (!matchAll) {
                log_err("FAIL: All values for locale \"%s\" does not match expected.\n", loc);
            }
            
            uenum_close(all);
        }
    } else {
        log_err_status(status, "Failed to get ALL keyword values for default locale %s: %s.\n", uloc_getDefault(), u_errorName(status));
    }
    uenum_close(ALL);
}

/*
 * Weekend tests, ported from
 * icu4j/main/core/src/test/java/com/ibm/icu/dev/test/calendar/IBMCalendarTest.java
 * and extended a bit. Notes below from IBMCalendarTest.java ...
 * This test tests for specific locale data. This is probably okay
 * as far as US data is concerned, but if the Arabic/Yemen data
 * changes, this test will have to be updated.
 */

typedef struct {
    int32_t year;
    int32_t month;
    int32_t day;
    int32_t hour;
    int32_t millisecOffset;
    UBool   isWeekend;
} TestWeekendDates;
typedef struct {
    const char * locale;
    const        TestWeekendDates * dates;
    int32_t      numDates;
} TestWeekendDatesList;

static const TestWeekendDates weekendDates_en_US[] = {
    { 2000, UCAL_MARCH, 17, 23,  0, 0 }, /* Fri 23:00        */
    { 2000, UCAL_MARCH, 18,  0, -1, 0 }, /* Fri 23:59:59.999 */
    { 2000, UCAL_MARCH, 18,  0,  0, 1 }, /* Sat 00:00        */
    { 2000, UCAL_MARCH, 18, 15,  0, 1 }, /* Sat 15:00        */
    { 2000, UCAL_MARCH, 19, 23,  0, 1 }, /* Sun 23:00        */
    { 2000, UCAL_MARCH, 20,  0, -1, 1 }, /* Sun 23:59:59.999 */
    { 2000, UCAL_MARCH, 20,  0,  0, 0 }, /* Mon 00:00        */
    { 2000, UCAL_MARCH, 20,  8,  0, 0 }, /* Mon 08:00        */
};
static const TestWeekendDates weekendDates_ar_OM[] = {
    { 2000, UCAL_MARCH, 15, 23,  0, 0 }, /* Wed 23:00        */
    { 2000, UCAL_MARCH, 16,  0, -1, 0 }, /* Wed 23:59:59.999 */
    { 2000, UCAL_MARCH, 16,  0,  0, 0 }, /* Thu 00:00        */
    { 2000, UCAL_MARCH, 16, 15,  0, 0 }, /* Thu 15:00        */
    { 2000, UCAL_MARCH, 17, 23,  0, 1 }, /* Fri 23:00        */
    { 2000, UCAL_MARCH, 18,  0, -1, 1 }, /* Fri 23:59:59.999 */
    { 2000, UCAL_MARCH, 18,  0,  0, 1 }, /* Sat 00:00        */
    { 2000, UCAL_MARCH, 18,  8,  0, 1 }, /* Sat 08:00        */
};
static const TestWeekendDatesList testDates[] = {
    { "en_US", weekendDates_en_US, UPRV_LENGTHOF(weekendDates_en_US) },
    { "ar_OM", weekendDates_ar_OM, UPRV_LENGTHOF(weekendDates_ar_OM) },
};

typedef struct {
    UCalendarDaysOfWeek  dayOfWeek;
    UCalendarWeekdayType dayType;
    int32_t              transition; /* transition time if dayType is UCAL_WEEKEND_ONSET or UCAL_WEEKEND_CEASE; else must be 0 */
} TestDaysOfWeek;
typedef struct {
    const char *           locale;
    const TestDaysOfWeek * days;
    int32_t                numDays;
} TestDaysOfWeekList;

static const TestDaysOfWeek daysOfWeek_en_US[] = {
    { UCAL_MONDAY,   UCAL_WEEKDAY,       0        },
    { UCAL_FRIDAY,   UCAL_WEEKDAY,       0        },
    { UCAL_SATURDAY, UCAL_WEEKEND,       0        },
    { UCAL_SUNDAY,   UCAL_WEEKEND,       0        },
};
static const TestDaysOfWeek daysOfWeek_ar_OM[] = { /* Friday:Saturday */
    { UCAL_WEDNESDAY,UCAL_WEEKDAY,       0        },
    { UCAL_THURSDAY, UCAL_WEEKDAY,       0        },
    { UCAL_FRIDAY,   UCAL_WEEKEND,       0        },
    { UCAL_SATURDAY, UCAL_WEEKEND,       0        },
};
static const TestDaysOfWeek daysOfWeek_hi_IN[] = { /* Sunday only */
    { UCAL_MONDAY,   UCAL_WEEKDAY,       0        },
    { UCAL_FRIDAY,   UCAL_WEEKDAY,       0        },
    { UCAL_SATURDAY, UCAL_WEEKDAY,       0        },
    { UCAL_SUNDAY,   UCAL_WEEKEND,       0        },
};
static const TestDaysOfWeekList testDays[] = {
    { "en_US", daysOfWeek_en_US, UPRV_LENGTHOF(daysOfWeek_en_US) },
    { "ar_OM", daysOfWeek_ar_OM, UPRV_LENGTHOF(daysOfWeek_ar_OM) },
    { "hi_IN", daysOfWeek_hi_IN, UPRV_LENGTHOF(daysOfWeek_hi_IN) },
    { "en_US@rg=OMZZZZ", daysOfWeek_ar_OM, UPRV_LENGTHOF(daysOfWeek_ar_OM) },
    { "hi@rg=USZZZZ",    daysOfWeek_en_US, UPRV_LENGTHOF(daysOfWeek_en_US) },
};

static const UChar logDateFormat[] = { 0x0045,0x0045,0x0045,0x0020,0x004D,0x004D,0x004D,0x0020,0x0064,0x0064,0x0020,0x0079,
                                       0x0079,0x0079,0x0079,0x0020,0x0047,0x0020,0x0048,0x0048,0x003A,0x006D,0x006D,0x003A,
                                       0x0073,0x0073,0x002E,0x0053,0x0053,0x0053,0 }; /* "EEE MMM dd yyyy G HH:mm:ss.SSS" */
enum { kFormattedDateMax = 2*UPRV_LENGTHOF(logDateFormat) };

static void TestWeekend(void) {
    const TestWeekendDatesList * testDatesPtr = testDates;
    const TestDaysOfWeekList *   testDaysPtr = testDays;
    int32_t count, subCount;

    UErrorCode fmtStatus = U_ZERO_ERROR;
    UDateFormat * fmt = udat_open(UDAT_NONE, UDAT_NONE, "en", NULL, 0, NULL, 0, &fmtStatus);
    if (U_SUCCESS(fmtStatus)) {
        udat_applyPattern(fmt, false, logDateFormat, -1);
    } else {
        log_data_err("Unable to create UDateFormat - %s\n", u_errorName(fmtStatus));
        return;
    }
    for (count = UPRV_LENGTHOF(testDates); count-- > 0; ++testDatesPtr) {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar * cal = ucal_open(NULL, 0, testDatesPtr->locale, UCAL_GREGORIAN, &status);
        log_verbose("locale: %s\n", testDatesPtr->locale);
        if (U_SUCCESS(status)) {
            const TestWeekendDates * weekendDatesPtr = testDatesPtr->dates;
            for (subCount = testDatesPtr->numDates; subCount--; ++weekendDatesPtr) {
                UDate dateToTest;
                UBool isWeekend;
                char  fmtDateBytes[kFormattedDateMax] = "<could not format test date>"; /* initialize for failure */

                ucal_clear(cal);
                ucal_setDateTime(cal, weekendDatesPtr->year, weekendDatesPtr->month, weekendDatesPtr->day,
                                 weekendDatesPtr->hour, 0, 0, &status);
                dateToTest = ucal_getMillis(cal, &status) + weekendDatesPtr->millisecOffset;
                isWeekend = ucal_isWeekend(cal, dateToTest, &status);
                if (U_SUCCESS(fmtStatus)) {
                    UChar fmtDate[kFormattedDateMax];
                    (void)udat_format(fmt, dateToTest, fmtDate, kFormattedDateMax, NULL, &fmtStatus);
                    if (U_SUCCESS(fmtStatus)) {
                        u_austrncpy(fmtDateBytes, fmtDate, kFormattedDateMax);
                        fmtDateBytes[kFormattedDateMax-1] = 0;
                    } else {
                        fmtStatus = U_ZERO_ERROR;
                    }
                }
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: locale %s date %s isWeekend() status %s\n", testDatesPtr->locale, fmtDateBytes, u_errorName(status) );
                    status = U_ZERO_ERROR;
                } else if ( (isWeekend!=0) != (weekendDatesPtr->isWeekend!=0) ) {
                    log_err("FAIL: locale %s date %s isWeekend %d, expected the opposite\n", testDatesPtr->locale, fmtDateBytes, isWeekend );
                } else {
                    log_verbose("OK:   locale %s date %s isWeekend %d\n", testDatesPtr->locale, fmtDateBytes, isWeekend );
                }
            }
            ucal_close(cal);
        } else {
            log_data_err("FAIL: ucal_open for locale %s failed: %s - (Are you missing data?)\n", testDatesPtr->locale, u_errorName(status) );
        }
    }
    if (U_SUCCESS(fmtStatus)) {
        udat_close(fmt);
    }

    for (count = UPRV_LENGTHOF(testDays); count-- > 0; ++testDaysPtr) {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar * cal = ucal_open(NULL, 0, testDaysPtr->locale, UCAL_GREGORIAN, &status);
        log_verbose("locale: %s\n", testDaysPtr->locale);
        if (U_SUCCESS(status)) {
            const TestDaysOfWeek * daysOfWeekPtr = testDaysPtr->days;
            for (subCount = testDaysPtr->numDays; subCount--; ++daysOfWeekPtr) {
                int32_t transition = 0;
                UCalendarWeekdayType dayType = ucal_getDayOfWeekType(cal, daysOfWeekPtr->dayOfWeek, &status);
                if ( dayType == UCAL_WEEKEND_ONSET || dayType == UCAL_WEEKEND_CEASE ) {
                    transition = ucal_getWeekendTransition(cal, daysOfWeekPtr->dayOfWeek, &status); 
                }
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: locale %s DOW %d getDayOfWeekType() status %s\n", testDaysPtr->locale, daysOfWeekPtr->dayOfWeek, u_errorName(status) );
                    status = U_ZERO_ERROR;
                } else if ( dayType != daysOfWeekPtr->dayType || transition != daysOfWeekPtr->transition ) {
                    log_err("FAIL: locale %s DOW %d type %d, expected %d\n", testDaysPtr->locale, daysOfWeekPtr->dayOfWeek, dayType, daysOfWeekPtr->dayType );
                } else {
                    log_verbose("OK:   locale %s DOW %d type %d\n", testDaysPtr->locale, daysOfWeekPtr->dayOfWeek, dayType );
                }
            }
            ucal_close(cal);
        } else {
            log_data_err("FAIL: ucal_open for locale %s failed: %s - (Are you missing data?)\n", testDaysPtr->locale, u_errorName(status) );
        }
    }
}

/**
 * TestFieldDifference
 */

typedef struct {
    const UChar * timezone;
    const char *  locale;
    UDate         start;
    UDate         target;
    UBool         progressive; /* true to compute progressive difference for each field, false to reset calendar after each call */
    int32_t       yDiff;
    int32_t       MDiff;
    int32_t       dDiff;
    int32_t       HDiff;
    int32_t       mDiff;
    int32_t       sDiff; /* 0x7FFFFFFF indicates overflow error expected */
} TFDItem;

static const UChar tzUSPacific[] = { 0x55,0x53,0x2F,0x50,0x61,0x63,0x69,0x66,0x69,0x63,0 }; /* "US/Pacific" */
static const UChar tzGMT[] = { 0x47,0x4D,0x54,0 }; /* "GMT" */

static const TFDItem tfdItems[] = {
    /* timezone    locale          start              target           progress yDf  MDf    dDf     HDf       mDf         sDf */
    /* For these we compute the progressive difference for each field - not resetting the calendar after each call */
    { tzUSPacific, "en_US",        1267459800000.0,   1277772600000.0,  true,     0,   3,    27,      9,       40,          0 }, /* 2010-Mar-01 08:10 -> 2010-Jun-28 17:50 */
    { tzUSPacific, "en_US",        1267459800000.0,   1299089280000.0,  true,     1,   0,     1,      1,       58,          0 }, /* 2010-Mar-01 08:10 -> 2011-Mar-02 10:08 */
    /* For these we compute the total difference for each field - resetting the calendar after each call */
    { tzGMT,       "en_US",        0.0,               1073692800000.0,  false,   34, 408, 12427, 298248, 17894880, 1073692800 }, /* 1970-Jan-01 00:00 -> 2004-Jan-10 00:00 */
    { tzGMT,       "en_US",        0.0,               1073779200000.0,  false,   34, 408, 12428, 298272, 17896320, 1073779200 }, /* 1970-Jan-01 00:00 -> 2004-Jan-11 00:00 */
    { tzGMT,       "en_US",        0.0,               2147472000000.0,  false,   68, 816, 24855, 596520, 35791200, 2147472000 }, /* 1970-Jan-01 00:00 -> 2038-Jan-19 00:00 */
    { tzGMT,       "en_US",        0.0,               2147558400000.0,  false,   68, 816, 24856, 596544, 35792640, 0x7FFFFFFF }, /* 1970-Jan-01 00:00 -> 2038-Jan-20 00:00, seconds diff overflow */
    { tzGMT,       "en_US",        0.0,              -1073692800000.0,  false,  -34,-408,-12427,-298248,-17894880,-1073692800 }, /* 1970-Jan-01 00:00 -> 1935-Dec-24 00:00 */
    { tzGMT,       "en_US",        0.0,              -1073779200000.0,  false,  -34,-408,-12428,-298272,-17896320,-1073779200 }, /* 1970-Jan-01 00:00 -> 1935-Dec-23 00:00 */
    /* check fwd/backward on either side of era boundary and across era boundary */
    { tzGMT,       "en_US",       -61978089600000.0,-61820409600000.0,  false,    4,  59,  1825,  43800,  2628000,  157680000 }, /* CE   5-Dec-31 00:00 -> CE  10-Dec-30 00:00 */
    { tzGMT,       "en_US",       -61820409600000.0,-61978089600000.0,  false,   -4, -59, -1825, -43800, -2628000, -157680000 }, /* CE  10-Dec-30 00:00 -> CE   5-Dec-31 00:00 */
    { tzGMT,       "en_US",       -62451129600000.0,-62293449600000.0,  false,    4,  59,  1825,  43800,  2628000,  157680000 }, /* BCE 10-Jan-04 00:00 -> BCE  5-Jan-03 00:00 */
    { tzGMT,       "en_US",       -62293449600000.0,-62451129600000.0,  false,   -4, -59, -1825, -43800, -2628000, -157680000 }, /* BCE  5-Jan-03 00:00 -> BCE 10-Jan-04 00:00 */
    { tzGMT,       "en_US",       -62293449600000.0,-61978089600000.0,  false,    9, 119,  3650,  87600,  5256000,  315360000 }, /* BCE  5-Jan-03 00:00 -> CE   5-Dec-31 00:00 */
    { tzGMT,       "en_US",       -61978089600000.0,-62293449600000.0,  false,   -9,-119, -3650, -87600, -5256000, -315360000 }, /* CE   5-Dec-31 00:00 -> BCE  5-Jan-03 00:00 */
    { tzGMT, "en@calendar=roc",    -1672704000000.0, -1515024000000.0,  false,    4,  59,  1825,  43800,  2628000,  157680000 }, /* MG   5-Dec-30 00:00 -> MG  10-Dec-29 00:00 */
    { tzGMT, "en@calendar=roc",    -1515024000000.0, -1672704000000.0,  false,   -4, -59, -1825, -43800, -2628000, -157680000 }, /* MG  10-Dec-29 00:00 -> MG   5-Dec-30 00:00 */
    { tzGMT, "en@calendar=roc",    -2145744000000.0, -1988064000000.0,  false,    4,  59,  1825,  43800,  2628000,  157680000 }, /* BMG 10-Jan-03 00:00 -> BMG  5-Jan-02 00:00 */
    { tzGMT, "en@calendar=roc",    -1988064000000.0, -2145744000000.0,  false,   -4, -59, -1825, -43800, -2628000, -157680000 }, /* BMG  5-Jan-02 00:00 -> BMG 10-Jan-03 00:00 */
    { tzGMT, "en@calendar=roc",    -1988064000000.0, -1672704000000.0,  false,    9, 119,  3650,  87600,  5256000,  315360000 }, /* BMG  5-Jan-02 00:00 -> MG   5-Dec-30 00:00 */
    { tzGMT, "en@calendar=roc",    -1672704000000.0, -1988064000000.0,  false,   -9,-119, -3650, -87600, -5256000, -315360000 }, /* MG   5-Dec-30 00:00 -> BMG  5-Jan-02 00:00 */
    { tzGMT, "en@calendar=coptic",-53026531200000.0,-52868851200000.0,  false,    4,  64,  1825,  43800,  2628000,  157680000 }, /* Er1  5-Nas-05 00:00 -> Er1 10-Nas-04 00:00 */
    { tzGMT, "en@calendar=coptic",-52868851200000.0,-53026531200000.0,  false,   -4, -64, -1825, -43800, -2628000, -157680000 }, /* Er1 10-Nas-04 00:00 -> Er1  5-Nas-05 00:00 */
    { tzGMT, "en@calendar=coptic",-53499571200000.0,-53341891200000.0,  false,    4,  64,  1825,  43800,  2628000,  157680000 }, /* Er0 10-Tou-04 00:00 -> Er0  5-Tou-02 00:00 */
    { tzGMT, "en@calendar=coptic",-53341891200000.0,-53499571200000.0,  false,   -4, -64, -1825, -43800, -2628000, -157680000 }, /* Er0  5-Tou-02 00:00 -> Er0 10-Tou-04 00:00 */
    { tzGMT, "en@calendar=coptic",-53341891200000.0,-53026531200000.0,  false,    9, 129,  3650,  87600,  5256000,  315360000 }, /* Er0  5-Tou-02 00:00 -> Er1  5-Nas-05 00:00 */
    { tzGMT, "en@calendar=coptic",-53026531200000.0,-53341891200000.0,  false,   -9,-129, -3650, -87600, -5256000, -315360000 }, /* Er1  5-Nas-05 00:00 -> Er0  5-Tou-02 00:00 */
    { NULL,        NULL,           0.0,               0.0,              false,    0,   0,     0,      0,        0,          0 }  /* terminator */
};

void TestFieldDifference(void) {
    const TFDItem * tfdItemPtr;
    for (tfdItemPtr = tfdItems; tfdItemPtr->timezone != NULL; tfdItemPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar* ucal = ucal_open(tfdItemPtr->timezone, -1, tfdItemPtr->locale, UCAL_DEFAULT, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL: for locale \"%s\", ucal_open had status %s\n", tfdItemPtr->locale, u_errorName(status) );
        } else {
            int32_t yDf, MDf, dDf, HDf, mDf, sDf;
            if (tfdItemPtr->progressive) {
                ucal_setMillis(ucal, tfdItemPtr->start, &status);
                yDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_YEAR, &status);
                MDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_MONTH, &status);
                dDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_DATE, &status);
                HDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_HOUR, &status);
                mDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_MINUTE, &status);
                sDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_SECOND, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: for locale \"%s\", start %.1f, target %.1f, ucal_setMillis or ucal_getFieldDifference had status %s\n",
                            tfdItemPtr->locale, tfdItemPtr->start, tfdItemPtr->target, u_errorName(status) );
                } else if ( yDf !=  tfdItemPtr->yDiff ||
                            MDf !=  tfdItemPtr->MDiff ||
                            dDf !=  tfdItemPtr->dDiff ||
                            HDf !=  tfdItemPtr->HDiff ||
                            mDf !=  tfdItemPtr->mDiff ||
                            sDf !=  tfdItemPtr->sDiff ) {
                    log_data_err("FAIL: for locale \"%s\", start %.1f, target %.1f, expected y-M-d-H-m-s progressive diffs %d-%d-%d-%d-%d-%d, got %d-%d-%d-%d-%d-%d\n",
                            tfdItemPtr->locale, tfdItemPtr->start, tfdItemPtr->target,
                            tfdItemPtr->yDiff, tfdItemPtr->MDiff, tfdItemPtr->dDiff, tfdItemPtr->HDiff, tfdItemPtr->mDiff, tfdItemPtr->sDiff,
                            yDf, MDf, dDf, HDf, mDf, sDf);
                }
            } else {
                ucal_setMillis(ucal, tfdItemPtr->start, &status);
                yDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_YEAR, &status);
                ucal_setMillis(ucal, tfdItemPtr->start, &status);
                MDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_MONTH, &status);
                ucal_setMillis(ucal, tfdItemPtr->start, &status);
                dDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_DATE, &status);
                ucal_setMillis(ucal, tfdItemPtr->start, &status);
                HDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_HOUR, &status);
                ucal_setMillis(ucal, tfdItemPtr->start, &status);
                mDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_MINUTE, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: for locale \"%s\", start %.1f, target %.1f, ucal_setMillis or ucal_getFieldDifference (y-M-d-H-m) had status %s\n",
                            tfdItemPtr->locale, tfdItemPtr->start, tfdItemPtr->target, u_errorName(status) );
                } else if ( yDf !=  tfdItemPtr->yDiff ||
                            MDf !=  tfdItemPtr->MDiff ||
                            dDf !=  tfdItemPtr->dDiff ||
                            HDf !=  tfdItemPtr->HDiff ||
                            mDf !=  tfdItemPtr->mDiff ) {
                    log_data_err("FAIL: for locale \"%s\", start %.1f, target %.1f, expected y-M-d-H-m total diffs %d-%d-%d-%d-%d, got %d-%d-%d-%d-%d\n",
                            tfdItemPtr->locale, tfdItemPtr->start, tfdItemPtr->target,
                            tfdItemPtr->yDiff, tfdItemPtr->MDiff, tfdItemPtr->dDiff, tfdItemPtr->HDiff, tfdItemPtr->mDiff,
                            yDf, MDf, dDf, HDf, mDf);
                }
                ucal_setMillis(ucal, tfdItemPtr->start, &status);
                sDf = ucal_getFieldDifference(ucal, tfdItemPtr->target, UCAL_SECOND, &status);
                if (tfdItemPtr->sDiff != 0x7FFFFFFF) {
                    if (U_FAILURE(status)) {
                        log_err("FAIL: for locale \"%s\", start %.1f, target %.1f, ucal_setMillis or ucal_getFieldDifference (seconds) had status %s\n",
                                tfdItemPtr->locale, tfdItemPtr->start, tfdItemPtr->target, u_errorName(status) );
                    } else if (sDf !=  tfdItemPtr->sDiff) {
                        log_data_err("FAIL: for locale \"%s\", start %.1f, target %.1f, expected seconds progressive diff %d, got %d\n",
                                tfdItemPtr->locale, tfdItemPtr->start, tfdItemPtr->target, tfdItemPtr->sDiff, sDf);
                    }
                } else if (!U_FAILURE(status)) {
                    log_err("FAIL: for locale \"%s\", start %.1f, target %.1f, for ucal_getFieldDifference (seconds) expected overflow error, got none\n",
                            tfdItemPtr->locale, tfdItemPtr->start, tfdItemPtr->target );
                }
            }
            ucal_close(ucal);
        }
    }
}

void TestAmbiguousWallTime(void) {
    UErrorCode status = U_ZERO_ERROR;
    UChar tzID[32];
    UCalendar* ucal;
    UDate t, expected;

    u_uastrcpy(tzID, "America/New_York");
    ucal = ucal_open(tzID, -1, "en_US", UCAL_DEFAULT, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: Failed to create a calendar");
        return;
    }

    if (ucal_getAttribute(ucal, UCAL_REPEATED_WALL_TIME) != UCAL_WALLTIME_LAST) {
        log_err("FAIL: Default UCAL_REPEATED_WALL_TIME value is not UCAL_WALLTIME_LAST");
    }

    if (ucal_getAttribute(ucal, UCAL_SKIPPED_WALL_TIME) != UCAL_WALLTIME_LAST) {
        log_err("FAIL: Default UCAL_SKIPPED_WALL_TIME value is not UCAL_WALLTIME_LAST");
    }

    /* UCAL_WALLTIME_FIRST on US fall transition */
    ucal_setAttribute(ucal, UCAL_REPEATED_WALL_TIME, UCAL_WALLTIME_FIRST);
    ucal_clear(ucal);
    ucal_setDateTime(ucal, 2011, 11-1, 6, 1, 30, 0, &status);
    t = ucal_getMillis(ucal, &status);
    expected = 1320557400000.0; /* 2011-11-06T05:30:00Z */
    if (U_FAILURE(status)) {
        log_err("FAIL: Calculating time 2011-11-06 01:30:00 with UCAL_WALLTIME_FIRST - %s\n", u_errorName(status));
        status = U_ZERO_ERROR;
    } else if (t != expected) {
        log_data_err("FAIL: 2011-11-06 01:30:00 with UCAL_WALLTIME_FIRST - got: %f, expected: %f\n", t, expected);
    }

    /* UCAL_WALLTIME_LAST on US fall transition */
    ucal_setAttribute(ucal, UCAL_REPEATED_WALL_TIME, UCAL_WALLTIME_LAST);
    ucal_clear(ucal);
    ucal_setDateTime(ucal, 2011, 11-1, 6, 1, 30, 0, &status);
    t = ucal_getMillis(ucal, &status);
    expected = 1320561000000.0; /* 2011-11-06T06:30:00Z */
    if (U_FAILURE(status)) {
        log_err("FAIL: Calculating time 2011-11-06 01:30:00 with UCAL_WALLTIME_LAST - %s\n", u_errorName(status));
        status = U_ZERO_ERROR;
    } else if (t != expected) {
        log_data_err("FAIL: 2011-11-06 01:30:00 with UCAL_WALLTIME_LAST - got: %f, expected: %f\n", t, expected);
    }

    /* UCAL_WALLTIME_FIRST on US spring transition */
    ucal_setAttribute(ucal, UCAL_SKIPPED_WALL_TIME, UCAL_WALLTIME_FIRST);
    ucal_clear(ucal);
    ucal_setDateTime(ucal, 2011, 3-1, 13, 2, 30, 0, &status);
    t = ucal_getMillis(ucal, &status);
    expected = 1299997800000.0; /* 2011-03-13T06:30:00Z */
    if (U_FAILURE(status)) {
        log_err("FAIL: Calculating time 2011-03-13 02:30:00 with UCAL_WALLTIME_FIRST - %s\n", u_errorName(status));
        status = U_ZERO_ERROR;
    } else if (t != expected) {
        log_data_err("FAIL: 2011-03-13 02:30:00 with UCAL_WALLTIME_FIRST - got: %f, expected: %f\n", t, expected);
    }

    /* UCAL_WALLTIME_LAST on US spring transition */
    ucal_setAttribute(ucal, UCAL_SKIPPED_WALL_TIME, UCAL_WALLTIME_LAST);
    ucal_clear(ucal);
    ucal_setDateTime(ucal, 2011, 3-1, 13, 2, 30, 0, &status);
    t = ucal_getMillis(ucal, &status);
    expected = 1300001400000.0; /* 2011-03-13T07:30:00Z */
    if (U_FAILURE(status)) {
        log_err("FAIL: Calculating time 2011-03-13 02:30:00 with UCAL_WALLTIME_LAST - %s\n", u_errorName(status));
        status = U_ZERO_ERROR;
    } else if (t != expected) {
        log_data_err("FAIL: 2011-03-13 02:30:00 with UCAL_WALLTIME_LAST - got: %f, expected: %f\n", t, expected);
    }

    /* UCAL_WALLTIME_NEXT_VALID on US spring transition */
    ucal_setAttribute(ucal, UCAL_SKIPPED_WALL_TIME, UCAL_WALLTIME_NEXT_VALID);
    ucal_clear(ucal);
    ucal_setDateTime(ucal, 2011, 3-1, 13, 2, 30, 0, &status);
    t = ucal_getMillis(ucal, &status);
    expected = 1299999600000.0; /* 2011-03-13T07:00:00Z */
    if (U_FAILURE(status)) {
        log_err("FAIL: Calculating time 2011-03-13 02:30:00 with UCAL_WALLTIME_NEXT_VALID - %s\n", u_errorName(status));
        status = U_ZERO_ERROR;
    } else if (t != expected) {
        log_data_err("FAIL: 2011-03-13 02:30:00 with UCAL_WALLTIME_NEXT_VALID - got: %f, expected: %f\n", t, expected);
    }

    /* non-lenient on US spring transition */
    ucal_setAttribute(ucal, UCAL_LENIENT, 0);
    ucal_clear(ucal);
    ucal_setDateTime(ucal, 2011, 3-1, 13, 2, 30, 0, &status);
    t = ucal_getMillis(ucal, &status);
    if (U_SUCCESS(status)) {
        /* must return error */
        log_data_err("FAIL: Non-lenient did not fail with 2011-03-13 02:30:00\n");
        status = U_ZERO_ERROR;
    }

    ucal_close(ucal);
}

/**
 * TestAddRollEra0AndEraBounds, for #9226
 */
 
 typedef struct {
     const char * locale;
     UBool era0YearsGoBackwards; /* until we have API to get this, per #9393 */
 } EraTestItem;

static const EraTestItem eraTestItems[] = {
    /* calendars with non-modern era 0 that goes backwards, max era == 1 */
    { "en@calendar=gregorian", true },
    { "en@calendar=roc", true },
    { "en@calendar=coptic", true },
    /* calendars with non-modern era 0 that goes forwards, max era > 1 */
    { "en@calendar=japanese", false },
    { "en@calendar=chinese", false },
    /* calendars with non-modern era 0 that goes forwards, max era == 1 */
    { "en@calendar=ethiopic", false },
    /* calendars with only one era  = 0, forwards */
    { "en@calendar=buddhist", false },
    { "en@calendar=hebrew", false },
    { "en@calendar=islamic", false },
    { "en@calendar=indian", false },
    { "en@calendar=persian", false },
    { "en@calendar=ethiopic-amete-alem", false },
    { NULL, false }
};

static const UChar zoneGMT[] = { 0x47,0x4D,0x54,0 };

void TestAddRollEra0AndEraBounds(void) {
    const EraTestItem * eraTestItemPtr;
    for (eraTestItemPtr = eraTestItems; eraTestItemPtr->locale != NULL; eraTestItemPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UCalendar *ucalTest = ucal_open(zoneGMT, -1, eraTestItemPtr->locale, UCAL_DEFAULT, &status);
        if ( U_SUCCESS(status) ) {
            int32_t yrBefore, yrAfter, yrMax, eraAfter, eraMax, eraNow;

            status = U_ZERO_ERROR;
            ucal_clear(ucalTest);
            ucal_set(ucalTest, UCAL_YEAR, 2);
            ucal_set(ucalTest, UCAL_ERA, 0);
            yrBefore = ucal_get(ucalTest, UCAL_YEAR, &status);
            ucal_add(ucalTest, UCAL_YEAR, 1, &status);
            yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
            if (U_FAILURE(status)) {
                log_err("FAIL: set era 0 year 2 then add 1 year and get year for %s, error %s\n",
                        eraTestItemPtr->locale, u_errorName(status));
            } else if ( (eraTestItemPtr->era0YearsGoBackwards && yrAfter>yrBefore) ||
                        (!eraTestItemPtr->era0YearsGoBackwards && yrAfter<yrBefore) ) {
                log_err("FAIL: era 0 add 1 year does not move forward in time for %s\n", eraTestItemPtr->locale);
            }
            
            status = U_ZERO_ERROR;
            ucal_clear(ucalTest);
            ucal_set(ucalTest, UCAL_YEAR, 2);
            ucal_set(ucalTest, UCAL_ERA, 0);
            yrBefore = ucal_get(ucalTest, UCAL_YEAR, &status);
            ucal_roll(ucalTest, UCAL_YEAR, 1, &status);
            yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
            if (U_FAILURE(status)) {
                log_err("FAIL: set era 0 year 2 then roll 1 year and get year for %s, error %s\n",
                        eraTestItemPtr->locale, u_errorName(status));
            } else if ( (eraTestItemPtr->era0YearsGoBackwards && yrAfter>yrBefore) ||
                        (!eraTestItemPtr->era0YearsGoBackwards && yrAfter<yrBefore) ) {
                log_err("FAIL: era 0 roll 1 year does not move forward in time for %s\n", eraTestItemPtr->locale);
            }
            
            status = U_ZERO_ERROR;
            ucal_clear(ucalTest);
            ucal_set(ucalTest, UCAL_YEAR, 1);
            ucal_set(ucalTest, UCAL_ERA, 0);
            if (eraTestItemPtr->era0YearsGoBackwards) {
                ucal_roll(ucalTest, UCAL_YEAR, 1, &status); /* roll forward in time to era 0 boundary */
                yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
                eraAfter = ucal_get(ucalTest, UCAL_ERA, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: set era 0 year 1 then roll 1 year and get year,era for %s, error %s\n",
                            eraTestItemPtr->locale, u_errorName(status));
                /* all calendars with era0YearsGoBackwards have "unbounded" era0 year values, so we should pin at yr 1 */
                } else if (eraAfter != 0 || yrAfter != 1) {
                    log_err("FAIL: era 0 roll 1 year from year 1 does not stay within era or pin to year 1 for %s (get era %d year %d)\n",
                            eraTestItemPtr->locale, eraAfter, yrAfter);
                }
            } else {
                /* roll backward in time to where era 0 years go negative, except for the Chinese
                   calendar, which uses negative eras instead of having years outside the range 1-60 */
                const char * calType = ucal_getType(ucalTest, &status);
                ucal_roll(ucalTest, UCAL_YEAR, -2, &status);
                yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
                eraAfter = ucal_get(ucalTest, UCAL_ERA, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: set era 0 year 1 then roll -2 years and get year,era for %s, error %s\n",
                            eraTestItemPtr->locale, u_errorName(status));
                } else if ( uprv_strcmp(calType,"chinese")!=0 && (eraAfter != 0 || yrAfter != -1) ) {
                    log_err("FAIL: era 0 roll -2 years from year 1 does not stay within era or produce year -1 for %s (get era %d year %d)\n",
                            eraTestItemPtr->locale, eraAfter, yrAfter);
                }
            }
            
            status = U_ZERO_ERROR;
            ucal_clear(ucalTest);
            {
                int32_t eraMin = ucal_getLimit(ucalTest, UCAL_ERA, UCAL_MINIMUM, &status);
                const char * calType = ucal_getType(ucalTest, &status);
                if (eraMin != 0 && uprv_strcmp(calType, "chinese") != 0) {
                    log_err("FAIL: ucal_getLimit returns minimum era %d (should be 0) for calType %s, error %s\n", eraMin, calType, u_errorName(status));
                }
            }

            status = U_ZERO_ERROR;
            ucal_clear(ucalTest);
            ucal_set(ucalTest, UCAL_YEAR, 1);
            ucal_set(ucalTest, UCAL_ERA, 0);
            eraMax = ucal_getLimit(ucalTest, UCAL_ERA, UCAL_MAXIMUM, &status);
            if ( U_SUCCESS(status) && eraMax > 0 ) {
                /* try similar tests for era 1 (if calendar has it), in which years always go forward */
                status = U_ZERO_ERROR;
                ucal_clear(ucalTest);
                ucal_set(ucalTest, UCAL_YEAR, 2);
                ucal_set(ucalTest, UCAL_ERA, 1);
                yrBefore = ucal_get(ucalTest, UCAL_YEAR, &status);
                ucal_add(ucalTest, UCAL_YEAR, 1, &status);
                yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: set era 1 year 2 then add 1 year and get year for %s, error %s\n",
                            eraTestItemPtr->locale, u_errorName(status));
                } else if ( yrAfter<yrBefore ) {
                    log_err("FAIL: era 1 add 1 year does not move forward in time for %s\n", eraTestItemPtr->locale);
                }
                
                status = U_ZERO_ERROR;
                ucal_clear(ucalTest);
                ucal_set(ucalTest, UCAL_YEAR, 2);
                ucal_set(ucalTest, UCAL_ERA, 1);
                yrBefore = ucal_get(ucalTest, UCAL_YEAR, &status);
                ucal_roll(ucalTest, UCAL_YEAR, 1, &status);
                yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: set era 1 year 2 then roll 1 year and get year for %s, error %s\n",
                            eraTestItemPtr->locale, u_errorName(status));
                } else if ( yrAfter<yrBefore ) {
                    log_err("FAIL: era 1 roll 1 year does not move forward in time for %s\n", eraTestItemPtr->locale);
                }
                
                status = U_ZERO_ERROR;
                ucal_clear(ucalTest);
                ucal_set(ucalTest, UCAL_YEAR, 1);
                ucal_set(ucalTest, UCAL_ERA, 1);
                yrMax = ucal_getLimit(ucalTest, UCAL_YEAR, UCAL_ACTUAL_MAXIMUM, &status); /* max year value for era 1 */
                ucal_roll(ucalTest, UCAL_YEAR, -1, &status); /* roll down which should pin or wrap to end */
                yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
                eraAfter = ucal_get(ucalTest, UCAL_ERA, &status);
                if (U_FAILURE(status)) {
                    log_err("FAIL: set era 1 year 1 then roll -1 year and get year,era for %s, error %s\n",
                            eraTestItemPtr->locale, u_errorName(status));
                /* if yrMax is reasonable we should wrap to that, else we should pin at yr 1 */
                } else if (yrMax >= 32768) {
                    if (eraAfter != 1 || yrAfter != 1) {
                        log_err("FAIL: era 1 roll -1 year from year 1 does not stay within era or pin to year 1 for %s (get era %d year %d)\n",
                                eraTestItemPtr->locale, eraAfter, yrAfter);
                    }
                } else if (eraAfter != 1 || yrAfter != yrMax) {
                    log_err("FAIL: era 1 roll -1 year from year 1 does not stay within era or wrap to year %d for %s (get era %d year %d)\n",
                            yrMax, eraTestItemPtr->locale, eraAfter, yrAfter);
                } else {
                    /* now roll up which should wrap to beginning */
                    ucal_roll(ucalTest, UCAL_YEAR, 1, &status); /* now roll up which should wrap to beginning */
                    yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
                    eraAfter = ucal_get(ucalTest, UCAL_ERA, &status);
                    if (U_FAILURE(status)) {
                        log_err("FAIL: era 1 roll 1 year from end and get year,era for %s, error %s\n",
                                eraTestItemPtr->locale, u_errorName(status));
                    } else if (eraAfter != 1 || yrAfter != 1) {
                        log_err("FAIL: era 1 roll 1 year from year %d does not stay within era or wrap to year 1 for %s (get era %d year %d)\n",
                                yrMax, eraTestItemPtr->locale, eraAfter, yrAfter);
                    }
                }

                /* if current era  > 1, try the same roll tests for current era */
                ucal_setMillis(ucalTest, ucal_getNow(), &status);
                eraNow = ucal_get(ucalTest, UCAL_ERA, &status);
                if ( U_SUCCESS(status) && eraNow > 1 ) {
                    status = U_ZERO_ERROR;
                    ucal_clear(ucalTest);
                    ucal_set(ucalTest, UCAL_YEAR, 1);
                    ucal_set(ucalTest, UCAL_ERA, eraNow);
                    yrMax = ucal_getLimit(ucalTest, UCAL_YEAR, UCAL_ACTUAL_MAXIMUM, &status); /* max year value for this era */
                    ucal_roll(ucalTest, UCAL_YEAR, -1, &status);
                    yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
                    eraAfter = ucal_get(ucalTest, UCAL_ERA, &status);
                    if (U_FAILURE(status)) {
                        log_err("FAIL: set era %d year 1 then roll -1 year and get year,era for %s, error %s\n",
                                eraNow, eraTestItemPtr->locale, u_errorName(status));
                    /* if yrMax is reasonable we should wrap to that, else we should pin at yr 1 */
                    } else if (yrMax >= 32768) {
                        if (eraAfter != eraNow || yrAfter != 1) {
                            log_err("FAIL: era %d roll -1 year from year 1 does not stay within era or pin to year 1 for %s (get era %d year %d)\n",
                                    eraNow, eraTestItemPtr->locale, eraAfter, yrAfter);
                        }
                    } else if (eraAfter != eraNow || yrAfter != yrMax) {
                        log_err("FAIL: era %d roll -1 year from year 1 does not stay within era or wrap to year %d for %s (get era %d year %d)\n",
                                eraNow, yrMax, eraTestItemPtr->locale, eraAfter, yrAfter);
                    } else {
                        /* now roll up which should wrap to beginning */
                        ucal_roll(ucalTest, UCAL_YEAR, 1, &status); /* now roll up which should wrap to beginning */
                        yrAfter = ucal_get(ucalTest, UCAL_YEAR, &status);
                        eraAfter = ucal_get(ucalTest, UCAL_ERA, &status);
                        if (U_FAILURE(status)) {
                            log_err("FAIL: era %d roll 1 year from end and get year,era for %s, error %s\n",
                                    eraNow, eraTestItemPtr->locale, u_errorName(status));
                        } else if (eraAfter != eraNow || yrAfter != 1) {
                            log_err("FAIL: era %d roll 1 year from year %d does not stay within era or wrap to year 1 for %s (get era %d year %d)\n",
                                    eraNow, yrMax, eraTestItemPtr->locale, eraAfter, yrAfter);
                        }
                    }
                }
            }

            ucal_close(ucalTest);
        } else {
            log_data_err("FAIL: ucal_open fails for zone GMT, locale %s, UCAL_DEFAULT\n", eraTestItemPtr->locale);
        }
    }
}

/**
 * TestGetTZTransition, for #9606
 */
 
typedef struct {
    const char *descrip;    /* test description */
    const UChar * zoneName; /* pointer to zero-terminated zone name */
    int32_t year;           /* starting point for test is gregorian calendar noon on day specified by y,M,d here */
    int32_t month;
    int32_t day;
    UBool hasPrev;          /* does it have a previous transition from starting point? If so we test inclusive from that */
    UBool hasNext;          /* does it have a next transition from starting point? If so we test inclusive from that */
} TZTransitionItem;

/* have zoneGMT above */
static const UChar zoneUSPacific[] = { 0x55,0x53,0x2F,0x50,0x61,0x63,0x69,0x66,0x69,0x63,0 }; /* "US/Pacific" */
static const UChar zoneCairo[]     = { 0x41,0x66,0x72,0x69,0x63,0x61,0x2F,0x43,0x61,0x69,0x72,0x6F,0 }; /* "Africa/Cairo", DST cancelled since 2011 */
static const UChar zoneIceland[]   = { 0x41,0x74,0x6C,0x61,0x6E,0x74,0x69,0x63,0x2F,0x52,0x65,0x79,0x6B,0x6A,0x61,0x76,0x69,0x6B,0 }; /* "Atlantic/Reykjavik", always on DST (since when?) */

static const TZTransitionItem tzTransitionItems[] = {
    { "USPacific mid 2012", zoneUSPacific, 2012, UCAL_JULY, 1, true , true  },
    { "USPacific mid  100", zoneUSPacific,  100, UCAL_JULY, 1, false, true  }, /* no transitions before 100 CE... */
    { "Cairo     mid 2012", zoneCairo,     2012, UCAL_JULY, 1, true , true  }, /* DST cancelled since 2011 (Changed since 2014c) */
    { "Iceland   mid 2012", zoneIceland,   2012, UCAL_JULY, 1, true , false }, /* always on DST */
    { NULL,                 NULL,             0,         0, 0, false, false } /* terminator */
};

void TestGetTZTransition(void) {
    UErrorCode status = U_ZERO_ERROR;
    UCalendar * ucal = ucal_open(zoneGMT, -1, "en", UCAL_GREGORIAN, &status);
    if ( U_SUCCESS(status) ) {
        const TZTransitionItem * itemPtr;
        for (itemPtr = tzTransitionItems; itemPtr->descrip != NULL; itemPtr++) {
            UDate curMillis;
            ucal_setTimeZone(ucal, itemPtr->zoneName, -1, &status);
            ucal_setDateTime(ucal, itemPtr->year, itemPtr->month, itemPtr->day, 12, 0, 0, &status);
            curMillis = ucal_getMillis(ucal, &status);
            (void)curMillis;    /* Suppress set but not used warning. */
            if ( U_SUCCESS(status) ) {
                UDate transition1, transition2;
                UBool result;
                
                result = ucal_getTimeZoneTransitionDate(ucal, UCAL_TZ_TRANSITION_PREVIOUS, &transition1, &status);
                if (U_FAILURE(status) || result != itemPtr->hasPrev) {
                    log_data_err("FAIL: %s ucal_getTimeZoneTransitionDate prev status %s, expected result %d but got %d\n",
                            itemPtr->descrip, u_errorName(status), itemPtr->hasPrev, result);
                } else if (result) {
                    ucal_setMillis(ucal, transition1, &status);
                    result = ucal_getTimeZoneTransitionDate(ucal, UCAL_TZ_TRANSITION_PREVIOUS_INCLUSIVE, &transition2, &status);
                    if (U_FAILURE(status) || !result || transition2 != transition1) {
                        log_err("FAIL: %s ucal_getTimeZoneTransitionDate prev_inc status %s, result %d, expected date %.1f but got %.1f\n",
                                itemPtr->descrip, u_errorName(status), result, transition1, transition2);
                    }
                }
                status = U_ZERO_ERROR;

                result = ucal_getTimeZoneTransitionDate(ucal, UCAL_TZ_TRANSITION_NEXT, &transition1, &status);
                if (U_FAILURE(status) || result != itemPtr->hasNext) {
                    log_data_err("FAIL: %s ucal_getTimeZoneTransitionDate next status %s, expected result %d but got %d\n",
                            itemPtr->descrip, u_errorName(status), itemPtr->hasNext, result);
                } else if (result) {
                    ucal_setMillis(ucal, transition1, &status);
                    result = ucal_getTimeZoneTransitionDate(ucal, UCAL_TZ_TRANSITION_NEXT_INCLUSIVE, &transition2, &status);
                    if (U_FAILURE(status) || !result || transition2 != transition1) {
                        log_err("FAIL: %s ucal_getTimeZoneTransitionDate next_inc status %s, result %d, expected date %.1f but got %.1f\n",
                                itemPtr->descrip, u_errorName(status), result, transition1, transition2);
                    }
                }
                status = U_ZERO_ERROR;
            } else {
                log_data_err("FAIL setup: can't setup calendar for %s, status %s\n",
                            itemPtr->descrip, u_errorName(status));
                status = U_ZERO_ERROR;
            }
        }
        ucal_close(ucal);
    } else {
        log_data_err("FAIL setup: ucal_open status %s\n", u_errorName(status));
    }
}

static const UChar winEastern[] = /* Eastern Standard Time */
    {0x45,0x61,0x73,0x74,0x65,0x72,0x6E,0x20,0x53,0x74,0x61,0x6E,0x64,0x61,0x72,0x64,0x20,0x54,0x69,0x6D,0x65,0x00};

static const UChar tzNewYork[] = /* America/New_York */
    {0x41,0x6D,0x65,0x72,0x69,0x63,0x61,0x2F,0x4E,0x65,0x77,0x5F,0x59,0x6F,0x72,0x6B,0x00};
static const UChar tzTronto[] = /* America/Toronto */
    {0x41,0x6D,0x65,0x72,0x69,0x63,0x61,0x2F,0x54,0x6F,0x72,0x6F,0x6E,0x74,0x6F,0x00};

static const UChar sBogus[] = /* Bogus */
    {0x42,0x6F,0x67,0x75,0x73,0x00};

#ifndef U_DEBUG
static const UChar sBogusWithVariantCharacters[] = /* Bogus with Variant characters: Hèℓℓô Wôřℓδ */
    {0x48,0xE8,0x2113,0x2113,0xF4,0x20,0x57,0xF4,0x159,0x2113,0x3B4,0x00};
#endif

void TestGetWindowsTimeZoneID(void) {
    UErrorCode status;
    UChar winID[64];
    int32_t len;

    {
        status = U_ZERO_ERROR;
        len = ucal_getWindowsTimeZoneID(tzNewYork, u_strlen(tzNewYork), winID, UPRV_LENGTHOF(winID), &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: Windows ID for America/New_York, status %s\n", u_errorName(status)); 
        } else if (len != u_strlen(winEastern) || u_strncmp(winID, winEastern, len) != 0) {
            log_data_err("FAIL: Windows ID for America/New_York\n");
        }
    }
    {
        status = U_ZERO_ERROR;
        len = ucal_getWindowsTimeZoneID(tzTronto, u_strlen(tzTronto), winID, UPRV_LENGTHOF(winID), &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: Windows ID for America/Toronto, status %s\n", u_errorName(status)); 
        } else if (len != u_strlen(winEastern) || u_strncmp(winID, winEastern, len) != 0) {
            log_data_err("FAIL: Windows ID for America/Toronto\n");
        }
    }
    {
        status = U_ZERO_ERROR;
        len = ucal_getWindowsTimeZoneID(sBogus, u_strlen(sBogus), winID, UPRV_LENGTHOF(winID), &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: Windows ID for Bogus, status %s\n", u_errorName(status)); 
        } else if (len != 0) {
            log_data_err("FAIL: Windows ID for Bogus\n");
        }
    }
}

void TestGetTimeZoneIDByWindowsID(void) {
    UErrorCode status;
    UChar tzID[64];
    int32_t len;

    {
        status = U_ZERO_ERROR;
        len = ucal_getTimeZoneIDForWindowsID(winEastern, -1, NULL, tzID, UPRV_LENGTHOF(tzID), &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: TZ ID for Eastern Standard Time, status %s\n", u_errorName(status)); 
        } else if (len != u_strlen(tzNewYork) || u_strncmp(tzID, tzNewYork, len) != 0) {
            log_err("FAIL: TZ ID for Eastern Standard Time\n");
        }
    }
    {
        status = U_ZERO_ERROR;
        len = ucal_getTimeZoneIDForWindowsID(winEastern, u_strlen(winEastern), "US", tzID, UPRV_LENGTHOF(tzID), &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: TZ ID for Eastern Standard Time - US, status %s\n", u_errorName(status)); 
        } else if (len != u_strlen(tzNewYork) || u_strncmp(tzID, tzNewYork, len) != 0) {
            log_err("FAIL: TZ ID for Eastern Standard Time - US\n");
        }
    }
    {
        status = U_ZERO_ERROR;
        len = ucal_getTimeZoneIDForWindowsID(winEastern, u_strlen(winEastern), "CA", tzID, UPRV_LENGTHOF(tzID), &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: TZ ID for Eastern Standard Time - CA, status %s\n", u_errorName(status)); 
        } else if (len != u_strlen(tzTronto) || u_strncmp(tzID, tzTronto, len) != 0) {
            log_err("FAIL: TZ ID for Eastern Standard Time - CA\n");
        }
    }
    {
        status = U_ZERO_ERROR;
        len = ucal_getTimeZoneIDForWindowsID(sBogus, -1, NULL, tzID, UPRV_LENGTHOF(tzID), &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: TZ ID for Bogus, status %s\n", u_errorName(status)); 
        } else if (len != 0) {
            log_err("FAIL: TZ ID for Bogus\n");
        }
    }
#ifndef U_DEBUG
    // This test is only for release mode because it will cause an assertion failure in debug builds.
    // We don't check the API result for errors as the only purpose of this test is to ensure that
    // input variant characters don't cause abort() to be called and/or that ICU doesn't crash.
    {
        status = U_ZERO_ERROR;
        len = ucal_getTimeZoneIDForWindowsID(sBogusWithVariantCharacters, -1, NULL, tzID, UPRV_LENGTHOF(tzID), &status);
    }
#endif
}

// The following currently assumes that Reiwa is the last known/valid era.
// Filed ICU-20551 to generalize this when we have more time...
void TestJpnCalAddSetNextEra(void) {
    UErrorCode status = U_ZERO_ERROR;
    UCalendar *jCal = ucal_open(NULL, 0, "ja_JP@calendar=japanese", UCAL_DEFAULT, &status);
    if ( U_FAILURE(status) ) {
        log_data_err("FAIL: ucal_open for ja_JP@calendar=japanese, status %s\n", u_errorName(status));
    } else {
        ucal_clear(jCal); // This sets to 1970, in Showa
        int32_t sEra = ucal_get(jCal, UCAL_ERA, &status); // Don't assume era number for Showa
        if ( U_FAILURE(status) ) {
            log_data_err("FAIL: ucal_get ERA for Showa, status %s\n", u_errorName(status));
        } else {
            int32_t iEra, eYear;
            int32_t startYears[4] = { 1926, 1989, 2019, 0 }; // start years for Showa, Heisei, Reiwa; 0 marks invalid era
            for (iEra = 1; iEra < 3; iEra++) {
                status = U_ZERO_ERROR;
                ucal_clear(jCal);
                ucal_set(jCal, UCAL_ERA, sEra+iEra);
                eYear = ucal_get(jCal, UCAL_EXTENDED_YEAR, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: set %d, ucal_get EXTENDED_YEAR, status %s\n", iEra, u_errorName(status));
                } else if (eYear != startYears[iEra]) {
                    log_err("ERROR: set %d, expected start year %d but get %d\n", iEra, startYears[iEra], eYear);
                } else {
                    ucal_add(jCal, UCAL_ERA, 1, &status);
                    if ( U_FAILURE(status) ) {
                        log_err("FAIL: set %d, ucal_add ERA 1, status %s\n", iEra, u_errorName(status));
                    } else {
                        eYear = ucal_get(jCal, UCAL_EXTENDED_YEAR, &status);
                        if ( U_FAILURE(status) ) {
                            log_err("FAIL: set %d then add ERA 1, ucal_get EXTENDED_YEAR, status %s\n", iEra, u_errorName(status));
                        } else {
                            // If this is the last valid era, we expect adding an era to pin to the current era
                            int32_t nextEraStart = (startYears[iEra+1] == 0)? startYears[iEra]: startYears[iEra+1];
                            if (eYear != nextEraStart) {
                                log_err("ERROR: set %d then add ERA 1, expected start year %d but get %d\n", iEra, nextEraStart, eYear);
                            }
                        }
                    }
                }
             }
        }
        ucal_close(jCal);
    }
}

void TestUcalOpenBufferRead(void) {
    // ICU-21004: The issue shows under valgrind or as an Address Sanitizer failure.
    UErrorCode status = U_ZERO_ERROR;
    // string length: 157 + 1 + 100 = 258
    const char *localeID = "x-privatebutreallylongtagfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobar-foobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoorbarfoobarfoo";
    UCalendar *cal = ucal_open(NULL, 0, localeID, UCAL_GREGORIAN, &status);
    ucal_close(cal);
}


/*
 * Testing ucal_getTimeZoneOffsetFromLocal
 */
void
TestGetTimeZoneOffsetFromLocal(void) {
    static const UChar utc[] = u"Etc/GMT";

    const int32_t HOUR = 60*60*1000;
    const int32_t MINUTE = 60*1000;

    const int32_t DATES[][6] = {
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

    // Expected offsets by
    // void U_ucal_getTimeZoneOffsetFromLocal(
    //   const UCalendar* cal,
    //   UTimeZoneLocalOption nonExistingTimeOpt,
    //   UTimeZoneLocalOption duplicatedTimeOpt,
    //   int32_t* rawOffset, int32_t* dstOffset, UErrorCode* status);
    // with nonExistingTimeOpt=UCAL_TZ_LOCAL_STANDARD and
    // duplicatedTimeOpt=UCAL_TZ_LOCAL_STANDARD
    const int32_t OFFSETS2[][2] = {
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

    // Expected offsets by
    // void U_ucal_getTimeZoneOffsetFromLocal(
    //   const UCalendar* cal,
    //   UTimeZoneLocalOption nonExistingTimeOpt,
    //   UTimeZoneLocalOption duplicatedTimeOpt,
    //   int32_t* rawOffset, int32_t* dstOffset, UErrorCode* status);
    // with nonExistingTimeOpt=UCAL_TZ_LOCAL_DAYLIGHT and
    // duplicatedTimeOpt=UCAL_TZ_LOCAL_DAYLIGHT
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
    UCalendar *cal = ucal_open(utc, -1, "en", UCAL_GREGORIAN, &status);
    if (U_FAILURE(status)) {
        log_data_err("ucal_open: %s", u_errorName(status));
        return;
    }

    // Calculate millis
    UDate MILLIS[UPRV_LENGTHOF(DATES)];
    for (int32_t i = 0; i < UPRV_LENGTHOF(DATES); i++) {
        ucal_setDateTime(cal, DATES[i][0], DATES[i][1], DATES[i][2],
                         DATES[i][3], DATES[i][4], 0, &status);
        MILLIS[i] = ucal_getMillis(cal, &status);
        if (U_FAILURE(status)) {
            log_data_err("ucal_getMillis failed");
            return;
        }
    }
    ucal_setTimeZone(cal, AMERICA_LOS_ANGELES, -1, &status);

    // Test void ucal_getTimeZoneOffsetFromLocal(
    // const UCalendar* cal,
    // UTimeZoneLocalOption nonExistingTimeOpt,
    // UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t* rawOffset, int32_t* dstOffset, UErrorCode* status);
    // with nonExistingTimeOpt=UCAL_TZ_LOCAL_STANDARD and
    // duplicatedTimeOpt=UCAL_TZ_LOCAL_STANDARD
    for (int m = 0; m < UPRV_LENGTHOF(DATES); m++) {
        status = U_ZERO_ERROR;
        ucal_setMillis(cal, MILLIS[m], &status);
        if (U_FAILURE(status)) {
            log_data_err("ucal_setMillis: %s\n", u_errorName(status));
        }

        ucal_getTimeZoneOffsetFromLocal(cal, UCAL_TZ_LOCAL_STANDARD_FORMER, UCAL_TZ_LOCAL_STANDARD_LATTER,
            &rawOffset, &dstOffset, &status);
        if (U_FAILURE(status)) {
            log_err("ERROR: ucal_getTimeZoneOffsetFromLocal((%d-%d-%d %d:%d:0),"
                    "UCAL_TZ_LOCAL_STANDARD_FORMER, UCAL_TZ_LOCAL_STANDARD_LATTER: %s\n",
                    DATES[m][0], DATES[m][1], DATES[m][2], DATES[m][3], DATES[m][4],
                    u_errorName(status));
        } else if (rawOffset != OFFSETS2[m][0] || dstOffset != OFFSETS2[m][1]) {
            log_err("Bad offset returned at (%d-%d-%d %d:%d:0) "
                    "(wall/UCAL_TZ_LOCAL_STANDARD_FORMER/UCAL_TZ_LOCAL_STANDARD_LATTER) \n- Got: %d / %d "
                    " Expected %d / %d\n",
                    DATES[m][0], DATES[m][1], DATES[m][2], DATES[m][3], DATES[m][4],
                    rawOffset, dstOffset, OFFSETS2[m][0], OFFSETS2[m][1]);
        }
    }

    // Test void ucal_getTimeZoneOffsetFromLocal(
    // const UCalendar* cal,
    // UTimeZoneLocalOption nonExistingTimeOpt,
    // UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t* rawOffset, int32_t* dstOffset, UErrorCode* status);
    // with nonExistingTimeOpt=UCAL_TZ_LOCAL_DAYLIGHT and
    // duplicatedTimeOpt=UCAL_TZ_LOCAL_DAYLIGHT
    for (int m = 0; m < UPRV_LENGTHOF(DATES); m++) {
        status = U_ZERO_ERROR;
        ucal_setMillis(cal, MILLIS[m], &status);
        if (U_FAILURE(status)) {
            log_data_err("ucal_setMillis: %s\n", u_errorName(status));
        }

        ucal_getTimeZoneOffsetFromLocal(cal, UCAL_TZ_LOCAL_DAYLIGHT_LATTER, UCAL_TZ_LOCAL_DAYLIGHT_FORMER,
            &rawOffset, &dstOffset, &status);
        if (U_FAILURE(status)) {
            log_err("ERROR: ucal_getTimeZoneOffsetFromLocal((%d-%d-%d %d:%d:0),"
                    "UCAL_TZ_LOCAL_DAYLIGHT_LATTER, UCAL_TZ_LOCAL_DAYLIGHT_FORMER: %s\n",
                    DATES[m][0], DATES[m][1], DATES[m][2], DATES[m][3], DATES[m][4],
                    u_errorName(status));
        } else if (rawOffset != OFFSETS3[m][0] || dstOffset != OFFSETS3[m][1]) {
            log_err("Bad offset returned at (%d-%d-%d %d:%d:0) "
                    "(wall/UCAL_TZ_LOCAL_DAYLIGHT_LATTER/UCAL_TZ_LOCAL_DAYLIGHT_FORMER) \n- Got: %d / %d "
                    " Expected %d / %d\n",
                    DATES[m][0], DATES[m][1], DATES[m][2], DATES[m][3], DATES[m][4],
                    rawOffset, dstOffset, OFFSETS3[m][0], OFFSETS3[m][1]);
        }
    }

    // Test void ucal_getTimeZoneOffsetFromLocal(
    // const UCalendar* cal,
    // UTimeZoneLocalOption nonExistingTimeOpt,
    // UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t* rawOffset, int32_t* dstOffset, UErrorCode* status);
    // with nonExistingTimeOpt=UCAL_TZ_LOCAL_FORMER and
    // duplicatedTimeOpt=UCAL_TZ_LOCAL_LATTER
    for (int m = 0; m < UPRV_LENGTHOF(DATES); m++) {
        status = U_ZERO_ERROR;
        ucal_setMillis(cal, MILLIS[m], &status);
        if (U_FAILURE(status)) {
            log_data_err("ucal_setMillis: %s\n", u_errorName(status));
        }

        ucal_getTimeZoneOffsetFromLocal(cal, UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_LATTER,
            &rawOffset, &dstOffset, &status);
        if (U_FAILURE(status)) {
            log_err("ERROR: ucal_getTimeZoneOffsetFromLocal((%d-%d-%d %d:%d:0),"
                    "UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_LATTER: %s\n",
                    DATES[m][0], DATES[m][1], DATES[m][2], DATES[m][3], DATES[m][4],
                    u_errorName(status));
        } else if (rawOffset != OFFSETS2[m][0] || dstOffset != OFFSETS2[m][1]) {
            log_err("Bad offset returned at (%d-%d-%d %d:%d:0) "
                    "(wall/UCAL_TZ_LOCAL_FORMER/UCAL_TZ_LOCAL_LATTER) \n- Got: %d / %d "
                    " Expected %d / %d\n",
                    DATES[m][0], DATES[m][1], DATES[m][2], DATES[m][3], DATES[m][4],
                    rawOffset, dstOffset, OFFSETS2[m][0], OFFSETS2[m][1]);
        }
    }

    // Test void ucal_getTimeZoneOffsetFromLocal(
    // const UCalendar* cal,
    // UTimeZoneLocalOption nonExistingTimeOpt,
    // UTimeZoneLocalOption duplicatedTimeOpt,
    // int32_t* rawOffset, int32_t* dstOffset, UErrorCode* status);
    // with nonExistingTimeOpt=UCAL_TZ_LOCAL_LATTER and
    // duplicatedTimeOpt=UCAL_TZ_LOCAL_FORMER
    for (int m = 0; m < UPRV_LENGTHOF(DATES); m++) {
        status = U_ZERO_ERROR;
        ucal_setMillis(cal, MILLIS[m], &status);
        if (U_FAILURE(status)) {
            log_data_err("ucal_setMillis: %s\n", u_errorName(status));
        }

        ucal_getTimeZoneOffsetFromLocal(cal, UCAL_TZ_LOCAL_LATTER, UCAL_TZ_LOCAL_FORMER,
            &rawOffset, &dstOffset, &status);
        if (U_FAILURE(status)) {
            log_err("ERROR: ucal_getTimeZoneOffsetFromLocal((%d-%d-%d %d:%d:0),"
                    "UCAL_TZ_LOCAL_LATTER, UCAL_TZ_LOCAL_FORMER: %s\n",
                    DATES[m][0], DATES[m][1], DATES[m][2], DATES[m][3], DATES[m][4],
                    u_errorName(status));
        } else if (rawOffset != OFFSETS3[m][0] || dstOffset != OFFSETS3[m][1]) {
            log_err("Bad offset returned at (%d-%d-%d %d:%d:0) "
                    "(wall/UCAL_TZ_LOCAL_LATTER/UCAL_TZ_LOCAL_FORMER) \n- Got: %d / %d "
                    " Expected %d / %d\n",
                    DATES[m][0], DATES[m][1], DATES[m][2], DATES[m][3], DATES[m][4],
                    rawOffset, dstOffset, OFFSETS3[m][0], OFFSETS3[m][1]);
        }
    }
    ucal_close(cal);
}

void
TestFWWithISO8601(void) {
    /* UCAL_SUNDAY is 1, UCAL_MONDAY is 2, ..., UCAL_SATURDAY is 7 */
    const char* LOCALES[] = {
        "",
        "en-u-ca-iso8601-fw-sun",
        "en-u-ca-iso8601-fw-mon",
        "en-u-ca-iso8601-fw-tue",
        "en-u-ca-iso8601-fw-wed",
        "en-u-ca-iso8601-fw-thu",
        "en-u-ca-iso8601-fw-fri",
        "en-u-ca-iso8601-fw-sat",
    };
    for (int32_t i = UCAL_SUNDAY; i <= UCAL_SATURDAY; i++) {
        const char* locale = LOCALES[i];
        UErrorCode status = U_ZERO_ERROR;
        UCalendar* cal = ucal_open(0, 0, locale, UCAL_TRADITIONAL, &status);
        if(U_FAILURE(status)){
            log_data_err("FAIL: error in ucal_open caldef : %s\n - (Are you missing data?)", u_errorName(status));
        }
        int32_t actual = ucal_getAttribute(cal, UCAL_FIRST_DAY_OF_WEEK);
        if (i != actual) {
            log_err("ERROR: ucal_getAttribute(\"%s\", UCAL_FIRST_DAY_OF_WEEK) should be %d but get %d\n",
                    locale, i, actual);
        }
        ucal_close(cal);
    }
}

void
TestGetIanaTimeZoneID(void) {
    const UChar* UNKNOWN = u"Etc/Unknown";
    typedef struct {
        const UChar* id;
        const UChar* expected;
    } IanaTimeZoneIDTestData;
    
    const IanaTimeZoneIDTestData TESTDATA[] = {
        {u"",                   UNKNOWN},
        {0,                     UNKNOWN},
        {UNKNOWN,               UNKNOWN},
        {u"America/New_York",   u"America/New_York"},
        {u"Asia/Calcutta",      u"Asia/Kolkata"},
        {u"Europe/Kiev",        u"Europe/Kyiv"},
        {u"Europe/Zaporozhye",  u"Europe/Kyiv"},
        {u"Etc/GMT-1",          u"Etc/GMT-1"},
        {u"Etc/GMT+20",         UNKNOWN},
        {u"PST8PDT",            u"PST8PDT"},
        {u"GMT-08:00",          UNKNOWN},
        {0,                     0}
    };

    for (int32_t i = 0; TESTDATA[i].expected != 0; i++) {
        UErrorCode sts = U_ZERO_ERROR;
        UChar ianaID[128];
        int32_t ianaLen = 0;

        ianaLen = ucal_getIanaTimeZoneID(TESTDATA[i].id, -1, ianaID, sizeof(ianaID), &sts);

        if (u_strcmp(TESTDATA[i].expected, UNKNOWN) == 0) {
            if (sts != U_ILLEGAL_ARGUMENT_ERROR) {
                log_err("Expected U_ILLEGAL_ERROR: TESTDATA[%d]", i);
            }
        } else {
            if (u_strlen(TESTDATA[i].expected) != ianaLen || u_strncmp(TESTDATA[i].expected, ianaID, ianaLen) != 0) {
                log_err("Error: TESTDATA[%d]", i);
            }
            // Calling ucal_getIanaTimeZoneID with an IANA ID should return the same
            UChar ianaID2[128];
            int32_t ianaLen2 = 0;
            ianaLen2 = ucal_getIanaTimeZoneID(ianaID, ianaLen, ianaID2, sizeof(ianaID2), &sts);
            if (U_FAILURE(sts) || ianaLen != ianaLen2 || u_strncmp(ianaID, ianaID2, ianaLen) != 0) {
                    log_err("Error: IANA ID for IANA ID %s", ianaID);
                }
        }
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
