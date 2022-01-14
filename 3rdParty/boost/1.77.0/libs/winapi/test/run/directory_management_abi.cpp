/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   directory_management_abi.cpp
 * \author Andrey Semashev
 * \date   09.03.2018
 *
 * \brief  This file contains ABI test for directory_management.hpp
 */

#include <boost/winapi/directory_management.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if !defined( BOOST_NO_ANSI_APIS )
#if BOOST_WINAPI_PARTITION_APP_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetTempPathA);
#endif
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(RemoveDirectoryA);
#endif
#if BOOST_WINAPI_PARTITION_APP_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetTempPathW);
#endif
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(RemoveDirectoryW);

    return boost::report_errors();
}
