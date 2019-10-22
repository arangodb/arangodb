/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   priority_class_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for priority_class.hpp
 */

#include <boost/winapi/priority_class.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
#if BOOST_WINAPI_PARTITION_DESKTOP_SYSTEM

    BOOST_WINAPI_TEST_CONSTANT(NORMAL_PRIORITY_CLASS);
    BOOST_WINAPI_TEST_CONSTANT(IDLE_PRIORITY_CLASS);
    BOOST_WINAPI_TEST_CONSTANT(HIGH_PRIORITY_CLASS);
    BOOST_WINAPI_TEST_CONSTANT(REALTIME_PRIORITY_CLASS);
    BOOST_WINAPI_TEST_CONSTANT(BELOW_NORMAL_PRIORITY_CLASS);
    BOOST_WINAPI_TEST_CONSTANT(ABOVE_NORMAL_PRIORITY_CLASS);

#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
    BOOST_WINAPI_TEST_CONSTANT(PROCESS_MODE_BACKGROUND_BEGIN);
    BOOST_WINAPI_TEST_CONSTANT(PROCESS_MODE_BACKGROUND_END);
#endif

    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(GetPriorityClass);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(SetPriorityClass);

#endif // BOOST_WINAPI_PARTITION_DESKTOP_SYSTEM

    return boost::report_errors();
}
