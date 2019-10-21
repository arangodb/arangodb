/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   environment_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for environment.hpp
 */

#include <boost/winapi/environment.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

#undef GetEnvironmentStrings

int main()
{
#if BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM

#if !defined(BOOST_NO_ANSI_APIS)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetEnvironmentStrings);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(FreeEnvironmentStringsA);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetEnvironmentVariableA);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(SetEnvironmentVariableA);
#endif // !defined(BOOST_NO_ANSI_APIS)

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetEnvironmentStringsW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(FreeEnvironmentStringsW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetEnvironmentVariableW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(SetEnvironmentVariableW);

#endif // BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM

    return boost::report_errors();
}
