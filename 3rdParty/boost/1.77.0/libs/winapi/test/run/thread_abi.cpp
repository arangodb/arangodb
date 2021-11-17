/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   thread_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for thread.hpp
 */

#include <boost/winapi/thread.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_APP_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(SleepEx);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(Sleep);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(SwitchToThread);
#endif // BOOST_WINAPI_PARTITION_APP_SYSTEM

    return boost::report_errors();
}
