/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   page_protection_flags_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for page_protection_flags.hpp
 */

#include <boost/winapi/page_protection_flags.hpp>
#include <windows.h>
#include <boost/predef/platform/windows_uwp.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_CONSTANT(PAGE_NOACCESS);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_READONLY);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_READWRITE);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_WRITECOPY);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_GUARD);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_NOCACHE);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_WRITECOMBINE);

#if BOOST_PLAT_WINDOWS_SDK_VERSION >= BOOST_WINAPI_WINDOWS_SDK_10_0
    // Older Windows SDK versions don't define these constants or define for limited API partitions
    BOOST_WINAPI_TEST_CONSTANT(PAGE_EXECUTE);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_EXECUTE_READ);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_EXECUTE_READWRITE);
    BOOST_WINAPI_TEST_CONSTANT(PAGE_EXECUTE_WRITECOPY);
#endif // BOOST_PLAT_WINDOWS_SDK_VERSION >= BOOST_WINAPI_WINDOWS_SDK_10_0

    return boost::report_errors();
}
