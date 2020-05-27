/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   time.hpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for time.hpp
 */

#include <boost/winapi/time.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_DESKTOP_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetTickCount);
#endif
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetTickCount64);
#endif

    BOOST_WINAPI_TEST_STRUCT(FILETIME, (dwLowDateTime)(dwHighDateTime));
    BOOST_WINAPI_TEST_STRUCT(SYSTEMTIME, (wYear)(wMonth)(wDayOfWeek)(wDay)(wHour)(wMinute)(wSecond)(wMilliseconds));

    return boost::report_errors();
}
