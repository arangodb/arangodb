/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   bcrypt_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for bcrypt.hpp
 */

#include <boost/winapi/bcrypt.hpp>
#include <windows.h>
#include <wchar.h>
#include <boost/predef/platform/windows_uwp.h>
#if (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6)
#include <bcrypt.h>
#endif
#include "abi_test_tools.hpp"

int main()
{
#if (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6)
#if BOOST_WINAPI_PARTITION_APP_SYSTEM

    BOOST_WINAPI_TEST_TYPE_SAME(BCRYPT_ALG_HANDLE);

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(BCryptCloseAlgorithmProvider);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(BCryptGenRandom);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(BCryptOpenAlgorithmProvider);

    BOOST_TEST(wcscmp(BCRYPT_RNG_ALGORITHM, boost::winapi::BCRYPT_RNG_ALGORITHM_) == 0);

#endif // BOOST_WINAPI_PARTITION_APP_SYSTEM
#endif // (BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6)

    return boost::report_errors();
}
