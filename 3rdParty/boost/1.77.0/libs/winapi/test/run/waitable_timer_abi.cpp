/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   waitable_timer_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for waitable_timer.hpp
 */

#include <boost/winapi/waitable_timer.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_APP_SYSTEM

    BOOST_WINAPI_TEST_TYPE_SAME(PTIMERAPCROUTINE);

    BOOST_WINAPI_TEST_CONSTANT(TIMER_ALL_ACCESS);
    BOOST_WINAPI_TEST_CONSTANT(TIMER_MODIFY_STATE);
    BOOST_WINAPI_TEST_CONSTANT(TIMER_QUERY_STATE);

#if !defined( BOOST_NO_ANSI_APIS )
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(OpenWaitableTimerA);
#endif
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(OpenWaitableTimerW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(CancelWaitableTimer);

#endif // BOOST_WINAPI_PARTITION_APP_SYSTEM

    return boost::report_errors();
}
