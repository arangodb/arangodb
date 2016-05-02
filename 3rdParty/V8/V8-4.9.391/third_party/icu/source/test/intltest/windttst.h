/*
********************************************************************************
*   Copyright (C) 2005-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINDTTST.H
*
********************************************************************************
*/

#ifndef __WINDTTST
#define __WINDTTST

#include "unicode/utypes.h"

#if U_PLATFORM_HAS_WIN32_API

#if !UCONFIG_NO_FORMATTING

/**
 * \file 
 * \brief C++ API: Format dates using Windows API.
 */

class TestLog;

class Win32DateTimeTest
{
public:
    static void testLocales(TestLog *log);

private:
    Win32DateTimeTest();
};

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // U_PLATFORM_HAS_WIN32_API

#endif // __WINDTTST
