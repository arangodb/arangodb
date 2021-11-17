/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   local_memory_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for local_memory.hpp
 */

#include <boost/winapi/local_memory.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_APP_SYSTEM
    BOOST_WINAPI_TEST_TYPE_SAME(HLOCAL);

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(LocalAlloc);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(LocalReAlloc);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(LocalFree);
#endif // BOOST_WINAPI_PARTITION_APP_SYSTEM

    return boost::report_errors();
}
