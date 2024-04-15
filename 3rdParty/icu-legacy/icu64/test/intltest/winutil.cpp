// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2005-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINUTIL.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if U_PLATFORM_HAS_WIN32_API

#if !UCONFIG_NO_FORMATTING

#include "cmemory.h"
#include "winutil.h"
#include "locmap.h"
#include "unicode/uloc.h"

#   define WIN32_LEAN_AND_MEAN
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX
#   include <windows.h>
#   include <stdio.h>
#   include <string.h>

static Win32Utilities::LCIDRecord *lcidRecords = NULL;
static int32_t lcidCount  = 0;
static int32_t lcidMax = 0;

// TODO: Note that this test will skip locale names and only hit locales with assigned LCIDs
BOOL CALLBACK EnumLocalesProc(LPSTR lpLocaleString)
{
    char localeID[ULOC_FULLNAME_CAPACITY];
	int32_t localeIDLen;
    UErrorCode status = U_ZERO_ERROR;

    if (lcidCount >= lcidMax) {
        Win32Utilities::LCIDRecord *newRecords = new Win32Utilities::LCIDRecord[lcidMax + 32];

        for (int i = 0; i < lcidMax; i += 1) {
            newRecords[i] = lcidRecords[i];
        }

        delete[] lcidRecords;
        lcidRecords = newRecords;
        lcidMax += 32;
    }

    sscanf(lpLocaleString, "%8x", &lcidRecords[lcidCount].lcid);

    localeIDLen = uprv_convertToPosix(lcidRecords[lcidCount].lcid, localeID, UPRV_LENGTHOF(localeID), &status);
    if (U_SUCCESS(status)) {
        lcidRecords[lcidCount].localeID = new char[localeIDLen + 1];
        memcpy(lcidRecords[lcidCount].localeID, localeID, localeIDLen);
        lcidRecords[lcidCount].localeID[localeIDLen] = 0;
    } else {
        lcidRecords[lcidCount].localeID = NULL;
    }

    lcidCount += 1;

    return TRUE;
}

// TODO: Note that this test will skip locale names and only hit locales with assigned LCIDs
Win32Utilities::LCIDRecord *Win32Utilities::getLocales(int32_t &localeCount)
{
    LCIDRecord *result;

    EnumSystemLocalesA(EnumLocalesProc, LCID_INSTALLED);

    localeCount = lcidCount;
    result      = lcidRecords;

    lcidCount = lcidMax = 0;
    lcidRecords = NULL;

    return result;
}

void Win32Utilities::freeLocales(LCIDRecord *records)
{
    for (int i = 0; i < lcidCount; i++) {
        delete lcidRecords[i].localeID;
    }
    delete[] records;
}

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* U_PLATFORM_HAS_WIN32_API */
