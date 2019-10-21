/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   get_system_directory_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for get_system_directory.hpp
 */

#include <boost/winapi/get_system_directory.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_DESKTOP
#if !defined(BOOST_NO_ANSI_APIS)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetSystemDirectoryA);
#endif
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetSystemDirectoryW);
#endif // BOOST_WINAPI_PARTITION_DESKTOP

    return boost::report_errors();
}
