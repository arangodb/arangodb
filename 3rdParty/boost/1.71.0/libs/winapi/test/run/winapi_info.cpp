/*
 *             Copyright Andrey Semashev 2017.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   winapi_info.cpp
 * \author Andrey Semashev
 * \date   25.06.2017
 *
 * \brief  This file contains a test that displays information about Windows SDK.
 */

#define BOOST_USE_WINDOWS_H

#include <windows.h>
#include <boost/winapi/config.hpp>
#include <stdio.h>
#include <string.h>
#include <boost/config.hpp>
#include <boost/predef/platform/windows_uwp.h>

void print_macro(const char* name, const char* value)
{
    // If a macro is not defined, value will contain the macro name after the starting dot
    if (strcmp(name, value + 1) == 0)
    {
        printf("%s is not defined\n", name);
    }
    else
    {
        printf("%s: \"%s\"\n", name, value + 1);
    }
}

#define PRINT_MACRO(x) print_macro(#x, BOOST_STRINGIZE(.x))

void print_winsdk_macros()
{
    printf("Windows SDK macros:\n===================\n\n");

    PRINT_MACRO(_WIN32_WINNT);
    PRINT_MACRO(WINVER);
    PRINT_MACRO(NTDDI_VERSION);
    PRINT_MACRO(_WIN32_IE);
    PRINT_MACRO(BOOST_USE_WINAPI_VERSION);
    PRINT_MACRO(BOOST_NO_ANSI_APIS);
    PRINT_MACRO(BOOST_WINAPI_IS_MINGW);
    PRINT_MACRO(BOOST_WINAPI_IS_MINGW_W64);
    PRINT_MACRO(BOOST_WINAPI_IS_CYGWIN);
    PRINT_MACRO(BOOST_PLAT_WINDOWS_SDK_VERSION);
    PRINT_MACRO(__W32API_VERSION);
    PRINT_MACRO(__W32API_MAJOR_VERSION);
    PRINT_MACRO(__W32API_MINOR_VERSION);
    PRINT_MACRO(__W32API_PATCHLEVEL);
    PRINT_MACRO(WINAPI_FAMILY);
    PRINT_MACRO(UNDER_CE);
    PRINT_MACRO(_WIN32_WCE_EMULATION);
    PRINT_MACRO(WIN32);
    PRINT_MACRO(_WIN32);
    PRINT_MACRO(__WIN32__);
    PRINT_MACRO(_WIN64);
    PRINT_MACRO(__CYGWIN__);
    PRINT_MACRO(_X86_);
    PRINT_MACRO(_AMD64_);
    PRINT_MACRO(_IA64_);
    PRINT_MACRO(_ARM_);
    PRINT_MACRO(_ARM64_);
    PRINT_MACRO(_MAC);
    PRINT_MACRO(_MANAGED);
    PRINT_MACRO(UNICODE);
    PRINT_MACRO(_UNICODE);
    PRINT_MACRO(STRICT);
    PRINT_MACRO(NO_STRICT);
    PRINT_MACRO(WIN32_LEAN_AND_MEAN);

    printf("\n");

    PRINT_MACRO(WINBASEAPI);
    PRINT_MACRO(NTSYSAPI);
    PRINT_MACRO(WINAPI);
    PRINT_MACRO(NTAPI);
    PRINT_MACRO(CALLBACK);
    PRINT_MACRO(APIENTRY);
    PRINT_MACRO(BOOST_WINAPI_WINAPI_CC);
    PRINT_MACRO(BOOST_WINAPI_NTAPI_CC);
    PRINT_MACRO(BOOST_WINAPI_CALLBACK_CC);
    PRINT_MACRO(VOID);
    PRINT_MACRO(CONST);
}

int main(int, char*[])
{
    print_winsdk_macros();
    return 0;
}
