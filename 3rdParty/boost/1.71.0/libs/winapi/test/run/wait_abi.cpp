/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   wait_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for wait.hpp
 */

#include <boost/winapi/wait.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_CONSTANT(INFINITE);
    BOOST_WINAPI_TEST_CONSTANT(WAIT_ABANDONED);
    BOOST_WINAPI_TEST_CONSTANT(WAIT_OBJECT_0);
    BOOST_WINAPI_TEST_CONSTANT(WAIT_TIMEOUT);
    BOOST_WINAPI_TEST_CONSTANT(WAIT_FAILED);

#if BOOST_WINAPI_PARTITION_APP || BOOST_WINAPI_PARTITION_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(WaitForSingleObjectEx);
#endif
#if BOOST_WINAPI_PARTITION_DESKTOP || BOOST_WINAPI_PARTITION_SYSTEM
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_NT4
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(SignalObjectAndWait);
#endif
#endif

#if BOOST_WINAPI_PARTITION_APP_SYSTEM
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(WaitForMultipleObjects);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(WaitForMultipleObjectsEx);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(WaitForSingleObject);
#endif

    return boost::report_errors();
}
