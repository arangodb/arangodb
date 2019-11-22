/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   event_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for event.hpp
 */

#include <boost/winapi/event.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_CONSTANT(EVENT_ALL_ACCESS);
    BOOST_WINAPI_TEST_CONSTANT(EVENT_MODIFY_STATE);
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
    BOOST_WINAPI_TEST_CONSTANT(CREATE_EVENT_INITIAL_SET);
    BOOST_WINAPI_TEST_CONSTANT(CREATE_EVENT_MANUAL_RESET);
#endif

#if !defined(BOOST_NO_ANSI_APIS)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(OpenEventA);
#endif
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(OpenEventW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(SetEvent);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(ResetEvent);

    return boost::report_errors();
}
