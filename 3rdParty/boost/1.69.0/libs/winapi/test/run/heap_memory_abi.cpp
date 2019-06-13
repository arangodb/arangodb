/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   heap_memory_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for heap_memory.hpp
 */

#include <boost/winapi/heap_memory.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_DESKTOP_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetProcessHeaps);
#endif

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetProcessHeap);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(HeapAlloc);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(HeapFree);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(HeapReAlloc);

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(HeapCreate);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(HeapDestroy);
#endif

    return boost::report_errors();
}
