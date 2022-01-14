/*
 *             Copyright Andrey Semashev 2018.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   mutex_abi.cpp
 * \author Andrey Semashev
 * \date   10.03.2018
 *
 * \brief  This file contains ABI test for mutex.hpp
 */

#include <boost/winapi/mutex.hpp>
#include <windows.h>
#include "abi_test_tools.hpp"

int main()
{
    BOOST_WINAPI_TEST_CONSTANT(MUTEX_ALL_ACCESS);
    BOOST_WINAPI_TEST_CONSTANT(MUTEX_MODIFY_STATE);
#if BOOST_USE_WINAPI_VERSION >= BOOST_WINAPI_VERSION_WIN6
    BOOST_WINAPI_TEST_CONSTANT(CREATE_MUTEX_INITIAL_OWNER);
#endif

#if !defined(BOOST_NO_ANSI_APIS)
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(OpenMutexA);
#endif
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(OpenMutexW);
    BOOST_WINAPI_TEST_FUNCTION_SIGNATURE(ReleaseMutex);

    return boost::report_errors();
}
